#include "KWindowResizer.h"
//
#include "keng_game.h"
#include "KInternal.h"
#include "KSig.h"
#include "KKeyboard.h"
#include "KScreen.h"

namespace Kamilo {

class CWndResize {
	int m_BaseW;
	int m_BaseH;
public:
	CWndResize() {
		m_BaseW = 0;
		m_BaseH = 0;
	}
	CWndResize(int base_w, int base_h) {
		init(base_w, base_h);
	}
	void init(int base_w, int base_h) {
		m_BaseW = base_w;
		m_BaseH = base_h;
	}
	float getMaxScale() const {
		// ウィンドウに適用可能な最大倍率を返す。
		// 横方向と縦方向の倍率のうち小さい方を返す。
		// このメソッドはマルチモニタをサポートするため、
		// モニタごとに解像度が異なっていても正しく動作する
		int max_w = KEngine::getStatus(KEngine::ST_SCREEN_W);
		int max_h = KEngine::getStatus(KEngine::ST_SCREEN_H);
		float max_scale_w = (float)max_w / m_BaseW;
		float max_scale_h = (float)max_h / m_BaseH;
		return KMath::min(max_scale_w, max_scale_h);
	}
	void getCurrentScale(float *horz, float *vert) const {
		// 現在のウィンドウ倍率を、横方向と縦方向それぞれ返す
		int wnd_w = KEngine::getStatus(KEngine::ST_WINDOW_CLIENT_SIZE_W);
		int wnd_h = KEngine::getStatus(KEngine::ST_WINDOW_CLIENT_SIZE_H);
		if (horz) *horz = (float)wnd_w / m_BaseW;
		if (vert) *vert = (float)wnd_h / m_BaseH;
	}
	void applyScale(int scale) {
		// 倍率を指定してウィンドウサイズを変更する。
		// 最大化状態だった場合はこれを解除する
		int new_w = m_BaseW * scale;
		int new_h = m_BaseH * scale;
		KEngine::resizeWindow(new_w, new_h);
	}
	void applyMaximize(bool full) {
		// ウィンドウを最大化する
		if (full) {
			KEngine::setWindowMode(KEngine::WND_WINDOWED_FULLSCREEN);
		} else {
			KEngine::setWindowMode(KEngine::WND_MAXIMIZED);
		}
	}
	void restoreWindow() {
		// ウィンドウモードに戻す。
		// KWindow::restoreWindow とは異なり、
		// ボーダレスウィンドウからの復帰も含む
		KEngine::setWindowMode(KEngine::WND_WINDOWED);
	}
	void resize(bool zoom) {
		// ウィンドウサイズを逐次切り替えする
		// zoom 拡大方向に切り替えるなら true、縮小方向に切り替えるなら false
		bool full = KEngine::getStatus(KEngine::ST_VIDEO_IS_WINDOWED_FULLSCREEN) != 0;

		// （本当の）フルスクリーンモードの場合は切り替え要求を無視する
		if (KEngine::getStatus(KEngine::ST_VIDEO_IS_FULLSCREEN)) {
			return;
		}

		// 適用可能な最大ウィンドウ倍率
		int maxscale = (int)getMaxScale();
	
		if (KEngine::getStatus(KEngine::ST_WINDOW_IS_MAXIMIZED)) {
			// 現在最大化状態になっている。

			if (zoom) {
				// 拡大方向へ切り替える。
				if (full) {
					// すでに最大化状態なので、等倍に戻る
					applyScale(1);
				} else {
					// なんちゃってフルスクリーン
					applyMaximize(true);
				}
			} else {
				// 縮小方向へ切り替える
				if (full) {
					// なんちゃってフルスクリーン
					applyMaximize(false);
				} else {
					applyScale(maxscale);
				}
			}
	
		} else {
			// 現在通常ウィンドウ状態になっている。
			if (zoom) {
				// 拡大方向へ切り替える。

				// ウィンドウサイズはユーザーによって変更されている可能性がある。
				// 現在の実際のウィンドウサイズを元にして、何倍で表示されているかを求める
				float scalex, scaley;
				getCurrentScale(&scalex, &scaley);

				int newscale;
				if (KMath::equals(scalex, scaley, 0.01f)) {
					// アスペクト比が（ほとんど）正しい場合はそのまま1段階上の倍率にする
					newscale = (int)floorf(scalex) + 1;

				} else {
					// ぴったりの倍率になっていない場合は、まず長い方の倍率に合わせる
					newscale = (int)ceilf(KMath::max(scalex, scaley));
				}

				// 最大倍率を超えた場合は最大化する
				if (newscale <= maxscale) {
					applyScale(newscale);
				} else {
					applyMaximize(false);
				}

			} else {
				// 縮小方向へ切り替える。

				// ウィンドウサイズはユーザーによって変更されている可能性がある。
				// 現在の実際のウィンドウサイズを元にして、何倍で表示されているかを求める
				float scalex, scaley;
				getCurrentScale(&scalex, &scaley);

				int newscale;
				if (KMath::equals(scalex, scaley, 0.01f)) {
					// アスペクト比が（ほとんど）正しい場合はそのまま1段階下の倍率にする
					newscale = (int)ceilf(scalex) - 1;

				} else {
					// ぴったりの倍率になっていない場合は、まず短い方の倍率に合わせる
					newscale = (int)KMath::min(scalex, scaley);
				}

				// 等倍を下回った場合は最大化する
				if (newscale >= 1) {
					applyScale(newscale);
				} else {
					applyMaximize(true);
				}
			}
		}
	}
}; // CWndResize



class CWindowResizer: public KManager {
	CWndResize m_Sizer;
public:
	CWindowResizer() {
		KEngine::addManager(this);
	}
	virtual void on_manager_signal(KSig &sig) override {
		if (sig.check(K_SIG_WINDOW_KEY_DOWN)) {
			// ウィンドウイベントは別スレッドから飛んでくる場合があることに注意
			// ビデオモードの変更などは必ずメインスレッドで行うようにしておく
			KKey key = (KKey)sig.getInt("key");
			bool shift = KKeyboard::isKeyDown(KKey_SHIFT);
			bool alt = KKeyboard::isKeyDown(KKey_ALT);
			if (key == KKey_ENTER && alt) {
				// Alt + Enter
				if (isWindowed()) {
					setFullscreenTruly();
				} else {
					setWindowed();
				}
			}
			if (key == KKey_F11) {
				// [F11] or [Shift + F11]
				if (!isWindowed()) {
					// 真のフルスクリーンだったらいったんウィンドウモードに戻す
					setWindowed();

				} else {
					if (shift) {
						// Shift + F11
						resizePrev();

					} else {
						// F11
						resizeNext();
					}
				}
			}
			return;
		}
	}
	virtual void on_manager_start() override {
		int w = 0;
		int h = 0;
		KScreen::getGameSize(&w, &h);
		m_Sizer.init(w, h);
	}
	bool isFullscreenTruly() {
		return KEngine::getStatus(KEngine::ST_VIDEO_IS_FULLSCREEN) != 0;
	}
	bool isFullscreenBorderless() {
		return KEngine::getStatus(KEngine::ST_VIDEO_IS_WINDOWED_FULLSCREEN) != 0;
	}
	bool isWindowed() {
		return KEngine::getStatus(KEngine::ST_VIDEO_IS_WINDOWED) != 0;
	}
	void setFullscreenTruly() {
		KEngine::setWindowMode(KEngine::WND_FULLSCREEN);
	}
	void setFullscreenBorderless() {
		KEngine::setWindowMode(KEngine::WND_WINDOWED_FULLSCREEN);
		m_Sizer.applyMaximize(true);
	}
	void setWindowed() {
		m_Sizer.applyScale(1);
	}
	void resizeNext() {
		m_Sizer.resize(true); // 拡大
	}
	void resizePrev() {
		m_Sizer.resize(false); // 縮小
	}
}; // CWindowResizer

#pragma region KWindowResizer
static CWindowResizer *g_Resizer = nullptr;

void KWindowResizer::install() {
	g_Resizer = new CWindowResizer();
}
void KWindowResizer::uninstall() {
	if (g_Resizer) {
		g_Resizer->drop();
		g_Resizer = nullptr;
	}
}
bool KWindowResizer::isFullscreenTruly() {
	K__ASSERT(g_Resizer);
	return g_Resizer->isFullscreenTruly();
}
bool KWindowResizer::isFullscreenBorderless() {
	K__ASSERT(g_Resizer);
	return g_Resizer->isFullscreenBorderless();
}
bool KWindowResizer::isWindowed() {
	K__ASSERT(g_Resizer);
	return g_Resizer->isWindowed();
}
void KWindowResizer::setFullscreenTruly() {
	K__ASSERT(g_Resizer);
	g_Resizer->setFullscreenTruly();
}
void KWindowResizer::setFullscreenBorderless() {
	K__ASSERT(g_Resizer);
	g_Resizer->setFullscreenBorderless();
}
void KWindowResizer::setWindowed() {
	K__ASSERT(g_Resizer);
	g_Resizer->setWindowed();
}
void KWindowResizer::resizeNext() {
	K__ASSERT(g_Resizer);
	g_Resizer->resizeNext();
}
void KWindowResizer::resizePrev() {
	K__ASSERT(g_Resizer);
	g_Resizer->resizePrev();
}
#pragma endregion // KWindowResizer



} // namespace
