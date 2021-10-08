#include "KLog.h"
#include <mutex>
#include <queue>
#include <assert.h>
#include <Windows.h> // OutputDebugStringW
#include "KInternal.h"

// ログ自身の詳細ログを取る
#ifndef K_USE_LOGLOG
#	define K_USE_LOGLOG 0
#endif


// ログの書き出し用スレッドを使う
// ※実際にスレッドを使うかどうかは g_UseThread の値による
#define USE_LOG_THREAD 1


#if USE_LOG_THREAD
#	include "KThread.h"
#endif


// ログ自身がつかうログ機構（ログのログを取るために使う）
#if K_USE_LOGLOG
#	define K__LOGLOG_BEGIN(type, u8) K::outputDebugFmt("# %s [%c] \"%s\"", __FUNCTION__, (isprint(type) ? type : ' '), u8)
#	define K__LOGLOG_END()           K::outputDebugFmt("# %s [END]",       __FUNCTION__)
#else
#	define K__LOGLOG_BEGIN(type, u8)  /* empty */
#	define K__LOGLOG_END()            /* empty */
#endif


// ログ自身のアサート
#ifdef _DEBUG
#  define K__LOGLOG_ASSERT(x)     (!(x) ? DebugBreak() : (void)0)
#else
#  define K__LOGLOG_ASSERT(x)
#endif


#define K__LOG_SPRINTF_BUFSIZE (1024 * 4)


// K__LOG_SEPARATOR には末尾の改行を含めないようにする
// 改行文字 \n が \r\n に変換されてしまう可能性があるため、K__LOG_SEPARATOR で検索したときに不一致になり見つけられない可能性があるため
#define K__LOG_SEPARATOR  "-------------------------------------------------------------------"



