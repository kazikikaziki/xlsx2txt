#include "KWindow.h"

#include <Windows.h>
#include <WindowsX.h>
#include <vector>
#include "KInternal.h"
#include "KKeyboard.h"

#define USE_WINDOW_THREAD 0 // ウィンドウリサイズ時にゲーム画面がリサイズされない？

#if USE_WINDOW_THREAD
#include <mutex>
#include "KThread.h"
#endif

namespace Kamilo {

static bool _streq(const char *s, const char *t) {
	return s && t && strcmp(s, t)==0;
}

#if 0
#	define _DEBUG_PRINT(fmt, ...) K::print(fmt, ##__VA_ARGS__)
#else
#	define _DEBUG_PRINT(fmt, ...)
#endif


#pragma region KIcon
class CWin32ResourceIcons {
public:
	typedef std::vector<HICON> std_vector_HICON;
	
	static BOOL CALLBACK callback(HINSTANCE instance, LPCTSTR type, LPTSTR name, void *std_vector_hicon) {
		std_vector_HICON *icons = reinterpret_cast<std_vector_HICON *>(std_vector_hicon);
		HICON icon = LoadIcon(instance, name);
		if (icon) {
			icons->push_back(icon);
		}
		return TRUE;
	}

public:
	std::vector<HICON> m_icons;

public:
	CWin32ResourceIcons() {
		// この実行ファイルに含まれているアイコンを取得する
		HINSTANCE hInst = (HINSTANCE)GetModuleHandleW(nullptr);
		EnumResourceNames(hInst, RT_GROUP_ICON, (ENUMRESNAMEPROC)callback, (LONG_PTR)&m_icons);
	}
	int size() const {
		return (int)m_icons.size();
	}
	HICON get(int index) const {
		if (0 <= index && index < (int)m_icons.size()) {
			return m_icons[index];
		} else {
			return nullptr;
		}
	}
};
#pragma endregion // KIcon


#define WND_CHECK   if (m_hWnd == nullptr) K__ERROR("No window: %s", __FUNCTION__);
static const wchar_t *CLASS_NAME = L"K_WND_CLASS_NAME";

static LRESULT CALLBACK _WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

class CWin32Window: public KCoreWindow {
	static void makeFullFill(HWND hWnd, bool val) {
		if (val) {
			SetMenu(hWnd, nullptr);
			SetWindowLongPtrW(hWnd, GWL_STYLE, WS_POPUP|WS_VISIBLE);
			MoveWindow(hWnd, GetSystemMetrics(SM_XVIRTUALSCREEN),
					GetSystemMetrics(SM_YVIRTUALSCREEN),
					GetSystemMetrics(SM_CXVIRTUALSCREEN),
					GetSystemMetrics(SM_CYVIRTUALSCREEN), TRUE);
		} else {
			SetWindowLongPtrW(hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
			ShowWindow(hWnd, SW_RESTORE);
		}
	}
	static void makeWindowed(HWND hWnd, LONG_PTR style, LONG_PTR styleEx, const RECT *rect) {
		K__ASSERT_RETURN(rect);
		SetWindowLongPtrW(hWnd, GWL_STYLE, style);
		SetWindowLongPtrW(hWnd, GWL_EXSTYLE, styleEx);
		ShowWindow(hWnd, SW_RESTORE);
		SetWindowPos(hWnd, HWND_NOTOPMOST, rect->left, rect->top, rect->right-rect->left, rect->bottom-rect->top, SWP_NOSIZE);
	}
	static void computeWindowSize(HWND hWnd, int cw, int ch, int *req_w, int *req_h) {
		K__ASSERT_RETURN(cw >= 0);
		K__ASSERT_RETURN(ch >= 0);
		BOOL has_menu = GetMenu(hWnd) != nullptr;
		LONG_PTR style = GetWindowLongPtrW(hWnd, GWL_STYLE);
		LONG_PTR exstyle = GetWindowLongPtrW(hWnd, GWL_EXSTYLE);
		RECT rect = {0, 0, cw, ch};
		AdjustWindowRectEx(&rect, (DWORD)style, has_menu, (DWORD)exstyle); // AdjustWindowRectEx では Window style を 32 ビットのまま扱っている
		if (req_w) *req_w = rect.right - rect.left;
		if (req_h) *req_h = rect.bottom - rect.top;
	}
	static BOOL hasWindowStyle(HWND hWnd, LONG_PTR style) {
		LONG_PTR s = GetWindowLongPtrW(hWnd, GWL_STYLE);
		return (s & style) != 0;
	}
	static BOOL hasWindowStyleEx(HWND hWnd, LONG_PTR style) {
		LONG_PTR s = GetWindowLongPtrW(hWnd, GWL_EXSTYLE);
		return (s & style) != 0;
	}
	static void setWindowStyle(HWND hWnd, LONG_PTR s, BOOL enabled) {
		LONG_PTR style = GetWindowLongPtrW(hWnd, GWL_STYLE);
		if (enabled) {
			style |= s;
		} else {
			style &= ~s;
		}
		SetWindowLongPtrW(hWnd, GWL_STYLE, style);

		// ウィンドウスタイルの変更を反映させる
		SetWindowPos(hWnd, nullptr, 0, 0, 0, 0,
			SWP_FRAMECHANGED|SWP_NOACTIVATE|SWP_NOMOVE|
			SWP_NOSIZE|SWP_NOZORDER|SWP_NOOWNERZORDER);
	}
	static void setWindowStyleEx(HWND hWnd, LONG_PTR s, BOOL enabled) {
		LONG_PTR style = GetWindowLongPtrW(hWnd, GWL_EXSTYLE);
		if (enabled) {
			style |= s;
		} else {
			style &= ~s;
		}
		SetWindowLongPtrW(hWnd, GWL_EXSTYLE, style);

		// ウィンドウスタイルの変更を反映させる
		SetWindowPos(hWnd, nullptr, 0, 0, 0, 0,
			SWP_FRAMECHANGED|SWP_NOACTIVATE|SWP_NOMOVE|
			SWP_NOSIZE|SWP_NOZORDER|SWP_NOOWNERZORDER);
	}
public:
	static constexpr int HOTKEY_SNAPSHOT_DESKTOP = 1;
	static constexpr int HOTKEY_SNAPSHOT_CLIENT  = 2;
	KWindowCallback *m_cb;
	HWND m_hWnd;
	LONG_PTR m_restore_style;
	LONG_PTR m_restore_style_ex;
	RECT m_restore_rect;
	bool m_kill_alt_f10;
	bool m_kill_monitor_power_off;
	bool m_kill_screensaver;
	bool m_kill_snapshot;
	bool m_mouse_in_window;
public:
	CWin32Window() {
		zero_clear();
	}
	virtual ~CWin32Window() {
		shutdown();
	}
	void zero_clear() {
		m_hWnd = nullptr;
		m_cb = nullptr;
		m_restore_style = 0;
		m_restore_style_ex = 0;
		memset(&m_restore_rect, 0, sizeof(m_restore_rect));
		m_kill_alt_f10 = false;
		m_kill_monitor_power_off = false;
		m_kill_screensaver = false;
		m_mouse_in_window = false;
		m_kill_snapshot = false;
	}
	bool init(int w, int h, const char *title_u8) {
		m_mouse_in_window = false;
		m_hWnd = nullptr;

		CWin32ResourceIcons icons;

		WNDCLASSEXW wc;
		ZeroMemory(&wc, sizeof(WNDCLASSEXW));
		wc.cbSize = sizeof(WNDCLASSEXW);
		wc.lpszClassName = CLASS_NAME;
		wc.lpfnWndProc = _WndProc;
		wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
		wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wc.hIcon = icons.get(0); // リソースファイルの最初に入っているアイコン
		RegisterClassExW(&wc);
		m_hWnd = CreateWindowExW(0, CLASS_NAME, nullptr, WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			nullptr, nullptr, nullptr, nullptr
		);
		if (m_hWnd == nullptr) {
			return false;
		}
		setClientSize(w, h);
		setTitle(title_u8);
		setWindowVisible(true);
		return true;
	}
	void shutdown() {
		if (m_hWnd) {
			// 処理の一貫性を保つため必ず WM_CLOSE を発行する
			if (IsWindowVisible(m_hWnd)) {
				// 注意： CloseWindow(hWnd) を使うと WM_CLOSE が来ないまま終了する。
				// CloseWindow はウィンドウを最小化するための命令であることに注意。
				// ただ見えないだけなので閉じた後にも WM_MOVE などが来てしまう。
				// トラブルを避けるために、明示的に SendMessage(WM_CLOSE, ...) でメッセージを流すこと。
				SendMessageW(m_hWnd, WM_CLOSE, 0, 0);
			}
			DestroyWindow(m_hWnd);
		}
		zero_clear();
	}
	virtual void setCallback(KWindowCallback *cb) override {
		WND_CHECK;
		m_cb = cb;
	}
	virtual void * getHandle() const override {
		return m_hWnd;
	}
	virtual void setTitle(const char *text_u8) override {
		WND_CHECK;
		if (text_u8) {
			wchar_t ws[256] = {0};
			MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text_u8, -1, ws, sizeof(ws)/sizeof(wchar_t));
			SetWindowTextW(m_hWnd, ws);
		}
	}

