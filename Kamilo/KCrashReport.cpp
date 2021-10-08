#include "KCrashReport.h"

//#define NO_DBGHELP // use dbghelp.h, dbghelp.lib


#include <Windows.h>
#ifndef NO_DBGHELP
#	include <DbgHelp.h>  // SymInitialize : requires dbghelp.lib
#endif
#include <tlhelp32.h> // CreateToolhelp32Snapshot 
#include <Shlwapi.h>
#include <time.h>
#include <algorithm>
#include <vector>
#include "KDebug.h"
#include "KLog.h"
#include "KSystem.h"
#include "KInternal.h"

#define EXCEPTION_LOG_FILE_NAME   "###ERROR###.txt"
#define UNREPOTED_EXCEPTION_FILE  "~exception.tmp"
#define MEMMAP_FILENAME           "~memmap.tmp"

namespace Kamilo {

/// Windowsのイベントを得る
class CWin32EventLog {
public:
	/// 文字列パラメータの最大数
	static const WORD MAX_STRING_ARGS = 256;

	/// レコードに記録されている文字列パラメータ
	struct STRPARAMS {
		LPWSTR source; ///< イベントの発生元
		LPWSTR computer; ///< コンピュータ名
		LPWSTR args[MAX_STRING_ARGS]; ///< ログメッセージを構築するための文字列パラメータ
		WORD num_args; ///< 使用している文字列パラメータの数
	};

	CWin32EventLog() {
		hLog_ = NULL;
		count_ = 0;
		index_ = 0;
		eof_ = true;
	}

	/// イベントログを開く
	/// name はイベントビュアーのカテゴリを指定する。
	/// アプリケーションのログなら "Application" を指定する
	/// これは、Win10ならタスクバーのWindowsロゴ（スタートメニュー）で右クリック
	/// → イベントビューア → Windowsログ → Application で見るのと同じ
	bool openLog(const wchar_t *name=L"Application") {
		count_ = 0;
		index_ = 0;
		hLog_ = OpenEventLogW(NULL, name);
		if (hLog_ == NULL) return false;
		GetNumberOfEventLogRecords(hLog_, &count_);

		// ログレコード１個分だけ確保する
		bufsize_ = sizeof(EVENTLOGRECORD);
		buf_ = (EVENTLOGRECORD *)malloc(bufsize_);
		eof_ = false;
		return true;
	}

	/// ログを閉じる
	void closeLog() {
		CloseEventLog(hLog_);
		free(buf_);
		buf_ = NULL;
		bufsize_ = 0;
		eof_ = true;
	}

	/// 次のレコードに移動
	bool readNext() {
		// イベントログの読み込み  http://www.yuboo.net/~ybsystem/sys_build/win_client/evtlog_read.html
		// Event Log       http://plaza.rakuten.co.jp/u703331/diary/200608110000/
		// Event Log その２ http://plaza.rakuten.co.jp/u703331/diary/200608110001/
		// Event Log その３ http://plaza.rakuten.co.jp/u703331/diary/200608110002/
		// Event Log その４ http://plaza.rakuten.co.jp/u703331/diary/200608110003/

		// ReadEventLog は指定したバッファサイズに詰め込めるだけのイベントを返すため、複数のイベントを戻してくる場合がある
		// 常にイベントを１つだけ取得するため、最低バッファサイズを毎回調べる

		// まずバッファサイズ 0 にして ReadEventLog を呼ぶ。
		// サイズが足りないため必ず失敗する。そこで nextsize の値を調べる
		DWORD tmp_bufsize = sizeof(EVENTLOGRECORD); // 実際のバッファはこれより大きく確保されているが、一番小さいサイズを指定しておく
		DWORD readsize = 0, reqsize = 0;
		SetLastError(0);
		ReadEventLogW(hLog_, EVENTLOG_BACKWARDS_READ|EVENTLOG_SEQUENTIAL_READ, 0, buf_, tmp_bufsize, &readsize, &reqsize);
		DWORD err = GetLastError();
		if (err == ERROR_HANDLE_EOF) {
			eof_ = true;
			return false; // ファイル終端に達した
		}
		if (err == ERROR_INSUFFICIENT_BUFFER || err == S_OK/*成功するはずはないが...*/) {
			// 当然のごとく、サイズが足りない。これで良い
		} else {
		//	HRESULT hr = HRESULT_FROM_WIN32(err);
			eof_ = true;
			return false; // 想定外のエラー
		}

		// 要求されたサイズを満たすようにバッファを調整
		if (bufsize_ < reqsize) {
			bufsize_ = reqsize;
			buf_ = (EVENTLOGRECORD *)realloc(buf_, bufsize_);
			K__ASSERT(buf_);
		}

		// サイズを正しく設定して再取得
		tmp_bufsize = reqsize;
		if (!ReadEventLogW(hLog_, EVENTLOG_BACKWARDS_READ|EVENTLOG_SEQUENTIAL_READ, 0, buf_, tmp_bufsize, &readsize, &reqsize)) {
		//	HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
			eof_ = true;
			return false; // 想定外のエラー
		}
		K__ASSERT(buf_->Length == tmp_bufsize);
		K__ASSERT(buf_->Length == readsize);
		return true;
	}

