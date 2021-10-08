#include "KDialog.h"

#include <string>
#include <unordered_set>
#include <Windows.h>
#include "KInternal.h"

namespace Kamilo {

#define KDFLAG_SEPARATOR 1
#define KDFLAG_EXPAND_X  2

// そもそも TCHAR を全く使っていないため UNICODE マクロが定義されているかどうかに関係になく
// 常にワイド文字版を使いたい。そうでないと勝手に char と wchar_t が切り替わってしまう可能性がある。
// そのため関数マクロを使わずにわざわざ末尾に W が付いているものを使っているわけだが
// IDC_ARROW は WinUser.h 内で
//
// #define IDC_ARROW   MAKEINTRESOURCE(32512)
//
// と定義されていて、その MAKEINTRESOURCE は
//
// #ifdef UNICODE
// #define MAKEINTRESOURCE  MAKEINTRESOURCEW
// #else
// #define MAKEINTRESOURCE  MAKEINTRESOURCEA
//
// と定義されているため IDC_ARROW の Ansi版 IDC_ARROWA や
// Wide版の IDC_ARROWW は初めから存在していない。
// 
// そこで UNICODE の設定とは無関係に常にワイド文字を使うように、自前でWide版の IDC_ARROW を定義しておく
//
#define K_IDC_ARROWW   MAKEINTRESOURCEW(32512)

// Win32API - ウィンドウの概要
// http://moeprog.web.fc2.com/moe/Win32API/GUI/Windows.htm


#define K__MAX(a, b) ((a) > (b) ? (a) : (b))


static void _WideToUtf8(const wchar_t *ws, char *u8, int size) {
	WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, ws, -1, u8, size, NULL, NULL);
}
static std::wstring _Utf8ToWide(const char *u8) {
	std::wstring ws(1024, L'\0');
	MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, u8, -1, &ws[0], ws.size());
	size_t actual_size = wcslen(ws.c_str());
	ws.resize(actual_size);
	return ws;
}
static std::string _Utf8ToAnsi(const char *u8) {
	std::string mb(1024, '\0');
	std::wstring ws(1024, L'\0');
	MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, u8, -1, &ws[0], ws.size()); // UTF8 --> WIDE
	WideCharToMultiByte(CP_ACP, 0, ws.c_str(), -1, &mb[0], mb.size(), NULL, NULL); // WIDE --> ANSI
	size_t actual_size = strlen(mb.c_str());
	mb.resize(actual_size);
	return mb;
}
static void _MoveToCenterInParent(HWND hWnd, HWND hParent) {
	if (hParent == NULL) hParent = GetDesktopWindow();
	RECT parentRect;
	RECT wndRect;
	GetWindowRect(hParent, &parentRect);
	GetWindowRect(hWnd, &wndRect);
	int x = (parentRect.left + parentRect.right ) / 2 - (wndRect.right + wndRect.left) / 2;
	int y = (parentRect.top  + parentRect.bottom) / 2 - (wndRect.bottom + wndRect.top) / 2;
	SetWindowPos(hWnd, NULL, x, y, 0, 0, SWP_NOZORDER|SWP_NOSIZE);
}
static void _ComputeTextSize(HWND hWnd, const wchar_t *text, int *w, int *h) {
	RECT rect = {0, 0, 0, 0};
	HDC hDC = GetDC(hWnd);
	SelectObject(hDC, (HFONT)GetStockObject(DEFAULT_GUI_FONT));
	DrawTextW(hDC, text, -1, &rect, DT_CALCRECT);
	ReleaseDC(hWnd, hDC);
	K__ASSERT(w);
	K__ASSERT(h);
	*w = rect.right - rect.left;
	*h = rect.bottom - rect.top;
}
static void _ComputeTextSizeFromCaption(HWND hWnd, int *w, int *h) {
	static const int SIZE = 1024;
	wchar_t text[SIZE] = {0};
	GetWindowTextW(hWnd, text, SIZE);
	_ComputeTextSize(hWnd, text, w, h);
}
static void _ConvertLineCode(std::wstring &s) {
	for (size_t i=0; i<s.size(); i++) {
		if (s[i]=='\r' && s[i+1]=='\n') { i++; break; }
		if (s[i]=='\n') { s.insert(i, 1, '\r'); i++; }
	}
}
static void _SetToolTip(HWND hItem, const char *text_u8) {
#if USE_COMMONCTRL
	HWND hTool = CreateWindowExW(0, TOOLTIPS_CLASSW, NULL,
		TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		hItem, NULL, (HINSTANCE)GetWindowLongPtrW(hItem, GWLP_HINSTANCE), NULL);

	// いつもは _UNICODE の設定とは無関係に、末尾が W の WIN32API を使うことで常にワイド文字を利用するようにしているが、
	// TTTOOLINFOW を使ってワイド文字でヒントテキストを設定しても _UNICODE が未定義だとうまくいかない。
	// TTTOOLINFO に関しては _UNICODE の設定が影響しているのかもしれない。
	// ここの部分だけは _UNICODE の値に応じて Ansi/Wide をきちんと切り替えるようにする
	TTTOOLINFO ti;
	ZeroMemory(&ti, sizeof(ti));
	ti.cbSize = sizeof(ti);
	ti.uFlags = TTF_SUBCLASS;
	ti.hwnd = hItem;
	// ここだけはワイド文字列とマルチバイト文字列を手動で切り替える
#ifdef _UNICODE
	std::wstring ws = _Utf8ToWide(text_u8);
	ti.lpszText = const_cast<wchar_t*>(ws.c_str());
#else
	std::string mb = _Utf8ToAnsi(text_u8);
	ti.lpszText = const_cast<char*>(mb.c_str());
#endif
	GetClientRect(hItem, &ti.rect);
	BOOL ok = (BOOL)SendMessage(hTool, TTM_ADDTOOL, 0, (LPARAM)&ti);
	if (!ok) {
		// ここで失敗した場合は以下の原因を疑う
		//
		// ツールチップ と コモンコントロール のバージョン差異による落とし穴
		// https://ameblo.jp/blueskyame/entry-10398978729.html
		//
		// Adding tooltip controls in unicode fails
		// https://social.msdn.microsoft.com/Forums/vstudio/en-US/5cc9a772-5174-4180-a1ca-173dc81886d9/adding-tooltip-controls-in-unicode-fails?forum=vclanguage
		//
		ti.cbSize = sizeof(ti) - sizeof(void*); // <== ここで小細工
		SendMessage(hTool, TTM_ADDTOOL, 0, (LPARAM)&ti);
	}
	SendMessage(hTool, TTM_SETMAXTIPWIDTH, 0, 640); // 複数行に対応させる。指定幅または改行文字で改行されるようになる
	//	リサイズなどで領域が変化した場合は領域を更新すること
	//	GetClientRect(hWnd , &ti.rect);
	//	SendMessageW(hTool , TTM_NEWTOOLRECT , 0 , (LPARAM)&ti);
#endif
}

