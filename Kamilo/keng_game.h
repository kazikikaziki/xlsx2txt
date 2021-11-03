/// @file
/// Copyright (c) 2020 Heliodor
/// This software is released under the MIT License.
/// http://opensource.org/licenses/mit-license.php

#pragma once
#include <assert.h>
#include <typeinfo>
#include <unordered_map>
#include <unordered_set>
#include "KRef.h"
#include "KNode.h"
#include "KInspector.h"
#include "KSig.h"

#if 1
#	define K_OPENMP 0
#else
#	ifdef _OPENMP
#		define K_OPENMP 1
#	else
#		define K_OPENMP 0
#	endif
#endif

#if K_OPENMP
#	define K_THREAD_SAFE 1
#else
#	define K_THREAD_SAFE 0
#endif

namespace Kamilo {


class KNode;
class KEntityFilterCallback;
class KScreen;
class KCoreWindow;
class KCoreKeyboard;
class KCoreMouse;
class KCoreJoystick;
class KStorage;

#pragma region signals

// no params
K_DECL_SIGNAL(K_SIG_WINDOW_WINDOW_CLOSING);

// "x": position x (int)
// "y": position y (int)
K_DECL_SIGNAL(K_SIG_WINDOW_WINDOW_MOVE);

// "x": client size x (int)
// "y": client size y (int)
K_DECL_SIGNAL(K_SIG_WINDOW_WINDOW_SIZE);

// "x": client x(int)
// "y": client y(int)
K_DECL_SIGNAL(K_SIG_WINDOW_MOUSE_ENTER);

// no params
K_DECL_SIGNAL(K_SIG_WINDOW_MOUSE_EXIT);

// "x": client x(int)
// "y": client y(int)
// "delta": wheel delta(int)
K_DECL_SIGNAL(K_SIG_WINDOW_MOUSE_WHEEL);

// "x": client x(int)
// "y": client y(int)
// "button": mouse button (int) 0=left, 1=right, 2=middle
K_DECL_SIGNAL(K_SIG_WINDOW_MOUSE_MOVE);

// "x": client x (int)
// "y": client y (int)
// "button": mouse button (int) 0=left, 1=right, 2=middle
K_DECL_SIGNAL(K_SIG_WINDOW_MOUSE_DOWN);

// "x": client x (int)
// "y": client y (int)
// "button": mouse button 0=left, 1=right, 2=middle
K_DECL_SIGNAL(K_SIG_WINDOW_MOUSE_UP);

// "key"  : KKey (int)
// "shift": with shift key (bool)
// "ctrl" : with ctrl key(bool)
// "alt"  : with alt key(bool)
K_DECL_SIGNAL(K_SIG_WINDOW_KEY_DOWN);
K_DECL_SIGNAL(K_SIG_WINDOW_KEY_UP);

// "chr": wchar_t (int)
K_DECL_SIGNAL(K_SIG_WINDOW_KEY_CHAR);

// "file_u8": file name in utf8 (string)
// "index": current file index (int)
// "total": num files (int)
K_DECL_SIGNAL(K_SIG_WINDOW_DROPFILE);

// no params
K_DECL_SIGNAL(K_SIG_WINDOW_VIDEO_DEVICE_LOST);
K_DECL_SIGNAL(K_SIG_WINDOW_VIDEO_DEVICE_RESET);



/// ゲームループが開始または再開された
// no params
K_DECL_SIGNAL(K_SIG_ENGINE_PLAY);

/// ステップ実行した
// no params
K_DECL_SIGNAL(K_SIG_ENGINE_PLAYPAUSE);

/// 一時停止した
// no params
K_DECL_SIGNAL(K_SIG_ENGINE_PAUSE);

/// フルスクリーンに変更した
/// @see KEngine::setFullscreen()
// no params
K_DECL_SIGNAL(K_SIG_ENGINE_FULLSCREEN);

/// ウィンドウモードに変更した
/// @see KEngine::restoreWindow()
// no params
K_DECL_SIGNAL(K_SIG_ENGINE_WINDOWED);

/// エンティティのインスペクターを描画しようとしている
/// このタイミングで ImGui を使うと
/// インスペクター上に独自のボタン類を追加することができる
/// "node": target node (KNode*)
K_DECL_SIGNAL(K_SIG_INSPECTOR_ENTITY);

/// KTextureBank::findTexture でテクスチャを取得しようとした時、 modifier が指定されていればこのコールバックが呼ばれる。
/// newtex には取得しようとしたテクスチャ basetex のコピーが入っているので、modifier の値に応じて newtex の画像を書き換える
///
/// "node"       : KNode*  KTextureBank::findTexture の data 引数に渡された値。（基本的には EID の値を入れることを想定している）
/// "newtex"     : KTEXID  動的に生成されたテクスチャ。サイズは basetex と同じで、中身は basetex のコピーで初期化されている
/// "basetexname": string  元になるテクスチャの名前
/// "basetex"    : KTEXID  元になるテクスチャ
/// "modifier"   : int     適用するエフェクトの種類。何番のときに何のエフェクトをかけるかはユーザー定義。（ただし0はエフェクトなしとして予約済み）
K_DECL_SIGNAL(K_SIG_TEXBANK_MODIFIER);

/// 新しいアニメクリップがセットされた
// "target"  : KNode*
// "clipname": string
K_DECL_SIGNAL(K_SIG_ANIMATION_NEWCLIP);

/// アニメ進行によって、ユーザー定義コマンドが登録されているフレームに達したときに呼ばれる
// "target"  : KNode*
// "cmd"     : string
// "val"     : string
// "clipname": string
// "clipptr" : KClipRes
K_DECL_SIGNAL(K_SIG_ANIMATION_COMMAND);

/// APPキーとして登録したボタンが押された時に呼ばれる
/// @see KInputMapAct::addButton
/// "button": string
K_DECL_SIGNAL(K_SIG_INPUT_APP_BUTTON);

// 衝突シグナル
// "hitbox1" : KHitbox* (as KNode*) 
// "hitbox2" : KHitbox* (as KNode*)
K_DECL_SIGNAL(K_SIG_HITBOX_ENTER);
K_DECL_SIGNAL(K_SIG_HITBOX_STAY);
K_DECL_SIGNAL(K_SIG_HITBOX_EXIT);


#pragma endregion // signals


class KIniFile: public KRef {
public:
	virtual void writeString(const std::string &key, const std::string &val) = 0;
	virtual void writeInt(const std::string &key, int x) = 0;
	virtual void writeInt2(const std::string &key, int x, int y) = 0;
	virtual void writeInt4(const std::string &key, int x, int y, int z, int w) = 0;
	virtual bool readString(const std::string &key, std::string *p_val) const = 0;
	virtual bool readInt(const std::string &key, int *x) const = 0;
	virtual bool readInt2(const std::string &key, int *x, int *y) const = 0;
	virtual bool readInt4(const std::string &key, int *x, int *y, int *z, int *w) const = 0;
	virtual void save() = 0;
};

KIniFile * createIniFile(const std::string &filename);





class KManager: public virtual KRef {
public:
	KManager() {}
	virtual ~KManager() {}