namespace Kamilo {

bool g_UseThread = false;

#pragma region KLogFile
KLogFile::KLogFile() {
	m_File = nullptr;
}
bool KLogFile::open(const char *filename_u8) {
	close();
	FILE *fp = K::fileOpen(filename_u8, "a");
	if (fp) {
		m_File = fp;
		m_FileName = filename_u8;
	} else {
		m_File = nullptr;
		m_FileName = "";
		K::outputDebugFmt("[Log] ERROR Failed to open log file: '%s'", filename_u8);
	}
	return m_File != nullptr;
}
bool KLogFile::isOpen() {
	return m_File != nullptr;
}
void KLogFile::close() {
	if (m_File) {
		fflush(m_File);
		fclose(m_File);
		m_File = nullptr;
		m_FileName = "";
	}
}
void KLogFile::writeLine(const char *u8) {
	if (m_File == nullptr) return;
	fputs(u8, m_File);
	fputs("\n", m_File);
	fflush(m_File);
}
void KLogFile::writeRecord(const KLog::Record &rec) {
	if (m_File == nullptr) return;
	fprintf(m_File, "%02d-%02d-%02d ", rec.time_year, rec.time_mon, rec.time_mday);
	fprintf(m_File, "%02d:%02d:%02d.%03d ", rec.time_hour, rec.time_min, rec.time_sec, rec.time_msec);
	fprintf(m_File, "(%6d) @%08x ", rec.app_msec, K::sysGetCurrentProcessId());
	switch (rec.type) {
	case KLog::LEVEL_AST: fprintf(m_File, "[assertion error] "); break;
	case KLog::LEVEL_ERR: fprintf(m_File, "[error] "); break;
	case KLog::LEVEL_WRN: fprintf(m_File, "[warning] "); break;
	case KLog::LEVEL_INF: fprintf(m_File, "[info] "); break;
	case KLog::LEVEL_DBG: fprintf(m_File, "[debug] "); break;
	case KLog::LEVEL_VRB: fprintf(m_File, "[verbose] "); break;
	default:       fprintf(m_File, "[%c] ", rec.type); break;
	}
	fprintf(m_File, "%s\n", rec.text_u8.c_str());
	fflush(m_File);
}
bool KLogFile::clampBySeparator(int number) {
	// いったん閉じる (writeモードなので)
	if (m_File) {
		fflush(m_File);
		fclose(m_File);
		m_File = nullptr;
	}

	// テキストをロード
	std::string text = K::fileLoadString(m_FileName);

	// 内容修正
	{
		// 切り取り線を探す
		// 引数に注意。text はヌル終端文字列ではなく、単なるバイト列として渡す。
		// UTF16などで0x00を含む可能性があるため、ヌル終端を期待すると予期せぬ場所で切られてしまう場合がある
		int pos = findSeparatorByIndex(text.data(), text.size(), number);

		// 切り取り線よりも前の部分を削除
		if (pos >= 0) {
			text.erase(0, pos);
		}
	}

	// 新しい内容を書き込む
	K::fileSaveString(m_FileName, text);

	// 再び開く
	m_File = K::fileOpen(m_FileName, "a");
	return m_File != nullptr;
}
bool KLogFile::clampBySize(int clamp_size_bytes) {
	// いったん閉じる (writeモードなので) 
	fflush(m_File);
	fclose(m_File);

	// テキストをロード
	std::string text = K::fileLoadString(m_FileName);

	// 内容修正
	{
		if ((int)text.size() > clamp_size_bytes) {
			// 適当な改行位置を境にして古いテキストを削除する
			int findstart = text.size() - clamp_size_bytes;
			int pos = text.find("\n", findstart);
			text.erase(0, pos+1); // 改行文字も消すので pos に +1 する

			// ログ削除メッセージを挿入する
			const char *msg = "=========== OLD LOGS ARE REMOVED ===========\n";
			text.insert(0, msg);
		}
	}

	// 新しい内容を書き込む
	K::fileSaveString(m_FileName, text);

	// 再び開く
	m_File = K::fileOpen(m_FileName, "a");
	return m_File != nullptr;
}
int KLogFile::findSeparatorByIndex(const char *text, size_t size, int index) {
	// 区切り線を探す
	// number には区切り線のインデックスを指定する。（負の値を指定した場合は末尾から数える）
	K__LOGLOG_ASSERT(text);
	// 既存のログファイルのエンコードがユーザーによって改変されてしまっている可能性に注意。
	// UTF16などでのエンコードに代わっていると、0x00 の部分をヌル終端と判断してしまい、それより先の検索ができなくなる。
	// ある時点でユーザーがログのエンコードを変えて保存してしまったとしても、プログラムによって追記した部分は
	// SJIS などになっているため、途中にある 0x00 を無視して検索をしていけばそのうち K__LOG_SEPARATOR に一致する部分が見つかるはずである。
	// （今見つからなかったとしても、プログラムを起動するたびに新しい K__LOG_SEPARATOR が追記されるならば、何回目かの起動で見つかるはず）
	// というわけで、ヌル終端には頼らず、必ずバイトサイズを見る
	int txtlen = size; // strlen(text) は使わない。理由は↑を参照
	int sublen = strlen(K__LOG_SEPARATOR);
	if (index >= 0) {
		// 先頭から [number] 番目を探す
		for (int i=0; txtlen-sublen; i++) {
			if (strncmp(text+i, K__LOG_SEPARATOR, sublen) == 0) {
				if (index == 0) {
					return i + sublen;
				}
				index--;
			}
		}
	} else {
		// 末尾から [1-number] 番目を探す
		for (int i=txtlen-sublen; i>=0; i--) {
			if (strncmp(text+i, K__LOG_SEPARATOR, sublen) == 0) {
				index++;
				if (index == 0) {
					return i + sublen;
				}
			}
		}
	}
	return -1;
}
#pragma endregion // KLogFile




#pragma region KLogConsole
KLogConsole::KLogConsole() {
	m_Stdout = nullptr;
}
bool KLogConsole::open(bool no_taskbar) {
	close();
	#ifndef _CONSOLE
	AllocConsole();
	if (no_taskbar) {
		// コンソールウィンドウがタスクバーに出ないようにする
		std::string title = K::str_sprintf("%s (#%d)", K::sysGetCurrentExecName().c_str(), K::sysGetCurrentProcessId());
		SetConsoleTitleA(title.c_str()); // プロセスIDを含めたユニークなタイトルを用意して設定する
		int timeout = 60;
		uint32_t msec = K::clockMsec32();
		HWND hWnd = NULL;
		while (hWnd == NULL && K::clockMsec32() < msec + timeout) {
			hWnd = FindWindowA(NULL, title.c_str()); // タイトルをもとにしてコンソールウィンドウを探す
			Sleep(10);
		}
		SetWindowLongA(hWnd, GWL_EXSTYLE, WS_EX_TOOLWINDOW); // ツールウィンドウ用のウィンドウスタイルを適用する
	}
	freopen_s(&m_Stdout, "CON", "w", stdout);
	#endif
	return true;
}
bool KLogConsole::isOpen() {
	return m_Stdout != nullptr;
}
void KLogConsole::close() {
	if (m_Stdout) fclose(m_Stdout);
	m_Stdout = nullptr;
	#ifndef _CONSOLE
	if (0) {
		FreeConsole();
	}
	#endif
}
void KLogConsole::setColorFlags(int flags) {
	// コンソールウィンドウの文字属性コードを設定する
	// FOREGROUND_BLUE      0x0001 // text color contains blue.
	// FOREGROUND_GREEN     0x0002 // text color contains green.
	// FOREGROUND_RED       0x0004 // text color contains red.
	// FOREGROUND_INTENSITY 0x0008 // text color is intensified.
	// BACKGROUND_BLUE      0x0010 // background color contains blue.
	// BACKGROUND_GREEN     0x0020 // background color contains green.
	// BACKGROUND_RED       0x0040 // background color contains red.
	// BACKGROUND_INTENSITY 0x0080 // background color is intensified.
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), (WORD)flags);
}
int KLogConsole::getColorFlags() {
	// コンソールウィンドウの文字属性コードを返す
	CONSOLE_SCREEN_BUFFER_INFO info;
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info);
	return (int)info.wAttributes;
}
void KLogConsole::writeLine(const char *u8) {
	std::wstring ws = K::strUtf8ToWide(u8);
	std::string mb = K::strWideToAnsi(ws, "");
	fputs(mb.c_str(), stdout);
	fputs("\n", stdout);
}
void KLogConsole::writeRecord(const KLog::Record &rec) {
	// 現在の設定を退避
	int restore_flags = getColorFlags();

	// 時刻
	switch (0) {
	case 0:
		break;

	case 1:
		// 詳細時刻
		setColorFlags(BG_BLACK|FG_PURPLE);
		printf("%02d:%02d:%02d.%03d (%d) ",
			rec.time_hour,
			rec.time_min,
			rec.time_sec,
			rec.time_msec,
			rec.app_msec
		);
		break;

	case 2:
		// 経過時間
		setColorFlags(BG_BLACK|FG_PURPLE);
		printf("%d.%03d ", rec.app_msec/1000, rec.app_msec%1000);
		break;
	}

	// 色を取得
	uint16_t typecolor = 0;
	uint16_t msgcolor = 0;
	getLevelColorFlags(rec.type, &typecolor, &msgcolor);

	// メッセージ種類
	if (1) {
		if (rec.type != KLog::LEVEL_NUL) {
			char tp = (char)(rec.type & 0x7F); // 念のため ascii 範囲に収まるようにしておく
			setColorFlags(typecolor);
			printf(" %c ", tp);
		}
	}

	// メッセージ文字列
	if (1) {
		std::wstring ws = K::strUtf8ToWide(rec.text_u8); // UTF8-->ANSIの直接変換はできないので一度ワイド文字を経由する
		std::string mb = K::strWideToAnsi(ws, ""); // 現ロケールでのマルチバイト文字列
		setColorFlags(msgcolor);
		printf(" %s\n", mb.c_str());
	}

	// 文字属性を元に戻す
	setColorFlags(restore_flags);
}
void KLogConsole::getLevelColorFlags(char type, uint16_t *typecolor, uint16_t *msgcolor) {
	// メッセージ属性に応じた色の組み合わせを得る。
	// ※この色の組み合わせの元ネタ
	// https://bitbucket.org/brunobraga/logcat-colorize
	//
	// type 属性
	// typecolor 属性文字列の前景＆背景色
	// msgcolor  メッセージ文字列の前景＆背景色
	uint16_t f1 = BG_BLACK|FG_WHITE;
	uint16_t f2 = BG_BLACK|FG_WHITE;
	switch (type) {
	case KLog::LEVEL_AST:
	case KLog::LEVEL_ERR:
		f1 = BG_RED|FG_WHITE;
		f2 = BG_BLACK|FG_RED;
		break;

	case KLog::LEVEL_WRN:
		f1 = BG_YELLOW|FG_BLACK;
		f2 = BG_BLACK|FG_YELLOW;
		break;

	case KLog::LEVEL_INF:
		f1 = BG_TEAL|FG_WHITE;
		f2 = BG_BLACK|FG_TEAL;
		break;

	case KLog::LEVEL_DBG:
		f1 = BG_PURPLE|FG_WHITE;
		f2 = BG_BLACK|FG_PURPLE;
		break;

	case KLog::LEVEL_VRB:
		f1 = BG_BLUE|FG_WHITE;
		f2 = BG_BLACK|FG_BLUE;
		break;
	}
	*typecolor = f1;
	*msgcolor = f2;
}
#pragma endregion // KLogConsole