	virtual bool isWindowVisible() const override {
		return IsWindowVisible(m_hWnd) != 0;
	}
	virtual void setWindowVisible(bool value) override {
		WND_CHECK;
		ShowWindow(m_hWnd, value ? SW_SHOW : SW_HIDE);
	}

	virtual void getClientSize(int *cw, int *ch) const override {
		WND_CHECK;
		RECT rc = {0, 0, 0, 0};
		GetClientRect(m_hWnd, &rc); // GetClientRect が失敗する可能性に注意
		if (cw) *cw = rc.right - rc.left;
		if (ch) *ch = rc.bottom - rc.top;
	}
	virtual void setClientSize(int cw, int ch) override {
		WND_CHECK;
		if (cw < 0) cw = 0;
		if (ch < 0) ch = 0;
		int ww, hh;
		computeWindowSize(m_hWnd, cw, ch, &ww, &hh);
		SetWindowPos(m_hWnd, nullptr, 0, 0, ww, hh, SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOMOVE);
	}

	virtual void getWindowPosition(int *x, int *y) const override {
		WND_CHECK;
		RECT rect;
		GetWindowRect(m_hWnd, &rect);
		if (x) *x = rect.left;
		if (y) *y = rect.top;
	}
	virtual void setWindowPosition(int x, int y) override {
		WND_CHECK;
		SetWindowPos(m_hWnd, nullptr, x, y, 0, 0, SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOSIZE);
	}

