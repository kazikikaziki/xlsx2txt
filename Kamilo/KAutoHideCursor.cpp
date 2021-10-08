#include "KAutoHideCursor.h"
//
#include "keng_game.h"
#include "KInternal.h"
#include "KWindow.h"
#include "KSig.h"

namespace Kamilo {

class CAutoHideCursor: public KManager {
public:
	uint32_t m_Time;
	bool m_Hiding;

	CAutoHideCursor() {
		m_Time = K::clockMsec32();
		m_Hiding = false;
		KEngine::addManager(this);
	}
	virtual void on_manager_frame() override {
		if (!m_Hiding) {
			uint32_t diff = K::clockMsec32() - m_Time;
			if (diff >= 3 * 1000) {
				m_Hiding = true;
				KWindow::command("hide_cursor", NULL);
			}
		}
	}
	virtual void on_manager_signal(KSig &sig) override {
		if (sig.check(K_SIG_WINDOW_MOUSE_MOVE)) {
			m_Time = K::clockMsec32();
			if (m_Hiding) {
				m_Hiding = false;
				KWindow::command("show_cursor", NULL);
			}
			return;
		}
	}
};
	
static CAutoHideCursor *g_AutoHideCursor = NULL;

void KAutoHideCursor::install() {
	g_AutoHideCursor = new CAutoHideCursor();
}
void KAutoHideCursor::uninstall() {
	if (g_AutoHideCursor) {
		g_AutoHideCursor->drop();
		g_AutoHideCursor = NULL;
	}
}

} // namespace

