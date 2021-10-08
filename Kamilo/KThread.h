#pragma once
#include <inttypes.h>

namespace Kamilo {


#pragma region Thread
class KThread {
public:
	KThread();
	virtual ~KThread();
	virtual void run() = 0;
	void start();
	void stop();
	bool isRunning() const;
	bool shouldExit() const;
private:
	void *m_thread; // HANDLE
	bool m_exit;
};

#pragma endregion


} // namespace
