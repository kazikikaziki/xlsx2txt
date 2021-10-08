#include "KScreen.h"

#include <algorithm>
#include "KImGui.h"
#include "KInspector.h"
#include "KRes.h"
#include "KCamera.h"
#include "keng_game.h"
#include "KSig.h"
#include "KAction.h"
#include "KInternal.h"

namespace Kamilo {



static const char *DEFAULT_SNAPSHOT_FILENAME = "_ScreenTexture.png";
static const float AXIS_LEN = 32;

// 不透明部分と透明部分の境界に輪郭線を描画するためのシェーダー
#pragma region g_outline_hlsl
static const char *g_outline_hlsl =
	"uniform float4x4 mo__MatrixView;											\n"
	"uniform float4x4 mo__MatrixProj;											\n"
	"uniform texture  mo__Texture;												\n"
	"uniform float4   mo__Color;												\n"
	"uniform float2   mo__TextureSize;											\n"
	"sampler samMain = sampler_state {											\n"
	"	Texture = <mo__Texture>;												\n"
	"	MinFilter = Point;														\n"
	"	MagFilter = Point;														\n"
	"	MipFilter = Point;														\n"
	"	AddressU  = Clamp;														\n"
	"	AddressV  = Clamp;														\n"
	"};																			\n"
	"struct RET {																\n"
	"	float4 pos: POSITION;													\n"
	"	float2 uv: TEXCOORD0;													\n"
	"	float4 color: COLOR0;													\n"
	"};																			\n"
	"RET vs(float4 pos: POSITION, float2 uv: TEXCOORD0, float4 color: COLOR0) {	\n"
	"	float4x4 mat = mul(mo__MatrixView, mo__MatrixProj);						\n"
	"	RET ret = (RET)0;														\n"
	"	ret.pos = mul(pos, mat);												\n"
	"	ret.uv = uv;															\n"
	"	ret.color = color;														\n"
	"	return ret;																\n"
	"}																			\n"
	"float4 ps(float2 uv: TEXCOORD0): COLOR {									\n"
	"	float4 color = {0, 0, 0, 0};											\n"
	"	float dx = 1.0 / mo__TextureSize.x;										\n"
	"	float dy = 1.0 / mo__TextureSize.y;										\n"
	"	float a = tex2D(samMain, uv).a;											\n"
	"	if (a == 0) {															\n"
	"		float b = tex2D(samMain, uv + float2( dx, 0)).a;					\n"
	"		float c = tex2D(samMain, uv + float2(-dx, 0)).a;					\n"
	"		float d = tex2D(samMain, uv + float2(0, -dy)).a;					\n"
	"		float e = tex2D(samMain, uv + float2(0,  dx)).a;					\n"
	"		if (b+c+d+e > 0) {													\n"
	"			color = float4(1, 0, 1, 1);										\n"
	"		}																	\n"
	"	}																		\n"
	"	return color;															\n"
	"}																			\n"
	"technique main {															\n"
	"	pass P0 {																\n"
	"		VertexShader = compile vs_2_0 vs();									\n"
	"		PixelShader  = compile ps_2_0 ps();									\n"
	"	}																		\n"
	"}																			\n"
	;
#pragma endregion // g_outline_hlsl

class CRenderFilter: public KEntityFilterCallback {
public:
	bool m_selections_only; // インスペクターで選択されているものだけを描画する

	CRenderFilter() {
		m_selections_only = false;
	}
	virtual void on_entityfilter_check(KNode *node, bool *skip) override {
		if (KInspector::isInstalled()) {
			// 非表示設定されているノードを除外
			if (!KInspector::isEntityVisible(node)) {
				*skip = true;
				return;
			}
			// 選択されていないノードを除外
			if (m_selections_only) {
				if (!KInspector::entityShouldRednerAsSelected(node)) {
					*skip = true;
					return;
				}
			}
		}
	}
};

// 強調オブジェクトの輪郭
struct OUTLINE {
	KTEXID m_seltex;
	KSHADERID m_shader;
	KMaterial m_material;

	OUTLINE() {
		m_seltex = nullptr;
		m_shader = nullptr;
	}
	void init(int w, int h) {
		m_seltex = KVideo::createRenderTexture(w, h);

		// 輪郭描画用のマテリアルを用意
		m_shader = KVideo::createShaderFromHLSL(g_outline_hlsl, "Engine_OutlineFx");
		m_material.blend = KBlend_ALPHA;
		m_material.shader = m_shader;
		m_material.color = KColor::WHITE;
	}
	void shutdown() {
		KVideo::deleteShader(m_shader);
		KVideo::deleteTexture(m_seltex);
		m_shader = nullptr;
		m_seltex = nullptr;
	}
};

class CCameraSortPred {
public:
	bool operator()(KNode *node1, KNode *node2) const {
		int layer1 = node1->getLayerInTree();
		int layer2 = node2->getLayerInTree();
		return layer1 < layer2;
	}
};

#pragma region CScreenImpl
template <class T> void _EraseT(std::vector<T> &list, T value) {
	auto it = std::find(list.begin(), list.end(), value);
	if (it != list.end()) {
		list.erase(it);
	}
}
class CScreenImpl: public KInspectorCallback {
	static const int MAX_PASS = 4;
	KTEXID m_pass_tex[MAX_PASS]; // パスごとの描画結果
	float m_pass_alpha_filling[MAX_PASS]; // パスごとの描画結果に対してアルファを均一にする
	KGizmo *m_gizmo;
	OUTLINE m_debug_outline;
	OUTLINE m_user_outline;
	KTEXID m_tmp_target; // 画面の一時描画用
	KNodeArray m_tmp_nodes;

