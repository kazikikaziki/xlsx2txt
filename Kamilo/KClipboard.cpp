#include "KClipboard.h"
#include <Windows.h>

namespace Kamilo {

static bool KClipboard__SetTextW(const std::wstring &text) {
	// クリップボードに送るデータはグローバルメモリでないといけない。
	// グローバルメモリを確保してテキストをコピーする
	HGLOBAL hMem = NULL;
	{
		size_t num_wchars = text.size() + 1; // with null terminator
		hMem = GlobalAlloc(GHND|GMEM_SHARE, sizeof(wchar_t) * num_wchars);
		if (hMem) {
			wchar_t *ptr = (wchar_t *)GlobalLock(hMem);
			if (ptr) {
				wcscpy_s(ptr, num_wchars, text.c_str());
				GlobalUnlock(hMem);
			}
		}
	}

	// クリップボードを開いてバッファをセットする。
	// セットすると GlobalAlloc で確保したメモリはOS側で管理されるようになるので
	// GlobalFree は呼ばなくてよい（呼んではいけない）
	// ※ただしセットに失敗した場合は自分で解放する
	if (OpenClipboard(NULL)) {
		EmptyClipboard();
		SetClipboardData(CF_UNICODETEXT, hMem);
		CloseClipboard();
		return true;
	} else {
		GlobalFree(hMem);
		return false;
	}
}
static bool KClipboard__GetTextW(std::wstring &text) {
	bool result = false;
	if (OpenClipboard(NULL)) {
		HGLOBAL hBuf = GetClipboardData(CF_UNICODETEXT);
		if (hBuf) {
			wchar_t *data = (wchar_t *)GlobalLock(hBuf);
			if (data) {
				text = data;
				GlobalUnlock(hBuf);
				result = true;
			}
		}
		CloseClipboard();
	}
	return result;
};
static bool KClipboard__SetTextU8(const std::string &text_u8) {
	std::wstring ws;
	ws.resize(text_u8.size() + 256);
	MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text_u8.c_str(), -1, &ws[0], ws.size());
	return KClipboard__SetTextW(ws.c_str());
}
static bool KClipboard__GetTextU8(std::string &out_text_u8) {
	std::wstring ws;
	if (KClipboard__GetTextW(ws)) {
		out_text_u8.resize(ws.size() * sizeof(wchar_t) + 256);
		WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, ws.c_str(), -1, &out_text_u8[0], out_text_u8.size(), NULL, NULL);
		// out_text_u8 は文字列の長さとは一致しない (\0 以降のスペースも含んでいる）ので、正しい文字列長さを得る
		int u8len = strlen(out_text_u8.c_str());
		out_text_u8.resize(u8len); // strlen, size, length の結果を一致させる
		return true;
	}
	return false;
}

bool KClipboard::setText(const std::string &text_u8) {
	return KClipboard__SetTextU8(text_u8);
}
bool KClipboard::getText(std::string &out_text_u8) {
	return KClipboard__GetTextU8(out_text_u8);
}


} // namespace
