#include "KSystem.h"
//
#include "KLog.h"
#include "KInternal.h"
#include <assert.h>
#include <intrin.h> // __cpuid
#include <time.h>
#include <Windows.h>
#include <Shlobj.h> // SHGetFolderPath
#include <locale.h>

namespace Kamilo {

static void _ReplaceW(wchar_t *s, wchar_t before, wchar_t after) {
	assert(s);
	for (size_t i=0; s[i]; i++) {
		if (s[i] == before) {
			s[i] = after;
		}
	}
}

static void KSys_GetLocalTimeString(char *s, int size) {
	time_t gmt = time(nullptr);
	struct tm lt;
	localtime_s(&lt, &gmt);
	strftime(s, size, "%y/%m/%d-%H:%M:%S", &lt);
}
static void KSys_cpuid(int *int4, int id) {
	__cpuid(int4, id); // <intrin.h>
}
static bool KSys_HasSSE2() {
	int x[4] = {0};
	KSys_cpuid(x, 1);
	return (x[3] & (1<<26)) != 0;

}
static void KSys_GetMemorySize(int *total_kb, int *avail_kb) {
	assert(total_kb);
	assert(avail_kb);
	MEMORYSTATUSEX ms = {sizeof(MEMORYSTATUSEX)};
	if (GlobalMemoryStatusEx(&ms)) {
		*total_kb = (int)(ms.ullTotalPhys / 1024);
		*avail_kb = (int)(ms.ullAvailPhys / 1024);
	}
}


static void KSys_GetSelfFileName(char *name_u8, int maxsize) {
	wchar_t ws[MAX_PATH];
	GetModuleFileNameW(nullptr, ws, MAX_PATH);
	_ReplaceW(ws, L'\\', L'/'); // Blackslash --> Slash
	WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, ws, -1, name_u8, maxsize, nullptr, nullptr);
}
static void KSys_GetFontDir(char *dir_u8, int maxsize) {
	wchar_t ws[MAX_PATH];
	SHGetFolderPathW(nullptr, CSIDL_FONTS, nullptr, SHGFP_TYPE_CURRENT, ws);
	_ReplaceW(ws, L'\\', L'/'); // Blackslash --> Slash
	WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, ws, -1, dir_u8, maxsize, nullptr, nullptr);
}
static int KSys_GetLangId() {
	return GetUserDefaultUILanguage();
}

static void KSys_GetCpuName(char *s, int maxsize) {
	assert(s);
	// https://msdn.microsoft.com/ja-jp/library/hskdteyh.aspx
	// https://msdn.microsoft.com/ja-jp/library/hskdteyh(v=vs.110).aspx
	// https://gist.github.com/t-mat/3769328
	// http://caspar.hazymoon.jp/OpenBSD/annex/cpuid.html
	// https://www.timbreofprogram.info/blog/archives/951
	int x[12] = {0};
	KSys_cpuid(x+0, 0x80000002);
	KSys_cpuid(x+4, 0x80000003);
	KSys_cpuid(x+8, 0x80000004);
	const char *ss = (const char *)x;
	while (isspace(*ss)) ss++; // skip space
	strcpy_s(s, maxsize, ss);
}
void KSys_GetCpuVendor(char *s, int maxsize) {
	assert(s);
	char x[sizeof(int) * 4 + 1]; // 終端文字を入れるために+1しておく
	memset(x, 0, sizeof(x));
	KSys_cpuid((int*)x, 0);
	// cpu vendoer は ebx, ecx, edx に入っている。
	// つまり x[4..16] に char として12文字文ある。さらに↑の memsetで入れた 0 が終端文字として残っている
	strcpy_s(s, maxsize, x+4);
}
void KSys_GetProductName(char *name_u8, int maxsize) {
	assert(name_u8);
	// バージョン情報を取得したいが、GetVersionInfo は Win8で使えない。
	// かといって VersionHelpers.h を使おうとすると WinXPが未対応になる。
	// ならば VerifyVersionInfo を使えばよいように思えるがこれにも罠があって、
	// Win8.1以降の場合、マニフェストで動作対象に Win8なりWin10なりを明示的に指定しておかないと
	// 常に Win8 扱いになってしまう。
	// もう面倒なのでレジストリを直接見る。
	//
	// Windows 8.1 でバージョン判別するときの注意点
	// http://grabacr.net/archives/1175
	//
	HKEY hKey = nullptr;
	RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &hKey);
	WCHAR data[MAX_PATH] = {0};
	DWORD dataBytes = sizeof(data);
	DWORD type = REG_SZ;
	RegQueryValueExW(hKey, L"ProductName", nullptr, &type, (LPBYTE)data, &dataBytes);
	RegCloseKey(hKey);
	WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, data, -1, name_u8, maxsize, nullptr, nullptr);
}

