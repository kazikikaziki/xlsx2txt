#pragma once
#include <inttypes.h>
#include "KKeyboard.h"

namespace Kamilo {

typedef void *       _HWND;
typedef unsigned int _UINT;
typedef uintptr_t    _WPARAM;
typedef long         _LPARAM;
typedef long         _LRESULT;


enum KWindowAttr {
	KWindowAttr_RESIZABLE,
	KWindowAttr_KEEP_ASPECT,
	KWindowAttr_ASPECT_RATIO_W,
	KWindowAttr_ASPECT_RATIO_H,
	KWindowAttr_HAS_BORDER,
	KWindowAttr_HAS_TITLE,
	KWindowAttr_HAS_MAXIMIZE_BUTTON, ///< 最大化ボタンを表示（最大化を許可）
	KWindowAttr_HAS_ICONIFY_BUTTON,  ///< 最小化ボタンを表示（最小化を許可）
	KWindowAttr_HAS_CLOSE_BUTTON,    ///< 閉じるボタンを表示（ユーザーによる終了を許可）
	KWindowAttr_TOPMOST, ///< 最前面に表示（タスクバーよりも手前）
	KWindowAttr_FULLFILL, ///< フルスクリーン用ウィンドウ

	/// [WIN32]
	/// ALT キー と F10 キーのイベントを横取りする。
	/// 通常、 ALT または F10 を押した場合アプリケーションから制御が奪われ、ゲーム全体が一時停止状態になってしまう。
	/// これを回避したい場合は KILL_ALT_F10 を 1 に設定し、キーイベントを握りつぶす
	KWindowAttr_KILL_ALT_F10,

	/// [WIN32]
	/// SnapShot, PrtScr キーをシステムに任せずに自前で処理する。
	/// 通常、 PtrScr キーは WM_KEYDOWN イベントを送出しないので PtrScr キーを押したタイミングを知ることができないが、
	/// この値を 1 に設定すると PrtScr キーを押したときに KWindowEvent::KEY_DOWN イベントが発生し、
	/// キーコード KKeyboard::SNAPSHOT とともにイベント配信されるようになる
	KWindowAttr_KILL_SNAPSHOT,

	KWindowAttr_ENUM_MAX
};


class KWindowCallback {
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
	virtual void onWindowKeyDown(KKey key) {}
	virtual void onWindowKeyUp(KKey key) {}
	virtual void onWindowKeyChar(wchar_t wc) {}
	virtual void onWindowMessage(_HWND hWnd, _UINT msg, _WPARAM wParam, _LPARAM lParam, _LRESULT *p_lResult) {}
};


class KCoreWindow: public KRef {
public:
	virtual void setCallback(KWindowCallback *cb) = 0;

	/// 内部で保持しているウィンドウ識別子を返す。
	/// Windows なら HWND の値になる
	virtual void * getHandle() const = 0;

	/// ウィンドウのクライアント領域のサイズを取得する。
	/// 不要なパラメータには nullptr を指定してもよい
	/// あくまでも「現在の値」なので、最大化最小化していると意図しないサイズが入ることがある
	/// @see getWindowNormalRect
	virtual void getClientSize(int *cw, int *ch) const = 0;

	/// ウィンドウの位置を得る。
	/// 得られる座標は、ウィンドウの最外枠の左上の点の位置を、スクリーン座標系で表したもの
	/// あくまでも「現在の値」なので、最大化最小化していると意図しないサイズが入ることがある
	/// @see getWindowNormalRect
	virtual void getWindowPosition(int *x, int *y) const = 0;

	/// 通常状態でのウィンドウ位置とサイズを得る
	/// 現在のウィンドウが最大化最小化上位であっても常に通常時の値を得る
	virtual void getWindowNormalRect(int *x, int *y, int *w, int *h) const = 0;
	virtual void setWindowNormalRect(int x, int y, int w, int h) = 0;

	/// スクリーン座標からクライアント座標へ変換する
	virtual void screenToClient(int *x, int *y) const = 0;

	/// クライアント座標からスクリーン座標へ変換する
	virtual void clientToScreen(int *x, int *y) const = 0;

	/// ウィンドウが表示されているかどうか
	virtual bool isWindowVisible() const = 0;

	/// ウィンドウが入力フォーカスを持っているかどうか
	virtual bool isWindowFocused() const = 0;

	/// ウィンドウがアイコン化されているかどうか
	virtual bool isIconified() const = 0;

	/// ウィンドウが最大化されているかどうか
	virtual bool isMaximized() const = 0;

	/// ウィンドウを最大化する。
	/// フルスクリーンモードでは、画面の「すべての領域」がウィンドウによって占有される。
	/// ウィンドウの最大化では、OSによって表示されているタスクバーやステータスバーなどを除いた領域内で最大の大きさになる
	virtual void maximizeWindow() = 0;

	/// 最大化、または最小化されているウィンドウを通常の状態に戻す。
	/// ただし、フルスクリーンモードの場合は動作しない。
	/// フルスクリーンを解除するには setFullScreen(false, ...) を呼び出す
	virtual void restoreWindow() = 0;

	/// ウィンドウを表示する
	virtual void setWindowVisible(bool value) = 0;

	/// ウィンドウ位置を設定する。
	/// ウィンドウの最外枠の左上の点の位置を、スクリーン座標系で指定する
	virtual void setWindowPosition(int x, int y) = 0;

	/// ウィンドウのクライアント領域のサイズを設定する
	virtual void setClientSize(int cw, int ch) = 0;

	/// ウィンドウのタイトル文字列を設定する
	virtual void setTitle(const char *text_u8) = 0;

	virtual int getAttribute(KWindowAttr attr) const = 0;
	virtual void setAttribute(KWindowAttr attr, int value) = 0;

	// その他の細かい操作など
	virtual void command(const char *cmd, void *data) = 0;

	// 現在のマウスカーソルの位置をウィンドウクライアント座標で取得して cx, cy にセットする。
	// それがウィンドウクライアント領域内であれば true を返す
	virtual bool getMouseCursorPos(int *cx, int *cy) const = 0;

	// このウィンドウが表示されているディスプレイの座標範囲。
	// マルチモニター環境で、サブ画面をメイン画面の左側に設定している場合などは
	// 負の座標になる場合がある
	virtual bool getOwnerDisplayRect(int *x, int *y, int *w, int *h) const = 0;

	// ウィンドウ位置とサイズがスクリーン画面の外側に行ってしまわないように調整した値を得る
	// ※このメソッドは調整後の値を計算するだけで、ウィンドウの位置もサイズも変更しない
	virtual bool adjustWindowPosAndSize(int *x, int *y, int *w, int *h) const = 0;
};


KCoreWindow * createCoreWindow(int w, int h, const char *text_u8);


class KWindow {
public:
	static bool init(int w, int h, const char *text_u8);
	static void shutdown();
	static KCoreWindow * get();

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
};

} // namespace