	virtual void on_manager_will_start() {}

	/// システムが開始したときに1度だけ呼ばれる
	///
	/// システムを KEngine に登録するのと同じタイミングで初期化しようとすると、
	/// 他のシステムに依存している場合に正しく初期化できない事がある（依存システムが既に登録済みとは限らないため）。
	/// 他のシステムを参照するような初期設定は on_manager_start で行うようにする
	virtual void on_manager_start() {}

	/// システムが終了したとき、ノードを削除する前に呼ばれる
	virtual void on_manager_will_end() {}
	
	/// システムが終了する時に1度だけ呼ばれる
	virtual void on_manager_end() {}

	/// エンティティ用のインスペクターを更新する時に呼ばれる
	virtual void on_manager_nodeinspector(KNode *node) {}

	/// エンティティの登録を解除する時に呼ばれる
	virtual void on_manager_detach(KNode *node) {}

	/// エンティティがアタッチされているか問い合わせあったときに呼ばれる
	virtual bool on_manager_isattached(KNode *node) { return false; }

	/// フレームの最初で呼ばれる
	/// @note KEngine::pause() でゲームループが停止している場合は呼ばれない
	virtual void on_manager_beginframe() {}

	/// フレームの更新時に呼ばれる
	/// @note KEngine::pause() でゲームループが停止している場合は呼ばれない
	virtual void on_manager_frame() {}

	/// フレームの更新時に、すべてのシステムの on_manager_frame() が実行された後で呼ばれる
	/// @note KEngine::pause() でゲームループが停止している場合は呼ばれない
	virtual void on_manager_frame2() {}

	/// フレームの最後に呼ばれる
	/// @note KEngine::pause() でゲームループが停止している場合は呼ばれない
	virtual void on_manager_endframe() {}

	/// 毎フレームの更新時に呼ばれる
	/// @note on_manager_frame() とは異なり KEngine::pause() でゲームループを停止している間にも呼ばれ続ける
	virtual void on_manager_appframe() {}