bool KSystem::getString(StrProp id, char *out_u8, int size) {
	switch (id) {
	case STRPROP_CPUNAME:
		KSys_GetCpuName(out_u8, size);
		return true;
	case STRPROP_CPUVENDOR:
		KSys_GetCpuVendor(out_u8, size);
		return true;
	case STRPROP_PRODUCTNAME:
		KSys_GetProductName(out_u8, size);
		return true;
	case STRPROP_LOCALTIME:
		KSys_GetLocalTimeString(out_u8, size);
		return true;
	case STRPROP_SELFPATH:
		KSys_GetSelfFileName(out_u8, size);
		return true;
	case STRPROP_FONTDIR:
		KSys_GetFontDir(out_u8, size);
		return true;
	}
	return false;
}
int KSystem::getInt(IntProp id) {
	switch (id) {
	case INTPROP_LANGID:
		return GetUserDefaultUILanguage();
	case INTPROP_SYSMEM_TOTAL_KB:
		{
			MEMORYSTATUSEX ms = {sizeof(MEMORYSTATUSEX)};
			if (GlobalMemoryStatusEx(&ms)) {
				return (int)(ms.ullTotalPhys / 1024); // KB
			}
			return 0;
		}
	case INTPROP_SYSMEM_AVAIL_KB:
		{
			MEMORYSTATUSEX ms = {sizeof(MEMORYSTATUSEX)};
			if (GlobalMemoryStatusEx(&ms)) {
				return (int)(ms.ullAvailPhys / 1024); // KB
			}
			return 0;
		}
	case INTPROP_APPMEM_TOTAL_KB:
		{
			MEMORYSTATUSEX ms = {sizeof(MEMORYSTATUSEX)};
			if (GlobalMemoryStatusEx(&ms)) {
				return (int)(ms.ullTotalVirtual / 1024); // KB
			}
			return 0;
		}
	case INTPROP_APPMEM_AVAIL_KB:
		{
			MEMORYSTATUSEX ms = {sizeof(MEMORYSTATUSEX)};
			if (GlobalMemoryStatusEx(&ms)) {
				return (int)(ms.ullAvailVirtual / 1024); // KB
			}
			return 0;
		}
	case INTPROP_SSE2:
		return KSys_HasSSE2();
	case INTPROP_SCREEN_W:
		return GetSystemMetrics(SM_CXSCREEN);
	case INTPROP_SCREEN_H:
		return GetSystemMetrics(SM_CYSCREEN);
	case INTPROP_MAX_WINDOW_SIZE_W:
		return GetSystemMetrics(SM_CXMAXIMIZED);
	case INTPROP_MAX_WINDOW_SIZE_H:
		return GetSystemMetrics(SM_CYMAXIMIZED);
	case INTPROP_MAX_WINDOW_CLIENT_W:
		return GetSystemMetrics(SM_CXFULLSCREEN);
	case INTPROP_MAX_WINDOW_CLIENT_H:
		return GetSystemMetrics(SM_CYFULLSCREEN);
	case INTPROP_KEYREPEAT_DELAY_MSEC:
		{
			// https://msdn.microsoft.com/library/cc429946.aspx
			int val = 0;
			if (SystemParametersInfo(SPI_GETKEYBOARDDELAY, 0, &val, 0)) {
				// リピート遅延: 0～3 の値。0 で約250ms, 3で約1000msになる（入力機器によって誤差あり）
				return 250 + 250 * val; 
			}
			return -1;
		}
	case INTPROP_KEYREPEAT_INTERVAL_MSEC:
		{
			#define NORM(t, a, b)  (float)(t - a) / (b - a)
			#define LERP(a, b, t)  (a + (b - a) * t)

			// https://msdn.microsoft.com/library/cc429946.aspx
			int val = 0;
			if (SystemParametersInfo(SPI_GETKEYBOARDSPEED, 0, &val, 0)) {
				// リピート速度: 0～31 の値。0 で秒間2.5回、31で秒間約30回になる（入力機器によって誤差あり）
				const float FREQ_MIN = 2.5f;
				const float FREQ_MAX = 30.0f;
				float k = NORM(val, 0, 31);
				float freq = LERP(FREQ_MIN, FREQ_MAX, k);
				return (int)(1000.0f / freq);
			}
			return -1;

			#undef LERP
			#undef NORM
		}
	}
	return -1;
}