// EnumResourceNames のコールバック用。最初に見つかった HICON を得る
static BOOL CALLBACK _ResNamesCallback(HINSTANCE instance, LPCTSTR type, LPTSTR name, void *data) {
	HICON icon = LoadIcon(instance, name);
	if (icon) {
		*(HICON*)data = icon; 
		return FALSE;
	}
	return TRUE; // FIND NEXT
}


class CDialogImpl {
	HINSTANCE m_hInstance;
	HWND m_hWnd;
	HWND m_hParent;
	HWND m_hFg;
	KDialog::Callback *m_CB;
	KDialog *m_ThisDialog;
	KDialog::Flags m_Flags;
	KDialog::LayoutCursor m_LayoutCursor;
	std::unordered_set<HWND> m_CmdCtrls; // コマンドコントロール一覧
	int m_LastCommandId; // 最後に配置したコマンドコントロール

	static LRESULT CALLBACK wndproc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
		switch (msg) {
		case WM_CREATE:
			{
				DragAcceptFiles(hWnd, TRUE);
				break;
			}
		case WM_CLOSE:
			{
				CDialogImpl *dialog = reinterpret_cast<CDialogImpl *>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
				if (dialog) dialog->on_WM_CLOSE();
				break;
			}
		case WM_DESTROY:
			{
				CDialogImpl *dialog = reinterpret_cast<CDialogImpl *>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
				if (dialog && (dialog->m_Flags & KDialog::NEVER_SEND_QUIT)) {
					// WM_QUIT 送らない
					break;
				}
				PostQuitMessage(0);
				break;
			}
		case WM_COMMAND:
			{
				KDialogId id = (KDialogId)LOWORD(wp);
				CDialogImpl *dialog = reinterpret_cast<CDialogImpl *>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
				HWND hItem = (HWND)lp;
				if (dialog) dialog->on_WM_COMMAND(hItem, id);
				break;
			}
		}
		return DefWindowProcW(hWnd, msg, wp, lp);
	}