	/// ゲーム内の GUI を更新する時に呼ばれる。
	///
	/// GUI を利用したい場合はこのメソッド内で Dear ImGui を利用する。
	/// Dead ImGui のライブラリは既にソースツリーに同梱されているので、改めてダウンロード＆インストールする必要はない。
	/// ImGui の関数などは imgui/imgui.h を参照すること。使い方などは imgui/imgui.cpp に書いてある。
	///
	/// ※デフォルトではこのメソッドは呼ばれない。
	/// 　このメソッドが呼ばれるようにするには KEngine::addManager 後に setManagerGuiEnabled で true を設定する
	///
	/// @see KImGui_.h
	/// @see https://github.com/ocornut/imgui
	virtual void on_manager_gui() {}

	/// ゲームウィンドウに表示するべきものをすべて描画した後に呼ばれる。
	/// ゲームウィンドウの最前面に描画したい場合は、このメソッド内で KVideo を利用する
	///
	/// ※デフォルトではこのメソッドは呼ばれない。
	/// 　このメソッドが呼ばれるようにするには KEngine::addManager 後に setManagerRenderTopEnabled で true を設定する
	///
	virtual void on_manager_rendertop() {}

	/// ゲーム内オブジェクトを描画する
	///
	/// ※デフォルトではこのメソッドは呼ばれない。
	/// 　このメソッドが呼ばれるようにするには KEngine::addManager 後に setManagerRenderWorldEnabled で true を設定する
	///
	virtual void on_manager_get_renderworld_nodes(KNode *cameranode, KNode *start, KEntityFilterCallback *filter, KNodeArray &result) {}
	virtual void on_manager_renderworld(KNode *cameranode, const KNodeArray &render_nodes) {}

	/// デバッグ情報を描画する
	///
	/// ※デフォルトではこのメソッドは呼ばれない。
	/// 　このメソッドが呼ばれるようにするには KEngine::addManager 後に setManagerRenderDebugEnabled で true を設定する
	///
	virtual void on_manager_renderdebug(KNode *cameranode, KNode *start, KEntityFilterCallback *filter) {}

	virtual void on_manager_signal(KSig &sig) {}

	bool isAttached(KNode *node) {
		return on_manager_isattached(node);
	}
};













struct KEngineDef {
	KEngineDef() {
		use_inspector = true;
		use_console = true;
		fps = 60;
		resolution_x = 640;
		resolution_y = 480;
		callback = nullptr;
		storage = nullptr;
		ini_filename = "user.ini";
	}
	bool use_inspector;
	bool use_console;
	int fps;
	int resolution_x;
	int resolution_y;
	std::string ini_filename;
	KManager *callback;
	KStorage *storage;
};

class KEngine {
public:
	static const int MAXPASS = 4;

	static bool create(const KEngineDef *def);
	static void destroy();
	static std::string passImageName(int index); ///< 描画パスごとのレンダーテクスチャ名を得る。実際のテクスチャは KBank::getTextureBank()->findTexture で取得できる
	static void removeInvalidatedIds(); ///< 無効化マークがつけられたノードを実際に削除する
	static void removeAllIds(); ///< 無効化マークの有無に関係なく、現在有効な全てのノードを削除する

	/// システムを追加する
	///
	/// これ以降、システムの各メソッドが呼ばれるようになる。
	/// 最初に呼ばれるのは KManager::on_manager_start である
	static void addManager(KManager *s);
	static void setManagerGuiEnabled(KManager *mgr, bool value);
	static void setManagerRenderWorldEnabled(KManager *mgr, bool value);
	static void setManagerRenderDebugEnabled(KManager *mgr, bool value);
	static void setManagerRenderTopEnabled(KManager *mgr, bool value);
	static int getManagerCount(); ///< システムの数を返す
	static KManager * getManager(int index); ///< インデックスを指定してシステムを取得する

	static void broadcastSignal(KSig &sig);
	static void broadcastSignalAsync(KSig &sig);

	static void addInspectorCallback(KInspectorCallback *cb, const char *label=""); ///< インスペクター項目を登録する
	static void removeInspectorCallback(KInspectorCallback *cb); ///< コールバックを解除する

	static void run(); ///< ゲームループを実行する
	static bool isPaused(); ///< ポーズ中かどうか
	static void play(); ///< ポーズを解除する
	static void playStep(); ///< ループを１フレームだけ進める
	static void pause(); ///< ポーズする（更新は停止するが、描画は継続する）
	static void quit(); ///< ループを終了する
	static void setFps(int fps, int skip); ///< 更新サイクルなどを指定する

	static KCoreWindow * getWindow();
	static KCoreKeyboard * getKeyboard();
	static KCoreMouse * getMouse();
	static KCoreJoystick * getJoystick();
	static KIniFile * getIniFile();
	static KStorage * getStorage();