#pragma region KLogDebugger
KLogDebugger::KLogDebugger() {
	m_IsOpen = false;
}
bool KLogDebugger::open() {
	close();
	m_IsOpen = K::_IsDebuggerPresent();
	return m_IsOpen;
}
bool KLogDebugger::isOpen() {
	return m_IsOpen;
}
void KLogDebugger::close() {
	m_IsOpen = false;
}
void KLogDebugger::writeLine(const char *u8) {
	// デバッガーに出力する。
	// Visual Studio の「出力」ウィンドウでメッセージを見ることができる
	if (!m_IsOpen) return;
	std::wstring ws = K::strUtf8ToWide(u8);
	OutputDebugStringW(ws.c_str());
	OutputDebugStringW(L"\n");
}
void KLogDebugger::writeRecord(const KLog::Record &rec) {
	if (!m_IsOpen) return;
	char s[K__LOG_SPRINTF_BUFSIZE];
	sprintf_s(s, sizeof(s), "[%c] %s", rec.type, rec.text_u8.c_str());
	writeLine(s);
}
#pragma endregion // KLogDebugger


#if USE_LOG_THREAD
class CLogThread: public KThread {
	std::mutex m_Mutex;
	std::queue<KLog::Record> m_Queue;
	std::queue<KLog::Record> m_Buf;
	bool m_IsBusy;
	int m_Locked;
public:
	CLogThread() {
		m_IsBusy = false;
		m_Locked = 0;
	}
	virtual void run() override {
		while (1) {
			// 待機キューにメッセージがたまるまで待機する。
			// or メッセージ処理がロックされている場合も、ロック解除まで待機する
			while (m_Buf.empty() || m_Locked > 0) {
				m_IsBusy = false;
				if (shouldExit()) {
					goto end;
				}
				Sleep(10);
			}

			m_IsBusy = true;

			// 待機キューにあるメッセージを実行キューに移動する
			m_Mutex.lock();
			while (!m_Buf.empty()) {
				m_Queue.push(m_Buf.front());
				m_Buf.pop();
			}
			m_Mutex.unlock();

			// 実行キューのメッセージを出力する
			while (!m_Queue.empty()) {
				const KLog::Record &rec = m_Queue.front();
				KLog::printRecord_unsafe(rec);
				m_Queue.pop();
			}
		}
	end:
		m_IsBusy = false;
		m_Locked = 0;
	}