public:
	explicit CDialogImpl(KDialog *this_dialog) {
		m_LastCommandId = 0; // clear_zero() ではクリアされない
		clear_zero();
		m_ThisDialog = this_dialog;
	}
	~CDialogImpl() {
		close();
	}
	void create(int w, int h, const char *text_u8, KDialog::Flags flags) {
		m_LastCommandId = 0; // clear_zero() ではクリアされない
		clear_zero();
		m_Flags = flags;

	#if USE_COMMONCTRL
		InitCommonControls();
	#endif

		// 現在アクティブなウィンドウを親とする
		m_hParent = GetActiveWindow();
		m_hInstance = (HINSTANCE)GetModuleHandle(NULL);

		// 最初に見つかったアイコン（＝アプリケーションのアイコンと同じ）をウィンドウのアイコンとして取得する
		HICON hIcon = NULL; // LoadIconW(NULL, IDI_ERROR);
		EnumResourceNames(m_hInstance, RT_GROUP_ICON, (ENUMRESNAMEPROC)_ResNamesCallback, (LONG_PTR)&hIcon);

		// ウィンドウクラスの登録
		WNDCLASSEXW wc;
		{
			ZeroMemory(&wc, sizeof(WNDCLASSEXW));
			wc.cbSize        = sizeof(WNDCLASSEXW);
			wc.lpszClassName = L"ZIALOG";
			wc.lpfnWndProc   = wndproc;
			wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE +1);
			wc.hIcon         = hIcon;
			wc.hCursor       = LoadCursorW(NULL, K_IDC_ARROWW);
			RegisterClassExW(&wc);
		}

		// ウィンドウ作成
		std::wstring ws = _Utf8ToWide(text_u8);
		m_hWnd = CreateWindowExW(0, wc.lpszClassName, ws.c_str(), WS_POPUPWINDOW|WS_CAPTION, CW_USEDEFAULT, CW_USEDEFAULT, w, h, m_hParent, NULL, NULL, NULL);

		// ウィンドウメッセージハンドラから this を参照できるように、ユーザーデータとしてウィンドウと this を関連付けておく
		SetWindowLongPtrW(m_hWnd, GWLP_USERDATA, (LONG_PTR)this);

		m_CB = NULL;
		m_LayoutCursor.margin_x = 16;
		m_LayoutCursor.margin_y = 16;
		m_LayoutCursor.padding_x = 8;
		m_LayoutCursor.padding_y = 6;
		m_LayoutCursor.curr_line_h = 0;
		m_LayoutCursor.next_x = m_LayoutCursor.margin_x;
		m_LayoutCursor.next_y = m_LayoutCursor.margin_y;
		m_LayoutCursor.next_w = 120;
		m_LayoutCursor.next_h = 12;
		m_LayoutCursor.line_min_h = 8;
		m_LayoutCursor.auto_sizing = true;
		m_LastCommandId = 0;
	}
	void close() {
		// 処理の一貫性を保つため必ず WM_CLOSE を発行する
		// 注意： CloseWindow(hWnd) を使うと WM_CLOSE が来ないまま終了する。
		// CloseWindow はウィンドウを最小化するための命令であることに注意。
		// ただ見えないだけなので閉じた後にも WM_MOVE などが来てしまう。
		// WM_CLOSE で終了処理をしていたり WM_MOVE のタイミングでウィンドウ位置を保存とかやっているとおかしな事になる。
		// トラブルを避けるために、明示的に SendMessageW(WM_CLOSE, ...) でメッセージを流す。
		SendMessageW(m_hWnd, WM_CLOSE, 0, 0);
		DestroyWindow(m_hWnd);
		clear_zero(); // ここで m_hWnd がクリアされるため、ウィンドウ破棄関連のメッセージを _wndproc で処理できなくなる可能性に注意
	}
	void getLayoutCursor(KDialog::LayoutCursor *cur) const {
		*cur = m_LayoutCursor;
	}
	void setLayoutCursor(const KDialog::LayoutCursor *cur) {
		m_LayoutCursor = *cur;
	}
	KDialogId addLabel(KDialogId id, const char *text_u8) {
		std::wstring ws = _Utf8ToWide(text_u8);
		HWND hItem = CreateWindowExW(0, L"STATIC", ws.c_str(),
			WS_CHILD|WS_VISIBLE,
			m_LayoutCursor.next_x, m_LayoutCursor.next_y, m_LayoutCursor.next_w, m_LayoutCursor.next_h,
			m_hWnd, 0, m_hInstance, NULL);
		SendMessageW(hItem, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(TRUE, 0));
		if (m_LayoutCursor.auto_sizing) {
			set_best_item_size(hItem, 0, 0);
		}
		move_cursor_to_next(hItem);
		return id;
	}
	KDialogId addEdit(KDialogId id, const char *text_u8) {
		std::wstring ws = _Utf8ToWide(text_u8);
		HWND hItem = CreateWindowExW(0, L"EDIT", ws.c_str(),
			WS_CHILD|WS_VISIBLE|WS_BORDER|ES_LEFT,
			m_LayoutCursor.next_x, m_LayoutCursor.next_y, m_LayoutCursor.next_w, m_LayoutCursor.next_h,
			m_hWnd, NULL, m_hInstance, NULL);
		SendMessageW(hItem, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(TRUE, 0));
		if (m_LayoutCursor.auto_sizing) {
			set_best_item_size(hItem, 0, 0);
		}
		move_cursor_to_next(hItem);
		return id;
	}
	KDialogId addMemo(KDialogId id, const char *text_u8, bool wrap, bool readonly) {
		std::wstring ws = _Utf8ToWide(text_u8);
		// 末尾が改行で終わるようにしておく（readonly の場合のみ。そのほうがコピペしやすいので）
		if (readonly && ws.size()>0 && ws[ws.size()-1] != '\n') {
			ws += L"\n";
		}
		_ConvertLineCode(ws); // "\n" --> "\r\n"
		DWORD rw = wrap ? 0 : ES_AUTOHSCROLL;
		DWORD ro = readonly ? ES_READONLY : 0;
		HWND hItem = CreateWindowExW(0, L"EDIT", ws.c_str(),
			WS_CHILD|WS_VISIBLE|WS_BORDER|WS_HSCROLL|WS_VSCROLL|
			ES_AUTOVSCROLL|ES_LEFT|ES_MULTILINE|ES_NOHIDESEL|rw|ro,
			m_LayoutCursor.next_x, m_LayoutCursor.next_y, m_LayoutCursor.next_w, m_LayoutCursor.next_h,
			m_hWnd, NULL, m_hInstance, NULL);
		SendMessageW(hItem, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(TRUE, 0));
		if (m_LayoutCursor.auto_sizing) {
			set_best_item_size(hItem, 0, 0);
		}
		move_cursor_to_next(hItem);
		return id;
	}
	KDialogId addCheckBox(KDialogId id, const char *text_u8, bool value) {
		std::wstring ws = _Utf8ToWide(text_u8);
		HWND hItem = CreateWindowExW(0, L"BUTTON", ws.c_str(),
			WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_CHECKBOX|BS_AUTOCHECKBOX,
			m_LayoutCursor.next_x, m_LayoutCursor.next_y, m_LayoutCursor.next_w, m_LayoutCursor.next_h,
			m_hWnd, (HMENU)id, m_hInstance, NULL);
		SendMessageW(hItem, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(TRUE, 0));
		if (m_LayoutCursor.auto_sizing) {
			set_best_item_size(hItem, 2, 0);
		}
		move_cursor_to_next(hItem);
		return id;
	}
	KDialogId addButton(KDialogId id, const char *text_u8) {
		std::wstring ws = _Utf8ToWide(text_u8);
		HWND hItem = CreateWindowExW(0, L"BUTTON", ws.c_str(),
			WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,
			m_LayoutCursor.next_x, m_LayoutCursor.next_y, m_LayoutCursor.next_w, m_LayoutCursor.next_h,
			m_hWnd, (HMENU)id, m_hInstance, NULL);
		SendMessageW(hItem, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(TRUE, 0));
		if (m_LayoutCursor.auto_sizing) {
			set_best_item_size(hItem, 0, 0);
		}
		move_cursor_to_next(hItem);
		return id;
	}
	KDialogId addCommandButton(KDialogId id, const char *text_u8) {
		std::wstring ws = _Utf8ToWide(text_u8);
		HWND hItem = CreateWindowExW(0, L"BUTTON", ws.c_str(),
			WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,
			m_LayoutCursor.next_x, m_LayoutCursor.next_y, m_LayoutCursor.next_w, m_LayoutCursor.next_h,
			m_hWnd, (HMENU)id, m_hInstance, NULL);
		SendMessageW(hItem, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(TRUE, 0));
		if (m_LayoutCursor.auto_sizing) {
			set_best_item_size(hItem, 0, 0);
		}
		m_CmdCtrls.insert(hItem);
		move_cursor_to_next(hItem);
		return id;
	}
	void setFocus() {
		SetFocus(get_last_item());
	}
	void setHint(const char *hint_u8) {
		_SetToolTip(get_last_item(), hint_u8);
	}
	void setExpand() {
		SetWindowLongPtrW(get_last_item(), GWLP_USERDATA, KDFLAG_EXPAND_X);
	}
	void separate() {
		newLine();
		HWND hItem = CreateWindowExW(0, L"STATIC", L"",
			WS_CHILD|WS_VISIBLE|SS_SUNKEN, 0, m_LayoutCursor.next_y, 0, 0, // サイズは後で決める
			m_hWnd, NULL, m_hInstance, NULL);
		SetWindowLongPtrW(hItem, GWLP_USERDATA, KDFLAG_SEPARATOR);
		newLine();
	}
	void newLine() {
		// カーソルを次の行の先頭に移動
		m_LayoutCursor.next_x = m_LayoutCursor.margin_x;
		m_LayoutCursor.next_y += m_LayoutCursor.curr_line_h + m_LayoutCursor.padding_y;
		m_LayoutCursor.curr_line_h = m_LayoutCursor.line_min_h;

		// コントロール矩形が収まるように、ウィンドウサイズを更新
		m_LayoutCursor._max_x = K__MAX(m_LayoutCursor._max_x, m_LayoutCursor.next_x);
		m_LayoutCursor._max_y = K__MAX(m_LayoutCursor._max_y, m_LayoutCursor.next_y);
	}
	void setInt(KDialogId id, int value) {
		HWND hItem = GetDlgItem(m_hWnd, id);
		SendMessageW(hItem, BM_SETCHECK, 0, value);
	}
	void setString(KDialogId id, const char *text_u8) {
		HWND hItem = GetDlgItem(m_hWnd, id);
		std::wstring ws = _Utf8ToWide(text_u8);
		_ConvertLineCode(ws);
		SetWindowTextW(hItem, ws.c_str());
	}
	int getInt(KDialogId id) {
		HWND hItem = GetDlgItem(m_hWnd, id);
		return (int)SendMessageW(hItem, BM_GETCHECK, 0, 0);
	}
	void getString(KDialogId id, char *text_u8, int maxbytes) {
		HWND hItem = GetDlgItem(m_hWnd, id);
		std::wstring ws(maxbytes, L'\0'); // ワイド文字数が maxbytes より大きくなることはないため、とりあえず maxbytes 文字分だけ確保しておく
		GetWindowTextW(hItem, &ws[0], (int)ws.size());
		_WideToUtf8(ws.c_str(), text_u8, maxbytes);
	}
	void * run_start(KDialog::Callback *cb) {
		m_CB = cb;
		if (m_LayoutCursor.auto_sizing) {
			setBestWindowSize(); // ウィンドウのサイズを自動決定する
		}
		update_item_size(); // 各コントロールのサイズを更新

		// 画面中心にウィンドウを表示
		_MoveToCenterInParent(m_hWnd, m_hParent);
		ShowWindow(m_hWnd, SW_SHOW);
		UpdateWindow(m_hWnd);

		// 生成のタイミングによっては、他のウィンドウの裏側に表示されてしまう場合があるため
		// 明示的にフォアグラウンドに移動する
		m_hFg = GetForegroundWindow();
		SetForegroundWindow(m_hWnd);

		return m_hWnd;
	}
	bool run_step() {
		MSG msg;
		if (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				return false;
			}
			if (IsDialogMessageW(m_hWnd, &msg)) {
				// Tab によるコントロールのフォーカス移動を実現するには
				// IsDialogMessage を呼ぶ必要がある。これが true を返した場合、
				// そのメッセージは既に処理済みなので TranslateMessage に渡してはいけない
			} else {
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			}
		}
		return true;
	}
	void run_end() {
		// フォアグラウンドを戻す
		SetForegroundWindow(m_hFg);
		m_CB = NULL;
		m_hFg = NULL;
	}
	void * get_handle() {
		return m_hWnd;
	}
	void run(KDialog::Callback *cb) {
		run_start(cb);
		while (run_step()/* && IsWindowVisible(m_hWnd)*/) {
			Sleep(10);
		}
		run_end();
	}
	void setBestWindowSize() {
		// 必要なクライアントサイズ
		int cw = m_LayoutCursor.content_w() + m_LayoutCursor.margin_x * 2;
		int ch = m_LayoutCursor.content_h() + m_LayoutCursor.margin_y * 2;

		// 必要なウィンドウサイズを計算
		LONG_PTR style = GetWindowLongPtrW(m_hWnd, GWL_STYLE);
		LONG_PTR exstyle = GetWindowLongPtrW(m_hWnd, GWL_EXSTYLE);
		RECT rect = {0, 0, cw, ch};
		AdjustWindowRectEx(&rect, (DWORD)style, FALSE, (DWORD)exstyle);

		// リサイズ
		int W = rect.right - rect.left;
		int H = rect.bottom - rect.top;
		SetWindowPos(m_hWnd, NULL, 0, 0, W, H, SWP_NOZORDER|SWP_NOMOVE);
	}
	int getCommand() {
		return m_LastCommandId;
	}