	// ゲーム画面の解像度
	int m_game_w;
	int m_game_h;

	// Gui 画面の解像度
	int m_gui_w;
	int m_gui_h;

	KPath m_export_screen_filename;
	bool m_export_screen_texture;
	bool m_keep_aspect;
	bool m_trace;
public:
	KColor m_bgcolor; // ゲーム画面のクリア色（ノードを全く描画しなかった部分の色になる）
	KColor m_margin_color; // ウィンドウとゲーム画面の比率があっていない時の余白の色
	bool m_alpha_filling; // レンダーターゲットの全ピクセルのアルファを 1.0 で塗りつぶす
	bool m_show_debug;
	bool m_show_debug2;
	bool m_user_outline_enabled;
	bool m_use_filter;
	KEntityFilterCallback *m_user_outline_filter;
	KTextureBank *m_texbank;
	std::vector<KManager *> m_mgr_call_gui;         // on_manager_gui を呼ぶ
	std::vector<KManager *> m_mgr_call_renderworld; // on_manager_renderworld を呼ぶ
	std::vector<KManager *> m_mgr_call_renderdebug; // on_manager_renderdebug を呼ぶ
	std::vector<KManager *> m_mgr_call_rendertop; // on_manager_rendertop を呼ぶ

	CScreenImpl() {
		zero_clear();
	}
	virtual ~CScreenImpl() {
		endVideo();
	}
	void zero_clear() {
		m_user_outline_enabled = false;
		m_user_outline_filter = nullptr;
		m_show_debug = false;
		m_show_debug2 = false;
		m_keep_aspect = false;
		m_trace = false;
		m_tmp_target = nullptr;
		m_gizmo = nullptr;
		m_game_w = 0;
		m_game_h = 0;
		m_gui_w = 0;
		m_gui_h = 0;
		m_export_screen_texture = 0;
		m_texbank = nullptr;
		m_alpha_filling = true;
		m_use_filter = false;
		for (int i=0; i<MAX_PASS; i++) {
			m_pass_tex[i] = nullptr;
			m_pass_alpha_filling[i] = false;
		}
	}
	void init(int game_w, int game_h, int gui_w, int gui_h) {
		K__ASSERT(game_w > 0);
		K__ASSERT(game_h > 0);
		K__ASSERT(gui_w > 0);
		K__ASSERT(gui_h > 0);
		zero_clear();
		m_game_w = game_w;
		m_game_h = game_h;
		m_gui_w = gui_w;
		m_gui_h = gui_h;
		m_keep_aspect = true;
		m_gizmo = new KGizmo();
		m_tmp_target = nullptr;
		m_bgcolor = KColor::ZERO;
		m_margin_color = KColor::BLACK;
		m_alpha_filling = true;
		for (int i=0; i<MAX_PASS; i++) {
			m_pass_tex[i] = nullptr;
			m_pass_alpha_filling[i] = false;
		}
	}

	// KInspectorCallback
	virtual void onInspectorGui() {
		ImGui::Checkbox("Show node debug", &m_show_debug);
		if (ImGui::IsItemHovered()) { ImGui::SetTooltip(u8"ノードによるデバッグ描画"); }

		ImGui::Checkbox("Show manager debug", &m_show_debug2);
		if (ImGui::IsItemHovered()) { ImGui::SetTooltip(u8"ノードマネージャによるデバッグ描画"); }

		ImGui::ColorEdit4("BgColor", m_bgcolor.floats());
		if (ImGui::IsItemHovered()) { ImGui::SetTooltip(u8"ゲーム画面のクリア色（ノードを全く描画しなかった部分の色"); }

		ImGui::ColorEdit4("MarginColor", m_margin_color.floats());
		if (ImGui::IsItemHovered()) { ImGui::SetTooltip(u8"ウィンドウとゲーム画面の縦横比率が一致しない場合の余白の色"); }
	}

