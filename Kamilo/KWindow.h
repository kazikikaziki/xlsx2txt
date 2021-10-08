#pragma once
#include <inttypes.h>
#include "KKeyboard.h"

namespace Kamilo {

typedef void *       _HWND;
typedef unsigned int _UINT;
typedef uintptr_t    _WPARAM;
typedef long         _LPARAM;
typedef long         _LRESULT;

class KWindow {
public:
	static bool init(int w, int h, const char *text_u8);
	static void shutdown();
	static bool isInit();

	/// メインループを継続するかどうかの判定と処理を行う。
	/// 必ず毎フレームの先頭で呼び出し、
	/// 戻り値をチェックすること。
	/// @retval true  ウィンドウは有効である
	/// @retval false ウィンドウを閉じ、アプリケーション終了の準備が整った
	static bool processEvents();

	/// ウィンドウイベントが来るまで待機し、メインループを継続するかどうかの判定と処理を行う。
	/// ゲームなど定期的に更新する必要のないアプリケーション向け。
	/// @snippet KWindow.cpp Test_window
	static bool processEventsWait();

public:
	enum Attr {
		ATTR_RESIZABLE,
		ATTR_KEEP_ASPECT,
		ATTR_ASPECT_RATIO_W,
		ATTR_ASPECT_RATIO_H,
		ATTR_HAS_BORDER,
		ATTR_HAS_TITLE,
		ATTR_HAS_MAXIMIZE_BUTTON, ///< 最大化ボタンを表示（最大化を許可）
		ATTR_HAS_ICONIFY_BUTTON,  ///< 最小化ボタンを表示（最小化を許可）
		ATTR_HAS_CLOSE_BUTTON,    ///< 閉じるボタンを表示（ユーザーによる終了を許可）
		ATTR_TOPMOST, ///< 最前面に表示（タスクバーよりも手前）
		ATTR_FULLFILL, ///< フルスクリーン用ウィンドウ

		/// [WIN32]
		/// ALT キー と F10 キーのイベントを横取りする。
		/// 通常、 ALT または F10 を押した場合アプリケーションから制御が奪われ、ゲーム全体が一時停止状態になってしまう。
		/// これを回避したい場合は KILL_ALT_F10 を 1 に設定し、キーイベントを握りつぶす
		ATTR_KILL_ALT_F10,

		/// [WIN32]
		/// SnapShot, PrtScr キーをシステムに任せずに自前で処理する。
		/// 通常、 PtrScr キーは WM_KEYDOWN イベントを送出しないので PtrScr キーを押したタイミングを知ることができないが、
		/// この値を 1 に設定すると PrtScr キーを押したときに KWindowEvent::KEY_DOWN イベントが発生し、
		/// キーコード KKeyboard::SNAPSHOT とともにイベント配信されるようになる
		ATTR_KILL_SNAPSHOT,

		ATTR_ENUM_MAX
	};
	class Callback {
	public:
		virtual void onWindowClosing() {}
		virtual void onWindowDropFile(int index, int total, const char *filename_u8) {}
		virtual void onWindowMove(int x, int y) {}
		virtual void onWindowResize(int x, int y) {}
		virtual void onWindowMouseEnter(int x, int y) {}
		virtual void onWindowMouseExit() {}
		virtual void onWindowMouseWheel(int x, int y, int delta) {}
		virtual void onWindowMouseMove(int x, int y, int btn) {}
		virtual void onWindowMouseButtonDown(int x, int y, int btn) {}
		virtual void onWindowMouseButtonUp(int x, int y, int btn) {}
		virtual void onWindowKeyDown(KKeyboard::Key key) {}
		virtual void onWindowKeyUp(KKeyboard::Key key) {}
		virtual void onWindowKeyChar(wchar_t wc) {}
		virtual void onWindowMessage(_HWND hWnd, _UINT msg, _WPARAM wParam, _LPARAM lParam, _LRESULT *p_lResult) {}
	};
	static void setCallback(Callback *cb);

	/// 内部で保持しているウィンドウ識別子を返す。
	/// Windows なら HWND の値になる
	static void * getHandle();

	/// ウィンドウのクライアント領域のサイズを取得する。
	/// 不要なパラメータには nullptr を指定してもよい
	static void getClientSize(int *cw, int *ch);

	/// ウィンドウの位置を得る。
	/// 得られる座標は、ウィンドウの最外枠の左上の点の位置を、スクリーン座標系で表したもの
	static void getWindowPosition(int *x, int *y);

	/// スクリーン座標からクライアント座標へ変換する
	static void screenToClient(int *x, int *y);

	/// クライアント座標からスクリーン座標へ変換する
	static void clientToScreen(int *x, int *y);

	/// ウィンドウが表示されているかどうか
	static bool isWindowVisible();

	/// ウィンドウが入力フォーカスを持っているかどうか
	static bool isWindowFocused();

	/// ウィンドウがアイコン化されているかどうか
	static bool isIconified();

	/// ウィンドウが最大化されているかどうか
	static bool isMaximized();

	/// ウィンドウを最大化する。
	/// フルスクリーンモードでは、画面の「すべての領域」がウィンドウによって占有される。
	/// ウィンドウの最大化では、OSによって表示されているタスクバーやステータスバーなどを除いた領域内で最大の大きさになる
	static void maximizeWindow();

	/// 最大化、または最小化されているウィンドウを通常の状態に戻す。
	/// ただし、フルスクリーンモードの場合は動作しない。
	/// フルスクリーンを解除するには setFullScreen(false, ...) を呼び出す
	static void restoreWindow();

	/// ウィンドウを表示する
	static void setWindowVisible(bool value);

	/// ウィンドウ位置を設定する。
	/// ウィンドウの最外枠の左上の点の位置を、スクリーン座標系で指定する
	static void setWindowPosition(int x, int y);

	/// ウィンドウのクライアント領域のサイズを設定する
	static void setClientSize(int cw, int ch);

	/// ウィンドウのタイトル文字列を設定する
	static void setTitle(const char *text_u8);
	/*
	/// ウィンドウのアプリケーションアイコンを設定する
	static void setIcon(const KIcon &icon);
	*/
	static int getAttribute(Attr attr);
	static void setAttribute(Attr attr, int value);

	// その他の細かい操作など
	static void command(const char *cmd, void *data);

	// 現在のマウスカーソルの位置をウィンドウクライアント座標で取得して cx, cy にセットする。
	// それがウィンドウクライアント領域内であれば true を返す
	static bool getMouseCursorPos(int *cx, int *cy);
};

} // namespace