private:
	void clear_zero() {
		m_hInstance = NULL;
		m_hWnd = NULL;
		m_hParent = NULL;
		m_hFg = NULL;
		m_CB = NULL;
		ZeroMemory(&m_LayoutCursor, sizeof(m_LayoutCursor));
		m_Flags = 0;
	//	m_LastCommandId = 0; // ウィンドウが閉じた後も m_LastCommandId の値が必要なのでクリアしない
		m_CmdCtrls.clear();
	}
	HWND get_last_item() {
		// 最後に作成したコントロールを得る。
		// m_hWnd の子要素のうち、Zオーダーが一番最後になっているものを得る
		return GetWindow(GetWindow(m_hWnd, GW_CHILD), GW_HWNDLAST);
	}
	// コントロールのサイズを自動決定する
	// moreW 横の追加サイズ
	// moreH 縦の追加サイズ
	void set_best_item_size(HWND hItem, int moreW, int moreH) {
		// このコントロールが文字を含むと仮定し、
		// その文字を描画するために必要なサイズを得る
		int w, h;
		_ComputeTextSizeFromCaption(hItem, &w, &h);
		w += moreW;
		h += moreH;
		// スクロールバーを考慮
		if (GetWindowLongPtrW(hItem, GWL_STYLE) & WS_VSCROLL) w += GetSystemMetrics(SM_CXHSCROLL); // 垂直バーの幅
		if (GetWindowLongPtrW(hItem, GWL_STYLE) & WS_HSCROLL) h += GetSystemMetrics(SM_CYHSCROLL); // 水平バーの高さ
		// 余白を追加する
		w += m_LayoutCursor.padding_x * 2; // 左右で2個分カウントする
		h += m_LayoutCursor.padding_y * 2; // 上下で2個分カウントする
		// コントロールのサイズが指定されている場合は、
		// その値よりも大きくなった場合にだけサイズ変更する
		if (m_LayoutCursor.next_w > 0) w = K__MAX(w, m_LayoutCursor.next_w);
		if (m_LayoutCursor.next_h > 0) h = K__MAX(h, m_LayoutCursor.next_h);
		// リサイズ
		SetWindowPos(hItem, NULL, 0, 0, w, h, SWP_NOZORDER|SWP_NOMOVE);
	}
	void update_item_size() {
		// 各コントロールのサイズを調整する
		HWND hItem = NULL;
		while ((hItem=FindWindowExW(m_hWnd, hItem, NULL, NULL)) != NULL) {
			LONG_PTR flags = GetWindowLongPtrW(hItem, GWLP_USERDATA);
			if (flags & KDFLAG_EXPAND_X) {
				// EXPAND 設定されている。
				// コントロール矩形の右端をウィンドウ右端に合わせる（余白を考慮）
				RECT rc;
				get_item_rect(hItem, &rc);
				SetWindowPos(hItem, NULL, 0, 0, m_LayoutCursor.content_w(), rc.bottom-rc.top, SWP_NOZORDER|SWP_NOMOVE);
			}
			if (flags & KDFLAG_SEPARATOR) {
				// 区切り用の水平線。
				// コントロール右端をウィンドウ右端いっぱいまで広げる
				SetWindowPos(hItem, NULL, 0, 0, 10000/*適当な巨大値*/, 2, SWP_NOZORDER|SWP_NOMOVE);
			}
		}
	}
	void get_item_rect(HWND hItem, RECT *rect) {
		RECT s;
		GetWindowRect(hItem, &s); // item rect in screen

		POINT p0 = {s.left, s.top};
		POINT p1 = {s.right, s.bottom};
		ScreenToClient(m_hWnd, &p0);
		ScreenToClient(m_hWnd, &p1);
		
		rect->left   = p0.x;
		rect->top    = p0.y;
		rect->right  = p1.x;
		rect->bottom = p1.y;
	}
	void move_cursor_to_next(HWND hItem) {
		// コントロールの位置とサイズ
		RECT rect;
		get_item_rect(hItem, &rect);
		int w = rect.right - rect.left;
		int h = rect.bottom - rect.top;
		
		// カーソルをコントロールの右側に移動
		m_LayoutCursor.next_x += w + m_LayoutCursor.padding_x;

		// コントロール配置行の高さを更新
		m_LayoutCursor.curr_line_h = K__MAX(m_LayoutCursor.curr_line_h, h);

		// コントロール矩形が収まるように、ウィンドウサイズを更新
		m_LayoutCursor._max_x = K__MAX(m_LayoutCursor._max_x, (int)rect.right);
		m_LayoutCursor._max_y = K__MAX(m_LayoutCursor._max_y, (int)rect.bottom);
	}
	void on_WM_CLOSE() {
		if (m_CB) m_CB->on_dialog_close(m_ThisDialog);
	}
	void on_WM_COMMAND(HWND hItem, KDialogId id) {
		m_LastCommandId = id;
		if (m_CB) m_CB->on_dialog_click(m_ThisDialog, id);
		// コマンドコントロールだった場合はウィンドウを閉じる
		if (m_CmdCtrls.find(hItem) != m_CmdCtrls.end()) {
			close();
		}
	}
}; // class


