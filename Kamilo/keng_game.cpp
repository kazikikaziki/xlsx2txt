#include "keng_game.h"
//
#include <algorithm>
#include <map>
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
#include "KMouse.h"
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


class CIniFileStab: public KIniFile {
public:
	virtual void writeInt(const std::string &key, int x) override {
		std::string s = K::str_sprintf("%d", x);
		writeString(key, s);
	}
	virtual void writeInt2(const std::string &key, int x, int y) override {
		std::string s = K::str_sprintf("%d,%d", x, y);
		writeString(key, s);
	}
	virtual void writeInt4(const std::string &key, int x, int y, int z, int w) override {
		std::string s = K::str_sprintf("%d,%d,%d,%d", x, y, z, w);
		writeString(key, s);
	}
	virtual bool readInt(const std::string &key, int *x) const override {
		std::string s;
		readString(key, &s);
		if (sscanf(s.data(), "%d", x) == 1) {
			return true;
		}
		return false;
	}
	virtual bool readInt2(const std::string &key, int *x, int *y) const override {
		std::string s;
		readString(key, &s);
		if (sscanf(s.data(), "%d,%d", x, y) == 2) {
			return true;
		}
		return false;
	}
	virtual bool readInt4(const std::string &key, int *x, int *y, int *z, int *w) const override {
		std::string s;
		readString(key, &s);
		if (sscanf(s.data(), "%d,%d,%d,%d", x, y, z, w) == 4) {
			return true;
		}
		return false;
	}
};


#pragma region CIniFileXml
#define CIniFileXml_USE_XML_ATTR 1

class CIniFileXml: public CIniFileStab {
public:
	KXmlElement *m_Xml;
	std::string m_FileName;

	CIniFileXml() {
		m_Xml = nullptr;
	}
	CIniFileXml(const std::string &filename) {
		m_Xml = nullptr;
		load(filename);
	}
	virtual ~CIniFileXml() {
		K__DROP(m_Xml);
	}
	virtual void writeString(const std::string &key, const std::string &val) override {
		if (m_Xml == nullptr) return;
		#if CIniFileXml_USE_XML_ATTR
		{
			m_Xml->setAttrString(key, val);
			save();
			return;
		}
		#else
		{
			KXmlElement *elm = m_Xml->findNode(key.c_str());
			if (elm) {
				elm->grab();
			} else {
				elm = m_Xml->addChild(key.c_str());
			}
			if (elm) {
				elm->setText(val.c_str());
				elm->drop();
				save();
				return;
			}
			return;
		}
		#endif
	}
	virtual bool readString(const std::string &key, std::string *p_val) const override {
		if (m_Xml == nullptr) return false;
		#if CIniFileXml_USE_XML_ATTR
		{
			const char *s = m_Xml->getAttrString(key.c_str());
			if (s) {
				if (p_val) *p_val = s;
				return true;
			} else {
				return false;
			}
		}
		#else
		{
			KXmlElement *elm = m_Xml->findNode(key.c_str());
			if (elm) {
				if (p_val) *p_val = elm->getText("");
				return true;
			} else {
				return false;
			}
		}
		#endif
	}
	virtual void save() override {
		if (m_Xml == nullptr) return;

		KOutputStream output = KOutputStream::fromFileName(m_FileName);
		if (!output.isOpen()) return;

		m_Xml->write(output, 0);
	}
	void load(const std::string &filename) {
		K__DROP(m_Xml);
		m_FileName = filename;
		if (K::pathExists(filename)) {
			KXmlElement *doc = KXmlElement::createFromFileName(m_FileName);
			if (doc) {
				KXmlElement *xUser = doc->findNode("User");
				if (xUser) {
					m_Xml = xUser->clone();
				}
				doc->drop();
			}
		}
		if (m_Xml == nullptr) {
			m_Xml = KXmlElement::create("User");
		}
	}
};
#pragma endregion // CIniFileXml


#pragma region CIniFileSimple
class CIniFileSimple: public CIniFileStab {
public:
	std::map<std::string, std::string> m_Values;
	std::string m_FileName;