	void post_emit(const KLog::Record &rec) {
		m_Mutex.lock();
		m_Buf.push(rec);
		m_Mutex.unlock();
	}
	void wait_for_idle() {
		while (m_IsBusy) {
			Sleep(1);
		}
	}
	void lock() {
		m_Locked++;
	}
	void unlock() {
		m_Locked--;
	}
};
static CLogThread g_LogThread;
#endif // USE_LOG_THREAD


class CLogContext {
	std::mutex m_Mutex;
	KLog::Callback *m_Callback;
	KLogFile m_File;
	KLogConsole m_Console;
	KLogDebugger m_Debugger;
	uint32_t m_StartMsec;
	int m_DialogMuted; // ダイアログ抑制のスタックカウンタ
	bool m_InfoEnabled;
	bool m_DebugEnabled;
	bool m_VerboseEnabled;
	bool m_DialogEnabled;
public:
	int m_TraceDepth;
	std::recursive_mutex m_TraceMutex;

	CLogContext() {
		m_Callback = nullptr;
		m_StartMsec = K::clockMsec32();
		m_InfoEnabled = true;
		m_DebugEnabled = true;
		m_VerboseEnabled = false;
		m_DialogEnabled = true;
		m_DialogMuted = 0;
		m_TraceDepth = 0;
		g_LogThread.start();
	}
	~CLogContext() {
		g_LogThread.stop();
	}
	bool isLevelEnabled(KLog::Level lv) const {
		if (lv==KLog::LEVEL_VRB && !m_VerboseEnabled) return false;
		if (lv==KLog::LEVEL_DBG && !m_DebugEnabled) return false;
		if (lv==KLog::LEVEL_INF && !m_InfoEnabled) return false;
		return true;
	}
	void printRecord_unsafe(const KLog::Record &rec) {
		m_Mutex.lock();
		if (rec.type == KLog::LEVEL_NUL) {
			//
			// 属性なしテキスト
			//
			if (m_Debugger.isOpen()) {
				m_Debugger.writeLine(rec.text_u8.c_str());
			}
			if (m_Console.isOpen()) {
				m_Console.writeLine(rec.text_u8.c_str());
			}
			if (m_File.isOpen()) {
				m_File.writeLine(rec.text_u8.c_str());
			}

		} else {
			//
			// 属性付きテキスト
			//
			if (m_Debugger.isOpen()) {
				m_Debugger.writeRecord(rec);
			}
			if (m_Console.isOpen()) {
				m_Console.writeRecord(rec);
			}
			if (m_File.isOpen()) {
				m_File.writeRecord(rec);
			}
		}
		m_Mutex.unlock();
	}

