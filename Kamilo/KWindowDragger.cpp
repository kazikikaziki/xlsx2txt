#include "KWindowDragger.h"
//
#include "KImGui.h"
#include "KInternal.h"
#include "keng_game.h"
#include "KSig.h"
#include "KWindow.h"

namespace Kamilo {




class CWindowDrag: public KManager {
	KVec3 m_last_mouse_pos;
	bool m_dragging;
	int m_grab_x;
	int m_grab_y;
	int m_locked;
public:
	CWindowDrag() {
		m_dragging = false;
		m_grab_x = 0;
		m_grab_y = 0;
		m_locked = 0;
		KEngine::addManager(this);
	}
	virtual void on_manager_start() override {
		m_dragging = false;
		m_grab_x = 0;
		m_grab_y = 0;
		m_locked = 0;
	}
	virtual void on_manager_signal(KSig &sig) override {
		if (sig.check(K_SIG_WINDOW_MOUSE_DOWN)) {
			// マウスボタンが押された。
			// 左ボタンかつ修飾キーを押していない場合に限ってドラッグ開始チェックする
			int btn = sig.getInt("button");
			if (isDragOK(btn)) {
				m_dragging = true;
				// ウィンドウ左上からのオフセットを入れる
				int wnd_x, wnd_y;
				KEngine::getWindow()->getWindowPosition(&wnd_x, &wnd_y);
				int x = sig.getInt("x");
				int y = sig.getInt("y");
				KEngine::getWindow()->clientToScreen(&x, &y);
				m_grab_x = x - wnd_x;
				m_grab_y = y - wnd_y;
			}
			return;
		}

		if (sig.check(K_SIG_WINDOW_MOUSE_MOVE)) {
			if (m_dragging) {
				int btn = sig.getInt("button");
				// 念のためにボタン状態は常にチェックしておく。
				// （何らかの原因でボタンを離したのにドラッグ状態が継続してしまった場合、
				// すぐにウィンドウ移動を辞められるように）
				if (isDragOK(btn)) {
					int x = sig.getInt("x");
					int y = sig.getInt("y");
					KEngine::getWindow()->clientToScreen(&x, &y);
					// このクライアント座標に対応する
					int wnd_x = x - m_grab_x;
					int wnd_y = y - m_grab_y;
					KEngine::moveWindow(wnd_x, wnd_y); // KEngine::getWindow()->setWindowPosition は使わない。フルスクリーンの時にウィンドウ移動しないように、KEngine を通す
				} else {
					m_dragging = false;
				}
			}
			return;
		}

		if (sig.check(K_SIG_WINDOW_MOUSE_UP)) {
			m_dragging = false;
			return;
		}
	}
	void lock() {
		m_locked++;
	}
	void unlock() {
		m_locked--;
	}
	// ドラッグ開始またはドラッグ継続ができるかどうか
	bool isDragOK(int btn) const {
		if (m_locked) {
			return false;
		}

		// 左ボタンかつ修飾キーを押していない場合のみ
		// ※処理が重い場合にマウスイベントが遅延して入ってくる場合がある。
		// GetAsyncKeyStateで「今この瞬間に」ボタンが押されているかで判別する
		if (btn != 0) { // 0=Left 1=Right 2=Middle
			return false;
		}
		if (!KKeyboard::matchModifiers(0)) {
			return false; // 修飾キーが押されていたらダメ
		}
		// フルスクリーンだったらダメ
		if (!KEngine::getStatus(KEngine::ST_VIDEO_IS_WINDOWED)) {
			return false;
		}
		// 最大化されていたらダメ
		if (KEngine::getStatus(KEngine::ST_WINDOW_IS_MAXIMIZED)) {
			return false;
		}
		// マウスカーソルが ImGUI のコントロール上にあるか、入力中だったらダメ
		if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)) {
			return false;
		}
		if (ImGui::IsAnyItemHovered()) {
			return false;
		}
		if (ImGui::IsAnyItemActive() || ImGui::IsAnyItemFocused()) {
			return false;
		}
		return true;
	}
};
static CWindowDrag *g_WindowDrag = nullptr;


#pragma region KWindowDragger
void KWindowDragger::install() {
	g_WindowDrag = new CWindowDrag();
}
void KWindowDragger::uninstall() {
	if (g_WindowDrag) {
		g_WindowDrag->drop();
		g_WindowDrag = nullptr;
	}
}
void KWindowDragger::lock() {
	K__ASSERT(g_WindowDrag);
	g_WindowDrag->lock();
}
void KWindowDragger::unlock() {
	K__ASSERT(g_WindowDrag);
	g_WindowDrag->unlock();
}
#pragma endregion // KWindowDragger

} // namespace