	virtual void getWindowNormalRect(int *x, int *y, int *w, int *h) const override {
		WINDOWPLACEMENT wp;
		wp.length = sizeof(wp);
		GetWindowPlacement(m_hWnd, &wp);
		if (x) *x = wp.rcNormalPosition.left;
		if (y) *y = wp.rcNormalPosition.top;
		if (w) *w = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
		if (h) *h = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
	}
	virtual void setWindowNormalRect(int x, int y, int w, int h) override {
		WINDOWPLACEMENT wp;
		wp.length = sizeof(wp);
		wp.showCmd = SW_SHOW;
		wp.rcNormalPosition.left   = x;
		wp.rcNormalPosition.top    = y;
		wp.rcNormalPosition.right  = x + w;
		wp.rcNormalPosition.bottom = y + h;
		SetWindowPlacement(m_hWnd, &wp);
	}

	virtual void screenToClient(int *x, int *y) const override {
		WND_CHECK;
		POINT p = {*x, *y};
		ScreenToClient(m_hWnd, &p);
		if (x) *x = p.x;
		if (y) *y = p.y;
	}
	virtual void clientToScreen(int *x, int *y) const override {
		WND_CHECK;
		POINT p = {*x, *y};
		ClientToScreen(m_hWnd, &p);
		if (x) *x = p.x;
		if (y) *y = p.y;
	}

