#pragma once
#include <string>

namespace Kamilo {

class KLog {
public:
	static void setThreadEnabled(bool value);

	enum Level {
		LEVEL_AST = 'A',

		/// エラーテキスト。
		/// KLog::setDialogEnabled() で true を指定した場合、エラーダイアログが自動的に表示される
		LEVEL_ERR = 'E',

		/// 警告テキスト
		LEVEL_WRN = 'W',

		/// 情報テキスト
		LEVEL_INF = 'I',

		/// デバッグテキスト
		LEVEL_DBG = 'D',

		/// 詳細なデバッグテキスト
		LEVEL_VRB = 'V',

		/// 無属性テキスト
		/// 無属性テキストはフィルタリングされず、常に表示される
		LEVEL_NUL = '\0',
	};

	struct Record {
		Record() {
			time_msec = 0;
			time_sec = 0;
			time_min = 0;
			time_hour = 0;
			time_mday = 0;
			time_mon = 0;
			time_year = 0;
			app_msec = 0;
			type = LEVEL_NUL;
		}
		int time_msec; // [0, 999]
		int time_sec;  // [0, 60] including leap second
		int time_min;  // [0, 59]
		int time_hour; // [0, 23]
		int time_mday; // [1, 31]
		int time_mon;  // [1, 12]
		int time_year;
		int app_msec;        // アプリケーション起動からの経過時間をミリ秒単位で表した値
		Level type;          // メッセージ属性（KLog::LEVEL_ERR や KLog::LEVEL_INFO など）
		std::string text_u8; // メッセージ文字列(UTF8)
	};

	/// KLog のイベントコールバック用のクラス
	/// @see KLog::setOutputCallback
	class Callback {
	public:
		/// ログ出力の直前でコールバックされる。
		/// @param level        ログの属性
		///                     - KLog::LEVEL_AST アサーション
		///                     - KLog::LEVEL_ERR エラー
		///                     - KLog::LEVEL_WRN 警告
		///                     - KLog::LEVEL_INF 情報
		///                     - KLog::LEVEL_DBG デバッグ
		///                     - KLog::LEVEL_VRB 詳細
		///                     - KLog::LEVEL_NUL 無属性
		/// @param text_u8     出力しようとしている文字列(utf8)
		/// @param no_emit     既定のログ出力（コンソールやテキストファイルなど）が不要なら true をセットする。デフォルト値は false
		/// @param no_dialog   既定のダイアログ処理が不要なら true をセットする。デフォルト値は false
		/// @note
		/// on_log_output の中でダイアログを表示する場合は KLog::isDialogAllowed() の値を確認すること
		virtual void on_log_output(Level level, const char *text_u8, bool *no_emit, bool *no_dialog) = 0;
	};

	static void init();

	/// エラーレベルのメッセージが発生したとき、既定のダイアログを出すかどうか。
	/// エラーが起きた時にすぐに気が付くよう、デフォルトでは true になっている。
	static void setDialogEnabled(bool value);

	/// ダイアログを抑制する。
	/// この効果は popDisableDialog まで続く。この呼び出しはネストすることができる
	static void muteDialog();
	static void unmuteDialog();

	/// コールバック関数への出力を設定する。
	static void setOutputCallback(Callback *cb);

	/// デバッガー（例えば Visual C++ の[出力]ウィンドウなど）への出力を設定する
	static void setOutputDebugger(bool value);

	/// テキストファイルへの出力を設定する。
	/// 空文字列 "" を指定すると出力を停止する
	/// 指定されたファイルを追記モードで開く
	///
	/// @param filename_u8 テキストファイル名(utf8)
	static void setOutputFileName(const char *filename_u8);

	/// コンソールウィンドウへの出力を設定する
	static void setOutputConsole(bool value, bool no_taskbar=false);

	/// ログメッセージを出力する。
	/// type には属性を表す文字を指定する。'E'rror, 'W'arning, 'I'nfo, 'D'ebug, 'V'erbose 無属性は '\0'
	static void emit(Level lv, const char *u8);
	static void emitv(Level lv, const char *fmt, va_list args);
	static void emitf(Level lv, const char *fmt, ...);
	static void emitv_w(Level lv, const wchar_t *fmt, va_list args);
	static void emitf_w(Level lv, const wchar_t *fmt, ...);

