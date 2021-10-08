#include "KRef.h"

// 参照カウンタの整合性を調べる
#ifndef K_REFCNT_DEBUG
#	define K_REFCNT_DEBUG 1
#endif

#include <unordered_set>
#include <mutex>
#include "KInternal.h"

namespace Kamilo {


class CRefChecker {
	std::unordered_set<KRef*> m_LockedObjects;
public:
	CRefChecker() {
	}
	~CRefChecker() {
		check_leak();
	}
	void check_leak() {
		if (m_LockedObjects.empty()) return;

		if (K::_IsDebuggerPresent()) {
			for (auto it=m_LockedObjects.begin(); it!=m_LockedObjects.end(); ++it) {
				KRef *ref = *it;
				// ログ出力中に参照カウンタが操作されないように注意
				const char *s = ref->getReferenceDebugString();
				K::print("%s: %s", typeid(*ref).name(), s ? s : "");
			}
		}
		K::print("%d object(s) are still remain.", m_LockedObjects.size());
		if (K::_IsDebuggerPresent()) {
			// ここに到達してブレークした場合、参照カウンタがまだ残っているオブジェクトがある。
			// デバッガーで m_LockedObjects の中身をチェックすること。
			m_LockedObjects; // <-- 中身をチェック
			K__ASSERT(0);
		}
	}
	void add(KRef *ref) {
		K__ASSERT(ref);
		m_LockedObjects.insert(ref);
	}
	void del(KRef *ref) {
		K__ASSERT(ref);
		m_LockedObjects.erase(ref);
	}
};



static CRefChecker g_RefChecker;
static std::mutex g_RefMutex;


#pragma region KRef
KRef::KRef() {
	m_RefCnt = 1;
	m_DebubBreakRefCnt = -1;
	if (K_REFCNT_DEBUG) {
		// 参照カウンタの整合性をチェック
		g_RefMutex.lock();
		g_RefChecker.add(this);
		g_RefMutex.unlock();
	}
}
KRef::~KRef() {
	K__ASSERT(m_RefCnt == 0); // ここで引っかかった場合、 drop しないで直接 delete してしまっている
	if (K_REFCNT_DEBUG) {
		// 参照カウンタの整合性をチェック
		g_RefMutex.lock();
		g_RefChecker.del(this);
		g_RefMutex.unlock();
	}
}
void KRef::grab() const {
	g_RefMutex.lock();
	m_RefCnt++;
	g_RefMutex.unlock();
}
void KRef::drop() const {
	g_RefMutex.lock();
	m_RefCnt--;
	g_RefMutex.unlock();
	K__ASSERT(m_RefCnt >= 0);
	if (m_RefCnt == m_DebubBreakRefCnt) { // 参照カウンタが m_DebubBreakRefCnt になったら中断する
		K::_break();
	}
	if (m_RefCnt == 0) {
		delete this;
	}
}
int KRef::getReferenceCount() const {
	return m_RefCnt;
}
const char * KRef::getReferenceDebugString() const {
	return nullptr;
}
void KRef::setReferenceDebugBreak(int cond_refcnt) {
	m_DebubBreakRefCnt = cond_refcnt;
}
#pragma endregion // KRef


} // namespace