	// テキストを出力する。
	// ユーザーによるコールバックを通さず、既定の出力先に直接書き込む。
	// コールバック内からログを出力したい時など、ユーザーコールバックの再帰呼び出しが邪魔になるときに使う
	void emitNoCallback(KLog::Level lv, const char *u8) {
		K__LOGLOG_BEGIN(type, u8);
		K__LOGLOG_ASSERT(u8);

		KLog::Record rec;
		{
			SYSTEMTIME st;
			GetLocalTime(&st);
			rec.time_year = st.wYear;
			rec.time_mon  = st.wMonth;
			rec.time_mday = st.wDay;
			rec.time_hour = st.wHour;
			rec.time_min  = st.wMinute;
			rec.time_sec  = st.wSecond;
			rec.time_msec = st.wMilliseconds;
			rec.app_msec = K::clockMsec32() - m_StartMsec;
			rec.type = lv;
			rec.text_u8 = u8;
		}

		if (g_UseThread) {
			g_LogThread.post_emit(rec);
		} else {
			printRecord_unsafe(rec);
		}
		K__LOGLOG_END();
	}

	void threadLock() {
		#if USE_LOG_THREAD
		if (g_UseThread) {
			// 現在処理中のメッセージキューが終わっても、
			// 新しいメッセージを処理し始めないようにロックしておく
			g_LogThread.lock();

			// 処理中のメッセージキューが終わるまで待つ
			g_LogThread.wait_for_idle();
		}
		#endif
	}
	void threadUnlock() {
		#if USE_LOG_THREAD
		if (g_UseThread) {
			g_LogThread.unlock();
		}
		#endif
	}

	void threadWait() {
		#if USE_LOG_THREAD
		if (g_UseThread) {
			g_LogThread.wait_for_idle();
		}
		#endif
	}

	// テキストを出力する。
	// type には属性を表す文字を指定する。'E'rror, 'W'arning, 'I'nfo, 'D'ebug, 'V'erbose 無属性は '\0'
	void emit(KLog::Level lv, const char *u8) {
		K__LOGLOG_BEGIN(type, u8);
		K__LOGLOG_ASSERT(u8);
		if (isLevelEnabled(lv)) {

			bool no_emit = false;
			bool no_dialog = false;

			// ユーザーによる処理を試みる
			if (m_Callback) {
				m_Callback->on_log_output(lv, u8, &no_emit, &no_dialog);
			}

			if (!no_dialog) {
				if (KLog::getState(KLog::STATE_DIALOG_ALLOWED) != 0) {
					if (lv == KLog::LEVEL_AST) {
						threadWait(); // ダイアログを出す前に、現在たまっているログを吐き出させる
						K::dialog(u8); // assertion error
					}
					if (lv == KLog::LEVEL_ERR) {
						threadWait(); // ダイアログを出す前に、現在たまっているログを吐き出させる
						K::dialog(u8); // error
					}
				}
			}
			if (!no_emit) {
				emitNoCallback(lv, u8);
			}
		}
		K__LOGLOG_END();
	}

