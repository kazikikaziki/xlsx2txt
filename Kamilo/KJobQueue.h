#pragma once
#include <memory> // std::shared_ptr

namespace Kamilo {

typedef int KJOBID;

typedef void (*K_JobFunc)(void *data);

class CJobQueueImpl; // internal

class KJobQueue {
public:
	enum Stat {
		STAT_INVALID, // 無効なジョブ
		STAT_RUNNING, // 実行中
		STAT_WAITING, // 待機列で実行待ち中
		STAT_DONE,    // 完了している
	};

	KJobQueue();

	/// ジョブを追加する
	///
	/// 追加したジョブを識別するための値を返す
	/// runfunc ジョブとして実行する関数
	/// delfunc データを削除するための関数。不要な場合は NULL でも良い
	/// data ジョブ関数の引数。ジョブ終了時にデータを削除する必要がある場合は delfunc を指定する
	KJOBID pushJob(K_JobFunc runfunc, K_JobFunc delfunc, void *data);

	/// キューに残っているジョブの数を返す。実行中のジョブと、待機列のジョブを合計した値になる
	int getRestJobCount();

	/// ジョブが実行待ち状態だった場合はキューの先頭に移動する
	/// すでに実行中だった場合は何もしない
	bool raiseJobPriority(KJOBID job_id);

	/// ジョブの状態を返す
	Stat getJobState(KJOBID job_id);

	/// ジョブが終わるまで待つ
	void waitJob(KJOBID job_id);

	/// すべてのジョブが終わるまで待つ
	void waitAllJobs();

	/// ジョブを削除する
	/// 削除できなかった場合は false を返す。削除したか、元々存在しないジョブだった場合は true を返す
	bool removeJob(KJOBID job_id);

	/// ジョブを削除してキューを空にする。
	/// 実行中のジョブが完了するまで待ち、待機列のジョブは未実行のまま全て削除される
	void clearJobs();

private:
	std::shared_ptr<CJobQueueImpl> m_Impl;
};

} // namespace
