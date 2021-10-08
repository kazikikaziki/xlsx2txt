#pragma once
namespace Kamilo {

class KSystem {
public:
	enum StrProp {
		STRPROP_CPUNAME,
		STRPROP_CPUVENDOR,
		STRPROP_PRODUCTNAME,
		STRPROP_LOCALTIME,
		STRPROP_SELFPATH,
		STRPROP_FONTDIR
	};
	enum IntProp {
		/// システムの言語ID
		///
		/// 0x0C09 : 英語
		/// 0x0411 : 日本語
		/// 0x0404 : 中国語（台湾）
		/// 0x0804 : 中国語（中国）
		/// 0x0C04 : 中国語（香港）
		/// 0x1004 : 中国語（シンガポール）
		/// 0x1404 : 中国語（マカオ）
		///
		/// ロケール ID (LCID) の一覧
		/// "https://docs.microsoft.com/ja-jp/previous-versions/windows/scripting/cc392381(v=msdn.10)"
		///
		INTPROP_LANGID,

		/// SSE2に対応しているかどうか
		///
		/// Intel は Pentium4 (2000年) 以降、AMD は Athlon64 (2004年) 以降に対応している。
		/// ちなみにSSE2非対応のPCはWin7の更新対象から外れされた (2018年3月) ほか、
		/// Google Chrome は2014年5月から、
		/// Firefox は2016年4月以降からSSE2必須になった (SSE2非対応マシンでは起動できない）
		/// https://security.srad.jp/story/18/06/21/0850222/
		/// https://rockridge.hatenablog.com/entry/2016/05/29/223926
		/// https://ameblo.jp/miyou55mane/entry-12272544473.html
		///
		INTPROP_SSE2,

		/// マシンが搭載しているメモリ
		INTPROP_SYSMEM_TOTAL_KB,
		INTPROP_SYSMEM_AVAIL_KB,

		/// アプリケーションに割り当てられたメモリ（普通は2GB）
		INTPROP_APPMEM_TOTAL_KB,
		INTPROP_APPMEM_AVAIL_KB,

		/// プライマリモニタの画面全体の幅と高さ
		INTPROP_SCREEN_W,
		INTPROP_SCREEN_H,

		/// プライマリモニタの最大化ウィンドウのサイズ
		INTPROP_MAX_WINDOW_SIZE_W,
		INTPROP_MAX_WINDOW_SIZE_H,

		/// プライマリモニタの最大化ウィンドウのクライアント領域の幅と高さ
		INTPROP_MAX_WINDOW_CLIENT_W,
		INTPROP_MAX_WINDOW_CLIENT_H,

		/// キーリピート認識までの遅延時間と間隔（ミリ秒）
		INTPROP_KEYREPEAT_DELAY_MSEC,
		INTPROP_KEYREPEAT_INTERVAL_MSEC,
	};

	static bool getString(StrProp id, char *out_u8, int size);
	static int getInt(IntProp id);
};

} // namespace