	void setFileName(const char *filename_u8) {
		threadLock(); // 出力設定が変化するので、処理中の出力処理が終わるまで待たないとダメ
		if (filename_u8 && filename_u8[0]) {
			m_File.open(filename_u8);
			m_File.clampBySeparator(-2); // 過去2回分のログだけ残し、それ以外を削除
			K::outputDebugFmt("[Log] file open: %s", filename_u8);
		} else {
			K::outputDebugFmt("[Log] file close");
			m_File.close();
		}
		threadUnlock();
	}
	void setCallback(KLog::Callback *cb) {
		threadLock();
		m_Callback = cb;
		threadUnlock();
	}
	void setConsoleEnabled(bool enabled, bool no_taskbar) {
		threadLock();
		if (enabled) {
			// コンソールへの出力を有効にする
			// 順番に注意：有効にしてからログを出す
			m_Console.open(no_taskbar);
			K::outputDebugFmt("[Log] console on");
		} else {
			// コンソールへの出力を停止する。
			// 順番に注意：ログを出してから無効にする
			K::outputDebugFmt("[Log] console off");
			m_Console.close();
		}
		threadUnlock();
	}
	void setDebuggerEnabled(bool value) {
		threadLock();
		if (value) {
			m_Debugger.open();
			K::outputDebugFmt("[Log] debugger on");
		} else {
			K::outputDebugFmt("[Log] debugger off");
			m_Debugger.close();
		}
		threadUnlock();
	}
	void setLevel_info(bool value) {
		threadLock();
		m_InfoEnabled = value;
		K::outputDebugFmt("[Log] level info %s", value ? "on" :"off");
		threadUnlock();
	}
	void setLevel_debug(bool value) {
		threadLock();
		m_DebugEnabled = value;
		K::outputDebugFmt("[Log] level debug %s", value ? "on" :"off");
		threadUnlock();
	}
	void setLevel_verbose(bool value) {
		threadLock();
		m_VerboseEnabled = value;
		K::outputDebugFmt("[Log] level verbose %s", value ? "on" :"off");;
		threadUnlock();
	}
	void setDialogEnabled(bool value) {
		threadLock();
		m_DialogEnabled = value;
		K::outputDebugFmt("[Log] dialog %s", value ? "on" :"off");
		threadUnlock();
	}
	void muteDialog() {
		threadLock();
		m_DialogMuted++;
		threadUnlock();
	}
	void unmuteDialog() {
		threadLock();
		m_DialogMuted--;
		threadUnlock();
	}
	int getState(KLog::State s) {
		switch (s) {
		case KLog::STATE_HAS_OUTPUT_CONSOLE:
			return m_Console.isOpen() ? 1 : 0;
		case KLog::STATE_HAS_OUTPUT_FILE:
			return m_File.isOpen() ? 1 : 0;
		case KLog::STATE_HAS_OUTPUT_DEBUGGER:
			return m_Debugger.isOpen() ? 1 : 0;
		case KLog::STATE_LEVEL_INFO:
			return m_InfoEnabled ? 1 : 0;
		case KLog::STATE_LEVEL_DEBUG:
			return m_DebugEnabled ? 1 : 0;
		case KLog::STATE_LEVEL_VERBOSE:
			return m_VerboseEnabled ? 1 : 0;
		case KLog::STATE_DIALOG_ALLOWED:
			return (m_DialogEnabled && m_DialogMuted <= 0) ? 1 : 0;
		default:
			return 0;
		}
	}
	void printBinaryText(const void *data, int size) {
		const unsigned char *p = (const unsigned char *)data;
		emit(KLog::LEVEL_NUL, "# dump >>>");
		if (p == nullptr) {
			size = 0;
			emit(KLog::LEVEL_NUL, "(nullptr)");
		} else {
			if (size <= 0) {
				size = (int)strlen((const char *)data);
			}
			std::string txt; // テキスト表記
			std::string bin; // バイナリ表記
			int i = 0;
			for (; i < size; i++) {

				// 1行分の文字列を出力
				if (i>0 && i%16==0) {
					char s[256];
					sprintf_s(s, sizeof(s), "# %s: %s", bin.c_str(), txt.c_str());

					emit(KLog::LEVEL_NUL, s);
					bin.clear();
					txt.clear();
				}

				// 16進数表記を追加
				char s[256];
				sprintf_s(s, sizeof(s), "%02x ", p[i]);
				bin += s;
				if (isprint(p[i])) {
					txt += p[i];
				} else {
					txt += '.';
				}
			}

			// 未出力のバイナリがあれば出力する
			if (!bin.empty()) {
				char s[256];
				bin.resize(3 * 16, ' '); // バイナリ16桁の幅に合わせる
				sprintf_s(s, sizeof(s), "# %s: %s", bin.c_str(), txt.c_str());
				emit(KLog::LEVEL_NUL, s);
				bin.clear();
				txt.clear();
			}
		}
		{
			char s[32];
			sprintf_s(s, sizeof(s), "# <<< dump (%d bytes)", size);
			emit(KLog::LEVEL_NUL, s);
		}
	}
	void printCutline() {
		emit(KLog::LEVEL_NUL, "");
		emit(KLog::LEVEL_NUL, "");
		emit(KLog::LEVEL_NUL, K__LOG_SEPARATOR);
		emit(KLog::LEVEL_NUL, "");
	}
};
static CLogContext g_Log;


#pragma region KLog

