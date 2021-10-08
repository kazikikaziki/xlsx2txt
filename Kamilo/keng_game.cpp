#include "keng_game.h"
//
#include <algorithm>
#include <unordered_map>
#include <queue>
#include "KAnimation.h"
#include "KCamera.h"
#include "KDebug.h"
#include "KDrawable.h"
#include "KHitbox.h"
#include "KImGui.h"
#include "KInspector.h"
#include "KInternal.h"
#include "KJoystick.h"
#include "KKeyboard.h"
#include "KMainLoopClock.h"
#include "KRes.h"
#include "KScreen.h"
#include "KSolidBody.h"
#include "KSound.h"
#include "KStorage.h"
#include "KSystem.h"
#include "KSig.h"
#include "KWindow.h"

namespace Kamilo {




std::string KEngine::passImageName(int index) {
	return K::str_sprintf("(EngineRenderPass%d)", index);
}


class CEngineImpl:
	public KNodeRemovingCallback,
	public KWindow::Callback {
public:
	std::vector<KManager *> m_managers; // バインドされているシステム
	std::vector<KManager *> m_mgr_call_start;       // on_manager_start を呼ぶ
private:
	KMainLoopClock m_clock;
	KEngine::Flags m_flags;
	std::queue<KSig> m_broadcast_queue;
	std::recursive_mutex m_signal_mutex;
	int m_abort_timer;
	int m_sleep_until;
	bool m_should_exit;
	bool m_fullscreen;
	bool m_mark_tofullscreen;
	bool m_mark_towindowed;
	KSig m_resize_req; // サイズ変更要求
	bool m_init_called;
	uint32_t m_thread_id;
public:
	CEngineImpl() {
		zero_clear();
		m_thread_id = K::sysGetCurrentThreadId();
	}
	virtual ~CEngineImpl() {
		shutdown();
	}
	bool init(const KEngineDef *params) {
		zero_clear();
		m_init_called = true;

		// コンソール
		if (params && params->use_console) {
			KLog::init();
			KLog::setOutputConsole(true);
		}

		m_clock.init();
		if (params && params->fps > 0) {
			m_clock.setFps(params->fps);
		}

		int w = 0;
		int h = 0;
		if (params) {
			w = params->resolution_x;
			h = params->resolution_y;
			K__ASSERT(w > 0);
			K__ASSERT(h > 0);
		}

		// ウィンドウ
		KWindow::init(w, h, "");
		KWindow::setCallback(this);

		if (params) {
			addManager(params->callback);
		}

		// 入力デバイス
		KJoystick::init();

		// スクリーン
		int cw = 0;
		int ch = 0;
		KWindow::getClientSize(&cw, &ch);
		KScreen::install(w, h, cw, ch);
		KVideo::init(KWindow::getHandle(), nullptr, nullptr);
		KSoundPlayer::init(KWindow::getHandle());
		KNodeTree::install();

		// GUI
		void *d3ddev9 = nullptr; // IDirect3DDevice9
		KVideo::getParameter(KVideo::PARAM_D3DDEV9, &d3ddev9);
		KImGui::InitWithDX9(KWindow::getHandle(), d3ddev9);

		// Guiスタイルの設定
		ImGui::StyleColorsDark();
		KImGui::StyleKK();

		// インスペクター
		if (params && params->use_inspector) {
			KInspector::install();
		}

		// ビデオリソースの管理を開始する
		// インスペクター用のコールバックを登録するので、インスペクター作成後にする
		KBank::install();

		KScreen::startVideo(KBank::getTextureBank());

		// 描画管理
		KDrawable::install();
		KCamera::install();

		// 衝突判定
		KSolidBody::install();
		KHitbox::install();

		// スプライトアニメ
		KAnimation::install();

		return true;
	}
	void shutdown() {
		if (!m_init_called) {
			return; // すでに終了済み or そもそも初期化されていない
		}
		manager_end();

		KAnimation::uninstall();
		KHitbox::uninstall();
		KCamera::uninstall();
		KDrawable::uninstall();
		KSolidBody::uninstall();
		KNodeTree::uninstall();
		KInspector::uninstall();
		KImGui::Shutdown();
		KBank::uninstall();
		KScreen::uninstall();
		KJoystick::shutdown();
		KSoundPlayer::shutdown();
		KWindow::shutdown();
		m_clock.shutdown();

		// メンバーをゼロで初期化
		zero_clear();
	}
	void zero_clear() {
		m_abort_timer = 0;
		m_sleep_until = 0;
		m_fullscreen = false;
		m_mark_tofullscreen = false;
		m_mark_towindowed = false;
		m_resize_req = KSig();
		m_init_called = false;
		m_should_exit = false;
		m_flags = KEngine::FLAG_PAUSE | KEngine::FLAG_MENU;
	}
	/// フルスクリーンモードへの切り替え要求が出ているなら切り替えを実行する。
	/// 切り替えを実行した場合は true を返す。
	/// 切り替える必要がないか、失敗した場合は false を返す
	bool process_fullscreen_query() {
		if (! m_mark_tofullscreen) return false;

		// 要求フラグをリセット
		m_mark_tofullscreen = false;
		m_mark_towindowed = false;

		// フルスクリーン解像度を得る
		int w = 0;
		int h = 0;
		if (0) {
			// ゲーム画面の解像度に合わせる
			// （モニターの解像度を変更する場合、切り替えにすごく時間がかかる）
			KScreen::getGameSize(&w, &h);
		} else {
			// ディスプレイの解像度に合わせる
			// （解像度の変更を伴わない場合、フルスクリーンへの切り替えは非常に速い）
			w = KSystem::getInt(KSystem::INTPROP_SCREEN_W);
			h = KSystem::getInt(KSystem::INTPROP_SCREEN_H);
		}
		bool reset_ok = false;
		if (KVideo::canFullscreen(w, h)) {
			// デバイスをフルスクリーン化する
			deviceLost();
			reset_ok = KVideo::resetDevice(w, h, 1/*1=fullscreen*/);
			deviceReset();
		}

		if (!reset_ok) {
			// 失敗
			// ウィンドウモード用のウィンドウスタイルに戻す
			KWindow::setClientSize(w, h);
			KLog::printInfo("Failed to switch to fullscreen mode.");
			return false;
		}
		
		// ビューポートの大きさをウィンドウに合わせる
		KScreen::setGuiSize(w, h);

		// フルスクリーン化完了
		m_fullscreen = true;
		KLog::printInfo("Switching to fullscreen -- OK");

		// イベント送信
		{
			KSig sig(K_SIG_ENGINE_FULLSCREEN);
			broadcastSignal(sig);
		}
		return true;
	}

	/// ウィンドウモードへの切り替え要求が出ているなら切り替えを実行する。
	/// 切り替えを実行した場合は true を返す。
	/// 切り替える必要がないか、失敗した場合は false を返す
	bool process_windowed_query() {
		if (! m_mark_towindowed) return false;

		// 要求フラグをリセット
		m_mark_tofullscreen = false;
		m_mark_towindowed = false;

		// ウィンドウモードでの解像度を得る
		int w = 0;
		int h = 0;
		KScreen::getGameSize(&w, &h);

		// デバイスをウィンドウモードに切り替える
		deviceLost();
		bool reset_ok = KVideo::resetDevice(w, h, 0 /*0=windowed*/);
		deviceReset();
		
		if (!reset_ok) {
			// 失敗
			KLog::printWarning("Failed to switch to window mode.");
			return false;
		}

		// ウィンドウのフルスクリーンスタイルを解除する。
		// デバイスのフルスクリーン解除の後で実行しないと、ウィンドウスタイルがおかしくなる
		KWindow::setClientSize(w, h);

		// ウィンドウモード化完了
		m_fullscreen = false;
		KLog::printInfo("Switching to windowed -- OK");

		// イベント送信
		{
			KSig sig(K_SIG_ENGINE_WINDOWED);
			broadcastSignal(sig);
		}
		return true;
	}
	/// 描画解像度の変更要求が出ているなら、変更を実行する
	/// （ウィンドウのサイズは既に変更されているものとする）
	/// 変更を実行した場合は true を返す。実行する必要がないか、失敗した場合は false を返す
	bool process_resize_query() {
		if (m_resize_req.empty()) return false;

		// ウィンドウシグナルは非同期で届くため、
		// ウィンドウリサイズが完了しているとは限らない。
		// 現在のウィンドウサイズではなく、シグナルとともに送られてきたサイズを使ってリサイズする
		m_signal_mutex.lock(); //<-- ウィンドウリサイズによる m_resize_req の書き換えに備える 
		int cw = m_resize_req.getInt("x");
		int ch = m_resize_req.getInt("y");
		m_resize_req.clear();
		m_signal_mutex.unlock();

		if (cw <= 0 || ch <= 0) {
			// 現在のウィンドウサイズを使う
			KWindow::getClientSize(&cw, &ch);
			KLog::printInfo("resize by current window size %d x %d", cw, ch);
		} else {
			KLog::printInfo("resize by signal %d x %d", cw, ch);
		}

		// リセット
		deviceLost();
		KVideo::resetDevice(cw, ch, -1 /*-1=keep mode*/);
		deviceReset();

		// ビューポートの大きさをウィンドウに合わせる
		KScreen::setGuiSize(cw, ch);

		// カメラのレンダーターゲットのサイズを更新する
		update_camera_render_target();
		return true;
	}

	void update_camera_render_target() {
		int game_w, game_h;
		KScreen::getGameSize(&game_w, &game_h);
		KNodeArray cameras;
		KCamera::getCameraNodes(cameras);
		for (auto it=cameras.begin(); it!=cameras.end(); ++it) {
			KNode *camera = *it;
			if (camera == nullptr) continue;
			std::string texname = K::str_sprintf("_ViewTexture_%s.tex", camera->getName().c_str()); // カメラ固有のレンダーターゲット名
			KTEXID rentex = KBank::getTextureBank()->addRenderTexture(texname, game_w, game_h, KTextureBank::F_OVERWRITE_SIZE_NOT_MATCHED|KTextureBank::F_PROTECT);
			KCamera::of(camera)->setRenderTarget(rentex);
		}
	}

	void process_query() {
		// フルスクリーンモードへの切り替え要求が出ているなら切り替えを実行する
		process_fullscreen_query();

		// ウィンドウモードへの切り替え要求が出ているなら切り替えを実行する
		process_windowed_query();

		// 描画解像度が変化しているなら、グラフィックデバイスの設定を更新する
		process_resize_query();
	}

	#pragma region KEngine
	void moveWindow(int w, int h) {
		// ウィンドウモードでのみ移動可能
		if (m_fullscreen) return;
		if (!KWindow::isInit()) return;
		if (KWindow::getAttribute(KWindow::ATTR_FULLFILL)) return;
		if (KWindow::isMaximized()) return;
		if (KWindow::isIconified()) return;
		if (!KWindow::isWindowFocused()) return;
		KWindow::setWindowPosition(w, h);
	}
	void resizeWindow(int w, int h) {
		if (m_fullscreen) {
			m_mark_tofullscreen = false;
			m_mark_towindowed = true;
		}
		process_query(); // 今すぐに切り替える
		KWindow::setAttribute(KWindow::ATTR_HAS_BORDER, 1);
		KWindow::setAttribute(KWindow::ATTR_HAS_TITLE, 1);
		KWindow::setAttribute(KWindow::ATTR_FULLFILL, 0);
		KWindow::setClientSize(w, h);
	}
	void maximizeWindow() {
		// 通常の最大化
		if (m_fullscreen) {
			m_mark_tofullscreen = false;
			m_mark_towindowed = true;
		}
		KWindow::setAttribute(KWindow::ATTR_HAS_BORDER, 1);
		KWindow::setAttribute(KWindow::ATTR_HAS_TITLE, 1);
		KWindow::setAttribute(KWindow::ATTR_FULLFILL, 0);
		KWindow::maximizeWindow();
	}
	void restoreWindow() {
		if (m_fullscreen) {
			m_mark_tofullscreen = false;
			m_mark_towindowed = true;
		}
		// ウィンドウモードに切り替える
		KWindow::restoreWindow();
		
		// ウィンドウモード向けのウィンドウスタイルにする
		KWindow::setAttribute(KWindow::ATTR_HAS_BORDER, 1);
		KWindow::setAttribute(KWindow::ATTR_HAS_TITLE, 1);
		KWindow::setAttribute(KWindow::ATTR_FULLFILL, 0);
	}
	void setFullscreen() {
		if (! m_fullscreen) {
			m_mark_tofullscreen = true;
			m_mark_towindowed = false;
		}
	}
	void setWindowedFullscreen() {
		if (m_fullscreen) {
			m_mark_tofullscreen = false;
			m_mark_towindowed = true;
		}
		process_query(); // 今すぐに切り替える
		KWindow::setAttribute(KWindow::ATTR_HAS_BORDER, 0);
		KWindow::setAttribute(KWindow::ATTR_HAS_TITLE, 0);
		KWindow::setAttribute(KWindow::ATTR_FULLFILL, 1);
		KWindow::maximizeWindow();
	}

	/// デバッグ用
	KEngine::Flags getFlags() const {
		return m_flags;
	}
	void setFlags(KEngine::Flags flags) {
		m_flags = flags;
		m_clock.m_nowait = (m_flags & KEngine::FLAG_NOWAIT) != 0;
	}
	int command(const char *s, void *n) {
		int ret = 0;
		if (KScreen::command(s, n, &ret)) {
			return ret;
		}
		return 0;
	}
	int getStatus(KEngine::Status s) {
		int val = 0;
		switch (s) {
		case KEngine::ST_FRAME_COUNT_APP: // アプリケーションレベルでの実行フレーム数
			return m_clock.getAppFrames();

		case KEngine::ST_FRAME_COUNT_GAME: // ゲームレベルでの実行フレーム数
			return m_clock.getGameFrames();

		case KEngine::ST_TIME_MSEC:
			return K::clockMsec32();

		case KEngine::ST_FPS_UPDATE: // 更新関数のFPS値
			m_clock.getFps(&val, nullptr);
			return val;

		case KEngine::ST_FPS_RENDER: // 描画関数のFPS値
			m_clock.getFps(nullptr, &val);
			return val;

		case KEngine::ST_FPS_REQUIRED:
			return m_clock.getFps(nullptr, nullptr);

		case KEngine::ST_MAX_SKIP_FRAMES:
			m_clock.getFrameskips(&val, nullptr);
			return val;

		case KEngine::ST_KEYBOARD_BLOCKED:
			return isKeyboardBlocked() ? 1 : 0;

		case KEngine::ST_WINDOW_FOCUSED:
			return KWindow::isWindowFocused() ? 1 : 0;

		case KEngine::ST_WINDOW_POSITION_X:
			KWindow::getWindowPosition(&val, nullptr);
			return val;

		case KEngine::ST_WINDOW_POSITION_Y:
			KWindow::getWindowPosition(nullptr, &val);
			return val;

		case KEngine::ST_WINDOW_CLIENT_SIZE_W:
			KWindow::getClientSize(&val, nullptr);
			return val;

		case KEngine::ST_WINDOW_CLIENT_SIZE_H:
			KWindow::getClientSize(nullptr, &val);
			return val;

		case KEngine::ST_WINDOW_IS_MAXIMIZED:
			return KWindow::isMaximized() ? 1 : 0;

		case KEngine::ST_WINDOW_IS_ICONIFIED:
			return KWindow::isIconified() ? 1 : 0;

		case KEngine::ST_VIDEO_IS_FULLSCREEN:
			// スクリーンが fullscreen ならば本物のフルスクリーンになっている
			return m_fullscreen;

		case KEngine::ST_VIDEO_IS_WINDOWED_FULLSCREEN:
			// ウィンドウモード（ボーダレスフルスクリーンは、ウィンドウモードでウィンドウを最大化しているだけなので）
			// かつ境界線が無ければ、ボーダレスフルスクリーン
			if (!m_fullscreen) {
				if (!KWindow::getAttribute(KWindow::ATTR_HAS_BORDER)) {
					return true;
				}
			}
			return false;

		case KEngine::ST_VIDEO_IS_WINDOWED:
			// ウィンドウモードかつボーダレスでなければ、通常ウィンドウモード
			if (!m_fullscreen) {
				if (KWindow::getAttribute(KWindow::ATTR_HAS_BORDER)) {
					return true;
				}
			}
			return false;

		case KEngine::ST_SCREEN_W:
			return KSystem::getInt(KSystem::INTPROP_SCREEN_W);

		case KEngine::ST_SCREEN_H:
			return KSystem::getInt(KSystem::INTPROP_SCREEN_H);
		}
		return 0;
	}
	void run() {
		K__ASSERT(m_init_called);

		// タイマー精度を変更する
		K::sleepPeriodBegin();

		// インスペクター開始
		if (KInspector::isInstalled()) {
			KInspector::onGameStart();
		}

		// ループ開始前の初期化
		for (auto it=m_managers.begin(); it!=m_managers.end(); ++it) {
			(*it)->on_manager_will_start();
		}

		while (!m_should_exit && KWindow::isWindowVisible() && KWindow::processEvents()) {
			if (should_update_now()) {
				frame_start();
				{
					if (m_clock.tickUpdate()) {
						frame_update();
					}
					if (m_clock.tickRender()) {
						frame_render();
					}
				}
				frame_end();
			}
			m_clock.syncFreq();
		}
		// ループ直後の終了処理
		{
			for (auto it=m_managers.begin(); it!=m_managers.end(); ++it) {
				(*it)->on_manager_will_end();
			}
			removeInvalidatedIds(); // 終了処理時に無効化されたエンティティを削除
			if (KInspector::isInstalled()) {
				KInspector::onGameEnd();
			}
		}
		// ノードをすべて削除
		removeAllIds();

		// サブシステムを解放
		manager_end();

		// ウィンドウを破棄
		KWindow::shutdown();

		// タイマー制度戻す
		K::sleepPeriodEnd();
	}
	void manager_end() {
		// on_manager_end は追加した順番と逆順で呼び出す
		for (auto it=m_managers.rbegin(); it!=m_managers.rend(); ++it) {
			KManager *s = *it;
			s->on_manager_end();
			s->drop();
		}
		m_managers.clear();
		m_mgr_call_start.clear(); // m_mgr_call_start は KManager を grab していない
	}

	/// 更新すべきかどうか判定する。
	/// デバッグ機能により一時停止されている場合や、デバイスロストなどで描画不可能な場合は
	/// 更新不要と判断して false を返す。
	/// 今が更新すべきタイミングであれば true を返す
	bool should_update_now() {
		if (m_sleep_until > 0) {
			// 描画トラブルにより、一定時間更新を停止している。
			// 解除時刻になるまで何もしない
			if (m_clock.getTimeMsec() < m_sleep_until) {
				return false;
			}
			// 解除時刻になった。更新を再開する
			m_sleep_until = 0;
		}

		// 最小化されているときは描画しない
		if (KWindow::isIconified()) {
			return false;
		}
		// デバイスロストの確認
		{
			int graphics_non_avaiable = 0;
			KVideo::getParameter(KVideo::PARAM_DEVICELOST, &graphics_non_avaiable);
			if (!graphics_non_avaiable) {
				m_abort_timer = -1;
				return true; // ロスト無し
			}
		}
		// グラフィックが無効になっている。復帰を試みるが、ゲーム内容の更新はしない
		frame_render(); // <-- Reset device を呼ぶために、あえて描画処理を呼んでおく

		if (1) {
			// 自分が最前面にあるにも関わらずデバイスロストが 15 秒間続いた場合は強制終了する
			if (KWindow::isWindowFocused()) {
				if (m_abort_timer == -1) {
					int fps = m_clock.getFps(nullptr, nullptr);
					m_abort_timer = 15 * 1000 / fps; // 15秒のフレーム数
				} else if (m_abort_timer > 0) {
					m_abort_timer--;
				} else if (m_abort_timer == 0) {
					// タイトルバーと境界線を付けて、ユーザーがウィンドウを移動したりリサイズできるようにしておく
					KWindow::setAttribute(KWindow::ATTR_HAS_BORDER, true);
					KWindow::setAttribute(KWindow::ATTR_HAS_TITLE, true);
					KWindow::setAttribute(KWindow::ATTR_RESIZABLE, true);
					K::dialog(u8"デバイスロストから回復できませんでした。ゲームエンジンを終了します");
					quit();
				}
			}
		}
		return false;
	}

	bool isKeyboardBlocked() {
		// ウィンドウがフォーカスを持っていない場合、キーボード入力はブロックされる
		if (!KWindow::isWindowFocused()) {
			return true;
		}
		// ImGui に入力受付状態になっているアイテムがある（テキストボックスなど）なら
		// ゲーム側へのキーボード入力はブロックされる
		if (ImGui::IsAnyItemActive()) {
			return true;
		}
		return false;
	}

	void frame_start() {

		// ウィンドウの状態を確認し、キーボードからの入力をゲーム側に反映してよいか判定する
		
		// 入力状態
		for (int i=0; i<KJoystick::MAX_CONNECT; i++) {
			KJoystick::poll(i);
		}
		update_camera_render_target();

		// システムのスタートアップ処理
		if (m_mgr_call_start.size() > 0) {
			for (int i=0; i<(int)m_mgr_call_start.size(); i++) {
				KManager *s = m_mgr_call_start[i];
				s->on_manager_start();
			}
			m_mgr_call_start.clear();
		}

		// キューにたまっているシグナルを配信する
		broadcastProcessQueue();

		// on_manager_appframe
		for (int i=0; i<(int)m_managers.size(); i++) {
			KManager *mgr = m_managers[i];
			mgr->on_manager_appframe();
		}

		// システムノードとして登録されたノード
		KNodeTree::tick_system_nodes();
	}
	void frame_update() {
		// on_manager_beginframe
		for (int i=0; i<(int)m_managers.size(); i++) {
			KManager *mgr = m_managers[i];
			mgr->on_manager_beginframe();
		}

		// on_manager_frame
		for (int i=0; i<(int)m_managers.size(); i++) {
			KManager *mgr = m_managers[i];
			mgr->on_manager_frame();
		}
		KNodeTree::tick_nodes(0);

		// on_manager_frame2
		for (int i=0; i<(int)m_managers.size(); i++) {
			KManager *mgr = m_managers[i];
			mgr->on_manager_frame2();
		}
		KNodeTree::tick_nodes2(0);
	}
	void frame_end() {
		// on_manager_endframe は追加した順番と逆順で呼び出す
		for (int i=(int)m_managers.size()-1; i>=0; i--) {
			KManager *mgr = m_managers[i];
			mgr->on_manager_endframe();
		}
		// 無効化されたエンティティを削除
		removeInvalidatedIds();
	}

	// KNodeRemovingCallback
	virtual void onRemoveNodes(KNode *nodes[], int size) {
		if (KInspector::isInstalled()) {
			for (int i=0; i<size; i++) {
				KInspector::onEntityRemoving(nodes[i]);
			}
		}
		for (int i=(int)m_managers.size()-1; i>=0; i--) {
			KManager *mgr = m_managers[i];
			for (int i=0; i<size; i++) {
				mgr->on_manager_detach(nodes[i]);
			}
		}
	}

	void removeInvalidatedIds() {
		// 無効化されたエンティティを削除
		KNodeTree::destroyMarkedNodes(this/*KNodeRemovingCallback*/);
	}
	void removeAllIds() {
		KNode *root = KNodeTree::getRoot();

		// ルート直下にある非システムノードを削除する
		{
			for (int i=0; i<root->getChildCount(); i++) {
				KNode *node = root->getChild(i);
				if (node->hasFlag(KNode::FLAG_SYSTEM)) {
					// DO NOT REMOVE!!
				} else {
					node->markAsRemove();
				}
			}
			removeInvalidatedIds();
		}

		// 残りのルート直下ノードを削除する
		{
			for (int i=0; i<root->getChildCount(); i++) {
				root->getChild(i)->markAsRemove();
			}
			removeInvalidatedIds();
		}

		// この時点でルートノードには子ノードが１つもないはず
		K__ASSERT(root->getChildCount() == 0);
	}
	void frame_render() {
		// ウィンドウが最小化されているなら描画処理しない
		if (KWindow::isIconified()) {
			m_sleep_until = m_clock.getTimeMsec() + 500; // しばらく一時停止
			return;
		}

		// 画面モードの切り替え要求が出ているなら処理する
		process_query();

		// リセット確認
		if (KVideo::shouldReset()) {
			deviceLost();
			KVideo::resetDevice(0, 0, -1);
			deviceReset();
			return;
		}

		// ウィンドウの内容を描画する
		KVideo::beginScene();
		KScreen::render();
		if (KVideo::endScene()) {
			m_sleep_until = 0;
			return; // OK
		}

		// 描画に失敗した。ゲーム進行を停止しておく
		m_sleep_until = m_clock.getTimeMsec() + 500;
	}
	void deviceLost() {
		KImGui::DeviceLost();
	}
	void deviceReset() {
		KImGui::DeviceReset();
	}


	bool isPaused() {
		return m_clock.isPaused();
	}
	void play() {
		m_clock.play();
		{
			KSig sig(K_SIG_ENGINE_PLAY);
			broadcastSignal(sig);
		}
	}
	void playStep() {
		// ステップ実行は、既にポーズされている状態でこそ意味がある。
		// 非ポーズ状態からのステップ実行は、単なるポーズ開始と同じ
		if (!m_clock.isPaused()) {
			pause();
			return;
		}

		// ステップ実行
		m_clock.playStep();
		{
			KSig sig(K_SIG_ENGINE_PLAYPAUSE);
			broadcastSignal(sig);
		}
	}
	void pause() {
		m_clock.pause();
		{
			KSig sig(K_SIG_ENGINE_PAUSE);
			broadcastSignal(sig);
		}
	}
	void quit() {
		m_should_exit = true;
	}
	void setFps(int fps, int skip) {
		if (fps >= 0) {
			m_clock.setFps(fps);
		}
		if (skip >= 0) {
			m_clock.setFrameskips(skip, skip * 100);
		}
	}
	KManager * getManager(int index) {
		if (0 <= index && index < (int)m_managers.size()) {
			return m_managers[index];
		}
		return nullptr;
	}
	int getManagerCount() const {
		return (int)m_managers.size();
	}
	void addManager(KManager *mgr) {
		if (mgr) {
			mgr->grab();
			m_managers.push_back(mgr);
			m_mgr_call_start.push_back(mgr);
		}
	}
	void setManagerGuiEnabled(KManager *mgr, bool value) {
		KScreen::set_call_gui(mgr, value);
	}
	void setManagerRenderWorldEnabled(KManager *mgr, bool value) {
		KScreen::set_call_renderworld(mgr, value);
	}
	void setManagerRenderDebugEnabled(KManager *mgr, bool value) {
		KScreen::set_call_renderdebug(mgr, value);
	}
	void setManagerRenderTopEnabled(KManager *mgr, bool value) {
		KScreen::set_call_rendertop(mgr, value);
	}

	///////////////////
	void broadcastSignal(KSig &sig) {
		if (m_thread_id == K::sysGetCurrentThreadId()) {
			// メインスレッから配信する。
			// スレッドセーフを考慮しなくてよいのですぐに配信する
			broadcastSignalAsync(sig);
		} else {
			// メインスレッドでないスレッドから配信する。
			// 安全のために次回更新時に同期配信する
			m_signal_mutex.lock();
			m_broadcast_queue.push(sig);
			m_signal_mutex.unlock();
		}
	}
	void broadcastProcessQueue() {
		while (!m_broadcast_queue.empty()) {
			KSig sig;
			{
				m_signal_mutex.lock();
				sig = m_broadcast_queue.front();
				m_broadcast_queue.pop();
				m_signal_mutex.unlock();
			}
			broadcastSignalAsync(sig);
		}
	}
	void broadcastSignalAsync(KSig &sig) {
		// マネージャに配信
		for (auto it=m_managers.begin(); it!=m_managers.end(); ++it) {
			(*it)->on_manager_signal(sig);
		}
		// ノードに送信
		if (KNodeTree::isInstalled()) {
			KNodeTree::broadcastSignal(sig);
		}
	}

	///////////////////
	void addInspectorCallback(KInspectorCallback *cb, const char *label) {
		if (KInspector::isInstalled() && cb) {
			KInspector::removeInspectable(cb); // 重複回避
			KInspector::addInspectable(cb, label);
		}
	}
	void removeInspectorCallback(KInspectorCallback *cb) {
		if (KInspector::isInstalled() && cb) {
			KInspector::removeInspectable(cb);
		}
	}
	#pragma endregion // KEngine overrides

	#pragma region KWindow::Callback
	void onWindowClosing() {
		KSig sig(K_SIG_WINDOW_WINDOW_CLOSING);
		broadcastSignal(sig);
	}
	void onWindowDropFile(int index, int total, const char *filename_u8) {
		KSig sig(K_SIG_WINDOW_DROPFILE);
		sig.setString("file_u8", filename_u8);
		sig.setInt("index", index);
		sig.setInt("total", total);
		broadcastSignal(sig);
	}
	void onWindowMove(int x, int y) {
	}
	void onWindowResize(int x, int y) {
		KLog::printInfo("onWindowResize %d %d", x, y);
		KSig sig(K_SIG_WINDOW_WINDOW_SIZE);
		sig.setInt("x", x);
		sig.setInt("y", y);
		broadcastSignal(sig);
		m_signal_mutex.lock(); // 解像度の変更中にメッセージが届いたときに備える
		m_resize_req = sig;
		m_signal_mutex.unlock();
	}
	void onWindowMouseEnter(int x, int y) {
		KSig sig(K_SIG_WINDOW_MOUSE_ENTER);
		sig.setInt("x", x);
		sig.setInt("y", y);
		broadcastSignal(sig);
	}
	void onWindowMouseExit() {
		KSig sig(K_SIG_WINDOW_MOUSE_EXIT);
		broadcastSignal(sig);
	}
	void onWindowMouseMove(int x, int y, int btn) {
		KSig sig(K_SIG_WINDOW_MOUSE_MOVE);
		sig.setInt("x", x);
		sig.setInt("y", y);
		sig.setInt("button", btn);
		broadcastSignalAsync(sig);
	//	broadcastSignal(sig);
	}
	void onWindowMouseWheel(int x, int y, int delta) {
		KSig sig(K_SIG_WINDOW_MOUSE_WHEEL);
		sig.setInt("x", x);
		sig.setInt("y", y);
		sig.setInt("delta", delta);
		broadcastSignal(sig);
	}
	void onWindowMouseButtonDown(int x, int y, int btn) {
		KSig sig(K_SIG_WINDOW_MOUSE_DOWN);
		sig.setInt("x", x);
		sig.setInt("y", y);
		sig.setInt("button", btn);
		broadcastSignal(sig);
	}
	void onWindowMouseButtonUp(int x, int y, int btn) {
		KSig sig(K_SIG_WINDOW_MOUSE_UP);
		sig.setInt("x", x);
		sig.setInt("y", y);
		sig.setInt("button", btn);
		broadcastSignal(sig);
	}
	void onWindowKeyDown(KKeyboard::Key key) {
		// ImGui に入力受付状態になっているアイテムがある（テキストボックスなど）なら
		// ゲーム側への入力とはみなさない。無視する
		if (ImGui::GetCurrentContext()) {
			if (!ImGui::IsAnyItemActive()) {
				KSig sig(K_SIG_WINDOW_KEY_DOWN);
				sig.setInt("key", key);
				broadcastSignal(sig);
			}
		}
		// ゲームエンジンの操作
		switch (key) {
		case KKeyboard::KEY_F1:
			if ((m_flags & KEngine::FLAG_PAUSE) && KKeyboard::matchModifiers(0)) {
				playStep();
			}
			break;

		case KKeyboard::KEY_F2:
			if ((m_flags & KEngine::FLAG_PAUSE) && KKeyboard::matchModifiers(0)) {
				play();
			}
			break;

		case KKeyboard::KEY_ESCAPE:
			if ((m_flags & KEngine::FLAG_MENU) && KKeyboard::matchModifiers(KKeyboard::MODIF_SHIFT)) {
				if (KInspector::isInstalled()) {
					KInspector::setVisible(!KInspector::isVisible());
				}
			}
			break;
		}
	}
	virtual void onWindowKeyUp(KKeyboard::Key key) override {
		// ImGui に入力受付状態になっているアイテムがある（テキストボックスなど）なら
		// ゲーム側への入力とはみなさない。無視する
		if (ImGui::GetCurrentContext() && !ImGui::IsAnyItemActive()) {
			KSig sig(K_SIG_WINDOW_KEY_UP);
			sig.setInt("key", key);
			broadcastSignal(sig);
		}
	}
	virtual void onWindowKeyChar(wchar_t wc) override {
		// ImGui に入力受付状態になっているアイテムがある（テキストボックスなど）なら
		// ゲーム側への入力とはみなさない。無視する
		if (ImGui::GetCurrentContext() && !ImGui::IsAnyItemActive()) {
			KSig sig(K_SIG_WINDOW_KEY_CHAR);
			sig.setInt("chr", wc);
			broadcastSignal(sig);
		}
	}
	virtual void onWindowMessage(_HWND hWnd, _UINT msg, _WPARAM wParam, _LPARAM lParam, _LRESULT *p_lResult) override {
		// ImGui 側のウィンドウプロシージャを呼ぶ。
		// ここで ImGui_ImplWin32_WndProcHandler が 1 を返した場合、そのメッセージは処理済みであるため
		// 次に伝搬してはならない。
		// ※マウスカーソルの形状設定などに影響する。
		// これを正しく処理しないと、ImGui のテキストエディタにカーソルを重ねてもカーソル形状が IBeam にならない。
		if (ImGui::GetCurrentContext()) {
			long lResult = KImGui::WndProc(hWnd, msg, wParam, lParam);
			if (lResult) {
				*p_lResult = lResult;
			}
		}
	}
	#pragma endregion // KWindow::Callback

}; // CEngineImpl

static CEngineImpl * g_EngineInstance = nullptr;

bool KEngine::create(const KEngineDef *def) {
	g_EngineInstance = new CEngineImpl();
	if (g_EngineInstance->init(def)) {
		return true;
	}
	destroy();
	return false;
}
void KEngine::destroy() {
	if (g_EngineInstance) {
		g_EngineInstance->shutdown();
		delete g_EngineInstance;
		g_EngineInstance = nullptr;
	}
}

/// 無効化マークがつけられたノードを実際に削除する。
///
/// addManager() で追加したすべてのシステムからノードをデタッチして、最後に内部テーブルからノードを削除する。
/// @see KManager::on_manager_detach()
/// @see removeAllIds()
void KEngine::removeInvalidatedIds() {
	K__ASSERT_RETURN(g_EngineInstance);
	g_EngineInstance->removeInvalidatedIds();
}

/// 無効化マークの有無に関係なく、現在有効な全てのノードを削除する。
///
/// 無効化マークのあるノードだけを削除したい場合は removeInvalidatedIds() を使う
/// @see KManager::on_manager_detach()
void KEngine::removeAllIds() {
	K__ASSERT_RETURN(g_EngineInstance);
	g_EngineInstance->removeAllIds();
}
void KEngine::addManager(KManager *s) {
	K__ASSERT_RETURN(g_EngineInstance);
	g_EngineInstance->addManager(s);
}
void KEngine::setManagerGuiEnabled(KManager *mgr, bool value) {
	K__ASSERT_RETURN(g_EngineInstance);
	g_EngineInstance->setManagerGuiEnabled(mgr, value);
}
void KEngine::setManagerRenderWorldEnabled(KManager *mgr, bool value) {
	K__ASSERT_RETURN(g_EngineInstance);
	g_EngineInstance->setManagerRenderWorldEnabled(mgr, value);
}
void KEngine::setManagerRenderDebugEnabled(KManager *mgr, bool value) {
	K__ASSERT_RETURN(g_EngineInstance);
	g_EngineInstance->setManagerRenderDebugEnabled(mgr, value);
}
void KEngine::setManagerRenderTopEnabled(KManager *mgr, bool value) {
	K__ASSERT_RETURN(g_EngineInstance);
	g_EngineInstance->setManagerRenderTopEnabled(mgr, value);
}
int KEngine::getManagerCount() {
	K__ASSERT_RETURN_ZERO(g_EngineInstance);
	return g_EngineInstance->getManagerCount();
}

/// インデックスを指定してシステムを取得する
/// @param index 0 以上 getManagerCount() 未満
KManager * KEngine::getManager(int index) {
	K__ASSERT_RETURN_ZERO(g_EngineInstance);
	return g_EngineInstance->getManager(index);
}
void KEngine::broadcastSignal(KSig &sig) {
	K__ASSERT_RETURN(g_EngineInstance);
	g_EngineInstance->broadcastSignal(sig);
}
void KEngine::broadcastSignalAsync(KSig &sig) {
	K__ASSERT_RETURN(g_EngineInstance);
	g_EngineInstance->broadcastSignalAsync(sig);
}

/// インスペクター項目を登録する
/// @param cb    インスペクターに表示する項目
/// @param label インスペクター上での名前文字列。nullptr や "" を指定した場合はクラス名が表示される
void KEngine::addInspectorCallback(KInspectorCallback *cb, const char *label) {
	K__ASSERT_RETURN(g_EngineInstance);
	g_EngineInstance->addInspectorCallback(cb, label);
}
void KEngine::removeInspectorCallback(KInspectorCallback *cb) {
	K__ASSERT_RETURN(g_EngineInstance);
	g_EngineInstance->removeInspectorCallback(cb);
}

/// ゲームループを実行する。
///
/// ゲームループ実行中は KManager の各コールバックが呼ばれる。
/// 実行終了するまで処理は戻ってこない
void KEngine::run() {
	K__ASSERT_RETURN(g_EngineInstance);
	g_EngineInstance->run();
}
bool KEngine::isPaused(){
	K__ASSERT_RETURN_ZERO(g_EngineInstance);
	return g_EngineInstance->isPaused();
}
void KEngine::play(){
	K__ASSERT_RETURN(g_EngineInstance);
	g_EngineInstance->play();
}
void KEngine::playStep(){
	K__ASSERT_RETURN(g_EngineInstance);
	g_EngineInstance->playStep();
}
void KEngine::pause(){
	K__ASSERT_RETURN(g_EngineInstance);
	g_EngineInstance->pause();
}
void KEngine::quit(){
	K__ASSERT_RETURN(g_EngineInstance);
	g_EngineInstance->quit();
}

/// 更新サイクルなどを指定する
///
/// @param fps
///        1秒あたりの更新＆描画回数。 -1 を指定すると既存の FPS 値をそのまま使う（つまり FPS 変更しない）
/// @param skip
///        フレームスキップを許可する場合、1秒間での最大スキップ可能フレーム数。
///        <ul>
///        <li>-1: 既存のSKIP値を変更せずに使う</li>
///        <li> 0: スキップしない。1以上の値 n を指定すると、1秒間に最大で n 回の描画を省略する</li>
///        <li> 1: 1秒間あたり 1 フレームまでスキップを許可する</li>
///        <li> 2: 1秒間あたり 2 フレームまでスキップを許可する</li>
///        <li> (以下同様) </li>
///        </ul>
///
/// 例えば FPS を変更せずに SKIP だけ 0 にしたいなら、 setFps(-1, 0) のようにする
/// @see ST_FPS_REQUIRED
/// @see ST_MAX_SKIP_FRAMES
void KEngine::setFps(int fps, int skip) {
	K__ASSERT_RETURN(g_EngineInstance);
	g_EngineInstance->setFps(fps, skip);
}

/// ウィンドウのサイズを変更する
/// @see ST_WINDOW_CLIENT_SIZE_W
/// @see ST_WINDOW_CLIENT_SIZE_H
void KEngine::resizeWindow(int w, int h) {
	K__ASSERT_RETURN(g_EngineInstance);
	g_EngineInstance->resizeWindow(w, h);
}

/// ウィンドウ位置を設定する
/// @see ST_WINDOW_POSITION_X
/// @see ST_WINDOW_POSITION_Y
void KEngine::moveWindow(int x, int y) {
	K__ASSERT_RETURN(g_EngineInstance);
	g_EngineInstance->moveWindow(x, y);
}

void KEngine::setWindowMode(WindowMode mode) {
	K__ASSERT_RETURN(g_EngineInstance);
	switch (mode) {
	case WND_WINDOWED:
		g_EngineInstance->restoreWindow();
		break;
	case WND_MAXIMIZED:
		g_EngineInstance->maximizeWindow();
		break;
	case WND_FULLSCREEN:
		g_EngineInstance->setFullscreen();
		break;
	case WND_WINDOWED_FULLSCREEN:
		g_EngineInstance->setWindowedFullscreen();
		break;
	}
}
int KEngine::getStatus(Status s) {
	K__ASSERT_RETURN_ZERO(g_EngineInstance);
	return g_EngineInstance->getStatus(s);
}
int KEngine::command(const char *s, void *n) {
	K__ASSERT_RETURN_ZERO(g_EngineInstance);
	return g_EngineInstance->command(s, n);
}
void KEngine::setFlags(KEngine::Flags flags) {
	K__ASSERT_RETURN(g_EngineInstance);
	g_EngineInstance->setFlags(flags);
}
KEngine::Flags KEngine::getFlags() {
	K__ASSERT_RETURN_ZERO(g_EngineInstance);
	return g_EngineInstance->getFlags();
}

} // namespace 