	virtual bool isMaximized() const override {
		WND_CHECK;
		return IsZoomed(m_hWnd) != 0;
	}
	virtual bool isIconified() const override {
		WND_CHECK;
		return IsIconic(m_hWnd) != 0;
	}
	virtual bool isWindowFocused() const override {
		return GetForegroundWindow() == m_hWnd;
	}
	virtual void fullscreenWindow() {
		WND_CHECK;
	}
	virtual void maximizeWindow() override {
		WND_CHECK;
		ShowWindow(m_hWnd, SW_MAXIMIZE);
	}
	virtual void restoreWindow() override {
		WND_CHECK;
		ShowWindow(m_hWnd, SW_RESTORE);
	}
	virtual int getAttribute(KWindowAttr attr) const override {
		WND_CHECK;
		switch (attr) {
		case KWindowAttr_HAS_BORDER:          return hasWindowStyle(m_hWnd, WS_CAPTION) && hasWindowStyle(m_hWnd, WS_THICKFRAME);
		case KWindowAttr_HAS_TITLE:           return hasWindowStyle(m_hWnd, WS_CAPTION);
		case KWindowAttr_HAS_MAXIMIZE_BUTTON: return hasWindowStyle(m_hWnd, WS_MAXIMIZEBOX);
		case KWindowAttr_HAS_ICONIFY_BUTTON:  return hasWindowStyle(m_hWnd, WS_MINIMIZEBOX);
		case KWindowAttr_HAS_CLOSE_BUTTON:    return hasWindowStyle(m_hWnd, WS_SYSMENU);
		case KWindowAttr_RESIZABLE:           return hasWindowStyle(m_hWnd, WS_THICKFRAME);
		case KWindowAttr_TOPMOST:             return hasWindowStyleEx(m_hWnd, WS_EX_TOPMOST);
		case KWindowAttr_KILL_ALT_F10:        return m_kill_alt_f10;
		case KWindowAttr_KILL_SNAPSHOT:       return m_kill_snapshot;
		}
		return 0;
	}
	virtual void setAttribute(KWindowAttr attr, int value) override {
		WND_CHECK;
		switch (attr) {
		case KWindowAttr_HAS_BORDER:
			// 境界線の有無は WS_THICKFRAME と WS_CAPTION の値で決まる
			setWindowStyle(m_hWnd, WS_THICKFRAME, value);
			setWindowStyle(m_hWnd, WS_CAPTION, value);
			break;

		case KWindowAttr_HAS_TITLE:
			setWindowStyle(m_hWnd, WS_CAPTION, value);
			break;

		case KWindowAttr_RESIZABLE:
			setWindowStyle(m_hWnd, WS_THICKFRAME, value);
			break;

		case KWindowAttr_HAS_MAXIMIZE_BUTTON:
			setWindowStyle(m_hWnd, WS_MAXIMIZEBOX, value);
			break;

		case KWindowAttr_HAS_ICONIFY_BUTTON:
			setWindowStyle(m_hWnd, WS_MINIMIZEBOX, value);
			break;

		case KWindowAttr_HAS_CLOSE_BUTTON:
			setWindowStyle(m_hWnd, WS_SYSMENU, value);
			break;
	
		case KWindowAttr_TOPMOST:
			setWindowStyleEx(m_hWnd, WS_EX_TOPMOST, value);
			SetWindowPos(
				m_hWnd, 
				value ? HWND_TOPMOST : HWND_NOTOPMOST,
				0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE);
			break;

		case KWindowAttr_FULLFILL:
			makeFullFill(m_hWnd, value);
			break;

		case KWindowAttr_KILL_ALT_F10:
			m_kill_alt_f10 = (value != 0);
			break;

		case KWindowAttr_KILL_SNAPSHOT:
			m_kill_snapshot = value;
			setSnapshotKilling(value);
			break;
		}
	}
	void setSnapshotKilling(bool value) {
		WND_CHECK;
		if (value) {
			// PrtScr キーは WM_KEYDOWN を送出しない(WM_KEYUPは来る）
			// PtrScr をホットキーとして登録して、WM_HOTKEY 経由で PrtScr の押下イベントを受け取れるようにする
			// https://gamedev.stackexchange.com/questions/20446/does-vk-snapshot-not-send-a-wm-keydown-only-wm-keyup
			RegisterHotKey(m_hWnd, HOTKEY_SNAPSHOT_DESKTOP, 0, VK_SNAPSHOT);
			RegisterHotKey(m_hWnd, HOTKEY_SNAPSHOT_CLIENT, KKeyModifier_ALT, VK_SNAPSHOT);
			_DEBUG_PRINT(u8"[PrtScr] を占有しました: HWND=0x%X", m_hWnd);
		} else {
			UnregisterHotKey(m_hWnd, HOTKEY_SNAPSHOT_DESKTOP);
			UnregisterHotKey(m_hWnd, HOTKEY_SNAPSHOT_CLIENT);
			_DEBUG_PRINT(u8"[PrtScr] を解放しました: HWND=0x%X", m_hWnd);
		}
	}
	virtual void command(const char *cmd, void *data) override {
		WND_CHECK;
		if (_streq(cmd, "set_cursor_arrow")) {
			SetCursor(LoadCursor(nullptr, IDC_ARROW));
			return;
		}
		if (_streq(cmd, "set_cursor_hand")) {
			SetCursor(LoadCursor(nullptr, IDC_HAND));
			return;
		}
		if (_streq(cmd, "set_cursor_sizeall")) {
			SetCursor(LoadCursor(nullptr, IDC_SIZEALL));
			return;
		}
		if (_streq(cmd, "show_cursor")) {
			while (ShowCursor(TRUE) < 0) {}
			return;
		}
		if (_streq(cmd, "hide_cursor")) {
			while (ShowCursor(FALSE) >= 0) {}
			return;
		}
	}
	virtual bool getMouseCursorPos(int *cx, int *cy) const override {
		WND_CHECK;
		POINT p = {0, 0};
		RECT rc = {0, 0, 0, 0};
		GetCursorPos(&p);
		ScreenToClient(m_hWnd, &p);
		GetClientRect(m_hWnd, &rc);
		if (cx) *cx = p.x;
		if (cy) *cy = p.y;
		if (p.x < rc.left || rc.right <= p.x) return false;
		if (p.y < rc.top || rc.bottom <= p.y) return false;
		return false;
	}