static void Log_emit_dbg(const char *u8) {
	K__LOGLOG_BEGIN(KLog::LEVEL_NUL, u8);
	K__LOGLOG_ASSERT(u8);
	KLog::emit(KLog::LEVEL_VRB, u8);
	K__LOGLOG_END();
}
static void Log_emit_msg(const char *u8) {
	K__LOGLOG_BEGIN(KLog::LEVEL_NUL, u8);
	K__LOGLOG_ASSERT(u8);
	KLog::emit(KLog::LEVEL_NUL, u8);
	K__LOGLOG_END();
}
static void Log_emit_warn(const char *u8) {
	K__LOGLOG_BEGIN(KLog::LEVEL_WRN, u8);
	K__LOGLOG_ASSERT(u8);
	KLog::emit(KLog::LEVEL_WRN, u8);
	K__LOGLOG_END();
}
static void Log_emit_err(const char *u8) {
	K__LOGLOG_BEGIN(KLog::LEVEL_ERR, u8);
	K__LOGLOG_ASSERT(u8);
	KLog::emit(KLog::LEVEL_ERR, u8);
	K__LOGLOG_END();
}
void KLog::init() {
	K::setDebugPrintHook(Log_emit_dbg); // K::debug   の出力先を KLog にする
	K::setPrintHook(Log_emit_msg);      // K::print   の出力先を KLog にする
	K::setWarningHook(Log_emit_warn);   // K::warning の出力先を KLog にする
	K::setErrorHook(Log_emit_err);      // K::error   の出力先を KLog にする
}
void KLog::setThreadEnabled(bool value) {
	g_UseThread = value;
}
void KLog::setDialogEnabled(bool value) {
	g_Log.setDialogEnabled(value);
}
void KLog::muteDialog() {
	g_Log.muteDialog();
}
void KLog::unmuteDialog() {
	g_Log.unmuteDialog();
}
void KLog::setOutputCallback(KLog::Callback *cb) {
	g_Log.setCallback(cb);
}
void KLog::setOutputDebugger(bool value) {
	g_Log.setDebuggerEnabled(value);
}
void KLog::setOutputFileName(const char *filename_u8) {
	g_Log.setFileName(filename_u8);
}
void KLog::setOutputConsole(bool value, bool no_taskbar) {
	g_Log.setConsoleEnabled(value, no_taskbar);
}
/// テキストを出力する。
/// ユーザーによるコールバックを通さず、既定の出力先に直接書き込む。
/// コールバック内からログを出力したい時など、ユーザーコールバックの再帰呼び出しが邪魔になるときに使う
void KLog::emit_nocallback(Level lv, const char *u8) {
	K__LOGLOG_BEGIN(lv, u8);
	K__LOGLOG_ASSERT(u8);
	g_Log.emitNoCallback(lv, u8);
	K__LOGLOG_END();
}
/// テキストを出力する。
/// type には属性を表す文字を指定する。'E'rror, 'W'arning, 'I'nfo, 'D'ebug, 'V'erbose 無属性は '\0'
void KLog::emit(Level lv, const char *u8) {
	g_Log.emit(lv, u8);
}
void KLog::emitv(Level lv, const char *fmt, va_list args) {
	char s[K__LOG_SPRINTF_BUFSIZE];
	vsnprintf(s, sizeof(s), fmt, args);
	s[sizeof(s)-1] = '\0'; // 念のため、バッファ最後にヌル文字を入れておく
	emit(lv, s);
}
void KLog::emitf(Level lv, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	emitv(lv, fmt, args);
	va_end(args);
}
void KLog::emitv_w(Level lv, const wchar_t *fmt, va_list args) {
	// Format String
	wchar_t ws[K__LOG_SPRINTF_BUFSIZE];
	vswprintf(ws, K__LOG_SPRINTF_BUFSIZE, fmt, args);
	ws[K__LOG_SPRINTF_BUFSIZE-1] = '\0'; // 念のため、バッファ最後にヌル文字を入れておく

	// Wide --> Utf8
	char u8[K__LOG_SPRINTF_BUFSIZE];
	K::strWideToUtf8(u8, sizeof(u8), ws);

	emit(lv, u8);
}
void KLog::emitf_w(Level lv, const wchar_t *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	emitv_w(lv, fmt, args);
	va_end(args);
}
void KLog::rawEmit(const char *s) {
	// 渡されたテキストを無変更、無加工で出力する
	// 文字コード変換も何も行わない
	K::outputDebugFmt(s);
}
void KLog::rawEmitv(const char *fmt, va_list args) {
	char s[K__LOG_SPRINTF_BUFSIZE];
	vsnprintf(s, sizeof(s), fmt, args);
	s[sizeof(s)-1] = '\0'; // 念のため、バッファ最後にヌル文字を入れておく
	K::outputDebugFmt(s);
}
void KLog::rawEmitf(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	rawEmitv(fmt, args);
	va_end(args);
}
void KLog::printTrace(const char *file, int line) {
	std::string indent = std::string(g_Log.m_TraceDepth * 2, '*');
	g_Log.m_TraceMutex.lock();
	emitf(LEVEL_NUL, "[TRACE] %s%s(%d)", indent.c_str(), file, line);
	g_Log.m_TraceMutex.unlock();
}
void KLog::printTracef(const char *file, int line, const char *fmt, ...) {
	char s[1024] = {0};
	va_list args;
	va_start(args, fmt);
	sprintf_s(s, sizeof(s), fmt, args);
	va_end(args);

	std::string indent = std::string(g_Log.m_TraceDepth * 2, '*');
	g_Log.m_TraceMutex.lock();
	emitf(LEVEL_NUL, "[TRACE] %s%s(%d): %s", indent.c_str(), file, line, s);
	g_Log.m_TraceMutex.unlock();
}
void KLog::printSeparator() {
	g_Log.printCutline();
}
void KLog::printBinary(const void *data, int size) {
	g_Log.printBinaryText(data, size);
}
void KLog::threadWait() {
	g_Log.threadWait();
}
void KLog::threadLock() {
	g_Log.threadLock();
}
void KLog::threadUnlock() {
	g_Log.threadUnlock();
}
int KLog::getState(State s) {
	return g_Log.getState(s);
}
void KLog::setLevelVisible(Level level, bool visible) {
	switch (level) {
	case LEVEL_INF:
		g_Log.setLevel_info(visible);
		break;
	case LEVEL_DBG:
		g_Log.setLevel_debug(visible);
		break;
	case LEVEL_VRB:
		g_Log.setLevel_verbose(visible);
		break;
	}
}
void KLog::printRecord_unsafe(const KLog::Record &rec) {
	g_Log.printRecord_unsafe(rec);
}
void KLog::printError(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	emitv(LEVEL_ERR, fmt, args);
	va_end(args);
}
void KLog::printWarning(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	emitv(LEVEL_WRN, fmt, args);
	va_end(args);
}
void KLog::printInfo(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	emitv(LEVEL_INF, fmt, args);
	va_end(args);
}
void KLog::printDebug(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	emitv(LEVEL_DBG, fmt, args);
	va_end(args);
}
void KLog::printVerbose(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	emitv(LEVEL_VRB, fmt, args);
	va_end(args);
}
void KLog::printText(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	emitv(LEVEL_NUL, fmt, args);
	va_end(args);
}
void KLog::printFatal(const char *msg) {
	KLog::rawEmit("**** A FATAL ERROR HAS OCCURRED. THIS PROGRAM WILL BE TERMINATED ***\n");

	// 余計なコールバックを通らないように KLog::emit_nocallback で直接出力する
	KLog::emit_nocallback(LEVEL_ERR, msg);
	#ifdef _WIN32
	if (K::_IsDebuggerPresent()) {
		K::_break();
	}
	#endif
	K::_exit();
}
#pragma endregion // KLog