	bool command(const char *s, void *n, int *retval) {
		if (K_StrEq(s, "set_filter")) {
			m_use_filter = (int)n != 0;
			return true;
		}
		if (K_StrEq(s, "set_debugdraw")) {
			m_show_debug = (int)n != 0;
			return true;
		}
		if (K_StrEq(s, "get_debugdraw")) {
			*retval = m_show_debug ? 1 : 0;
			return true;
		}
		if (K_StrEq(s, "set_debugdraw2")) {
			m_show_debug2 = (int)n != 0;
			return true;
		}
		if (K_StrEq(s, "get_debugdraw2")) {
			*retval = m_show_debug2 ? 1 : 0;
			return true;
		}
		if (K_StrEq(s, "set_basesize")) {
			int *vals = (int*)n;
			setGameSize(vals[0], vals[1]);
			return true;
		}
		if (K_StrEq(s, "set_bgcolor")) {
			float *rgba = (float*)n;
			m_bgcolor.r = rgba[0];
			m_bgcolor.g = rgba[1];
			m_bgcolor.b = rgba[2];
			m_bgcolor.a = rgba[3];
			return true;
		}
		if (K_StrEq(s, "set_alpha_filling")) {
			int *vals = (int*)n;
			m_alpha_filling = vals[0]!=0;
			return true;
		}
		if (K_StrEq(s, "get_bgcolor")) {
			float *rgba = (float*)n;
			rgba[0] = m_bgcolor.r;
			rgba[1] = m_bgcolor.g;
			rgba[2] = m_bgcolor.b;
			rgba[3] = m_bgcolor.a;
			*retval = (int)m_bgcolor.toColor32().toUInt32();
			return true;
		}
		if (K_StrEq(s, "set_outline_enabled")) {
			int *vals = (int*)n;
			m_user_outline_enabled = vals[0]!=0;
			return true;
		}
		if (K_StrEq(s, "get_outline_enabled")) {
			*retval = m_user_outline_enabled ? 1: 0;
			return true;
		}
		if (K_StrEq(s, "set_outline_filter_callback")) {
			m_user_outline_filter = reinterpret_cast<KEntityFilterCallback*>(n);
			return true;
		}
		return false;
	}
	void set_call_gui(KManager *mgr, bool value) {
		_EraseT(m_mgr_call_gui, mgr);
		if (value) {
			m_mgr_call_gui.push_back(mgr);
		}
	}
	void set_call_renderworld(KManager *mgr, bool value) {
		_EraseT(m_mgr_call_renderworld, mgr);
		if (value) {
			m_mgr_call_renderworld.push_back(mgr);
		}
	}
	void set_call_renderdebug(KManager *mgr, bool value) {
		_EraseT(m_mgr_call_renderdebug, mgr);
		if (value) {
			m_mgr_call_renderdebug.push_back(mgr);
		}
	}
	void set_call_rendertop(KManager *mgr, bool value) {
		_EraseT(m_mgr_call_rendertop, mgr);
		if (value) {
			m_mgr_call_rendertop.push_back(mgr);
		}
	}
	KGizmo * getGizmo() {
		return m_gizmo;
	}
	KTEXID getPerPassRenderTarget(int pass) {
		if (0 <= pass && pass < MAX_PASS) {
			KPath name = KEngine::passImageName(pass);
			return m_texbank->findTexture(name);
		}
		return nullptr;
	}
	void setPerPassRenderTargetSize(int pass, int w, int h) {
		if (0 <= pass && pass < MAX_PASS) {
			KPath name = KEngine::passImageName(pass);
			m_pass_tex[pass] = m_texbank->addRenderTexture(name, w, h, KTextureBank::F_OVERWRITE_SIZE_NOT_MATCHED); // name を変えずに中身だけ作り直す。KTEXID は変わるので注意
		}
	}
	void setPerPassAlphaFillingEnable(int pass, bool value) {
		if (0 <= pass && pass < MAX_PASS) {
			m_pass_alpha_filling[pass] = value;
		}
	}
	void startVideo(KTextureBank *texbank) {
		m_texbank = texbank;
		for (int i=0; i<MAX_PASS; i++) {
			KPath name = KEngine::passImageName(i);
			m_pass_tex[i] = m_texbank->addRenderTexture(name, m_game_w, m_game_h, KTextureBank::F_PROTECT);
		}
		m_debug_outline.init(m_game_w, m_game_h);
		m_user_outline.init(m_game_w, m_game_h);
	}
	void endVideo() {
		m_mgr_call_gui.clear();
		m_mgr_call_renderworld.clear();
		m_mgr_call_renderdebug.clear();
		m_mgr_call_rendertop.clear();
		if (m_gizmo) {
			delete m_gizmo;
			m_gizmo = nullptr;
		}
		m_debug_outline.shutdown();
		m_user_outline.shutdown();
		zero_clear();
	}
	void postExportScreenTexture(const KPath &filename) {
		m_export_screen_texture = true;
		m_export_screen_filename = filename;
	}
	void render_debug_gui(KNode *node) {
		if (node->getVisible()) {
			node->on_node_gui();

			KAction *act = node->getAction();
			if (act) {
				act->onDebugGUI();
			}
			for (int i=0; i<node->getChildCount(); i++) {
				render_debug_gui(node->getChild(i));
			}
		}
	}
	void render() {
		KVideo::clearColor(m_margin_color); // ウィンドウとゲーム画面の比率があっていない時の余白の色
		KVideo::clearDepth(1.0f);
		KVideo::clearStencil(0);

		// ゲーム画面を作成
		// ここで m_pass_tex[] にゲーム画面が描画される
		render_game();

		KRecti gamerect = getGameViewportRect();
		KVideo::setViewport(gamerect.x, gamerect.y, gamerect.w, gamerect.h);

		// 描画済みのゲーム画面を重ねる
		// ここで使うのはパス[0]のみ。ほかのパスで描画した画面は
		// スプライトやテクスチャとして明示的に表示しない限り画面には表れない（ただの裏バッファ）
		if (m_use_filter) {
			KMaterial m;
			m.filter = KFilter_LINEAR;
			KVideoUtils::blit(nullptr, KVideo::findTexture(m_pass_tex[0]), &m);
		} else {
			KVideoUtils::blit(nullptr, KVideo::findTexture(m_pass_tex[0]), nullptr);
		}
		// GUIを重ねる
		if (KImGui::IsActive()) {
			// ビューポートがウィンドウいっぱいになるようにリセットする
			KVideo::setViewport(0, 0, m_gui_w, m_gui_h);

			KImGui::BeginRender();

			// ノードGUI
			render_debug_gui(KNodeTree::getRoot());

			// マネージャGUI
			for (int i=0; i<(int)m_mgr_call_gui.size(); i++) {
				KManager *mgr = m_mgr_call_gui[i];
				mgr->on_manager_gui();
			}

			if (KInspector::isInstalled()) {
				KInspector::render(m_pass_tex[0]);
			}
			KImGui::EndRender();
		}

		// スクリーンショットが設定されていれば、画面のコピーを取る
		if (m_export_screen_texture) {
			KPath filename = m_export_screen_filename;
			if (filename.empty()) {
				filename = DEFAULT_SNAPSHOT_FILENAME;
			}
			save_texture(m_pass_tex[0], filename.u8());
			KLog::printInfo("Snapshot saved: %s", filename.getFullPath().u8());
			m_export_screen_texture = false;
		}
	}
	void getGameSize(int *w, int *h) const {
		*w = m_game_w;
		*h = m_game_h;
	}
	void setGameSize(int w, int h) {
		K__ASSERT(w >= 0 && h >= 0);
		m_game_w = w;
		m_game_h = h;
	}
	void getGuiSize(int *w, int *h) const {
		*w = m_gui_w;
		*h = m_gui_h;
	}
	void setGuiSize(int w, int h) {
		K__ASSERT(w >= 0 && h >= 0);
		m_gui_w = w;
		m_gui_h = h;
	}
	KVec3 windowClientToScreenPoint(const KVec3 &p) const {
		KRecti vp = getGameViewportRect();
		KVec3 out;
		float nx = KMath::linearStepUnclamped((float)vp.x, (float)(vp.x + vp.w), p.x);
		float ny = KMath::linearStepUnclamped((float)vp.y, (float)(vp.y + vp.h), p.y);
		out.x = KMath::lerpUnclamped(-1.0f,  1.0f, nx);
		out.y = KMath::lerpUnclamped( 1.0f, -1.0f, ny); // Ｙ軸反転。符号に注意
		out.z = p.z;
		return out;
	}
	KRecti getGameViewportRect() const {
		KRecti rect;
		int dst_w, dst_h;
		if (m_keep_aspect) {
			float gui_aspect = (float)m_gui_w / m_gui_h;
			float game_aspect = (float)m_game_w / m_game_h;
			if (gui_aspect < game_aspect) {
				// 縦長の画面
				// 横をGUI領域に合わせる
				dst_w = m_gui_w;
				dst_h = m_game_h * m_gui_w / m_game_w;
			} else {
				// 横長の画面
				// 縦をウィンドウサイズに合わせる
				dst_w = m_game_w * m_gui_h / m_game_h;
				dst_h = m_gui_h;
			}
		} else {
			dst_w = m_gui_w;
			dst_h = m_gui_h;
		}
		rect.x = (m_gui_w - dst_w) / 2;
		rect.y = (m_gui_h - dst_h) / 2;
		rect.w = dst_w;
		rect.h = dst_h;
		return rect;
	}

private:
	KNodeArray m_tmp_cameralist;
	std::vector<KNodeArray> m_tmp_pass_cameras;