#pragma region KDialog
KDialog::KDialog() {
	m_impl = new CDialogImpl(this);
}
KDialog::~KDialog() {
	delete m_impl;
}
void KDialog::create(const char *text_u8, Flags flags) {
	m_impl->create(0, 0, text_u8, flags);
}
void KDialog::create(int w, int h, const char *text_u8, Flags flags) {
	m_impl->create(w, h, text_u8, flags);
}
void KDialog::close() {
	m_impl->close();
}
KDialogId KDialog::addLabel(KDialogId id, const char *text_u8) {
	return m_impl->addLabel(id, text_u8);
}
KDialogId KDialog::addCheckBox(KDialogId id, const char *text_u8, bool value) {
	return m_impl->addCheckBox(id, text_u8, value);
}
KDialogId KDialog::addButton(KDialogId id, const char *text_u8) {
	return m_impl->addButton(id, text_u8);
}
KDialogId KDialog::addCommandButton(KDialogId id, const char *text_u8) {
	return m_impl->addCommandButton(id, text_u8);
}
KDialogId KDialog::addEdit(KDialogId id, const char *text_u8) {
	return m_impl->addEdit(id, text_u8);
}
KDialogId KDialog::addMemo(KDialogId id, const char *text_u8, bool wrap, bool readonly) {
	return m_impl->addMemo(id, text_u8, wrap, readonly);
}
void KDialog::setInt(KDialogId id, int value) {
	m_impl->setInt(id, value);
}
void KDialog::setString(KDialogId id, const char *text_u8) {
	m_impl->setString(id, text_u8);
}
int KDialog::getInt(KDialogId id) {
	return m_impl->getInt(id);
}
void KDialog::getString(KDialogId id, char *text_u8, int maxbytes) {
	m_impl->getString(id, text_u8, maxbytes);
}
void KDialog::getLayoutCursor(LayoutCursor *cur) const {
	m_impl->getLayoutCursor(cur);
}
void KDialog::setLayoutCursor(const LayoutCursor *cur) {
	m_impl->setLayoutCursor(cur);
}
void KDialog::newLine() {
	m_impl->newLine();
}
void KDialog::separate() {
	m_impl->separate();
}
void KDialog::setFocus() {
	m_impl->setFocus();
}
void KDialog::setHint(const char *hint_u8) {
	m_impl->setHint(hint_u8);
}
void KDialog::setExpand() {
	m_impl->setExpand();
}
void KDialog::setBestWindowSize() {
	m_impl->setBestWindowSize();
}
void KDialog::run(Callback *cb) {
	m_impl->run(cb);
}
void * KDialog::run_start(Callback *cb) {
	return m_impl->run_start(cb);
}
bool KDialog::run_step() {
	return m_impl->run_step();
}
void * KDialog::get_handle() {
	return m_impl->get_handle();
}
int KDialog::getCommand() {
	return m_impl->getCommand();
}
#pragma endregion // CDialogImpl