	/// 現在のレコードを取得する。
	/// ここで返されるポインタは readNext を呼ぶと無効になる
	EVENTLOGRECORD * getRecord() {
		return eof_ ? NULL : buf_;
	}

	/// EVENTLOGRECORD から STRPARAMS を取得する
	void getStringParams(STRPARAMS *params, const EVENTLOGRECORD *e) {
		#define OFFSET_PTR(p, offset) (((uint8_t *)(p)) + (offset)) // p の型に関係なく常に、offset バイトだけずらす
		K__ASSERT(params);
		K__ASSERT(e);
		ZeroMemory(params, sizeof(STRPARAMS));
		params->source = (LPWSTR)OFFSET_PTR(e, sizeof(EVENTLOGRECORD));
		params->computer = params->source + wcslen(params->source) + 1;
		params->num_args = e->NumStrings;
		LPWSTR msg_ptr = (LPWSTR)OFFSET_PTR(e, e->StringOffset);
		for (WORD i=0; i<e->NumStrings; i++) {
			params->args[i] = msg_ptr;
			msg_ptr += wcslen(msg_ptr) + 1;
		}
		#undef OFFSET_PTR
	}

	/// イベントレコードを可読化したものを fp に書き出す
	void printRecord(const EVENTLOGRECORD *e, FILE *fp) {
		time_t gen = (time_t)e->TimeGenerated;
		STRPARAMS params;
		getStringParams(&params, e);

		struct tm tm;
		memset(&tm, 0, sizeof(tm));
		localtime_s(&tm, &gen);
		fwprintf(fp, 
			L"Date      : %4.4d/%2.2d/%2.2d %2.2d:%2.2d:%2.2d\n",
			tm.tm_year + 1900,
			tm.tm_mon + 1,
			tm.tm_mday,
			tm.tm_hour,
			tm.tm_min,
			tm.tm_sec
		);
		switch(e->EventType) {
		case EVENTLOG_SUCCESS:          fwprintf(fp, L"Event Type: Success\n"); break;
		case EVENTLOG_ERROR_TYPE:       fwprintf(fp, L"Event Type: Error\n"); break;
		case EVENTLOG_WARNING_TYPE:     fwprintf(fp, L"Event Type: Warning\n"); break;
		case EVENTLOG_INFORMATION_TYPE: fwprintf(fp, L"Event Type: Information\n"); break;
		case EVENTLOG_AUDIT_SUCCESS:    fwprintf(fp, L"Event Type: Audit Success\n"); break;
		case EVENTLOG_AUDIT_FAILURE:    fwprintf(fp, L"Event Type: Audit Failure\n"); break;
		default:                        fwprintf(fp, L"Event Type: Unknown\n"); break;
		}

		fwprintf(fp, L"Event ID  : %d\n", e->EventID);
		fwprintf(fp, L"Source    : %ls\n", params.source);
		for (size_t i=0; i<params.num_args; i++) {
			fwprintf(fp, L"  %02d: %ls\n", (int)i, params.args[i]);
		}
		fwprintf(fp, L"\n");

		// 整形済みメッセージの取得に成功していれば、それを出力する
		fwprintf(fp, L"FORMATTED MESSAGE =========>\n\n");
		{
			std::wstring ws;
			if (getDetail(e, &ws)) {
				fwprintf(fp, L"%ls\n", ws.c_str());
			}
		}
		fwprintf(fp, L"<========= FORMATTED MESSAGE\n\n");
	}