	// 選択オブジェクトの座標軸を描画
	// ※既にカメラリストが m_tmp_pass_cameras に入っているものとする
	void render_selection_axis() {
		KNode *node = KInspector::getSelectedEntity(0);
		if (node == nullptr) return;
		for (int i=0; i<(int)m_tmp_pass_cameras.size(); i++) {
			const KNodeArray &list = m_tmp_pass_cameras[i];
			if (list.size() > 0) {
				for (int c=0; c<(int)list.size(); c++) {
					KNode *camera = list[c];
					if (camera->getLayerInTree() == node->getLayerInTree()) { // カメラのレイヤーと一致しないと描画対象にならない
						KMatrix4 cam_tr = camera->getWorld2LocalMatrix();
						KMatrix4 proj = KCamera::of(camera)->getProjectionMatrix();
						KMatrix4 local2world = node->getLocal2WorldMatrix();
						KMatrix4 m = local2world * cam_tr;
						m_gizmo->newFrame();
						m_gizmo->addAxis(m, KVec3(), AXIS_LEN);
						m_gizmo->render(proj);
						break;
					}
				}
			}
		}
	}

	// ゲーム画面を描画する
	void render_game() {
		// 描画する順番でカメラを得る
		KNodeArray nodes;
		KCamera::getCameraNodes(nodes);

		m_tmp_cameralist.clear();
		m_tmp_cameralist.reserve(nodes.size());
		for (auto it=nodes.begin(); it!=nodes.end(); ++it) {
			KNode *camera = *it;
			if (camera && camera->getEnableInTree()) {
				m_tmp_cameralist.push_back(camera);
			}
		}
		std::sort(m_tmp_cameralist.begin(), m_tmp_cameralist.end(), CCameraSortPred());
		
		// 開始ノード。このノードに属するツリーのみが描画対象となる
		KNode *start = KNodeTree::getRoot();

		// 必要なパス数を得る
		int maxpass = 0;
		for (auto it=m_tmp_cameralist.begin(); it!=m_tmp_cameralist.end(); ++it) {
			KNode *camera = *it;
			int pass = KCamera::of(camera)->getPass();
			maxpass = KMath::max(maxpass, pass);
		}

		// パスごとに使うカメラを得る
		m_tmp_pass_cameras.clear();
		m_tmp_pass_cameras.resize(maxpass + 1);
		for (auto it=m_tmp_cameralist.begin(); it!=m_tmp_cameralist.end(); ++it) {
			KNode *camera = *it;
			int pass = KCamera::of(camera)->getPass();
			if (0 <= pass && pass < m_tmp_pass_cameras.size()) {
				m_tmp_pass_cameras[pass].push_back(camera);
			}
		}

		// パスごとにゲーム画面を描画
		if (1) {
			CRenderFilter filter;
			filter.m_selections_only = false;
			for (int i=0; i<(int)m_tmp_pass_cameras.size(); i++) {
				const KNodeArray &list = m_tmp_pass_cameras[i];
				if (list.size() > 0) {
					render_world(list, start, &filter, m_pass_tex[i], false);
				}
				if (m_pass_alpha_filling[i]) { // ダメ。塗りつぶしたいパスと層でないパスがあるので、パスごとに塗りつぶすかどうかを設定できないといけない
					KVideo::fill(m_pass_tex[i], KColor::WHITE, KColorChannel_A); // アルファを 1.0 で塗りつぶす
				}
			}
		}

		// パス[0]はデフォルトで表示される画面なので、これに対して追加の描画を行う
		KTexture *target = KVideo::findTexture(m_pass_tex[0]);

		// 強調オブジェクトの輪郭（インスペクター用）
		if (KInspector::isInstalled()) {
			if (KInspector::isVisible() && KInspector::isHighlightSelectionsEnabled()) {
				// インスペクターで選択されているオブジェクトだけ描画
				CRenderFilter filter;
				filter.m_selections_only = true;
				for (int i=0; i<(int)m_tmp_pass_cameras.size(); i++) {
					const KNodeArray &list = m_tmp_pass_cameras[i];
					if (list.size() > 0) {
						render_world(list, start, &filter, m_debug_outline.m_seltex, true);
					}
				}
				// 描画結果の輪郭線を重ねる
				KVideoUtils::blit(target, KVideo::findTexture(m_debug_outline.m_seltex), &m_debug_outline.m_material);
			}
		}

		// 強調オブジェクトの輪郭（ユーザー利用）
		if (m_user_outline_enabled) {
			// インスペクターで選択されているオブジェクトだけ描画
			for (int i=0; i<(int)m_tmp_pass_cameras.size(); i++) {
				const KNodeArray &list = m_tmp_pass_cameras[i];
				if (list.size() > 0) {
					render_world(list, start, m_user_outline_filter, m_user_outline.m_seltex, true);
				}
			}
			// 描画結果の輪郭線を重ねる
			KVideoUtils::blit(target, KVideo::findTexture(m_user_outline.m_seltex), &m_user_outline.m_material);
		}

		// 強調オブジェクトの座標軸
		if (KInspector::isInstalled()) {
			if (KInspector::isVisible() && KInspector::getSelectedEntityCount() > 0 && KInspector::isAxisSelectionsEnabled()) {
				// インスペクターで選択されているオブジェクトだけ描画
				KVideo::pushRenderTarget(target->getId());
				render_selection_axis();
				KVideo::popRenderTarget();
			}
		}

		// ノードごとのデバッグ情報
		if (m_show_debug) {
			CRenderFilter filter;
			filter.m_selections_only = false;
			render_debug(m_tmp_cameralist, start, &filter, target ? target->getId() : nullptr);
		}

		// マネージャごとのデバッグ情報
		if (m_show_debug2 && m_mgr_call_renderdebug.size() > 0) {
			KVideo::pushRenderTarget(target->getId());
			CRenderFilter filter;
			filter.m_selections_only = false;
			{
				for (auto it=m_tmp_cameralist.begin(); it!=m_tmp_cameralist.end(); ++it) {
					KNode *cameranode = *it;
					{
						// 描画リストはカメラごとにリセットする。
						// そうしないと一つのギズモが複数のカメラで描画されてしまう
						if (m_gizmo) {
							m_gizmo->newFrame();
						}

						// ビューポートを設定
						KVideo::setViewport(0, 0, m_game_w, m_game_h);
	
						// 描画
						for (int i=0; i<(int)m_mgr_call_renderdebug.size(); i++) {
							KManager *mgr = m_mgr_call_renderdebug[i];
							mgr->on_manager_renderdebug(cameranode, start, &filter);
						}

						if (m_gizmo) {
							const KMatrix4 &matrix = KCamera::of(cameranode)->getProjectionMatrix();
							m_gizmo->render(matrix);
						}
					}
				}
			}
			KVideo::popRenderTarget();
		}

		// 最前面情報
		if (m_mgr_call_rendertop.size() > 0) {
			KVideo::pushRenderTarget(target->getId());
			for (int i=0; i<(int)m_mgr_call_rendertop.size(); i++) {
				KManager *mgr = m_mgr_call_rendertop[i];
				mgr->on_manager_rendertop();
			}
			KVideo::popRenderTarget();
		}

		// レンダーターゲットのアルファ値が不安定なので、
		// 完全不透明になるように 1.0 で上書きする
		if (m_alpha_filling) {
			KVideo::fill(target->getId(), KColor::BLACK, KColorChannel_A);
		}
	}