	/// テキストを出力する。
	/// ユーザーによるコールバックを通さず、既定の出力先に直接書き込む。
	/// コールバック内からログを出力したい時など、ユーザーコールバックの再帰呼び出しが邪魔になるときに使う
	static void emit_nocallback(Level lv, const char *u8);

	/// テキストを無加工のまま出力する。
	/// 意図しない再帰呼び出しを防ぐため、いかなるユーザー定義コールバックも呼ばず、
	/// 文字コード変換も行わず、そのまま fputs などに渡す
	static void rawEmit(const char *s);
	static void rawEmitv(const char *fmt, va_list args);
	static void rawEmitf(const char *fmt, ...);

	/// ログの区切り線を無属性テキストとして出力する
	/// この区切り線は古いログを削除するときの境界として使う
	/// @see clampFileByCutline
	static void printSeparator();

	/// バイナリデータの16進数表現を無属性テキストとして出力する。
	/// sizeに 0 を指定した場合は、ヌル文字終端の文字列が指定されたとみなす
	static void printBinary(const void *data, int size);

	/// トレース用のメッセージ
	static void printTrace(const char *file, int line);
	static void printTracef(const char *file, int line, const char *fmt, ...);

	/// 致命的エラーテキストを表示し、プログラムを exit(-1) で強制終了する。
	/// 続行不可能なエラーが発生したときに使う。
	static void printFatal(const char *msg);

	static void printRecord_unsafe(const KLog::Record &rec);

	static void printError(const char *fmt, ...);
	static void printWarning(const char *fmt, ...);
	static void printInfo(const char *fmt, ...);
	static void printDebug(const char *fmt, ...);
	static void printVerbose(const char *fmt, ...);
	static void printText(const char *fmt, ...);

	enum State {
		/// コンソールに出力するか
		STATE_HAS_OUTPUT_CONSOLE,

		/// テキストファイルに出力するか
		STATE_HAS_OUTPUT_FILE,

		/// デバッガーに出力するか
		STATE_HAS_OUTPUT_DEBUGGER,

		/// Info レベルのログが出力されるかどうか
		STATE_LEVEL_INFO,

		/// Debug レベルのログが出力されるかどうか
		STATE_LEVEL_DEBUG,

		/// Verbose レベルのログが出力されるかどうか
		STATE_LEVEL_VERBOSE,

		/// ダイアログの表示が許可されているか？
		/// KLog::setDialogEnabled で true が指定されていて、なおかつ KLog::muteDialog による抑制がされていない場合のみ true を返す
		STATE_DIALOG_ALLOWED,
	};
	/// 内部状態を得る
	static int getState(State s);

	/// レベル level のメッセージを表示するかどうか
	static void setLevelVisible(Level level, bool visible);

	static void threadWait();
	static void threadLock();
	static void threadUnlock();
};

class KLogFile {
public:
	KLogFile();
	bool open(const char *filename_u8);
	bool isOpen();
	void close();
	void writeLine(const char *u8);
	void writeRecord(const KLog::Record &rec);

	/// ログの区切り線よりも前の部分を削除する
	/// @see printSeparator
	///
	/// @param file_u8  テキストファイル名(utf8)
	/// @param number   区切り線の番号。0起算での区切り線インデックスか、負のインデックスを指定する。
	///                 正の値 n を指定すると、0 起算で n 本目の区切り線を探し、それより前の部分を削除する。
	///                 （毎回一番最初に区切り線を出力している場合、最初の n 回分のログが削除される）
	///                 負の値 -n を指定すると、末尾から n-1 本目の区切り線を探し、それより前の部分を削除する。
	///                 （毎回一番最初に区切り線を出力している場合、最新の n 回分のログだけ残る）
	bool clampBySeparator(int number);
	
