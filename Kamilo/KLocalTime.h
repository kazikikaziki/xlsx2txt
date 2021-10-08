#pragma once
#include <time.h>
#include <string>

namespace Kamilo {

const char * const K_TIME_FORMAT = "%Y-%m-%d %H:%M:%S";


/// 現在のロケールにおけるローカル時刻を扱うクラス
struct KLocalTime {

	/// 現在時刻を返す
	static KLocalTime now();

	KLocalTime();

	/// time_t をローカルタイムに変換してインポートする
	explicit KLocalTime(time_t t);
	explicit KLocalTime(const char *s, const char *fmt=K_TIME_FORMAT);

	///  構造体の各メンバに設定されている値に対応する struct tm を返す。ただしミリ秒は無視する
	struct tm get_tm() const;

	/// 構造体の各メンバに設定されている値に対応する time_t 値を返す
	time_t get_time() const;
	void set_time(time_t gmt);

	/// C関数 strftime を使って、KLocalTime が保持している時刻値の文字列表現を得る。
	/// fmt に指定する書式については strftime のマニュアルを参照すること。
	/// fmt に nullptr を指定した場合はデフォルトのフォーマット K_TIME_FORMAT を使う
	std::string format(const char *fmt=K_TIME_FORMAT) const;

	/// C関数 strptime を使って、文字列から時刻を設定する。
	/// str: 時刻を表している文字列
	/// fmt: str がどのようなフォーマットにしたがって記述されているかを指定する。この指定にしたがって str 文字列から時刻情報を得る
	///      詳細については strptime または strftime のマニュアルを参照すること。
	///      この値に nullptr を指定した場合は、デフォルトのフォーマット K_TIME_FORMAT に従っているものとみなす
	/// loc: ロケール。時刻を表している文字列が月名などを含む場合、ロケールを正しく設定しないと日付を解析できない。
	///      たとえば、ロケールが "JPN" の状態で "JAN 2001 1" のような日付を読もうとしても失敗する
	bool parse(const char *s, const char *fmt=K_TIME_FORMAT, const char *loc="");

	int year;   ///< 年 (1900年からの年数)
	int month;  ///< 月 (1から12)
	int day;    ///< 日 (1から31)
	int hour;   ///< 時 (0から23)
	int minute; ///< 分 (0から59)
	int second; ///< 秒 (0から59)
	int msec;   ///< ミリ秒 (0から999)
};

namespace Test {
void Test_localtime();
}

} // namespace