	// ゲームワールドを描画する
	// viewlist: ビューを描画順に並べた配列
	// filter: オブジェクトを描画するかどうかを判定するためのフィルター関数。使わないなら nullptr
	// target: 描画先のレンダーターゲットテクスチャ
	// isdebug: ゲームとは直接関係のない目的で描画をする場合に true を設定する（デバッグ用、スナップショット用など）。一部の処理が省略される
	void render_world(const KNodeArray &cameralist, KNode *start, KEntityFilterCallback *filter, KTEXID targetid, int isdebug) {
		bool need_newtex = true;

		// 作業用ターゲットの有無とサイズを確認
		KTexture *tmp_targettex = KVideo::findTexture(m_tmp_target);
		if (tmp_targettex && tmp_targettex->getWidth() == m_game_w && tmp_targettex->getHeight() == m_game_h) {
			need_newtex = false;
		}

		// 作業用ターゲットが存在しないか無効化されていれば再生成する
		if (need_newtex) {
			if (tmp_targettex) {
				KVideo::deleteTexture(tmp_targettex->getId());
			}
			m_tmp_target = KVideo::createRenderTexture(m_game_w, m_game_h);
			tmp_targettex = KVideo::findTexture(m_tmp_target);
		}

		// ターゲットを初期化
		KTexture *targettex = KVideo::findTexture(targetid);
		KVideo::pushRenderTarget(targettex->getId());
		KVideo::clearColor(m_bgcolor);
		KVideo::clearDepth(1.0f);
		KVideo::clearStencil(0);
		KVideo::popRenderTarget();

		// ビュー（カメラ）を順番に描画する
		KVideo::setViewport(0, 0, m_game_w, m_game_h);
		for (auto it=cameralist.begin(); it!=cameralist.end(); ++it) {
			KNode *cameranode = *it;
			if (m_mgr_call_renderworld.size() == 1) {
				KManager *mgr = m_mgr_call_renderworld[0];

				// 描画対象ノードを得る
				m_tmp_nodes.clear();
				mgr->on_manager_get_renderworld_nodes(cameranode, start, filter, m_tmp_nodes);

				if (m_tmp_nodes.size() > 0) {

					if (KCamera::of(cameranode)->isMaterialEnabled()) {
						// ビュー用のマテリアルを使う。
						// 作業用ターゲットに画面を描画し、それをマテリアル付きで転送する
						KVideo::pushRenderTarget(tmp_targettex->getId());
						mgr->on_manager_renderworld(cameranode, m_tmp_nodes);
						KVideo::popRenderTarget();
						const KMaterial &m = KCamera::of(cameranode)->getMaterial();
						KVideoUtils::blit(targettex, tmp_targettex, const_cast<KMaterial*>(&m)); // マテリアルを適用

					} else {
						// ビュー用のマテリアル使わない。
						// レンダーターゲットに直接描画する
						KVideo::pushRenderTarget(targettex->getId());
						mgr->on_manager_renderworld(cameranode, m_tmp_nodes);
						KVideo::popRenderTarget();
					}
					// この時点での描画結果を保存しておく。
					// ※isdebug が true の場合、これは選択オブジェクトの輪郭線を抽出するための
					// プロセスの一部として呼ばれているだけなので、ここでの結果を保存してはいけない（通常描画された分が上書きされてしまう）
					KTexture *cameratex = KVideo::findTexture(KCamera::of(cameranode)->getRenderTarget());
					if (!isdebug && cameratex) {
						KVideoUtils::blit(cameratex, targettex, nullptr);
					}
				}
			
			} else {
				// ノードを描画する
				if (KCamera::of(cameranode)->isMaterialEnabled()) {
					// ビュー用のマテリアルを使う。
					// 作業用ターゲットに画面を描画し、それをマテリアル付きで転送する
					KVideo::pushRenderTarget(tmp_targettex->getId());
					for (int i=0; i<(int)m_tmp_nodes.size(); i++) {
						KManager *mgr = m_mgr_call_renderworld[i];
						m_tmp_nodes.clear();
						mgr->on_manager_get_renderworld_nodes(cameranode, start, filter, m_tmp_nodes);
						mgr->on_manager_renderworld(cameranode, m_tmp_nodes);
					}
					KVideo::popRenderTarget();
					const KMaterial &m = KCamera::of(cameranode)->getMaterial();
					KVideoUtils::blit(targettex, tmp_targettex, const_cast<KMaterial*>(&m)); // マテリアルを適用
				} else {
					// ビュー用のマテリアル使わない。
					// レンダーターゲットに直接描画する
					KVideo::pushRenderTarget(targettex->getId());
					for (int i=0; i<(int)m_mgr_call_renderworld.size(); i++) {
						KManager *mgr = m_mgr_call_renderworld[i];
						m_tmp_nodes.clear();
						mgr->on_manager_get_renderworld_nodes(cameranode, start, filter, m_tmp_nodes);
						mgr->on_manager_renderworld(cameranode, m_tmp_nodes);
					}
					KVideo::popRenderTarget();
				}

				// この時点での描画結果を保存しておく。
				// ※isdebug が true の場合、これは選択オブジェクトの輪郭線を抽出するための
				// プロセスの一部として呼ばれているだけなので、ここでの結果を保存してはいけない（通常描画された分が上書きされてしまう）
					KTexture *cameratex = KVideo::findTexture(KCamera::of(cameranode)->getRenderTarget());
				if (!isdebug && cameratex) {
					KVideoUtils::blit(cameratex, targettex, nullptr);
				}
			}
		}
	}