	virtual bool getOwnerDisplayRect(int *x, int *y, int *w, int *h) const {
		// 現在のウィンドウ範囲
		RECT rect = {0, 0, 0, 0};
		GetWindowRect(m_hWnd, &rect);

		// ウィンドウ範囲を一番多く含んでいるモニター
		HMONITOR hMonitor = MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST);
		MONITORINFO mi;
		mi.cbSize = sizeof(MONITORINFO);
		GetMonitorInfoA(hMonitor, &mi);

		if (x) *x = mi.rcMonitor.left;
		if (y) *y = mi.rcMonitor.top;
		if (w) *w = mi.rcMonitor.right - mi.rcMonitor.left;
		if (h) *h = mi.rcMonitor.bottom - mi.rcMonitor.top;
		return true;
	}

	virtual bool adjustWindowPosAndSize(int *_x, int *_y, int *_w, int *_h) const {
		int x = *_x;
		int y = *_y;
		int w = *_w;
		int h = *_h;
		int MIN_WNDSIZE = 160;
		int dispX, dispY, dispW, dispH;
		if (getOwnerDisplayRect(&dispX, &dispY, &dispW, &dispH)) {
			if (dispX + dispW < x + w) {
				x -= (x + w) - (dispX + dispW);
			}
			if (dispY + dispH < y + h) {
				y -= (y + h) - (dispY + dispH);
			}
			if (x < dispX) {
				x = dispX;
			}
			if (y < dispY) {
				y = dispY;
			}
			if (w >= MIN_WNDSIZE && h >= MIN_WNDSIZE) {
				*_x = x;
				*_y = y;
				*_w = w;
				*_h = h;
				return true;
			} else {
				// 記録されているウィンドウサイズが小さすぎる
				// 既定のウィンドウサイズのままにしておく
			}
		}
		return false;
	}

