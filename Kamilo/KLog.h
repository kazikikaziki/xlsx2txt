#pragma once
#include <string>
#include "KRef.h"

namespace Kamilo {

enum KLogLv {
	KLogLv_NONE,    ///< 無属性テキスト（無属性テキストはフィルタリングされず、常に表示される）
	KLogLv_VERBOSE, ///< 詳細なデバッグテキスト
	KLogLv_DEBUG,   ///< デバッグテキスト
	KLogLv_INFO,    ///< 情報テキスト
	KLogLv_WARNING, ///< 警告テキスト
	KLogLv_ERROR,   ///< エラーテキスト。KLog::setDialogEnabled() で true を指定した場合、エラーダイアログが自動的に表示される
	KLogLv_CRITICAL,///< 続行不可能なエラー
	KLogLv_ENUM_MAX
};

struct KLogRecord {
	KLogRecord() {
		time_msec = 0;
		time_sec = 0;
		time_min = 0;
		time_hour = 0;
		time_mday = 0;
		time_mon = 0;
		time_year = 0;
		app_msec = 0;
		lv = KLogLv_NONE;
	}
	int time_msec; // [0, 999]
	int time_sec;  // [0, 60] including leap second
	int time_min;  // [0, 59]
	int time_hour; // [0, 23]
	int time_mday; // [1, 31]
	int time_mon;  // [1, 12]
	int time_year;
	int app_msec;        // アプリケーション起動からの経過時間をミリ秒単位で表した値
	KLogLv lv;
	std::string text_u8; // メッセージ文字列(UTF8)
};

enum KLogEmitFlag_ {
	KLogEmitFlag_APPEND      = 0x01, // 追記モード
	KLogEmitFlag_NODATETIME  = 0x02, // 日付時刻を省略
	KLogEmitFlag_NOAPPTIME   = 0x04, // 経過ミリ秒を省略
	KLogEmitFlag_NOPROCESSID = 0x08, // プロセスIDを省略
	KLogEmitFlag_SHORTLEVEL  = 0x10, // 短い形式のレベル表示を使う
	KLogEmitFlag_INIT_MUTE   = 0x20, // 初期化時には、全ての出力をOFFにした状態にしておく
};
typedef int KLogEmitFlags;

class KLogEmitter: public KRef {
public:
	virtual void emitString(KLogLv ll, const std::string &u8) = 0;
	virtual void emitRecord(const KLogRecord &rec) = 0;
	virtual void setFileOutput(const std::string &filename_u8, KLogEmitFlags flags) = 0;
	virtual void setConsoleOutput(bool enabled) = 0;
	virtual void setDebuggerOutput(bool enabled) = 0;
	virtual std::string getFileOutput() const = 0;
	virtual bool getConsoleOutput() const = 0;
	virtual bool getDebuggerOutput() const = 0;
};

class KLoggerCallback {
public:
	/// ログ出力の直前でコールバックされる。
	/// @param ll      ログレベル
	/// @param u8      出力しようとしている文字列(utf8)
	/// @param p_mute  既定のログ出力（コンソールやテキストファイルなど）が不要なら true をセットする。デフォルト値は false
	virtual void on_log(KLogLv ll, const std::string &u8, bool *p_mute) = 0;
};

class KLogger: public KRef {
public:
	static void open(KLogEmitFlags flags=0);
	static void close();
	static KLogger * get(const char *group="");

public:
	virtual KLogLv getLevel() const = 0;
	virtual void setLevel(KLogLv ll) = 0;
	virtual void emit(KLogLv ll, const std::string &u8) = 0;
	virtual void emit(KLogLv ll, const std::wstring &ws) = 0;
	virtual void emitf(KLogLv ll, const char *fmt, ...) = 0;
	virtual void setCallback(KLoggerCallback *cb) = 0;
	virtual void setEmitter(KLogEmitter *emitter) = 0;
	virtual KLogEmitter * getEmitter() = 0;
	virtual void pushLevel(KLogLv ll) = 0;
	virtual void popLevel() = 0;

	void critical(const std::string &s){ emit(KLogLv_CRITICAL, s); }
	void error(const std::string &s)   { emit(KLogLv_ERROR, s); }
	void warning(const std::string &s) { emit(KLogLv_WARNING, s); }
	void info(const std::string &s)    { emit(KLogLv_INFO, s); }
	void debug(const std::string &s)   { emit(KLogLv_DEBUG, s); }
	void verbose(const std::string &s) { emit(KLogLv_VERBOSE, s); }
	void print(const std::string &s)   { emit(KLogLv_NONE, s); }
};

class KLogFileOutput {
public:
	KLogFileOutput();
	bool open(const std::string &filename_u8, KLogEmitFlags flags);
	bool isOpen() const;
	std::string getFileName() const;
	void close();
	void writeLine(const char *u8);
	void writeRecord(const KLogRecord &rec);

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
	KLogEmitFlags m_Flags;
};

class KLogConsoleOutput {
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

	KLogConsoleOutput();
	bool open(bool no_taskbar);
	bool isOpen() const;
	void close();
	void writeLine(const char *u8);
	void writeRecord(const KLogRecord &rec);
	void setColorFlags(int flags);
	int getColorFlags() const;
private:
	void getLevelColorFlags(KLogLv level, uint16_t *typecolor, uint16_t *msgcolor);
	FILE *m_Stdout;
};

class KLogDebuggerOutput {
public:
	KLogDebuggerOutput();
	bool open();
	bool isOpen() const;
	void close();
	void writeLine(const char *u8);
	void writeRecord(const KLogRecord &rec);
private:
	bool m_IsOpen;
};

namespace Test {
void Test_log();
}

} // namespace