	CIniFileSimple() {
	}
	CIniFileSimple(const std::string &filename) {
		load(filename);
	}
	virtual ~CIniFileSimple() {
	}
	virtual void writeString(const std::string &key, const std::string &val) override {
		m_Values[key] = val;
		save();
	}
	virtual bool readString(const std::string &key, std::string *p_val) const override {
		auto it = m_Values.find(key);
		if (it != m_Values.end()) {
			if (p_val) *p_val = it->second;
			return true;
		}
		return false;
	}
	virtual void save() override {
		KOutputStream output = KOutputStream::fromFileName(m_FileName);
		if (!output.isOpen()) return;
		for (auto it=m_Values.begin(); it!=m_Values.end(); ++it) {
			std::string s = K::str_sprintf("%s = %s\n", it->first.c_str(), it->second.c_str());
			output.writeString(s);
		}
	}
	void load(const std::string &filename) {
		m_Values.clear();
		m_FileName = filename;
		KInputStream input = KInputStream::fromFileName(filename);
		if (input.isOpen()) {
			std::vector<std::string> lines = K::strSplitLines(input.readBin());
			for (auto it=lines.begin(); it!=lines.end(); ++it) {
				std::string k, v;
				K::strSplitLeft(*it, "=", &k, &v);
				if (k != "") {
					m_Values[k] = v;
				}
			}
		}
	}
};
#pragma endregion // CIniFileSimple


KIniFile * createIniFile(const std::string &filename) {
#if 0
	return new CIniFileXml(filename);
#else
	return new CIniFileSimple(filename);
#endif
}



std::string KEngine::passImageName(int index) {
	return K::str_sprintf("(EngineRenderPass%d)", index);
}