	LRESULT CALLBACK wndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		if (m_cb) {
			_LRESULT lResult = 0;
			m_cb->onWindowMessage((_HWND)hWnd, (_UINT)msg, (_WPARAM)wParam, (_LPARAM)lParam, &lResult);
			if (lResult) {
				return lResult;
			}
		}
		switch (msg) {
		case WM_CREATE:
			DragAcceptFiles(hWnd, TRUE);
			break;

		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;

		case WM_CLOSE:
			if (m_cb) m_cb->onWindowClosing();
			break;

		case WM_DROPFILES: {
			HDROP hDrop = (HDROP)wParam;
			UINT numFiles = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
			for (UINT i=0; i<numFiles; i++) {
				wchar_t name[MAX_PATH] = {0};
				DragQueryFileW(hDrop, i, name, MAX_PATH);
				if (m_cb) {
					std::string filename_u8 = K::strWideToUtf8(name);
					m_cb->onWindowDropFile(i, numFiles, filename_u8.c_str());
				}
			}
			DragFinish(hDrop);
			break;
		}
		case WM_MOVE:
			if (m_cb) {
				int x = (int)(short)LOWORD(lParam);
				int y = (int)(short)HIWORD(lParam);
				m_cb->onWindowMove(x, y);
			}
			break;

		case WM_SIZE:
			if (m_cb) {
				int w = (int)(short)LOWORD(lParam); // client size
				int h = (int)(short)HIWORD(lParam);
				m_cb->onWindowResize(w, h);
			}
			break;

		case WM_ACTIVATE:
			if (wParam == WA_INACTIVE) { // ウィンドウが非アクティブになった
				if (m_kill_snapshot) {
					setSnapshotKilling(false); // スナップショット乗っ取りを解除
				}
			} else { // ウィンドウがアクティブになった
				if (m_kill_snapshot) {
					setSnapshotKilling(true); // スナップショット乗っ取りを開始
				}
			}
			break;

		case WM_MOUSEWHEEL:
			if (m_cb) {
				POINT p;
				p.x = GET_X_LPARAM(lParam); // screen pos
				p.y = GET_Y_LPARAM(lParam);
				ScreenToClient(hWnd, &p);
				int delta = GET_WHEEL_DELTA_WPARAM(wParam) / 120;
				m_cb->onWindowMouseWheel(p.x, p.y, delta);
			}
			break;

		case WM_MOUSEMOVE: {
			int x = GET_X_LPARAM(lParam); // client pos
			int y = GET_Y_LPARAM(lParam);
			int btn = (wParam & MK_LBUTTON) ? 0 : (wParam==MK_RBUTTON) ? 1 : 2;
			// マウスポインタ侵入イベントがまだ発生していないなら、発生させる
			if (!m_mouse_in_window) {
				m_mouse_in_window = true;
				// 次回マウスカーソルが離れたときに WM_MOUSELEAVE が発生するようにする。
				// この設定はイベントが発生するたびにリセットされるため、毎回設定しなおす必要がある
				TRACKMOUSEEVENT e;
				e.cbSize = sizeof(e);
				e.hwndTrack = hWnd;
				e.dwFlags = TME_LEAVE;
				e.dwHoverTime = 0;
				TrackMouseEvent(&e);
				if (m_cb) m_cb->onWindowMouseEnter(x, y);
			}
			if (m_cb) m_cb->onWindowMouseMove(x, y, btn);
			break;
		}
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
			if (m_cb) {
				int x = GET_X_LPARAM(lParam); // client pos
				int y = GET_Y_LPARAM(lParam);
				int btn = (msg==WM_LBUTTONDOWN) ? 0 : (msg==WM_RBUTTONDOWN) ? 1 : 2;
				m_cb->onWindowMouseButtonDown(x, y, btn);
			}
			break;

		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
		case WM_MBUTTONUP:
			if (m_cb) {
				int x = GET_X_LPARAM(lParam); // client pos
				int y = GET_Y_LPARAM(lParam);
				int btn = (msg==WM_LBUTTONUP) ? 0 : (msg==WM_RBUTTONUP) ? 1 : 2;
				m_cb->onWindowMouseButtonUp(x, y, btn);
			}
			break;

		case WM_MOUSELEAVE:
			// マウスポインタがウィンドウから離れた。
			// 再び WM_MOUSELEAVE を発生させるには、
			// TrackMouseEvent() で再設定する必要があることに注意
			m_mouse_in_window = false;
			if (m_cb) m_cb->onWindowMouseExit();
			break;

		case WM_HOTKEY:
			// [PrtScr] キーは WM_KEYDOWN を送出しない。ホットキーとして登録してイベントを捕まえる
			if (wParam == HOTKEY_SNAPSHOT_DESKTOP) { // [PrtScr] キーの WM_KEY_DOWN の代替処理
				if (m_cb) m_cb->onWindowKeyDown(KKey_SNAPSHOT);
			}
			if (wParam == HOTKEY_SNAPSHOT_CLIENT) { // [Alt + PrtScr] キーの WM_KEY_DOWN の代替処理
				if (m_cb) m_cb->onWindowKeyDown(KKey_SNAPSHOT);
			}
			break;

		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			if (m_cb) {
				KKey key = KKeyboard::findKeyByVirtualKey(wParam);
				KKeyModifiers mods = 0;
				m_cb->onWindowKeyDown(key);
			}
			if (m_kill_alt_f10) {
				// Alt, F10 による一時停止を無効化する。
				// ゲーム中に ALT または F10 を押すと一時停止状態になってしまう。
				// 意図しない一時停止をさせたくない場合に使う
				if (wParam == VK_F10 || wParam == VK_MENU) {
					return 1; 
				}
			}
			break;

		case WM_KEYUP:
		case WM_SYSKEYUP:
			if (m_cb) {
				KKey key = KKeyboard::findKeyByVirtualKey(wParam);
				m_cb->onWindowKeyUp(key);
			}
			break;

		case WM_CHAR:
			if (m_cb) m_cb->onWindowKeyChar((wchar_t)wParam);
			break;

		case WM_SYSCOMMAND:
			// モニターのオートパワーオフを無効化
			if (m_kill_monitor_power_off) {
				if (wParam == SC_MONITORPOWER) {
					return 1;
				}
			}
			// スクリーンセーバーの起動を抑制する。
			// ゲームパッドでゲームしていると、キーボードもマウスも操作しない時間が続くために
			// スクリーンセーバーが起動してしまう事がある。それを阻止する
			if (m_kill_screensaver) {
				if (wParam == SC_SCREENSAVE) {
					return 1;
				}
			}
			break;
		}
		return DefWindowProcW(hWnd, msg, wParam, lParam);
	}
};