	void render_debug_node(KNode *node, int layer, const KMatrix4 &tr) {
		if (node->getLayerInTree() == layer) {
			KAction *act = node->getAction();
			if (act) {
				KMatrix4 node_tr = node->getLocal2WorldMatrix(); 
				act->onDebugDraw(tr * node_tr);
			}
		}
		for (int i=0; i<node->getChildCount(); i++) {
			render_debug_node(node->getChild(i), layer, tr);
		}
	}

	void render_debug(KNodeArray &cameralist, KNode *start, KEntityFilterCallback *filter, KTEXID target) {
		if (m_gizmo == nullptr) return;

		// ビュー（カメラ）を順番に描画する
		KVideo::setViewport(0, 0, m_game_w, m_game_h);
		for (auto it=cameralist.begin(); it!=cameralist.end(); ++it) {
			KNode *camera = *it;

			KMatrix4 tr;
			camera->getWorld2LocalMatrix(&tr);

			int layer = camera->getLayerInTree();
			m_gizmo->newFrame();
			{
				KNode *node = KNodeTree::getRoot();
				render_debug_node(node, layer, tr);
			}
			KVideo::pushRenderTarget(target);
			m_gizmo->render(KCamera::of(camera)->getProjectionMatrix());
			KVideo::popRenderTarget();
		}
	}