	/// イベントビューアで見らえるのと同じイベント詳細テキストを得る
	bool getDetail(const EVENTLOGRECORD *e, std::wstring *ws) {
		K__ASSERT(e);
		K__ASSERT(ws);
		STRPARAMS params;
		getStringParams(&params, e);

		ws->clear();

		// 文字列パラメータを列挙
		if (1) {
			*ws += L"■文字列パラメータ\n";
			for (int i=0; i<params.num_args; i++) {
				const int MAXSIZE = 256;
				wchar_t t[MAXSIZE];
				swprintf_s(t, MAXSIZE, L"[%d]: %s\n", i, params.args[i]);
				*ws += t;
			}
			*ws += L"\n";
		}
		// 整形済みメッセージを取得
		if (1) {
			// 整形文字列が入っているDLLファイル名を得る
			// http://plaza.rakuten.co.jp/u703331/diary/200608110003/
			WCHAR szExpandModuleName[MAX_PATH] = {0};
			{
				HKEY hAppKey = NULL;
				HKEY hSrcKey = NULL;
				WCHAR moduleName[MAX_PATH] = {0};
				DWORD moduleNameBytes = sizeof(moduleName);
				RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application", 0, KEY_READ, &hAppKey);
				RegOpenKeyExW(hAppKey, params.source, 0, KEY_READ, &hSrcKey);
				RegQueryValueExW(hSrcKey, L"EventMessageFile", NULL, NULL, (LPBYTE)moduleName, &moduleNameBytes);
				ExpandEnvironmentStringsW(moduleName, szExpandModuleName, MAX_PATH);
				RegCloseKey(hSrcKey);
				RegCloseKey(hAppKey);
			}

			// イベントログ用の整形済みメッセージを得る
			// これで、イベントビューアに表示されるものと同じメッセージ文字列が得られる
			void *pMessage = NULL;
			HMODULE hModule = LoadLibraryExW(szExpandModuleName, NULL, DONT_RESOLVE_DLL_REFERENCES|LOAD_LIBRARY_AS_DATAFILE);
		//	DWORD lang_en = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US); // 0x0C09
			DWORD lang_jp = MAKELANGID(LANG_JAPANESE, SUBLANG_JAPANESE_JAPAN); // 0x0411
			FormatMessageW(
				FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_HMODULE|FORMAT_MESSAGE_ARGUMENT_ARRAY,
				hModule, e->EventID, /*lang_en*/lang_jp, (LPWSTR)&pMessage, 0, (va_list *)&params.args[0]
			);
			if (pMessage) {
				*ws += L"■整形済みメッセージ\n";
				if (0) {
					// そのままコピー
					*ws += (LPWSTR)pMessage;
				} else {
					// 改行コード "\r\n"を "\n" に置換してコピー
					const wchar_t *msg = (LPWSTR)pMessage;
					const size_t len = wcslen(msg);
					for (size_t i=0; i<len; i++) {
						if (msg[i]==L'\r') continue;
						ws->push_back(msg[i]);
					}
				}
				*ws += L"\n";
				LocalFree(pMessage);
			}
		}
		return true;
	}
	
	/// イベントビューアを起動する
	bool executeEventViewer() {
		int stat = (int)ShellExecuteW(NULL, L"open", L"eventvwr.exe", L"/c:Application", NULL, SW_SHOWNORMAL);
		return stat > 32; // ShellExecute のエラーコードは 32 以下の値である
	}

private:
	HANDLE hLog_;
	DWORD count_;
	DWORD index_;
	DWORD bufsize_;
	EVENTLOGRECORD *buf_;
	bool eof_;
};

/// エラーイベントを検索し、過去 limit_seconds 秒以内に発生したエラーのうち最も新しいものを返す。
/// キーワード keyword が指定されていれば、その文字列を含むエラーだけを検索対象にする。
/// イベントが見つかれば、そのエラーメッセージを返す。
/// 見つからなければ空文字列を返す。
/// キーワードには、例えば実行ファイル名などを指定する
static std::wstring _FindErrorLog(int limit_seconds, const wchar_t *keyword, intptr_t *fault_addr) {
	if (limit_seconds < 0) limit_seconds = 0;
	std::wstring errmsg;
	CWin32EventLog log;
	log.openLog();
	time_t curr_time = ::time(NULL);
	while (log.readNext()) {
		EVENTLOGRECORD *e = log.getRecord();
		// ログ発生時刻を確認し、
		// 過去 sec_limit 秒以内に発生したログだけ調べる。
		// ちなみにイベントログは発生時刻順に並んでいるので、
		// TIME_SEC 秒以上経過したログが見つかった場合、そこで検索を打ち切ってよい
		if (limit_seconds > 0) {
			if (e->TimeGenerated < curr_time - limit_seconds) { 
				break;
			}
		}
		// ログの種類が「エラー」ならば、文字列パラメータを調べる。
		// このパラメータが特定のキーワードを含んでいる場合、目的のエラーログであるとみなす。
		if (e->EventType == EVENTLOG_ERROR_TYPE) {
			CWin32EventLog::STRPARAMS strs;
			log.getStringParams(&strs, e);
			for (size_t i=0; i<strs.num_args; i++) {
				const wchar_t *s = strs.args[i];
				if (keyword==NULL || wcsstr(s, keyword) != NULL) {
					// 目的のキーワードを含むエラーメッセージが見つかった！
					// 整形済みのエラーメッセージを得る
					log.getDetail(e, &errmsg);
					if (fault_addr) *fault_addr = wcstoul(strs.args[7], NULL, 16); // <-- strs.args[7] には例外発生オフセットが入っているが、プレフィックス 0x はついていないので基数16を明示する
					break;
				}
			}
		}
	}
	log.closeLog();
	return errmsg;
}

class CWin32ErrorReporting {
public:
	struct INFO {
		DWORD PID;
		DWORD ExceptionCode;
		DWORD ExceptionFlags;
		PVOID ExceptionAddress;
		DWORD SysMemTotalKB;
		DWORD SysMemAvailKB;
		DWORD AppMemTotalKB;
		DWORD AppMemAvailKB;
		time_t Time;
	};