bool KWindow::processEvents() {
	MSG msg;
	// メッセージが届いていればそれを処理する。
	// 届いていなければ何もしない。
	// WM_QUIT が来たらメインループを終了させる必要があるため false を返す
	if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
		if (msg.message == WM_QUIT) {
			return false;
		}
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
	return true;
}
bool KWindow::processEventsWait() {
	MSG msg;
	// メッセージが届いていればそれを処理する。
	// 届いていなければ、届くまで待つ。
	if (GetMessageW(&msg, 0, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
		return true;
	} else {
		return false;
	}
}





static CWin32Window *g_Window = nullptr;


#if USE_WINDOW_THREAD
class CWindowThread: public KThread {
public:
	std::mutex mMutex;
	int mInitWidth;
	int mInitHeight;
	std::string mInitTitleUtf8;
	int mStat;

	CWindowThread() {
		mInitWidth = 0;
		mInitHeight = 0;
		mStat = 0;
	}
	virtual void run() override {
		mStat = 0;
		g_Window = new CWin32Window();
		if (g_Window->init(mInitWidth, mInitHeight, mInitTitleUtf8.c_str())) {
			mStat = 1; // ウィンドウの作成を完了した

			MSG msg;
			while (GetMessageW(&msg, 0, 0, 0) > 0) {
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			}
			mStat = 2; // メッセーループから抜けた
		} else {
			mStat = 3; // エラー終了
		}
		g_Window->shutdown();
		K__DROP(g_Window);
	}
	void waitForReady() {
		while (mStat < 1) {
			Sleep(1);
		}
	}
};
static CWindowThread g_WindowThread;
#endif // USE_WINDOW_THREAD

#pragma region KWindow
static LRESULT CALLBACK _WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	return g_Window->wndProc(hWnd, msg, wParam, lParam);
}
bool KWindow::init(int w, int h, const char *text_u8) {
#if USE_WINDOW_THREAD
	g_WindowThread.mInitWidth = w;
	g_WindowThread.mInitHeight = h;
	g_WindowThread.mInitTitleUtf8 = text_u8;
	g_WindowThread.start();
	g_WindowThread.waitForReady(); // ウィンドウの作成が終わり、ウィンドウメッセージを受け付け可能になるまで待つ
	return g_WindowThread.mStat == 1;
#else
	g_Window = new CWin32Window();
	if (g_Window->init(w, h, text_u8)) {
		return true;
	}
#endif
	return false;
}
void KWindow::shutdown() {
	if (g_Window) {
		g_Window->shutdown();
		K__DROP(g_Window);
	}
}
KCoreWindow * KWindow::get() {
	K__ASSERT(g_Window);
	return g_Window;
}

#pragma endregion // KWindow


} // namespace
