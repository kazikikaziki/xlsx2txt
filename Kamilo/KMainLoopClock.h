#pragma once
#include <inttypes.h>

namespace Kamilo {

class KMainLoopClock {
public:
	KMainLoopClock();
	void init();
	void shutdown() {}

	/// アプリケーションレベルでの実行フレーム番号を返す
	int getAppFrames();

	/// ゲームレベルでの実行フレーム数を返す
	int getGameFrames();

	/// アプリケーションの実行時間（秒）を返す
	float getAppTimeSeconds();

	/// ゲームの実行時間（秒）を返す
	float getGameTimeSeconds();

	/// フレームスキップ設定
	/// max_skip_frames -- 最大スキップフレーム数。このフレーム数スキップが続いた場合は、１度描画してスキップ状態をリセットする
	/// max_skip_msec -- 最大スキップ時間（ミリ秒）この時間以上経過していたら、１度描画してスキップ状態をリセットする
	void setFrameskips(int max_skip_frames, int max_skip_msec);
	void getFrameskips(int *max_skip_frames, int *max_skip_msec) const;

	/// FPSを設定する
	void setFps(int fps);
	
	/// FPS値を取得する（setFps で設定されたもの）
	/// 引数を指定すると、追加情報が得られる
	/// fps_update -- 更新関数のFPS値
	/// fps_rebder -- 描画関数のFPS値
	int getFps(int *fps_update, int *fps_render);

	/// 強制スローモーションを設定する
	/// interval -- 更新間隔。通常のゲームフレーム単位で、interval フレームに１回だけ更新するよう設定する
	/// duration -- 継続フレーム数。スロー状態のフレームカウンタで duration フレーム経過するまでスローにする
	void setSlowMotion(int interval, int duration);

	/// ポーズ中かどうか
	bool isPaused();

	/// ポーズ解除
	void play();

	/// ループを１フレームだけ進める
	void playStep();

	/// ポーズする（更新は停止するが、描画は継続する）
	void pause();

	/// 現在時刻
	int getTimeMsec();

	/// FPS を維持するために待機するべきミリ秒数
	int getWaitMsec();

	/// 内部の更新カウンタを処理する
	/// ゲーム状態を更新してよければ true を返す。
	/// 一時停止中など、ゲーム全体が停止していて更新しなくてよい場合は false を返す
	bool tickUpdate();

	/// 描画すべきかどうか判定する。
	/// FPS の設定と前回描画からの経過時間を調べ、
	/// 今が描画すべきタイミングであれば true を返す。
	/// 描画しなくても良い場合は false を返す
	bool tickRender();

	/// 次のフレームに備えて内部状態を更新し、
	/// 設定されたFPSを保つように適切な時間だけ待機する
	void syncFreq();

	bool m_nowait;

private:
	int m_app_clock;
	int m_game_clock;
	int m_fps_update;   // 更新FPS（更新回数/秒）
	int m_fps_render;   // 描画FPS（描画回数/秒）
	int m_fps_required; // 要求されているFPS値
	uint32_t m_fpstime_next;     // 次回の更新時刻（ミリ秒）
	uint32_t m_fpstime_maxdelay; // 更新の最大待ち時間。この時間が経過するまでに次のフレームを更新できなかった場合はFPSカウントをやりなおす
	uint32_t m_fpstime_base;     // 1秒ごとの起点時刻。この時刻から1秒経過するまでに m_fps_update 回の更新を行うようにスケジュールする
	uint64_t m_startup_time64;
	int m_num_update; // 更新回数
	int m_num_render; // 描画回数
	int m_slow_motion_timer;
	int m_slow_motion_interval;
	int m_num_skips; // 現在の連続描画スキップ数。
	int m_max_skip_frames; // 最大の連続描画スキップ数。この回数に達した場合は、必ず一度描画する
	int m_max_skip_msec; // 最大の連続描画スキップ時間（ミリ秒）。直前の描画からこの時間が経過していた場合は、必ず一度描画する
	bool m_paused;
	bool m_step_once;
	bool m_game_update;
};

} // namesapce
