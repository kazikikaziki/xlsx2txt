/// @file
/// Copyright (c) 2020 Heliodor
/// This software is released under the MIT License.
/// http://opensource.org/licenses/mit-license.php

#pragma once
#include "KMath.h"
#include "KVideo.h"

namespace Kamilo {

class KGizmo;
class KManager;
class KNode;
class KTextureBank;
class KPath;

class KScreen {
public:
	static void install(int game_w, int game_h, int gui_w, int gui_h);
	static void uninstall();
	static void startVideo(KTextureBank *texbank);
	static void endVideo();
	static void getGameSize(int *w, int *h);
	static void setGameSize(int w, int h);
	static void getGuiSize(int *w, int *h);
	static void setGuiSize(int w, int h);
	static KGizmo * getGizmo();
	static KVec3 windowClientToScreenPoint(const KVec3 &p);
	static KRecti getGameViewportRect();
	static void postExportScreenTexture(const KPath &filename);
	static void render();
	static void render_debug_gui(KNode *node);
	static void set_call_gui(KManager *mgr, bool value);
	static void set_call_renderworld(KManager *mgr, bool value);
	static void set_call_renderdebug(KManager *mgr, bool value);
	static void set_call_rendertop(KManager *mgr, bool value);
	static bool command(const char *s, void *n, int *retval);

	/// 描画パス単位での描画結果を格納しているテクスチャを得る
	/// カメラ単位での描画結果を取得する場合は KCamera::getRenderTarget を使う
	static KTEXID getPerPassRenderTarget(int pass);

	static void setPerPassRenderTargetSize(int pass, int w, int h);

	/// 単位座標系（画面中央原点、-1.0～1.0、Y軸上向き）の座標を、
	/// ウィンドウクライアント座標系（左上原点、y軸下向き）で表す。
	/// ただし z 座標は加工しない
	static KVec3 getWindowCoordFromNormalizedScreenCoord(const KVec3 &p);

	/// ウィンドウクライアント座標系（左上原点、y軸下向き）の座標を、
	/// 単位座標系（画面中央原点、-1.0～1.0、Y軸上向き）で表す。
	/// ただし z 座標は加工しない
	static KVec3 getNormalizedScreenCoordFromWindowCoord(const KVec3 &p);
};


} // namespace