namespace Test {

void Test_log() {
	{
		KLogFile logfile;
		logfile.open("~test.log");
		logfile.clampBySeparator(-2); // ~test.log が存在すれば、切り取り線よりも前の部分を削除する
		logfile.close();
	}

	KLog::setOutputConsole(true);
	KLog::setOutputFileName("~test.log");

	{
		KLog::printSeparator(); // 切り取り線を出力（テキストファイル）
		KLog::setDialogEnabled(false); // エラーダイアログは表示しない
		KLog::setLevelVisible(KLog::LEVEL_VRB, true); // 詳細メッセージON
		KLog::setLevelVisible(KLog::LEVEL_DBG, true); // デバッグメッセージON
		KLog::setLevelVisible(KLog::LEVEL_INF, true); // 情報メッセージON
	}
	{
		KLog::printVerbose("VERBOSE MESSAGE");
		KLog::printDebug("DEBUG MESSAGE: '%s'", "This is a debug message");
		KLog::printInfo("INFO MESSAGE: '%s'", "This is an information text");
		KLog::printWarning("WARNING MESSAGE: '%s'", "Warning!");
		KLog::printError("ERROR MESSAGE: '%s'", "An error occurred!");
		KLog::printText("PLAIN TEXT: '%s'", "The quick brown fox jumps over the lazy dog"); // 無属性テキストを出力
	}
	{
		char data[256];
		for (int i=0; i<256; i++) data[i]=i;
		KLog::printBinary(data, sizeof(data));
	}
}

} // Test

} // namespace