void KSystem::printInfo(KLogger *log, const KUserAppDesc *desc) {
	// バージョン
	if (desc) {
		log->emitf(KLogLv_NONE, "");
		log->emitf(KLogLv_NONE, "---- App info ----");
		log->emitf(KLogLv_NONE, "## APPNAME : %s", desc->name.c_str());
		log->emitf(KLogLv_NONE, "## EDITION : %s", desc->edition.c_str());
		log->emitf(KLogLv_NONE, "## VERSION : %s", desc->version.c_str());
		log->emitf(KLogLv_NONE, "## INTERNAL_VERSION: %s", desc->internal_version.c_str());
		log->emitf(KLogLv_NONE, "## BUILD   : %s %s", __DATE__, __TIME__);
		log->emitf(KLogLv_NONE, "");
	}

	const int LEN = 256;
	char productname[LEN] = {0};
	char cpuname[LEN] = {0};
	char cpuvendor[LEN] = {0};
	char localtime[LEN] = {0};
	char exepath[LEN] = {0};
	KSystem::getString(KSystem::STRPROP_PRODUCTNAME, productname, LEN);
	KSystem::getString(KSystem::STRPROP_CPUNAME, cpuname, LEN);
	KSystem::getString(KSystem::STRPROP_CPUVENDOR, cpuvendor, LEN);
	KSystem::getString(KSystem::STRPROP_LOCALTIME, localtime, LEN);
	KSystem::getString(KSystem::STRPROP_SELFPATH, exepath, LEN);
	std::string curdir = K::sysGetCurrentDir();

	int langid = KSystem::getInt(KSystem::INTPROP_LANGID);
	int sysmem_total_kb = KSystem::getInt(KSystem::INTPROP_SYSMEM_TOTAL_KB);
	int sysmem_avail_kb = KSystem::getInt(KSystem::INTPROP_SYSMEM_AVAIL_KB);
	int appmem_total_kb = KSystem::getInt(KSystem::INTPROP_APPMEM_TOTAL_KB);
	int appmem_avail_kb = KSystem::getInt(KSystem::INTPROP_APPMEM_AVAIL_KB);
	int screen_w = KSystem::getInt(KSystem::INTPROP_SCREEN_W);
	int screen_h = KSystem::getInt(KSystem::INTPROP_SCREEN_H);
	int has_sse2 = KSystem::getInt(KSystem::INTPROP_SSE2);
	
	char tmp[256];
	strcpy_s(tmp, setlocale(LC_CTYPE, nullptr));
	setlocale(LC_CTYPE, ""); // ロケール確認のために OS 標準に戻す
	log->emitf(KLogLv_NONE, "---- PC Info ----");
	log->emitf(KLogLv_NONE, "Lib Build : %s %s", __DATE__, __TIME__);
	log->emitf(KLogLv_NONE, "OS Version: %s", productname);
	log->emitf(KLogLv_NONE, "CPU Name  : %s", cpuname);
	log->emitf(KLogLv_NONE, "CPU Vendor: %s", cpuvendor);
	log->emitf(KLogLv_NONE, "DateTime  : %s", localtime);
	log->emitf(KLogLv_NONE, "CurrDir   : %s", curdir.c_str());
	log->emitf(KLogLv_NONE, "ExecName  : %s", exepath);
	log->emitf(KLogLv_NONE, "Locale    : %s", setlocale(LC_CTYPE, nullptr));
	log->emitf(KLogLv_NONE, "LangID    : %d (0x%04x)", langid, langid);
	log->emitf(KLogLv_NONE, "Sys Mem Total : %.1f[MB]", sysmem_total_kb/1024.0f);
	log->emitf(KLogLv_NONE, "Sys Mem Using : %.1f[MB]", (sysmem_total_kb - sysmem_avail_kb)/1024.0f);
	log->emitf(KLogLv_NONE, "App Mem Total : %.1f[MB]", appmem_total_kb/1024.0f);
	log->emitf(KLogLv_NONE, "App Mem Using : %.1f[MB]", (appmem_total_kb - appmem_avail_kb)/1024.0f);
	log->emitf(KLogLv_NONE, "SSE2      : %s", has_sse2 ? "Yes" : "No");
	log->emitf(KLogLv_NONE, "Screen    : %d x %d", screen_w, screen_h);
	log->emitf(KLogLv_NONE, "");
	setlocale(LC_CTYPE, tmp); // ロケール戻す
}

} // namespace
