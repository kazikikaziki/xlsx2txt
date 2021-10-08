#pragma once
#include <inttypes.h>

namespace Kamilo {

/// 時間計測のためのクラス
class KClock {
public:
	/// システム時刻をナノ秒単位で得る
	static uint64_t getSystemTimeNano64();

	/// システム時刻をミリ秒単位で取得する
	static uint64_t getSystemTimeMsec64();
	static int getSystemTimeMsec();
	static float getSystemTimeMsecF();

	/// システム時刻を秒単位で取得する
	static int getSystemTimeSec();
	static float getSystemTimeSecF();

public:
	KClock();

	/// カウンタをゼロにセットする
	/// @note
	/// ここで前回のリセットからの経過時間を返せば便利そうだが、
	/// ミリ秒なのかナノ秒なのかがあいまいなので何も返さないことにする
	void reset();

	/// reset() を呼んでからの経過時間をナノ秒単位で返す
	uint64_t getTimeNano64() const;

	/// reset() を呼んでからの経過時間をミリ秒単位で返す
	uint64_t getTimeMsec64() const;

	/// reset() を呼んでからの経過時間を秒単位で返す
	float getTimeSecondsF() const;

	/// getTimeNano64() の int 版
	int getTimeNano() const;

	/// getTimeMsec64() の int 版
	int getTimeMsec() const;

	/// getTimeMsec64() の float 版
	float getTimeMsecF() const;

private:
	uint64_t m_Start;
};

} // namespace
