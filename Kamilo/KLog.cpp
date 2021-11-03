#include "KLog.h"
#include "KInternal.h"

#include <unordered_map>
#include <Windows.h> // Console


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
#	define K__LOGLOG_BEGIN(type, u8) K::outputDebugStringFmt("# %s [%c] \"%s\"", __FUNCTION__, (isprint(type) ? type : ' '), u8)
#	define K__LOGLOG_END()           K::outputDebugStringFmt("# %s [END]",       __FUNCTION__)
#else
#	define K__LOGLOG_BEGIN(type, u8)  /* empty */
#	define K__LOGLOG_END()            /* empty */
#endif


// ログ自身のアサート
#ifdef _DEBUG
#  define K__LOGLOG_ASSERT(x)     (!(x) ? __debugbreak() : (void)0)
#else
#  define K__LOGLOG_ASSERT(x)
#endif


#define K__LOG_SPRINTF_BUFSIZE (1024 * 4)


// K__LOG_SEPARATOR には末尾の改行を含めないようにする
// 改行文字 \n が \r\n に変換されてしまう可能性があるため、K__LOG_SEPARATOR で検索したときに不一致になり見つけられない可能性があるため
#define K__LOG_SEPARATOR  "-------------------------------------------------------------------"



namespace Kamilo {



bool g_UseThread = false;

#pragma region KLogFileOutput
KLogFileOutput::KLogFileOutput() {
	m_File = nullptr;
	m_Flags = 0;
}
bool KLogFileOutput::open(const std::string &filename_u8, KLogEmitFlags flags) {
	close();
	FILE *fp = K::fileOpen(filename_u8, (flags & KLogEmitFlag_APPEND) ? "a" : "w");
	if (fp) {
		m_File = fp;
		m_Flags = flags;
		m_FileName = filename_u8;
	} else {
		m_File = nullptr;
		m_Flags = 0;
		m_FileName = "";
		K::outputDebugStringFmt("[Log] ERROR Failed to open log file: '%s'", filename_u8.c_str());
	}
	return m_File != nullptr;
}
bool KLogFileOutput::isOpen() const {
	return m_File != nullptr;
}
void KLogFileOutput::close() {
	if (m_File) {
		fflush(m_File);
		fclose(m_File);
		m_File = nullptr;
		m_FileName = "";
	}
}
std::string KLogFileOutput::getFileName() const {
	return m_FileName;
}
void KLogFileOutput::writeLine(const char *u8) {
	if (m_File == nullptr) return;
	fputs(u8, m_File);
	fputs("\n", m_File);
	fflush(m_File);
}
void KLogFileOutput::writeRecord(const KLogRecord &rec) {
	if (m_File == nullptr) return;
	if ((m_Flags & KLogEmitFlag_NODATETIME) == 0) {
		fprintf(m_File, "%02d-%02d-%02d ", rec.time_year, rec.time_mon, rec.time_mday);
		fprintf(m_File, "%02d:%02d:%02d.%03d ", rec.time_hour, rec.time_min, rec.time_sec, rec.time_msec);
	}
	if ((m_Flags & KLogEmitFlag_NOAPPTIME) == 0) {
		fprintf(m_File, "(%6d) ", rec.app_msec);
	}
	if ((m_Flags & KLogEmitFlag_NOPROCESSID) == 0) {
		fprintf(m_File, "(%6d) @%08x ", rec.app_msec, K::sysGetCurrentProcessId());
	}
	if (m_Flags & KLogEmitFlag_SHORTLEVEL) {
		switch (rec.lv) {
		case KLogLv_CRITICAL: fprintf(m_File, "[C] "); break;
		case KLogLv_ERROR:    fprintf(m_File, "[E] "); break;
		case KLogLv_WARNING:  fprintf(m_File, "[W] "); break;
		case KLogLv_INFO:     fprintf(m_File, "[I] "); break;
		case KLogLv_DEBUG:    fprintf(m_File, "[D] "); break;
		case KLogLv_VERBOSE:  fprintf(m_File, "[V] "); break;
	//	default:              fprintf(m_File, "[%c] ", rec.lv); break;
		}
	} else {
		switch (rec.lv) {
		case KLogLv_CRITICAL: fprintf(m_File, "[Critical] "); break;
		case KLogLv_ERROR:    fprintf(m_File, "[Error] "); break;
		case KLogLv_WARNING:  fprintf(m_File, "[Warning] "); break;
		case KLogLv_INFO:     fprintf(m_File, "[Info] "); break;
		case KLogLv_DEBUG:    fprintf(m_File, "[Debug] "); break;
		case KLogLv_VERBOSE:  fprintf(m_File, "[Verbose] "); break;
	//	default:              fprintf(m_File, "[%c] ", rec.lv); break;
		}
	}
	fprintf(m_File, "%s\n", rec.text_u8.c_str());
	fflush(m_File);
}
bool KLogFileOutput::clampBySeparator(int number) {
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
bool KLogFileOutput::clampBySize(int clamp_size_bytes) {
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
int KLogFileOutput::findSeparatorByIndex(const char *text, size_t size, int index) {
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
#pragma endregion // KLogFileOutput



static char _GetLevelChar(KLogLv ll) {
	switch (ll) {
	case KLogLv_VERBOSE:  return 'V';
	case KLogLv_DEBUG:    return 'D';
	case KLogLv_INFO:     return 'I';
	case KLogLv_WARNING:  return 'W';
	case KLogLv_ERROR:    return 'E';
	case KLogLv_CRITICAL: return 'A'; // assertion
	}
	return '\0';
}


#pragma region KLogConsoleOutput
KLogConsoleOutput::KLogConsoleOutput() {
	m_Stdout = nullptr;
}
bool KLogConsoleOutput::open(bool no_taskbar) {
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
bool KLogConsoleOutput::isOpen() const {
	return m_Stdout != nullptr;
}
void KLogConsoleOutput::close() {
	if (m_Stdout) fclose(m_Stdout);
	m_Stdout = nullptr;
	#ifndef _CONSOLE
	if (0) {
		FreeConsole();
	}
	#endif
}
void KLogConsoleOutput::setColorFlags(int flags) {
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
int KLogConsoleOutput::getColorFlags() const {
	// コンソールウィンドウの文字属性コードを返す
	CONSOLE_SCREEN_BUFFER_INFO info;
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info);
	return (int)info.wAttributes;
}
void KLogConsoleOutput::writeLine(const char *u8) {
	std::wstring ws = K::strUtf8ToWide(u8);
	std::string mb = K::strWideToAnsi(ws, "");
	fputs(mb.c_str(), stdout);
	fputs("\n", stdout);
}
void KLogConsoleOutput::writeRecord(const KLogRecord &rec) {
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
	getLevelColorFlags(rec.lv, &typecolor, &msgcolor);

	// メッセージ種類
	if (1) {
		char tp = _GetLevelChar(rec.lv);
		if (tp != 0) {
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
void KLogConsoleOutput::getLevelColorFlags(KLogLv ll, uint16_t *typecolor, uint16_t *msgcolor) {
	// メッセージ属性に応じた色の組み合わせを得る。
	// ※この色の組み合わせの元ネタ
	// https://bitbucket.org/brunobraga/logcat-colorize
	//
	// ll ログレベル
	// sym 属性文字列の前景＆背景色
	// msg メッセージ文字列の前景＆背景色
	uint16_t sym = BG_BLACK|FG_WHITE;
	uint16_t msg = BG_BLACK|FG_WHITE;
	switch (ll) {
	case KLogLv_CRITICAL:
	case KLogLv_ERROR:
		sym = BG_RED|FG_WHITE;
		msg = BG_BLACK|FG_RED;
		break;

	case KLogLv_WARNING:
		sym = BG_YELLOW|FG_BLACK;
		msg = BG_BLACK|FG_YELLOW;
		break;

	case KLogLv_INFO:
		sym = BG_TEAL|FG_WHITE;
		msg = BG_BLACK|FG_TEAL;
		break;

	case KLogLv_DEBUG:
		if (1) {
			// 灰色
			sym = BG_GRAY|FG_WHITE;
			msg = BG_BLACK|FG_GRAY;
		} else {
			// 紫
			sym = BG_PURPLE|FG_WHITE;
			msg = BG_BLACK|FG_PURPLE;
		}
		break;

	case KLogLv_VERBOSE:
		if (0) {
			// 灰色
			sym = BG_GRAY|FG_WHITE;
			msg = BG_BLACK|FG_GRAY;
		} else {
			// 紫
			sym = BG_PURPLE|FG_WHITE;
			msg = BG_BLACK|FG_PURPLE;
		}
		break;
	}
	*typecolor = sym;
	*msgcolor = msg;
}
#pragma endregion // KLogConsoleOutput




#pragma region KLogDebuggerOutput
KLogDebuggerOutput::KLogDebuggerOutput() {
	m_IsOpen = false;
}
bool KLogDebuggerOutput::open() {
	close();
	m_IsOpen = K::_IsDebuggerPresent();
	return m_IsOpen;
}
bool KLogDebuggerOutput::isOpen() const {
	return m_IsOpen;
}
void KLogDebuggerOutput::close() {
	m_IsOpen = false;
}
void KLogDebuggerOutput::writeLine(const char *u8) {
	// デバッガーに出力する。
	// Visual Studio の「出力」ウィンドウでメッセージを見ることができる
	if (!m_IsOpen) return;
	std::wstring ws = K::strUtf8ToWide(u8);
	K::outputDebugStringW(ws);
}
void KLogDebuggerOutput::writeRecord(const KLogRecord &rec) {
	if (!m_IsOpen) return;
	char tp = _GetLevelChar(rec.lv);
	char s[K__LOG_SPRINTF_BUFSIZE];
	if (tp != '\0') {
		sprintf_s(s, sizeof(s), "%s", rec.text_u8.c_str());
	} else {
		sprintf_s(s, sizeof(s), "[%c] %s", tp, rec.text_u8.c_str());
	}
	writeLine(s);
}
#pragma endregion // KLogDebuggerOutput



#pragma region KLogger
#define LOGGER_LOG(fmt, ...)  K::outputDebugStringFmt(("[Log] " fmt), ##__VA_ARGS__)


class CLogEmitter: public KLogEmitter {
public:
	KLogFileOutput m_File;
	KLogConsoleOutput m_Console;
	KLogDebuggerOutput m_Debugger;
	uint32_t m_StartMsec;

	CLogEmitter() {
		m_StartMsec = K::clockMsec32();
	}
	// テキストを出力する
	// ユーザーによるコールバックを通さず、既定の出力先に直接書き込む。
	// コールバック内からログを出力したい時など、ユーザーコールバックの再帰呼び出しが邪魔になるときに使う
	virtual void emitString(KLogLv ll, const std::string &u8) override {
		KLogRecord rec;
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
			rec.lv = ll;
			rec.text_u8 = u8;
		}
		emitRecord(rec);
	}
	virtual void emitRecord(const KLogRecord &rec) override {
		if (rec.lv == KLogLv_NONE) {
			// 属性なしテキスト
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
			// 属性付きテキスト
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
	}
	virtual void setFileOutput(const std::string &filename_u8, KLogEmitFlags flags) override {
		if (filename_u8 != "") {

			m_File.open(filename_u8, flags);

			// 追記で開いた場合、一定以上古いログを削除する
			if (flags & KLogEmitFlag_APPEND) {
				m_File.clampBySeparator(-2); // 過去2回分のログだけ残し、それ以外を削除
			}
			LOGGER_LOG("file open: %s", filename_u8.c_str());
		} else {
			LOGGER_LOG("file close");
			m_File.close();
		}
	}
	virtual void setConsoleOutput(bool enabled) override {
		bool no_taskbar = true;
		if (enabled) {
			// コンソールへの出力を有効にする
			// 順番に注意：有効にしてからログを出す
			m_Console.open(no_taskbar);
			LOGGER_LOG("console on");
		} else {
			// コンソールへの出力を停止する。
			// 順番に注意：ログを出してから無効にする
			LOGGER_LOG("console off");
			m_Console.close();
		}
	}
	virtual void setDebuggerOutput(bool enabled) override {
		if (enabled) {
			m_Debugger.open();
			LOGGER_LOG("debugger on");
		} else {
			LOGGER_LOG("debugger off");
			m_Debugger.close();
		}
	}
	virtual std::string getFileOutput() const {
		return m_File.getFileName();
	}
	virtual bool getConsoleOutput() const {
		return m_Console.isOpen();
	}
	virtual bool getDebuggerOutput() const {
		return m_Debugger.isOpen();
	}
};


class CLogger: public KLogger {
	KLogLv m_Level;
	KLogEmitter *m_Emitter;
	KLoggerCallback *m_Callback;
	std::vector<KLogLv> m_Stack;
public:
	CLogger() {
		m_Level = KLogLv_DEBUG;
		m_Callback = nullptr;
		m_Emitter = new CLogEmitter();
	}
	virtual ~CLogger() {
		K__DROP(m_Emitter);
	}
	virtual KLogLv getLevel() const override {
		return m_Level;
	}
	virtual void setLevel(KLogLv ll) override {
		m_Level = ll;
		switch (m_Level) {
		case KLogLv_NONE:
			LOGGER_LOG("setLevel: NONE");
			break;
		case KLogLv_VERBOSE:
			LOGGER_LOG("setLevel: VERBOSE");
			break;
		case KLogLv_DEBUG:
			LOGGER_LOG("setLevel: DEBUG");
			break;
		case KLogLv_INFO:
			LOGGER_LOG("setLevel: INFO");
			break;
		case KLogLv_WARNING:
			LOGGER_LOG("setLevel: WARNING");
			break;
		case KLogLv_ERROR:
			LOGGER_LOG("setLevel: ERROR");
			break;
		case KLogLv_CRITICAL:
			LOGGER_LOG("setLevel: CRITICAL");
			break;
		}
	}
	virtual void pushLevel(KLogLv ll) override {
		m_Stack.push_back(m_Level);
		setLevel(ll);
	}
	virtual void popLevel() override {
		setLevel(m_Stack.back());
		m_Stack.pop_back();
	}
	virtual void setCallback(KLoggerCallback *cb) override {
		m_Callback = cb;
	}
	virtual KLogEmitter * getEmitter() override {
		return m_Emitter;
	}
	virtual void setEmitter(KLogEmitter *emitter) override {
		K__DROP(m_Emitter);
		m_Emitter = emitter;
		K__GRAB(m_Emitter);
	}
	virtual void emit(KLogLv ll, const std::wstring &ws) override {
		std::string u8 = K::strWideToUtf8(ws);
		emit(ll, u8);
	}
	virtual void emit(KLogLv ll, const std::string &u8) override {
		if (ll==KLogLv_NONE || m_Level <= ll) {
			// ユーザーによる処理を試みる
			bool mute = false;
			if (m_Callback) {
				m_Callback->on_log(ll, u8, &mute);
			}
			if (!mute && m_Emitter) {
				m_Emitter->emitString(ll, u8);
			}
		}
	}
	virtual void emitf(KLogLv ll, const char *fmt, ...) override {
		char s[1024] = {0};
		va_list args;
		va_start(args, fmt);
		vsnprintf(s, sizeof(s), fmt, args);
		va_end(args);
		emit(ll, s);
	}
};
#pragma endregion // KLogger


static void Logger_printBinaryText(KLogger *log, const void *data, int size) {
	const unsigned char *p = (const unsigned char *)data;
	log->emit(KLogLv_NONE, "# dump >>>");
	if (p == nullptr) {
		size = 0;
		log->emit(KLogLv_NONE, "(nullptr)");
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

				log->emit(KLogLv_NONE, s);
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
			log->emit(KLogLv_NONE, s);
			bin.clear();
			txt.clear();
		}
	}
	{
		char s[32];
		sprintf_s(s, sizeof(s), "# <<< dump (%d bytes)", size);
		log->emit(KLogLv_NONE, s);
	}
}
static void Logger_printCutline(KLogger *log) {
	log->emit(KLogLv_NONE, "");
	log->emit(KLogLv_NONE, "");
	log->emit(KLogLv_NONE, K__LOG_SEPARATOR);
	log->emit(KLogLv_NONE, "");
}
static void Logger_emit(KLogLv ll, const char *u8) {
	K__LOGLOG_BEGIN(ll, u8);
	K__LOGLOG_ASSERT(u8);
	KLogger *log = KLogger::get("kamilo");
	if (log) {
		log->emit(ll, u8);
	} else {
		K::outputDebugStringFmt("UNCAPTURED_LOG_MESSAGE: %s", u8);
	}
	K__LOGLOG_END();
}
static void Logger_emit_dbg(const char *u8) {
	Logger_emit(KLogLv_DEBUG, u8);
}
static void Logger_emit_msg(const char *u8) {
	Logger_emit(KLogLv_NONE, u8);
}
static void Logger_emit_warn(const char *u8) {
	Logger_emit(KLogLv_WARNING, u8);
}
static void Logger_emit_err(const char *u8) {
	Logger_emit(KLogLv_ERROR, u8);
}









#pragma region KLogger
static int g_LoggerOpen = 0;
static std::unordered_map<std::string, KLogger *> g_Loggers;

void KLogger::open(KLogEmitFlags flags) {
	if (g_Loggers[""] == nullptr) { // no root logger
		K::setDebugPrintHook(Logger_emit_dbg); // K::debug   の出力先を KLog にする
		K::setPrintHook(Logger_emit_msg);      // K::print   の出力先を KLog にする
		K::setWarningHook(Logger_emit_warn);   // K::warning の出力先を KLog にする
		K::setErrorHook(Logger_emit_err);      // K::error   の出力先を KLog にする

		KLogger *log = get("");
		K__ASSERT(log);

		if (flags & KLogEmitFlag_INIT_MUTE) {
			log->getEmitter()->setConsoleOutput(false);
			log->getEmitter()->setDebuggerOutput(false);
			log->getEmitter()->setFileOutput("", 0);
		} else {
			log->getEmitter()->setConsoleOutput(true);
			log->getEmitter()->setDebuggerOutput(true);
			log->getEmitter()->setFileOutput("log.txt", flags);
		}
	}
	g_LoggerOpen++;
}
void KLogger::close() {
	K__LOGLOG_ASSERT(g_LoggerOpen > 0);
	g_LoggerOpen--;
	if (g_LoggerOpen == 0) {
		for (auto it=g_Loggers.begin(); it!=g_Loggers.end(); ++it) {
			K__DROP(it->second);
		}
		g_Loggers.clear();


		K::setDebugPrintHook(nullptr);
		K::setPrintHook(nullptr);
		K::setWarningHook(nullptr);
		K::setErrorHook(nullptr);
	}
}
KLogger * KLogger::get(const char *group) {
	K__ASSERT(group);
	auto it = g_Loggers.find(""/*group*/);
	if (it != g_Loggers.end() && it->second) {
		return it->second;
	}
	KLogger *log = new CLogger();
	g_Loggers[group] = log;
	return log;
}

#pragma endregion // KLogger









namespace Test {

void Test_log() {
#if 0
	{
		KLogFileOutput logfile;
		logfile.open("~test.log");
		logfile.clampBySeparator(-2); // ~test.log が存在すれば、切り取り線よりも前の部分を削除する
		logfile.close();
	}

	KLog::setOutputConsole(true);
	KLog::setOutputFileName("~test.log");

	{
		KLog::printSeparator(); // 切り取り線を出力（テキストファイル）
		KLog::setDialogEnabled(false); // エラーダイアログは表示しない
		KLog::setLevelVisible(KLogLevel_VRB, true); // 詳細メッセージON
		KLog::setLevelVisible(KLogLevel_DBG, true); // デバッグメッセージON
		KLog::setLevelVisible(KLogLevel_INF, true); // 情報メッセージON
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
#endif
}

} // Test

} // namespace