	void save_texture(KTEXID texid, const char *filename) const {
		KTexture *tex = KVideo::findTexture(texid);
		if (tex) {
			KImage img = tex->exportTextureImage();
			img.saveToFileName(filename);
		}
	}

};
#pragma endregion // CScreenImpl


#pragma region KScreen
static CScreenImpl *g_Screen = nullptr;

void KScreen::install(int game_w, int game_h, int gui_w, int gui_h) {
	g_Screen = new CScreenImpl();
	g_Screen->init(game_w, game_h, gui_w, gui_h);
}
void KScreen::uninstall() {
	if (g_Screen) {
		endVideo();
		delete g_Screen;
		g_Screen = nullptr;
	}
}
void KScreen::startVideo(KTextureBank *texbank) {
	K__ASSERT(g_Screen);
	g_Screen->startVideo(texbank);

	KEngine::addInspectorCallback(g_Screen, u8"画面描画");
}
void KScreen::endVideo() {
	KEngine::removeInspectorCallback(g_Screen);

	K__ASSERT(g_Screen);
	g_Screen->endVideo();
}
void KScreen::getGameSize(int *w, int *h) {
	K__ASSERT(g_Screen);
	g_Screen->getGameSize(w, h);
}
void KScreen::setGameSize(int w, int h) {
	K__ASSERT(g_Screen);
	g_Screen->setGameSize(w, h);
}
void KScreen::getGuiSize(int *w, int *h) {
	K__ASSERT(g_Screen);
	g_Screen->getGuiSize(w, h);
}
void KScreen::setGuiSize(int w, int h) {
	K__ASSERT(g_Screen);
	g_Screen->setGuiSize(w, h);
}
KGizmo * KScreen::getGizmo() {
	K__ASSERT(g_Screen);
	return g_Screen->getGizmo();
}
KVec3 KScreen::windowClientToScreenPoint(const KVec3 &p) {
	K__ASSERT(g_Screen);
	return g_Screen->windowClientToScreenPoint(p);
}
KRecti KScreen::getGameViewportRect() {
	K__ASSERT(g_Screen);
	return g_Screen->getGameViewportRect();
}
void KScreen::postExportScreenTexture(const KPath &filename) {
	K__ASSERT(g_Screen);
	g_Screen->postExportScreenTexture(filename);
}
void KScreen::render() {
	K__ASSERT(g_Screen);
	g_Screen->render();
}
void KScreen::render_debug_gui(KNode *node) {
	K__ASSERT(g_Screen);
	g_Screen->render_debug_gui(node);
}
void KScreen::set_call_gui(KManager *mgr, bool value) {
	K__ASSERT(g_Screen);
	g_Screen->set_call_gui(mgr, value);
}
void KScreen::set_call_renderworld(KManager *mgr, bool value) {
	K__ASSERT(g_Screen);
	g_Screen->set_call_renderworld(mgr, value);
}
void KScreen::set_call_renderdebug(KManager *mgr, bool value) {
	K__ASSERT(g_Screen);
	g_Screen->set_call_renderdebug(mgr, value);
}
void KScreen::set_call_rendertop(KManager *mgr, bool value) {
	K__ASSERT(g_Screen);
	g_Screen->set_call_rendertop(mgr, value);
}
bool KScreen::command(const char *s, void *n, int *retval) {
	K__ASSERT(g_Screen);
	return g_Screen->command(s, n, retval);
}
KTEXID KScreen::getPerPassRenderTarget(int pass) {
	K__ASSERT(g_Screen);
	return g_Screen->getPerPassRenderTarget(pass);
}
void KScreen::setPerPassRenderTargetSize(int pass, int w, int h) {
	K__ASSERT(g_Screen);
	g_Screen->setPerPassRenderTargetSize(pass, w, h);
}
void KScreen::setPerPassAlphaFillingEnable(int pass, bool value) {
	K__ASSERT(g_Screen);
	g_Screen->setPerPassAlphaFillingEnable(pass, value);
}

/// 単位座標系（画面中央原点、-1.0～1.0、Y軸上向き）の座標を、
/// ウィンドウクライアント座標系（左上原点、y軸下向き）で表す。
/// ただし z 座標は加工しない
KVec3 KScreen::getWindowCoordFromNormalizedScreenCoord(const KVec3 &p) {
	KRecti vp = getGameViewportRect();
	KVec3 out;
	out.x = vp.x + vp.w * KMath::linearStepUnclamped(-1.0f, 1.0f, p.x);
	out.y = vp.y + vp.h * KMath::linearStepUnclamped(1.0f, -1.0f, p.y); // Ｙ軸反転。符号に注意
	out.z = p.z;
	return out;
}

/// ウィンドウクライアント座標系（左上原点、y軸下向き）の座標を、
/// 単位座標系（画面中央原点、-1.0～1.0、Y軸上向き）で表す。
/// ただし z 座標は加工しない
KVec3 KScreen::getNormalizedScreenCoordFromWindowCoord(const KVec3 &p) {
	KRecti vp = getGameViewportRect();
	if (vp.w <= 0) return KVec3();
	if (vp.h <= 0) return KVec3();
	KVec3 out;
	out.x = KMath::lerp(-1.0f, 1.0f, KMath::linearStepUnclamped((float)vp.x, (float)(vp.x + vp.w), p.x));
	out.y = KMath::lerp(1.0f, -1.0f, KMath::linearStepUnclamped((float)vp.y, (float)(vp.y + vp.h), p.y)); // Ｙ軸反転。符号に注意
	out.z = p.z;
	return out;
}
#pragma endregion // KScreen




} // namespace
