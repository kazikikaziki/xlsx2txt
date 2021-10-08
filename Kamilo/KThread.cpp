#include "KThread.h"
//
#include <process.h> // _beginthreadex
#include <Windows.h>

namespace Kamilo {

static unsigned int CALLBACK _ThreadCallback(void *data) {
	KThread *th = reinterpret_cast<KThread *>(data);
	th->run();
	return 0;
}
KThread::KThread() {
	m_thread = nullptr;
	m_exit = false;
}
KThread::~KThread() {
	stop();
}
void KThread::start() {
	m_thread = (HANDLE)_beginthreadex(nullptr, 0, _ThreadCallback, this, 0, nullptr);
}
void KThread::stop() {
	m_exit = true;
	WaitForSingleObject((HANDLE)m_thread, INFINITE);
	m_thread = nullptr;
}
bool KThread::shouldExit() const {
	return m_exit;
}
bool KThread::isRunning() const {
	return WaitForSingleObject((HANDLE)m_thread, 0) == WAIT_TIMEOUT;
}

} // namespace