class CEngineImpl:
	public KNodeRemovingCallback,
	public KWindowCallback {
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
	KCoreWindow *m_Window;
	KCoreKeyboard *m_Keyboard;
	KCoreMouse *m_Mouse;
	KCoreJoystick *m_Joystick;
	KIniFile *m_IniFile;
	KStorage *m_Storage;

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

		KEngineDef def; // この時点で def にはデフォルト値が入っている
		if (params) {
			def = *params;
		}

		m_clock.init();
		if (def.fps > 0) {
			m_clock.setFps(def.fps);
		}

		// Iniファイル
		if (def.ini_filename != "") {
			m_IniFile = Kamilo::createIniFile(def.ini_filename);
		}

		int w = def.resolution_x;
		int h = def.resolution_y;
		K__ASSERT(w > 0);
		K__ASSERT(h > 0);

		// ウィンドウ
		KWindow::init(w, h, "");
		m_Window = KWindow::get();
		m_Window->grab();
		m_Window->setCallback(this); // KWindowCallback

		// Iniファイルにウィンドウ位置とサイズがあるなら復元する
		if (m_IniFile) {
			int x=0, y=0, w=0, h=0;
			m_IniFile->readInt2("Pos", &x, &y);
			m_IniFile->readInt2("Size", &w, &h);
			if (m_Window->adjustWindowPosAndSize(&x, &y, &w, &h)) {
				m_Window->setWindowNormalRect(x, y, w, h);
			}

			int maximized = 0;
			m_IniFile->readInt("Maximized", &maximized);
			if (maximized) {
				m_Window->maximizeWindow();
			}
		}

		// 入力デバイス
		m_Keyboard = Kamilo::createKeyboardWin32();
		m_Mouse = Kamilo::createMouseWin32();
		m_Joystick = Kamilo::createJoystickWin32();

		if (def.storage) {
			m_Storage = def.storage;
			K__GRAB(m_Storage);
		} else {
			m_Storage = Kamilo::createStorage();
		}
		if (def.callback) {
			addManager(def.callback);
		}

		// スクリーン
		int cw = 0;
		int ch = 0;
		m_Window->getClientSize(&cw, &ch);
		KScreen::install(w, h, cw, ch);
		KVideo::init(m_Window->getHandle(), nullptr, nullptr);
		KSoundPlayer::init(m_Window->getHandle());
		KNodeTree::install();

		// GUI
		void *d3ddev9 = nullptr; // IDirect3DDevice9
		KVideo::getParameter(KVideo::PARAM_D3DDEV9, &d3ddev9);
		KImGui::InitWithDX9(m_Window->getHandle(), d3ddev9);

		// Guiスタイルの設定
		ImGui::StyleColorsDark();
		KImGui::StyleKK();

		// インスペクター
		if (def.use_inspector) {
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

		K__DROP(m_Keyboard);
		K__DROP(m_Mouse);
		K__DROP(m_Joystick);
		K__DROP(m_IniFile);
		K__DROP(m_Storage);
		K__DROP(m_Window);

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
		m_Keyboard = nullptr;
		m_Mouse = nullptr;
		m_Joystick = nullptr;
		m_IniFile = nullptr;
		m_Storage = nullptr;
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
			m_Window->setClientSize(w, h);
			K__PRINT("Failed to switch to fullscreen mode.");
			return false;
		}
		
		// ビューポートの大きさをウィンドウに合わせる
		KScreen::setGuiSize(w, h);

		// フルスクリーン化完了
		m_fullscreen = true;
		K__PRINT("Switching to fullscreen -- OK");

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
			K__WARNING("Failed to switch to window mode.");
			return false;
		}

		// ウィンドウのフルスクリーンスタイルを解除する。
		// デバイスのフルスクリーン解除の後で実行しないと、ウィンドウスタイルがおかしくなる
		m_Window->setClientSize(w, h);

		// ウィンドウモード化完了
		m_fullscreen = false;
		K__PRINT("Switching to windowed -- OK");

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
			m_Window->getClientSize(&cw, &ch);
			K__PRINT("resize by current window size %d x %d", cw, ch);
		} else {
			K__PRINT("resize by signal %d x %d", cw, ch);
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
		if (m_Window->getAttribute(KWindowAttr_FULLFILL)) return;
		if (m_Window->isMaximized()) return;
		if (m_Window->isIconified()) return;
		if (!m_Window->isWindowFocused()) return;
		m_Window->setWindowPosition(w, h);
	}
	void resizeWindow(int w, int h) {
		if (m_fullscreen) {
			m_mark_tofullscreen = false;
			m_mark_towindowed = true;
		}
		process_query(); // 今すぐに切り替える
		m_Window->setAttribute(KWindowAttr_HAS_BORDER, 1);
		m_Window->setAttribute(KWindowAttr_HAS_TITLE, 1);
		m_Window->setAttribute(KWindowAttr_FULLFILL, 0);
		m_Window->setClientSize(w, h);
	}
	void maximizeWindow() {
		// 通常の最大化
		if (m_fullscreen) {
			m_mark_tofullscreen = false;
			m_mark_towindowed = true;
		}
		m_Window->setAttribute(KWindowAttr_HAS_BORDER, 1);
		m_Window->setAttribute(KWindowAttr_HAS_TITLE, 1);
		m_Window->setAttribute(KWindowAttr_FULLFILL, 0);
		m_Window->maximizeWindow();
	}
	void restoreWindow() {
		if (m_fullscreen) {
			m_mark_tofullscreen = false;
			m_mark_towindowed = true;
		}
		// ウィンドウモードに切り替える
		m_Window->restoreWindow();
		
		// ウィンドウモード向けのウィンドウスタイルにする
		m_Window->setAttribute(KWindowAttr_HAS_BORDER, 1);
		m_Window->setAttribute(KWindowAttr_HAS_TITLE, 1);
		m_Window->setAttribute(KWindowAttr_FULLFILL, 0);
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
		m_Window->setAttribute(KWindowAttr_HAS_BORDER, 0);
		m_Window->setAttribute(KWindowAttr_HAS_TITLE, 0);
		m_Window->setAttribute(KWindowAttr_FULLFILL, 1);
		m_Window->maximizeWindow();
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
			return m_Window->isWindowFocused() ? 1 : 0;

		case KEngine::ST_WINDOW_POSITION_X:
			m_Window->getWindowPosition(&val, nullptr);
			return val;

		case KEngine::ST_WINDOW_POSITION_Y:
			m_Window->getWindowPosition(nullptr, &val);
			return val;

		case KEngine::ST_WINDOW_CLIENT_SIZE_W:
			m_Window->getClientSize(&val, nullptr);
			return val;

		case KEngine::ST_WINDOW_CLIENT_SIZE_H:
			m_Window->getClientSize(nullptr, &val);
			return val;

		case KEngine::ST_WINDOW_IS_MAXIMIZED:
			return m_Window->isMaximized() ? 1 : 0;

		case KEngine::ST_WINDOW_IS_ICONIFIED:
			return m_Window->isIconified() ? 1 : 0;

		case KEngine::ST_VIDEO_IS_FULLSCREEN:
			// スクリーンが fullscreen ならば本物のフルスクリーンになっている
			return m_fullscreen;

		case KEngine::ST_VIDEO_IS_WINDOWED_FULLSCREEN:
			// ウィンドウモード（ボーダレスフルスクリーンは、ウィンドウモードでウィンドウを最大化しているだけなので）
			// かつ境界線が無ければ、ボーダレスフルスクリーン
			if (!m_fullscreen) {
				if (!m_Window->getAttribute(KWindowAttr_HAS_BORDER)) {
					return true;
				}
			}
			return false;

		case KEngine::ST_VIDEO_IS_WINDOWED:
			// ウィンドウモードかつボーダレスでなければ、通常ウィンドウモード
			if (!m_fullscreen) {
				if (m_Window->getAttribute(KWindowAttr_HAS_BORDER)) {
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

		while (!m_should_exit && m_Window->isWindowVisible() && KWindow::processEvents()) {
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
		if (m_Window->isIconified()) {
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
			if (m_Window->isWindowFocused()) {
				if (m_abort_timer == -1) {
					int fps = m_clock.getFps(nullptr, nullptr);
					m_abort_timer = 15 * 1000 / fps; // 15秒のフレーム数
				} else if (m_abort_timer > 0) {
					m_abort_timer--;
				} else if (m_abort_timer == 0) {
					// タイトルバーと境界線を付けて、ユーザーがウィンドウを移動したりリサイズできるようにしておく
					m_Window->setAttribute(KWindowAttr_HAS_BORDER, true);
					m_Window->setAttribute(KWindowAttr_HAS_TITLE, true);
					m_Window->setAttribute(KWindowAttr_RESIZABLE, true);
					K::dialog(u8"デバイスロストから回復できませんでした。ゲームエンジンを終了します");
					quit();
				}
			}
		}
		return false;
	}

	bool isKeyboardBlocked() {
		// ウィンドウがフォーカスを持っていない場合、キーボード入力はブロックされる
		if (!m_Window->isWindowFocused()) {
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
		if (m_Joystick) {
			m_Joystick->poll();
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
		if (m_Window->isIconified()) {
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

	#pragma region KWindowCallback
	virtual void onWindowClosing() override {
		KSig sig(K_SIG_WINDOW_WINDOW_CLOSING);
		broadcastSignal(sig);

		// ウィンドウ位置とサイズを保存しておく
		if (m_IniFile) {
			int x=0, y=0, w=0, h=0;
			m_Window->getWindowNormalRect(&x, &y, &w, &h);
			m_IniFile->writeInt2("Pos", x, y);
			m_IniFile->writeInt2("Size", w, h);
			m_IniFile->writeInt("Maximized", m_Window->isMaximized());
		}
	}
	virtual void onWindowDropFile(int index, int total, const char *filename_u8) override {
		KSig sig(K_SIG_WINDOW_DROPFILE);
		sig.setString("file_u8", filename_u8);
		sig.setInt("index", index);
		sig.setInt("total", total);
		broadcastSignal(sig);
	}
	virtual void onWindowMove(int x, int y) override {
	}
	virtual void onWindowResize(int x, int y) override {
		K__VERBOSE("onWindowResize %d %d", x, y);
		KSig sig(K_SIG_WINDOW_WINDOW_SIZE);
		sig.setInt("x", x);
		sig.setInt("y", y);
		broadcastSignal(sig);
		m_signal_mutex.lock(); // 解像度の変更中にメッセージが届いたときに備える
		m_resize_req = sig;
		m_signal_mutex.unlock();
	}
	virtual void onWindowMouseEnter(int x, int y) override {
		KSig sig(K_SIG_WINDOW_MOUSE_ENTER);
		sig.setInt("x", x);
		sig.setInt("y", y);
		broadcastSignal(sig);
	}
	virtual void onWindowMouseExit() override {
		KSig sig(K_SIG_WINDOW_MOUSE_EXIT);
		broadcastSignal(sig);
	}
	virtual void onWindowMouseMove(int x, int y, int btn) override {
		KSig sig(K_SIG_WINDOW_MOUSE_MOVE);
		sig.setInt("x", x);
		sig.setInt("y", y);
		sig.setInt("button", btn);
		broadcastSignalAsync(sig);
	//	broadcastSignal(sig);
	}
	virtual void onWindowMouseWheel(int x, int y, int delta) override {
		KSig sig(K_SIG_WINDOW_MOUSE_WHEEL);
		sig.setInt("x", x);
		sig.setInt("y", y);
		sig.setInt("delta", delta);
		broadcastSignal(sig);
	}
	virtual void onWindowMouseButtonDown(int x, int y, int btn) override {
		KSig sig(K_SIG_WINDOW_MOUSE_DOWN);
		sig.setInt("x", x);
		sig.setInt("y", y);
		sig.setInt("button", btn);
		broadcastSignal(sig);
	}
	virtual void onWindowMouseButtonUp(int x, int y, int btn) override {
		KSig sig(K_SIG_WINDOW_MOUSE_UP);
		sig.setInt("x", x);
		sig.setInt("y", y);
		sig.setInt("button", btn);
		broadcastSignal(sig);
	}
	virtual void onWindowKeyDown(KKey key) override {
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
		case KKey_F1:
			if ((m_flags & KEngine::FLAG_PAUSE) && KKeyboard::matchModifiers(0)) {
				playStep();
			}
			break;

		case KKey_F2:
			if ((m_flags & KEngine::FLAG_PAUSE) && KKeyboard::matchModifiers(0)) {
				play();
			}
			break;

		case KKey_ESCAPE:
			if ((m_flags & KEngine::FLAG_MENU) && KKeyboard::matchModifiers(KKeyModifier_SHIFT)) {
				if (KInspector::isInstalled()) {
					KInspector::setVisible(!KInspector::isVisible());
				}
			}
			break;
		}
	}
	virtual void onWindowKeyUp(KKey key) override {
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
	#pragma endregion // KWindowCallback

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

KCoreWindow * KEngine::getWindow() {
	K__ASSERT_RETURN_ZERO(g_EngineInstance);
	return g_EngineInstance->m_Window;
}
KCoreKeyboard * KEngine::getKeyboard() {
	K__ASSERT_RETURN_ZERO(g_EngineInstance);
	return g_EngineInstance->m_Keyboard;
}
KCoreMouse * KEngine::getMouse() {
	K__ASSERT_RETURN_ZERO(g_EngineInstance);
	return g_EngineInstance->m_Mouse;
}
KCoreJoystick * KEngine::getJoystick() {
	K__ASSERT_RETURN_ZERO(g_EngineInstance);
	return g_EngineInstance->m_Joystick;
}
KIniFile * KEngine::getIniFile() {
	K__ASSERT_RETURN_ZERO(g_EngineInstance);
	return g_EngineInstance->m_IniFile;
}
KStorage * KEngine::getStorage() {
	K__ASSERT_RETURN_ZERO(g_EngineInstance);
	return g_EngineInstance->m_Storage;
}


} // namespace 