	// 例外ファイルを保存する
	static void saveExceptionToFile(FILE *file, struct _EXCEPTION_POINTERS *e) {
		MEMORYSTATUSEX ms = {sizeof(MEMORYSTATUSEX)};
		GlobalMemoryStatusEx(&ms);

		INFO info;
		ZeroMemory(&info, sizeof(INFO));
		info.PID = GetCurrentProcessId();
		info.ExceptionAddress = e->ExceptionRecord->ExceptionAddress;
		info.ExceptionCode = e->ExceptionRecord->ExceptionCode;
		info.ExceptionFlags = e->ExceptionRecord->ExceptionFlags;
		info.SysMemTotalKB = (int)(ms.ullTotalPhys / 1024);
		info.SysMemAvailKB = (int)(ms.ullAvailPhys / 1024);
		info.AppMemTotalKB = (int)(ms.ullTotalVirtual / 1024);
		info.AppMemAvailKB = (int)(ms.ullAvailVirtual / 1024);
		info.Time = time(NULL);
		fwrite(&info, sizeof(INFO), 1, file);
	}
	static void saveExceptionToFileName(const char *filename, struct _EXCEPTION_POINTERS *e) {
		FILE *file = fopen(filename, "w");
		if (file) {
			saveExceptionToFile(file, e);
			fflush(file);
			fclose(file);
		}
	}

	// 例外ファイルを読み出す
	static void loadExceptionFromFile(FILE *file, INFO *info) {
		fread(info, sizeof(INFO), 1, file);
	}
	static void loadExceptionFromFileName(const char *filename, INFO *info) {
		FILE *file = fopen(filename, "r");
		if (file) {
			loadExceptionFromFile(file, info);
			fclose(file);
		}
	}

	// 例外ファイルを消す
	static void removeExceptionFile(const char *filename) {
		DeleteFileA(filename);
	}

	static void printModules(FILE *strm) {
		HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
		if (hSnapshot != INVALID_HANDLE_VALUE) {
			MODULEENTRY32 me;
			me.dwSize = sizeof(MODULEENTRY32);
			Module32First(hSnapshot, &me);
			std::vector<std::string> list;
			do {
				char s[256] = {0};
				sprintf(s, "%p: %s", me.modBaseAddr, me.szModule);
				list.push_back(s);
			} while (Module32Next(hSnapshot, &me));
			CloseHandle(hSnapshot);
			std::sort(list.begin(), list.end()); // アドレス順でソート（先頭にアドレス文字列があるのでそれを利用する）
			for (auto it=list.begin(); it!=list.end(); it++) {
				fprintf(strm, "%s\n", it->c_str());
			}
		}
	}

