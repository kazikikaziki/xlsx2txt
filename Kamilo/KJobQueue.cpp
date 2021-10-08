#include "KJobQueue.h"
//
#include <Windows.h>
#include <mutex>
#include <vector>
#include <unordered_set>
#include "KInternal.h"

namespace Kamilo {

static const int JOBQUEUE_IDLE_WAIT_MSEC = 5;

class CJobQueueImpl {
	struct JQITEM {
		KJOBID id;
		K_JobFunc runfunc;
		K_JobFunc delfunc;
		void *data;
	};
	static void jq_sleep(int msec) {
		::Sleep(msec);
	}
	static void jq_deljob(JQITEM *job) {
		if (job) {
			if (job->delfunc) {
				job->delfunc(job->data);
			}
			delete job;
		}
	}
	static void jq_mainloop(CJobQueueImpl *q) {
		K__ASSERT(q);
		while (!q->m_ShouldAbort) {
			// 次のジョブ
			q->m_Mutex.lock();
			if (q->m_WaitingJobs.empty()) {
				K__ASSERT(q->m_Job == NULL);
			} else {
				q->m_Job = q->m_WaitingJobs.front();
				q->m_WaitingJobs.erase(q->m_WaitingJobs.begin());
			}
			q->m_Mutex.unlock();

			// ジョブがあれば実行。なければ待機
			if (q->m_Job) {
				// 実行
				q->m_Job->runfunc(q->m_Job->data);
				q->m_FinishedJobs.insert(q->m_Job->id);
				jq_deljob(q->m_Job);
				q->m_Job = NULL;

			} else {
				// 待機
				jq_sleep(JOBQUEUE_IDLE_WAIT_MSEC);
			}
		}
	}
private:
	KJOBID m_LastJobId; // 最後に発行したジョブ識別子
	JQITEM *m_Job; // 実行中のジョブ
	std::vector<JQITEM*> m_WaitingJobs; // 待機中のジョブ
	std::unordered_set<KJOBID> m_FinishedJobs; // 完了のジョブ
	std::mutex m_Mutex;
	std::thread m_Thread; // ジョブを処理するためのスレッド
	bool m_ShouldAbort; // 中断命令
public:
	CJobQueueImpl() {
		m_LastJobId = 0;
		m_Job = NULL;
		m_ShouldAbort = false;
		m_Thread = std::thread(jq_mainloop, this);
	}
	~CJobQueueImpl() {
		// ジョブを空っぽにする
		clearJobs();

		// ジョブ処理スレッドを停止
		m_ShouldAbort = true;
		if (m_Thread.joinable()) {
			m_Thread.join();
		}
	}
	KJOBID pushJob(K_JobFunc runfunc, K_JobFunc delfunc, void *data) {
		m_Mutex.lock();
		m_LastJobId++;

		JQITEM *job = new JQITEM;
		job->runfunc = runfunc;
		job->delfunc = delfunc;
		job->data = data;
		job->id = m_LastJobId;
		m_WaitingJobs.push_back(job);
		m_Mutex.unlock();
		return m_LastJobId; // = job.id;
	}
	bool removeJob(KJOBID job_id) {
		bool retval = false;
		m_Mutex.lock();
		if (m_Job && m_Job->id == job_id) {
			// 実行中のジョブは削除できない
			retval = false;

		} else if (m_FinishedJobs.find(job_id) != m_FinishedJobs.end()) {
			// 完了リストから削除
			m_FinishedJobs.erase(job_id);
			retval = true;

		} else {
			// 削除した場合でも、もともとキューになくて削除しなかった場合でも、
			// 指定された JOBID がキューに存在しないことに変わりはないので成功とする
			retval = true;
			for (size_t i=0; i<m_WaitingJobs.size(); i++) {
				if (m_WaitingJobs[i]->id == job_id) {
					m_WaitingJobs.erase(m_WaitingJobs.begin() + i);
					break;
				}
			}
		}
		m_Mutex.unlock();
		return retval;
	}
	KJobQueue::Stat getJobState(KJOBID job_id) {
		KJobQueue::Stat retval = KJobQueue::STAT_INVALID;
		m_Mutex.lock();
		if (m_Job && m_Job->id == job_id) {
			// ジョブは実行中
			retval = KJobQueue::STAT_RUNNING;

		} else if (m_FinishedJobs.find(job_id) != m_FinishedJobs.end()) {
			// ジョブは実行済み
			retval = KJobQueue::STAT_DONE;

		} else {
			// 待機中？
			for (size_t i=0; i<m_WaitingJobs.size(); i++) {
				const JQITEM *job = m_WaitingJobs[i];
				if (job->id == job_id) {
					retval = KJobQueue::STAT_WAITING;
					break;
				}
			}
		}
		m_Mutex.unlock();
		return retval;
	}
	int getRestJobCount() {
		int cnt;
		m_Mutex.lock();
		cnt = (int)m_WaitingJobs.size(); // 未実行のジョブ
		if (m_Job) cnt++; // 実行中のジョブ
		m_Mutex.unlock();
		return cnt;
	}
	bool raiseJobPriority(KJOBID job_id) {
		// ジョブが待機列に入っているなら、待機列先頭に移動する。
		// ジョブが先頭に移動した（または初めから先頭にいた）なら true を返す
		bool retval = false;
		m_Mutex.lock();

		// 目的のジョブはどこにいる？
		int job_index = -1;
		{
			for (size_t i=0; i<m_WaitingJobs.size(); i++) {
				JQITEM *job = m_WaitingJobs[i];
				if (job->id == job_id) {
					job_index = (int)i;
					break;
				}
			}
		}

		if (job_index == 0) {
			// 既に先頭にいる
			retval = true;

		} else if (job_index > 0) {
			// 待機列の先頭以外の場所にいる。
			// 先頭へ移動する
			JQITEM *job = m_WaitingJobs[job_index];
			m_WaitingJobs.erase(m_WaitingJobs.begin() + job_index);
			m_WaitingJobs.insert(m_WaitingJobs.begin(), job);
			K::print("Job(%d) moved to top of the waiting list", job_id);
			retval = true;
		}

		m_Mutex.unlock();
		return retval;
	}
	void waitJob(KJOBID job_id) {
		// 待機中の場合は、非待機状態になる待つ。
		// ちなみに「KJobStart_Done になるまで待機」という方法はダメ。
		// abort 等で強制中断命令が出た場合など KJobStart_Done にならないままジョブが削除される場合がある
		while (getJobState(job_id) == KJobQueue::STAT_WAITING) {
			jq_sleep(JOBQUEUE_IDLE_WAIT_MSEC);
		}

		// 処理中になっている場合は、非処理状態になるまで待つ
		while (getJobState(job_id) == KJobQueue::STAT_RUNNING) {
			jq_sleep(JOBQUEUE_IDLE_WAIT_MSEC);
		}
	}
	void waitAllJobs() {
		while (getRestJobCount() > 0) {
			jq_sleep(JOBQUEUE_IDLE_WAIT_MSEC);
		}
	}
	void clearJobs() {
		// 現時点で待機列にあるジョブを削除
		m_Mutex.lock();
		{
			for (size_t i=0; i<m_WaitingJobs.size(); i++) {
				jq_deljob(m_WaitingJobs[i]);
			}
			m_WaitingJobs.clear();
		}
		m_Mutex.unlock();

		// 実行中のジョブの終了を待つ
		waitAllJobs();
	}
};

#pragma region KJobQueue
KJobQueue::KJobQueue() {
	CJobQueueImpl *impl = new CJobQueueImpl();
	m_Impl = std::shared_ptr<CJobQueueImpl>(impl);
}
KJOBID KJobQueue::pushJob(K_JobFunc runfunc, K_JobFunc delfunc, void *data) {
	return m_Impl->pushJob(runfunc, delfunc, data);
}
int KJobQueue::getRestJobCount() {
	return m_Impl->getRestJobCount();
}
bool KJobQueue::raiseJobPriority(KJOBID job_id) {
	return m_Impl->raiseJobPriority(job_id);
}
KJobQueue::Stat KJobQueue::getJobState(KJOBID job_id) {
	return m_Impl->getJobState(job_id);
}
void KJobQueue::waitJob(KJOBID job_id) {
	return m_Impl->waitJob(job_id);
}
void KJobQueue::waitAllJobs() {
	return m_Impl->waitAllJobs();
}
bool KJobQueue::removeJob(KJOBID job_id) {
	return m_Impl->removeJob(job_id);
}
void KJobQueue::clearJobs() {
	m_Impl->clearJobs();
}
#pragma endregion // KJobQueue

} // namespace
