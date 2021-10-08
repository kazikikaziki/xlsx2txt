#include "KMainLoopClock.h"
#include "KInternal.h"

namespace Kamilo {


KMainLoopClock::KMainLoopClock() {
	init();
}
void KMainLoopClock::init() {
	m_fps_required = 60;
	m_fps_update = 0;
	m_fps_render = 0;
	m_app_clock = -1; // インクリメントして 0 になるように
	m_game_clock = -1; // インクリメントして 0 になるように
	m_num_update = 0;
	m_num_render = 0;
	m_num_skips = 0;
	m_max_skip_frames = 0;
	m_max_skip_msec = 0;
	m_slow_motion_timer = 0;
	m_slow_motion_interval = 2;
	m_game_update = false;
	m_paused = false;
	m_step_once = false;
	m_nowait = false;
	m_fpstime_base = 0;
	m_fpstime_next = 0;
	m_fpstime_maxdelay = 500;
	m_startup_time64 = K::clockMsec64();
}
void KMainLoopClock::setFps(int fps) {
	m_fps_required = fps;
	m_fpstime_next = 0; // reset
}
int KMainLoopClock::getFps(int *fps_update, int *fps_render) {
	if (fps_update) *fps_update = m_fps_update;
	if (fps_render) *fps_render = m_fps_render;
	return m_fps_required;
}
int KMainLoopClock::getAppFrames() {
	return m_app_clock;
}
int KMainLoopClock::getGameFrames() {
	return m_game_clock;
}
float KMainLoopClock::getAppTimeSeconds() {
	return (float)getAppFrames() / m_fps_required;
}
float KMainLoopClock::getGameTimeSeconds() {
	return (float)getGameFrames() / m_fps_required;
}
void KMainLoopClock::setFrameskips(int max_skip_frames, int max_skip_msec) {
	m_max_skip_frames = max_skip_frames;
	m_max_skip_msec = max_skip_msec;
}
void KMainLoopClock::getFrameskips(int *max_skip_frames, int *max_skip_msec) const {
	if (max_skip_frames) *max_skip_frames = m_max_skip_frames;
	if (max_skip_msec) *max_skip_msec = m_max_skip_msec;
}
void KMainLoopClock::setSlowMotion(int interval, int duration) {
	// スローモーション。スロー状態のフレームカウンタで duration フレーム経過するまでスローにする
	if (duration > 0) {
		m_slow_motion_timer = duration * m_slow_motion_interval;
	}
	// スローモーションの更新間隔。
	// 通常のゲームフレーム単位で、interval フレームに１回だけ更新するよう設定する
	if (interval >= 2) {
		m_slow_motion_interval = interval;
	}
}
void KMainLoopClock::playStep() {
	if (isPaused()) {
		m_paused = false; // 実行させるためにポーズフラグを解除
		m_step_once = true;
	} else {
		pause();
	}
}
void KMainLoopClock::pause() {
	m_step_once = false;
	m_paused = true;
}
void KMainLoopClock::play() {
	m_step_once = false;
	m_paused = false;
}
bool KMainLoopClock::isPaused() {
	return m_paused;
}
int KMainLoopClock::getTimeMsec() {
	uint64_t now = K::clockMsec64();
	uint64_t diff = now - m_startup_time64;
	return (int)diff;
}
bool KMainLoopClock::tickUpdate() {
	if (m_paused) {
		return false;
	}

	// ゲーム進行用のフレームカウンタを更新
	m_game_clock++;

	// ステップ実行した場合
	if (m_step_once) {
		m_step_once = false; // ステップ実行解除
		m_paused = true; // そのままポーズ状態へ
	}
	m_num_update++;
	return true;
}

bool KMainLoopClock::tickRender() {
	int msec_formal = 1000 * m_num_update / m_fps_required;
	int msec_actual = (int)(getTimeMsec() - m_fpstime_base);
	int max_skip_msec = msec_formal * 10;

	if (msec_actual <= msec_formal) {
		// 前回の描画からの経過時間が規定時間内に収まっている。描画する
		m_num_skips = 0;

	} else if (max_skip_msec <= msec_actual) {
		// 前回の描画から時間が経ちすぎている。スキップせずに描画する
		m_num_skips = 0;

	} else if (m_max_skip_frames <= m_num_skips) {
		// 最大連続スキップ回数を超えた。描画する
		m_num_skips = 0;

	} else {
		// 描画スキップする
		m_num_skips++;
	}
	if (m_num_skips > 0) {
		return false;
	}
	m_num_render++;
	m_num_skips = 0;
	return true;
}
int KMainLoopClock::getWaitMsec() {
	const uint32_t delta = 1000 / m_fps_required;
	const uint32_t t = getTimeMsec();
	if (t + m_fpstime_maxdelay < m_fpstime_next) {
		// 現在時刻に最大待ち時間を足しても、次の実行予定時間に届かない（実行予定時刻が遠すぎる）
		m_fpstime_next = t + delta;
		return 0;
	}
	if (m_fpstime_next + delta < t) {
		// 実行予定時刻をすでに過ぎている（実行が遅れている）
		m_fpstime_next = t + delta;
		return 0;
	}
	if (m_fpstime_next <= t) {
		// 実行予定時刻になった
		m_fpstime_next += delta;
		return 0;
	}
	K__ASSERT(m_fpstime_next > t);
	return m_fpstime_next - t;
}
void KMainLoopClock::syncFreq() {
	// アプリケーションのフレームカウンタを更新
	// デバッグ目的などでゲーム進行が停止している間でも常にカウントし続ける
	m_app_clock++;

	uint32_t time = getTimeMsec();
	if (time >= m_fpstime_base + 1000) {
		m_fpstime_base = time;
		m_fps_update = m_num_update;
		m_fps_render = m_num_render;
		m_num_update = 0;
		m_num_render = 0;
	}
	if (! m_nowait) {
		while (getWaitMsec() > 0) {
			K::sleep(1);
		}
	}
}


} // namespace