	/// ログが指定サイズになるよう、前半部分を削除する
	///
	/// @param file_u8    テキストファイル名(utf8)
	/// @param size_bytes サイズをバイト単位で指定する。
	///                   このサイズに収まるように、適当な改行文字よりも前の部分が削除される
	bool clampBySize(int size);

private:
	static int findSeparatorByIndex(const char *text, size_t size, int index);
	std::string m_FileName;
	FILE *m_File;
};

class KLogConsole {
public:
	// コンソールの文字と背景色のフラグ
	enum {
		// WIN32API (consoleapi2.h) のカラーフラグに対応した値
		_FOREGROUND_BLUE      = 0x01, // = FOREGROUND_BLUE
		_FOREGROUND_GREEN     = 0x02, // = FOREGROUND_GREEN
		_FOREGROUND_RED       = 0x04, // = FOREGROUND_RED
		_FOREGROUND_INTENSITY = 0x08, // = FOREGROUND_INTENSITY
		_BACKGROUND_BLUE      = 0x10, // = BACKGROUND_BLUE
		_BACKGROUND_GREEN     = 0x20, // = BACKGROUND_GREEN
		_BACKGROUND_RED       = 0x40, // = BACKGROUND_RED
		_BACKGROUND_INTENSITY = 0x80, // = BACKGROUND_INTENSITY
		
		// 独自のカラーフラグ
		BG_BLACK  = 0,
		BG_PURPLE = _BACKGROUND_RED|_BACKGROUND_BLUE,
		BG_WHITE  = _BACKGROUND_RED|_BACKGROUND_GREEN|_BACKGROUND_BLUE|_BACKGROUND_INTENSITY,
		BG_RED    = _BACKGROUND_RED|_BACKGROUND_INTENSITY,
		BG_YELLOW = _BACKGROUND_RED|_BACKGROUND_GREEN|_BACKGROUND_INTENSITY,
		BG_LIME   = _BACKGROUND_GREEN|_BACKGROUND_INTENSITY,
		BG_SILVER = _BACKGROUND_RED|_BACKGROUND_GREEN|_BACKGROUND_BLUE,
		BG_GRAY   = _BACKGROUND_INTENSITY,
		BG_TEAL   = _BACKGROUND_GREEN|_BACKGROUND_BLUE,
		BG_GREEN  = _BACKGROUND_GREEN,
		BG_BLUE   = _BACKGROUND_BLUE|_BACKGROUND_INTENSITY,

		FG_BLACK  = 0,
		FG_PURPLE = _FOREGROUND_RED|_FOREGROUND_BLUE,
		FG_WHITE  = _FOREGROUND_RED|_FOREGROUND_GREEN|_FOREGROUND_BLUE|_FOREGROUND_INTENSITY,
		FG_RED    = _FOREGROUND_RED|_FOREGROUND_INTENSITY,
		FG_YELLOW = _FOREGROUND_RED|_FOREGROUND_GREEN|_FOREGROUND_INTENSITY,
		FG_LIME   = _FOREGROUND_GREEN|_FOREGROUND_INTENSITY,
		FG_SILVER = _FOREGROUND_RED|_FOREGROUND_GREEN|_FOREGROUND_BLUE,
		FG_GRAY   = _FOREGROUND_INTENSITY,
		FG_TEAL   = _FOREGROUND_GREEN|_FOREGROUND_BLUE,
		FG_GREEN  = _FOREGROUND_GREEN,
		FG_BLUE   = _FOREGROUND_BLUE|_FOREGROUND_INTENSITY,
	};

	KLogConsole();
	bool open(bool no_taskbar);
	bool isOpen();
	void close();
	void writeLine(const char *u8);
	void writeRecord(const KLog::Record &rec);
	void setColorFlags(int flags);
	int getColorFlags();
private:
	void getLevelColorFlags(char type, uint16_t *typecolor, uint16_t *msgcolor);
	FILE *m_Stdout;
};

class KLogDebugger {
public:
	KLogDebugger();
	bool open();
	bool isOpen();
	void close();
	void writeLine(const char *u8);
	void writeRecord(const KLog::Record &rec);
private:
	bool m_IsOpen;
};

namespace Test {
void Test_log();
}

} // namespace