	static void printStackFrame(FILE *strm, const EXCEPTION_POINTERS *exception_or_null) {
	#ifndef NO_DBGHELP
		// How To Get a Stack Trace on Windows
		// https://www.rioki.org/2017/01/09/windows_stacktrace.html
		//
		PIMAGEHLP_SYMBOL pSym = (PIMAGEHLP_SYMBOL)GlobalAlloc(GMEM_FIXED, 10000);
		pSym->SizeOfStruct = 10000;
		pSym->MaxNameLength = 10000 - sizeof(IMAGEHLP_SYMBOL);
		STACKFRAME sf;
		ZeroMemory(&sf, sizeof(sf));
		if (exception_or_null) {
			// 例外情報からスタック情報を得る（例外発生時の呼び出し履歴）
			sf.AddrPC.Offset    = exception_or_null->ContextRecord->Eip;
			sf.AddrStack.Offset = exception_or_null->ContextRecord->Esp;
			sf.AddrFrame.Offset = exception_or_null->ContextRecord->Ebp;
		} else {
			// 現在のスタック情報を得る（この関数の呼び出し履歴）
			CONTEXT ctx = {};
			ctx.ContextFlags = CONTEXT_FULL;
			RtlCaptureContext(&ctx);
			sf.AddrPC.Offset    = ctx.Eip;
			sf.AddrStack.Offset = ctx.Esp;
			sf.AddrFrame.Offset = ctx.Ebp;
		}
		sf.AddrPC.Mode      = AddrModeFlat;
		sf.AddrStack.Mode   = AddrModeFlat;
		sf.AddrFrame.Mode   = AddrModeFlat;
		SymInitialize(GetCurrentProcess(), NULL, TRUE);
		SymSetOptions(SYMOPT_LOAD_LINES);

		while (1) {
			// 次のスタックフレームの取得
			BOOL ok = StackWalk(
				IMAGE_FILE_MACHINE_I386,
				GetCurrentProcess(),
				GetCurrentThread(),
				&sf,
				NULL,
				NULL,
				SymFunctionTableAccess,
				SymGetModuleBase,
				NULL
			);
			if(!ok) break;

			// プログラムカウンタから関数名を取得
			IMAGEHLP_LINE line;
			{
				DWORD offset = 0;
				ZeroMemory(&line, sizeof(line));
				SymGetLineFromAddr(GetCurrentProcess(), sf.AddrPC.Offset, &offset, &line);
			}

			DWORD code_offset;
			if (SymGetSymFromAddr(GetCurrentProcess(), sf.AddrPC.Offset, &code_offset, pSym)) {
				fprintf(strm, "0x%08x %s(%d) %s + 0x%x\n", sf.AddrPC.Offset, line.FileName, line.LineNumber, pSym->Name, code_offset);
			} else {
				fprintf(strm, "%08x, ---\n", sf.AddrPC.Offset);
			}
		}
		GlobalFree(pSym);
		SymCleanup(GetCurrentProcess());
	#endif
	}

	// スタックフレームなどを出力
	static void outputExceptionInfo(const EXCEPTION_POINTERS *e) {
		FILE *file = fopen(MEMMAP_FILENAME, "w");

		// DLL等のメモリ位置
		{
			fprintf(file, "### MEMMAP ###\n");
			printModules(file);
			fprintf(file, "\n");
		}

		// 例外情報
		{
			fprintf(file, "### EXCEPTION ###\n");
			fprintf(file, "ExceptionAddress: %p\n", e->ExceptionRecord->ExceptionAddress);
			fprintf(file, "ExceptionCode   : %08x\n", e->ExceptionRecord->ExceptionCode);
			fprintf(file, "ExceptionFlags  : %08x\n", e->ExceptionRecord->ExceptionFlags);
			fprintf(file, "\n");
		}

		// スタックフレーム
		{
			fprintf(file, "### STACK FRAME ###\n");
			printStackFrame(file, e);
			fprintf(file, "\n");
		}
		fclose(file);
	}


	// 例外発生時のコールバック
	static LONG WINAPI onExceptionOccurred(struct _EXCEPTION_POINTERS *e) {
		outputExceptionInfo(e);

		// 本当ならエラーダイアログを表示したいが、
		// フルスクリーンで動作しているときに下手にダイアログを出そうとすると非常に面倒なことになるので、
		// やめておく。その代わり、専用のテキストファイルに書き出しておく（ログ機構を使うとスマートだが、ログ機構が死んでいた場合に困る）
		saveExceptionToFileName(UNREPOTED_EXCEPTION_FILE, e);

		// ここで小細工。
		// 例外チェックモードにして複製を起動する。
		// このとき、自分が終了するまで待ってもらう必要があるので、自分のプロセスＩＤをパラメータとして渡す
		const int STRLEN = 256;
		wchar_t args[STRLEN];
		swprintf_s(args, STRLEN, L"errchk:%x", GetCurrentProcessId());

		// 例外チェックモードで自分の複製を起動する
		
		std::string exepath_u8 = K::sysGetCurrentExecName();
		std::wstring exepath_w = K::strUtf8ToWide(exepath_u8);
		ShellExecuteW(NULL, L"OPEN", exepath_w.c_str(), args, NULL, SW_SHOWNORMAL);

		return EXCEPTION_CONTINUE_SEARCH; // EXCEPTION_CONTINUE_SEARCH にしないとアプリ終了時にwindowsエラーログに残らない
	}