	enum WindowMode {
		/// 通常ウィンドウ
		/// 最大化または最小化されたウィンドウを通常状態に戻す。
		/// フルスクリーン状態だった場合は、ウィンドウモードに戻る
		WND_WINDOWED,

		/// 最大化ウィンドウ
		/// @see ST_WINDOW_IS_MAXIMIZED
		WND_MAXIMIZED,

		/// フルスクリーンモード（ディスプレイの解像度を変更する）
		///
		/// フルスクリーンを解除する時は restoreWindow() を使う
		WND_FULLSCREEN,

		/// 疑似フルスクリーンモード
		///
		/// ウィンドウを境界線なし最大化＆最前面状態にする。
		/// （ビデオモードは Windowed のままで、ウィンドウだけフルスクリーンにする。ディスプレイの解像度は変わらない）
		/// フルスクリーンを解除する時は restoreWindow() を使う
		/// @note 本当のフルスクリーンではないので、 KVideo での設定はあくまでも「ウィンドウモード」のままである
		WND_WINDOWED_FULLSCREEN,
	};
	static void setWindowMode(WindowMode mode);
	static void resizeWindow(int w, int h); ///< ウィンドウのサイズを変更する
	static void moveWindow(int x, int y); ///< ウィンドウ位置を設定する

	/// ゲームエンジンあるいは実行環境の情報を得るための識別子。
	/// @see KEngine::getStatus()
	enum Status {
		ST_NONE,
		ST_FRAME_COUNT_APP,  ///< アプリケーションレベルでの実行フレーム数
		ST_FRAME_COUNT_GAME, ///< ゲームレベルでの実行フレーム数
		ST_TIME_MSEC,        ///< ゲームエンジンを開始してからの経過ミリ秒
		ST_FPS_UPDATE,       ///< 更新関数のFPS値
		ST_FPS_RENDER,       ///< 描画関数のFPS値
		ST_FPS_REQUIRED,     ///< 要求されたFPS値 @see setFps
		ST_KEYBOARD_BLOCKED, /// キーボードの入力をゲーム側が無視するべきか
		
		/// 指定されたFPSで処理できなかった場合に、1秒間でスキップできる描画フレームの最大数。
		/// 0 ならスキップなし
		/// 1以上の値 n を指定すると、1秒あたり最大 n フレームまでスキップする
		ST_MAX_SKIP_FRAMES,

		ST_WINDOW_POSITION_X,    ///< ウィンドウX座標（ウィンドウのいちばん左上の座標）@see ST_SCREEN_AT_CLIENT_ORIGIN_X @see moveWindow()
		ST_WINDOW_POSITION_Y,    ///< ウィンドウY座標（ウィンドウのいちばん左上の座標）@see ST_SCREEN_AT_CLIENT_ORIGIN_Y @see moveWindow()
		ST_WINDOW_CLIENT_SIZE_W, ///< ウィンドウのクライアント幅 @see resizeWindow()
		ST_WINDOW_CLIENT_SIZE_H, ///< ウィンドウのクライアント高さ @see resizeWindow()
		ST_WINDOW_IS_MAXIMIZED,  ///< ウィンドウが最大化されている @see maximizeWindow()
		ST_WINDOW_IS_ICONIFIED,  ///< ウィンドウが最小化されている
		ST_WINDOW_FOCUSED,       ///< ウィンドウが入力フォーカスを持っているかどうか

		ST_VIDEO_IS_FULLSCREEN,          ///< フルスクリーンモードかどうか @see setFullscreen()
		ST_VIDEO_IS_WINDOWED_FULLSCREEN, ///< 疑似フルスクリーンモードかどうか（ウィンドウ境界線を消して最大化かつ最前面にしている） @see setWindowedFullscreen()
		ST_VIDEO_IS_WINDOWED,            ///< ウィンドウモードかどうか @see restoreWindow()

		ST_SCREEN_W, ///< 現在のモニタ（ゲームウィンドウを含んでいるモニタ）のX解像度
		ST_SCREEN_H, ///< 現在のモニタ（ゲームウィンドウを含んでいるモニタ）のY解像度
	};
	static int getStatus(Status s); ///< 実行環境の情報を得る

	enum Flag_ {
		FLAG_PAUSE  = 1,
		FLAG_MENU   = 2,
		FLAG_NOWAIT = 4,
	};
	typedef int Flags;
	static void setFlags(Flags flags);
	static Flags getFlags();

	static int command(const char *s, void *n); ///< デバッグ用
};



} // namespace