namespace Test {

const int ID_OK    = 1;
const int ID_DEBUG = 2;
const int ID_NEVER = 3;
const int ID_EXIT  = 4;

class CCallback: public KDialog::Callback {
public:
	bool m_show_debugger;
	bool m_never_show;

	CCallback() {
		m_show_debugger = false;
		m_never_show = false;
	}
	virtual void on_dialog_click(KDialog *wnd, KDialogId id) override {
		switch (id) {
		case ID_OK:
			wnd->close();
			m_show_debugger = false;
			break;
		case ID_DEBUG:
			wnd->close();
			m_show_debugger = true;
			break;
		case ID_EXIT:
			exit(-1); // 強制終了
			break;
		}
	}
	virtual void on_dialog_close(KDialog *wnd) override {
		// チェックボックス ID_NEVER にチェックが付いているかどうか
		m_never_show = (wnd->getInt(ID_NEVER) != 0);
	}
};
static CCallback s_cb;

void Test_dialog(const char *text_u8) {
	KDialog z;
	z.create(u8"エラーダイアログサンプル");
	z.addLabel(0, u8"エラーが発生しました。エラーメッセージは以下の通りです：");
	z.newLine();
	{
		// 次コントロールの位置とサイズを変更
		KDialog::LayoutCursor cur;
		z.getLayoutCursor(&cur);
		cur.next_h = 100;
		z.setLayoutCursor(&cur);
	}
	z.addMemo(0, text_u8, false, true);
	z.setHint(
		u8"プログラムによって報告されたエラーメッセージです。\n"
		u8"Ctrl+C または右クリックメニューでコピーできます");
	{
		// 次コントロールの位置とサイズを変更
		KDialog::LayoutCursor cur;
		z.getLayoutCursor(&cur);
		cur.next_h = 0;
		z.setLayoutCursor(&cur);
	}
	z.setExpand();
	z.newLine();
	z.separate();
	z.newLine();
	z.addCheckBox(ID_NEVER, u8"次回からこのダイアログを開かない", false);
	z.setHint(
		u8"次からはエラーが発生してもダイアログを出さないようにします。\n"
		u8"ただし、プログラムを終了するとこの設定はリセットされます");
	z.newLine();
	z.addButton(ID_OK, u8"OK");
	z.setFocus(); // [OK] ボタンに初期フォーカスを合わせておく
	z.addButton(ID_EXIT, u8"アプリ終了");

	// デバッガーから実行している場合、ブレークするかどうかの選択肢を出す
	if (IsDebuggerPresent()) {
		z.addButton(ID_DEBUG, u8"デバッグ");
	}
	z.run(&s_cb);
	if (s_cb.m_show_debugger) {
		DebugBreak();
	}
}

} // Test


} // namespace