	// 例外発生時のコールバックを登録する
	static void installExeptionHandler() {
		SetUnhandledExceptionFilter(onExceptionOccurred);
	}

	// 指定されたプロセスが実行中なら、それが終わるまで待つ
	static void waitForProcessFinish(DWORD pid) {
		HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
		WaitForSingleObject(hProcess, INFINITE);
		CloseHandle(hProcess);
	}

	// 例外発生時にWindowsイベントログに記録されたであろう例外イベントを探してイベントメッセージを返す。
	// 最も新しいイベントを探すが、それが limit_seconds 秒以上経過していた場合は見つからなかったものとする。
	// イベントが見つからなかった場合は空文字列を返す。
	static std::wstring findNewestErrorEventLog(int limit_seconds, intptr_t *fault_addr) {
		std::string exepath_u8 = K::sysGetCurrentExecName();

		// プログラム名を検索キーワードにする
		// ログイベント内の実行ファイル名はディレクトリ名を含まないので、ファイル名だけを使う
		std::wstring exepath_w = K::strUtf8ToWide(exepath_u8);
		const wchar_t *exename = PathFindFileNameW(exepath_w.c_str());

		return _FindErrorLog(limit_seconds, exename, fault_addr);
	}

	// Windowsエラーログから取得した詳細なエラーログを書き出す
	static void printErrorText(FILE *fp, const INFO *info, const char *errmsg_u8) {
		std::string info_u8 = (
			u8"■Windowsエラーログから取得した情報を示します\n"
			u8"なお、完全な情報を見たい場合は直接イベントビューアーを起動して内容を確認してください。\n"
			u8"\n"
			u8"・イベントビューア―の起動方法\n"
			u8"　Win8/Win10: Windows+Xキーを押すか、スタートメニューを右クリックして、「イベントビューアー」を選択\n"
			u8"　Win7: スタートメニューの「コンピューター」を右クリック、「プロパティ」をクリック。「コンピューターの管理」が起動したら、「イベントビューアー」を選択\n"
			u8"\n"
			u8"・イベントビューア―を起動したら\n"
			u8"　ツリービューから、「Windowsログ」「Application」を選択\n"
			u8"　「レベル」が「エラー」になっているものを、新しいものから順番に確認していく。\n"
			u8"\n"
			u8"■アプリケーションのビルド日時\n"
			u8"${BUILD_DATE} ${BUILD_TIME}\n"
			u8"\n"
			u8"■実行環境\n"
			u8"OS_VER:     ${OS_VER}\n"
			u8"CPU_NAME:   ${CPU_NAME}\n"
			u8"CPU_VENDOR: ${CPU_VENDOR}\n"
			u8"LANGID: ${LANGID} (${LANGID_HEX})\n"
			u8"\n"
			u8"■例外発生時のメモリ状況\n"
			u8"SYSMEM_TOTAL(MB): ${SYSMEM_TOTAL_MB}\n"
			u8"SYSMEM_AVAIL(MB): ${SYSMEM_AVAIL_MB}\n"
			u8"\n"
			u8"APPMEM_TOTAL(MB): ${APPMEM_TOTAL_MB}\n"
			u8"APPMEM_AVAIL(MB): ${APPMEM_AVAIL_MB}\n"
			u8"\n"
			u8"■例外情報\n"
			u8"EXCEPTION_CODE   : ${CODE}\n"
			u8"EXCEPTION_FLAGS  : ${FLAGS}\n"
			u8"EXCEPTION_ADDRESS: ${ADDRESS}\n"
			u8"\n"
		);
		{
			const int LEN = 256;
			char productname[LEN] = {0};
			char cpuname[LEN] = {0};
			char cpuvendor[LEN] = {0};
			KSystem::getString(KSystem::STRPROP_PRODUCTNAME, productname, LEN);
			KSystem::getString(KSystem::STRPROP_CPUNAME, cpuname, LEN);
			KSystem::getString(KSystem::STRPROP_CPUVENDOR, cpuvendor, LEN);

			int langid = GetUserDefaultUILanguage();
			K::strReplace(info_u8, "${BUILD_DATE}",   __DATE__);
			K::strReplace(info_u8, "${BUILD_TIME}",   __TIME__);
			K::strReplace(info_u8, "${OS_VER}",       productname);
			K::strReplace(info_u8, "${CPU_NAME}",     cpuname);
			K::strReplace(info_u8, "${CPU_VENDOR}",   cpuvendor);
			K::strReplace(info_u8, "${LANGID}",       K::str_sprintf("%d", langid));
			K::strReplace(info_u8, "${LANGID_HEX}",   K::str_sprintf("0x%04x", langid));
			K::strReplace(info_u8, "${SYSMEM_TOTAL_MB}", K::str_sprintf("%d", info->SysMemTotalKB/1024));
			K::strReplace(info_u8, "${SYSMEM_AVAIL_MB}", K::str_sprintf("%d", info->SysMemAvailKB/1024));
			K::strReplace(info_u8, "${APPMEM_TOTAL_MB}", K::str_sprintf("%d", info->AppMemTotalKB/1024));
			K::strReplace(info_u8, "${APPMEM_AVAIL_MB}", K::str_sprintf("%d", info->AppMemAvailKB/1024));
			K::strReplace(info_u8, "${CODE}",         K::str_sprintf("0x%08x", info->ExceptionCode));
			K::strReplace(info_u8, "${FLAGS}",        K::str_sprintf("0x%08x", info->ExceptionFlags));
			K::strReplace(info_u8, "${ADDRESS}",      K::str_sprintf("0x%08x", (intptr_t)info->ExceptionAddress));
		}

		fprintf(fp, "%s\n", info_u8.c_str());
		fprintf(fp, "\n");
		fprintf(fp, "%s\n", errmsg_u8);
	}
};


/// コマンド引数を見て例外確認モードで起動されたかどうかをチェックし、例外確認モードだったら
/// Windows のエラーログから例外情報を探して表示する。
/// これは _InstallExeptionHandler() とセットで使う
static bool _ErrorCheckProcess(const char *args, const char *comment_u8, const char *include_log_file) {

	// 例外報告用のファイルを調べる
	CWin32ErrorReporting::INFO info = {0};
	CWin32ErrorReporting::loadExceptionFromFileName(UNREPOTED_EXCEPTION_FILE, &info);
	if (info.PID == 0) return false;

	// 例外を出したプロセスが終了してくれないとWindowsイベントログに情報が追加されない。
	// 例外プロセスが終わるまで待ってからイベントログを調べる
	CWin32ErrorReporting::waitForProcessFinish(info.PID);

	// 例外を出したプロセスが終了した。
	// Windowsイベントログに反映されるまでの時間を考慮して、少し待機する
	Sleep(200);

	// Windowsイベントログに記録されたであろう例外イベントを探す
	// 例外送出後にシステムがダウンし、再起動した場合を想定し、十分な時間だけさかのぼって検索する
	intptr_t fault_addr = 0;
	std::wstring errmsg = CWin32ErrorReporting::findNewestErrorEventLog(10 * 60, &fault_addr);

	// 例外情報が取得できたかどうかに関係なく、例外報告用のファイルはここで消してしまう
	CWin32ErrorReporting::removeExceptionFile(UNREPOTED_EXCEPTION_FILE);

	// イベントログが見つからない。ここで終了
	if (errmsg.empty()) return false;

	// 環境情報を得る
	// 念のために言語情報も得るので、この次で setlocale するよりも前に取得しておく
	std::string exepath_u8 = K::sysGetCurrentExecName();

	// ファイルを保存する
	std::string report_path_u8 = K::pathGetFull(EXCEPTION_LOG_FILE_NAME);

	// エラーレポートを書き出す
	FILE *file = K::fileOpen(EXCEPTION_LOG_FILE_NAME, "w");
	if (file) {
		fprintf(file, "# coding: utf8\n");
		fprintf(file, "Application \"%s\" was crashed!\n", exepath_u8.c_str());
		fprintf(file, "\n");

		if (comment_u8[0]) {
			fprintf(file, "%s\n", comment_u8);
			fprintf(file, "\n");
		}

		std::string errmsg_u8 = K::strWideToUtf8(errmsg);
		CWin32ErrorReporting::printErrorText(file, &info, errmsg_u8.c_str());

#if 0
		const intptr_t BASE_ADDR = 0x00400000;
		fwprintf(file, L"\n");
		fwprintf(file, L"■このファイルについて\n");
		fwprintf(file, L"・EXEのビルド時にプロジェクトプロパティの【構成プロパティ|C/C++|出力ファイル|アセンブリ出力】を\n");
		fwprintf(file, L"　「アセンブリ コード、コンピューター語コード、ソース コード (/FAcs)」に設定しておいてください\n");
		fwprintf(file, L"・EXEのビルド時にプロジェクトプロパティの【構成プロパティ|リンカ|デバッグ|マップ ファイルの作成】を\n");
		fwprintf(file, L"　「はい (/MAP)」に設定しておいてください\n");
		fwprintf(file, L"・ベースアドレスが 0x%08x なら、障害オフセット 0x%08x を加えた値 0x%08x を .map ファイルから探してください\n", BASE_ADDR, fault_addr, BASE_ADDR+fault_addr);
#endif

		// 例外発生時に吐き出したファイルを取り込む
		if (1) {
			FILE *fp = fopen(MEMMAP_FILENAME, "r");
			if (fp) {
				char buf[1024] = {0};
				while (!feof(fp)) {
					fgets(buf, sizeof(buf), fp);
					fputs(buf, file);
				}
				fclose(fp);
				DeleteFileA(MEMMAP_FILENAME);
			}
		}

		if (include_log_file && include_log_file[0]) {
			// ログファイル名が指定されている。
			// そのログファイルの中身が KLog::printSeparator によって区切られていると仮定して、
			// その最後の実行時ログを一時ファイル tmpname にコピーする
			const char *tmpname = "~log.tmp";
			CopyFileA(include_log_file, tmpname, true);
			{
				KLogFile lf;
				lf.open(tmpname);
				lf.clampBySeparator(-1);
				lf.close();
			}
			// この時点で一時ファイル tmpname には最後に実行したときのログが入っている。
			// これをまるごとエラーレポートの末尾に含めておく
			FILE *tmp = fopen(tmpname, "r");
			if (tmp) {
				fprintf(file, "last log >>>>>>>>>\n");
				char buf[1024] = {0};
				size_t n;
				while (1) {
					n = fread(buf, 1, sizeof(buf), tmp);
					if (n == 0) break;
					fwrite(buf, 1, n, file);
				}
				fprintf(file, "<<<<<<<<< last log\n");
				fclose(tmp);
			}

			// 一時ファイルを削除
			DeleteFileA(tmpname);
		}

		fflush(file);
		fclose(file);
	}

	// ダイアログを出す
	{
		char s[1025];
		sprintf_s(s, sizeof(s), 
			u8"ご迷惑をおかけしております。%s がクラッシュしました。\n"
			u8"詳細を以下のファイルに保存しました:\n"
			u8"\n"
			u8"%s\n"
			u8"\n"
			u8"--\n"
			u8"%s\n",
			PathFindFileNameA(exepath_u8.c_str()),
			report_path_u8.c_str(),
			comment_u8);
		std::wstring ws = K::strUtf8ToWide(s);
		MessageBoxW(NULL, ws.c_str(), L"クラッシュ!", MB_ICONERROR|MB_SYSTEMMODAL);
	}
	return true;
}

/// 例外が発生したときの後処理機構を入れる。
/// これによって例外が処理された場合、例外確認モードで自分自身を多重起動する。
/// その時のコマンド引数パラメータは "errchk:###" である。### にはプロセスIDが10進数で入る
static void _InstallExeptionHandler() {
	CWin32ErrorReporting::installExeptionHandler();
}

/// 起動時パラメータの確認と、例外フックの設定を両方行う。
/// 例外フックが呼ばれた場合、例外確認オプションを指定して自分自身を二重起動する。
/// 起動時パラメータ args を確認し、例外確認オプションで起動されている場合は
/// Windows のエラーログに登録されているであろう例外イベントをチェックし、その中身を表示する。
/// 要するに、WinMain の一番最初でこの関数を呼ぶだけでよい。
/// 例外確認モードで起動されていた場合、Win32_ErrorChecker は必要な処理を行って true を返す。
/// その時は何もせずに正に終了させること。
bool K_ErrorCheck(const char *args, const char *comment_u8, const char *include_log_file) {
	if (_ErrorCheckProcess(args, comment_u8, include_log_file)) {
		return true;
	} else {
		_InstallExeptionHandler();
		return false;
	}
}

} // namespace
