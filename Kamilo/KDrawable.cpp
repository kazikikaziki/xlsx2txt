#include "KDrawable.h"
//
#include <algorithm> // std::stable_sort
#include "KAction.h"
#include "KCamera.h"
#include "KImGui.h"
#include "KRes.h"
#include "KInspector.h"
#include "KInternal.h"
#include "KScreen.h"

namespace Kamilo {



#define USING_DRAWLIST 1 // 描画リストに一度書き出してから描画する



#pragma region Sort
// 描画の優先度順で並び替える（優先度低いほうから高いほうへ）
class CSortByPriority {
public:
	bool operator()(const KNode *node1, const KNode *node2) const {
		// 優先度番号が大きいものから並べる（小さいほど優先度が高い。Z値と同じ大小関係）
		int pri1 = node1->getPriorityInTree();
		int pri2 = node2->getPriorityInTree();
		if (pri1 != pri2) {  
			return pri1 > pri2;
		}

		// 同一優先度の場合は順序を変えない（安全なソート）
		return false;
	}
};

/// 描画の優先度順およびＺ座標順で並び替える（優先度低いほうから高いほうへ）
class CSortByPriorityAndDepth {
public:
	bool operator()(const KNode *node1, const KNode *node2) const {
		// 優先度番号が大きいものから並べる（小さいほど優先度が高い。Z値と同じ大小関係）
		int pri1 = node1->getPriorityInTree();
		int pri2 = node2->getPriorityInTree();
		if (pri1 != pri2) {
			return pri1 > pri2;
		}
		KVec3 pos1 = node1->getWorldPosition();
		KVec3 pos2 = node2->getWorldPosition();
		// Z座標の大きなものから並べる
		if (pos1.z != pos2.z) {
			return pos1.z > pos2.z;
		}
		// Z座標が同一であれば、Y高度の低いほうから並べる
		if (pos1.y != pos2.y) {
			return pos1.y < pos2.y;
		}
		// 同一優先度の場合は順序を変えない（安全なソート）
		return false;
	}
};
#pragma endregion // Sort



class CRenderMgr: public KManager, public KInspectorCallback {
	KNodeArray m_Tmp0;
	KNodeArray m_Tmp1;
	KNodeArray m_Tmp1a;
	KNodeArray m_Tmp2;
	KDrawList m_DrawList;
	KRenderCallback *m_CB;
	std::unordered_map<KNode*, KDrawable*> m_Nodes;
	int m_NumDrawNodes;
	int m_NumDrawList;
	bool m_AdjSnap;
	bool m_AdjHalf;
public:
	CRenderMgr() {
		m_AdjSnap = MO_VIDEO_ENABLE_HALF_PIXEL;
		m_AdjHalf = MO_VIDEO_ENABLE_HALF_PIXEL;
		m_NumDrawNodes = 0;
		m_NumDrawList = 0;
		m_CB = nullptr;
		KEngine::addManager(this);
		KEngine::addInspectorCallback(this, u8"Renderer");
		KEngine::setManagerRenderWorldEnabled(this, true); // on_manager_renderworld
		KEngine::setManagerRenderDebugEnabled(this, true); // on_manager_renderdebug
	}
	virtual ~CRenderMgr() {
		on_manager_end();
	}
	void setCallback(KRenderCallback *cb) {
		m_CB = cb;
	}
	void addDrawable(KNode *node, KDrawable *drawable) {
		K__ASSERT(node);
		K__ASSERT(drawable);
		delDrawable(node);
		m_Nodes[node] = drawable;
		drawable->_setNode(node);
		drawable->grab();
	}
	void delDrawable(KNode *node) {
		auto it = m_Nodes.find(node);
		if (it != m_Nodes.end()) {
			KDrawable *drawable = it->second;
			drawable->_setNode(nullptr);
			drawable->drop();
			m_Nodes.erase(it);
		}
	}
	KDrawable * getDrawable(KNode *node) {
		auto it = m_Nodes.find(node);
		if (it != m_Nodes.end()) {
			return it->second;
		}
		return nullptr;
	}

	#pragma region KManager
	virtual void on_manager_nodeinspector(KNode *node) override {
		KDrawable *renderer = KDrawable::of(node);
		if (renderer) {
			renderer->updateInspector();
		}
	}
	virtual void on_manager_start() override {
	}
	virtual void on_manager_end() override {
		K__ASSERT(m_Nodes.empty()); // 正しく on_manager_detach が呼ばれていればノードは存在しないはず
		m_DrawList.destroy();
	}
	virtual void on_manager_detach(KNode *node) override {
		delDrawable(node);
	}
	virtual bool on_manager_isattached(KNode *node) override {
		return KDrawable::of(node) != nullptr;
	}
	virtual void on_manager_renderdebug(KNode *cameranode, KNode *start, KEntityFilterCallback *filter) override {
		if (KInspector::isInstalled()) {
			KColor color(1.0f, 1.0f, 1.0f, 1.0f);
			int camera_layer = cameranode->getLayerInTree();
			KMatrix4 tr = cameranode->getWorld2LocalMatrix();
			int num = KInspector::getSelectedEntityCount();
			for (int i=0; i<num; i++) {
				KNode *nd = KInspector::getSelectedEntity(i);
				if (nd && nd->getEnable() && nd->getLayerInTree()==camera_layer) { // カメラのレイヤーと一致しないと描画対象にならない
					// Hitboxは回転拡縮の影響を受けないので
					// knode->getLocal2WorldMatrix を使わないようにする
					// ※Hitboxの中心座標がどこに来るのかという計算にはノードの回転拡縮の影響があるが、
					// Hitboxそのものの大きさは回転角祝の影響を受けない
					KVec3 wpos = nd->getWorldPosition();
					KMatrix4 matrix = /*KMatrix4::fromTranslation(wpos) **/ tr;
					KVec3 aabb_min, aabb_max;
					if (KDrawable::of(nd)) {
						KDrawable::of(nd)->on_node_render_aabb(&aabb_min, &aabb_max);
						KScreen::getGizmo()->addAabb(matrix, aabb_min, aabb_max, color);
					}
				}
			}
		}
	}
	virtual void on_manager_get_renderworld_nodes(KNode *camera, KNode *start, KEntityFilterCallback *filter, KNodeArray &result) override {
		getRenderNodes(camera, start, filter, result);

		// 描画前の処理
		for (int i=0; i<(int)result.size(); i++) {
			result[i]->on_node_will_render(camera);
		}
	}

	virtual void on_manager_renderworld(KNode *camera, const KNodeArray &render_nodes) override {
		drawNodes(camera, render_nodes);
	}
	#pragma endregion // KManager

	#pragma region KInspectorCallback
	virtual void onInspectorGui() {
	#ifndef NO_IMGUI
		if (1) {
			ImGui::Separator();
			ImGui::Checkbox("Snap to integer coordinate", &m_AdjSnap);
			ImGui::Checkbox("Offset half pixel", &m_AdjHalf);
		}

		int dc = 0;
		KVideo::getParameter(KVideo::PARAM_DRAWCALLS, &dc);
		ImGui::Text("Draw calls  = %d", dc);
		ImGui::Text("Draw nodes  = %d", m_NumDrawNodes);
		ImGui::Text("Draw list   = %d", m_NumDrawList);
		m_NumDrawNodes = 0;
		m_NumDrawList = 0;
	#endif // !NO_IMGUI
	}
	#pragma endregion // KInspectorCallback

	void setConfig(KDrawable::EConfig conf, bool value) {
		switch (conf) {
		case KDrawable::C_MASTER_ADJ_SNAP:
			m_AdjSnap = value;
			break;
		case KDrawable::C_MASTER_ADJ_HALF:
			m_AdjHalf = value;
			break;
		}
	}
	bool getConfig(KDrawable::EConfig conf) const {
		switch (conf) {
		case KDrawable::C_MASTER_ADJ_SNAP:
			return m_AdjSnap;
		case KDrawable::C_MASTER_ADJ_HALF:
			return m_AdjHalf;
		}
		return false;
	}


	// 描画対象となるノードを得る
	void getRenderNodes(KNode *camera, KNode *start, KEntityFilterCallback *filter, KNodeArray &result) {
		renderqueue_add_nodes(result, start, camera, filter);
	}
	// ノード列を描画する
	void drawNodes(KNode *camera, const KNodeArray &render_nodes) {
		if (camera == nullptr) return;
		if (render_nodes.empty()) return;

		m_NumDrawNodes += render_nodes.size(); // on_manager_renderworld はカメラごとに呼ばれるので、代入ではなく加算する

		KMatrix4 tr;
		camera->getWorld2LocalMatrix(&tr);
		nodes_render(render_nodes, KCamera::of(camera)->getProjectionMatrix(), tr, camera);
	}

	void registerDrawables(KNode *node, int layer, std::vector<KNode*> &list, bool ignore_atomic, int this_layer_in_tree) {
		if (!node->getVisible()) {
			return;
		}
		if (!node->getEnable()) {
			return;
		}
	//	int this_layer = getLayerInTree();
		int this_layer = this_layer_in_tree;
		if (!ignore_atomic) {
			if (node->getRenderAtomic()) {
				// 不可分描画。
				// 子ツリーを全てまとめてひとつの塊として描画するため、
				// ここでは自分だけを追加し、子要素を隠しておく
				// ※子を対象とした操作なので、自分自身の Renderer の有無は関係ない
				// ※この操作により renderer を持っていないノードが描画リストに登録されることに注意
				if (layer < 0 || this_layer==layer) {
					list.push_back(node);
				}
				return;
			}
		}
		if (node->getRenderAfterChildren()) {
			// 子ツリーを追加
			for (int i=0; i<node->getChildCount(); i++) {
				KNode *child = node->getChild(i);
				int child_layer = child->getLayer();
				int child_layer_in_tree = child_layer ? child_layer : this_layer; // child のレイヤーがゼロの場合のみ親のレイヤーを継承する
				registerDrawables(child, layer, list, ignore_atomic, child_layer_in_tree);
			}
			// 自分を追加
			KDrawable *dw = KDrawable::of(node);
			if (dw) dw->onDrawable_register(layer, list);

		} else {
			// 自分を追加
			KDrawable *dw = KDrawable::of(node);
			if (dw) dw->onDrawable_register(layer, list);

			// 子ツリーを追加
			for (int i=0; i<node->getChildCount(); i++) {
				KNode *child = node->getChild(i);
				int child_layer = child->getLayer();
				int child_layer_in_tree = child_layer ? child_layer : this_layer; // child のレイヤーがゼロの場合のみ親のレイヤーを継承する
				registerDrawables(child, layer, list, ignore_atomic, child_layer_in_tree);
			}
		}
	}
private:
	void nodes_filter_by_frustum(KNodeArray &output, const KNodeArray &input, KNode *camera) {
		for (auto it=input.begin(); it!=input.end(); ++it) {
			KNode *node = *it;
			K__ASSERT(node);

			if (!node->getViewCullingInTree()) {
				// カメラ範囲によるカリング無効。常に描画する
				output.push_back(node);
			
			} else {
				// カリング有効。カメラの撮影範囲内にある場合のみ描画する
				KVec3 aabb_min, aabb_max;
				KDrawable *dw = getDrawable(node);
				if (dw) dw->on_node_render_aabb(&aabb_min, &aabb_max);
				KVec3 size = aabb_max - aabb_min;
				// aabb サイズがゼロの場合は無条件で描画とする
				if (size.isZero() || KCamera::of(camera)->isWorldAabbInFrustum(aabb_min, aabb_max)) {
					output.push_back(node);
				}
			}
		}
	}
	void nodes_filter_by_callback(KNodeArray &output, const KNodeArray &input, KEntityFilterCallback *cb) {
		// インスペクターによる選別を行う。
		// インスペクターが定義かつ表示されていれば、その設定にしたがって
		// 描画するべきノードを抽出する
		if (cb) {
			for (auto it=input.begin(); it!=input.end(); ++it) {
				KNode *node = *it;
				K__ASSERT(node);

				bool skip = false;
				cb->on_entityfilter_check(node, &skip);
				if (!skip) {
					output.push_back(node);
				}
			}
		} else {
			// インスペクターによる選別なし。
			// 無条件でコピーする
			output.assign(input.begin(), input.end());
		}
	}
	void sortByRenderingOrder(KCamera::Order order, KNodeArray &list) {
		switch (order) {
		case KCamera::ORDER_HIERARCHY:
			// ヒエラルキー上で親→子の順番になるようにソートする
			// KNode::_RegisterForRender でノードを登録した時点でヒエラルキー順になってるので、
			// 基本的にはこのままでよいのでだが、さらに描画優先度の値でソートしておく。
			// ソートキーが同値だった場合に順番を変えないよう std::sort ではなく stable_sort を使う
			std::stable_sort(list.begin(), list.end(), CSortByPriority());
			break;
		case KCamera::ORDER_ZDEPTH:
			// エンティティのワールド座標が奥(Z大)→手前(Z小)の順番になるようにソートする。
			// ソートキーが同値だった場合に順番を変えないよう std::sort ではなく stable_sort を使う
			std::stable_sort(list.begin(), list.end(), CSortByPriorityAndDepth());
			break;
		case KCamera::ORDER_CUSTOM:
			// ユーザー定義のソートを行う
			if (m_CB) {
				m_CB->on_render_custom_sort(list);
			}
			break;
		}
	}
	void renderqueue_add_nodes(KNodeArray &output, KNode *start, KNode *camera, KEntityFilterCallback *filter) {
		if (start == nullptr) return;
		if (camera == nullptr) return;

		int camera_layer = camera->getLayerInTree();
		int start_layer = start->getLayerInTree();

		m_Tmp0.clear();

		registerDrawables(start, camera_layer, m_Tmp0, false, start_layer);

		// カメラ範囲による選別
		if (1) {
			m_Tmp1.clear();
			nodes_filter_by_frustum(m_Tmp1, m_Tmp0, camera);
		} else {
			m_Tmp1.assign(m_Tmp0.begin(), m_Tmp0.end());
		}

		// コールバックによる選別
		m_Tmp1a.clear();
		nodes_filter_by_callback(m_Tmp1a, m_Tmp1, filter);

		// ソートする
		output.assign(m_Tmp1a.begin(), m_Tmp1a.end());
		KCamera::Order order = KCamera::of(camera)->getRenderingOrder();
		sortByRenderingOrder(order, output);
	}

	// 与えられたノードだけを描画する
	void renderSingleNode(KNode *node, const KMatrix4 &projection, const KMatrix4 &transform, KNode *camera, KDrawList *drawlist) {
		if (node == nullptr) return;
		KDrawable *renderer = KDrawable::of(node);
		if (renderer == nullptr) return; // 必ずチェックする。例えば ATOMIC が設定されているノードはレンダラーを持っていない場合もある

		KMatrix4 world_matrix;
		node->getLocal2WorldMatrix(&world_matrix);

		KDrawable::RenderArgs opt;
		opt.projection = projection;
		opt.transform = world_matrix * transform; // エンティティ座標を適用
		opt.cb = m_CB;
		opt.adj_snap = m_AdjSnap && KCamera::of(camera)->isSnapIntEnabled();
		opt.adj_half = m_AdjHalf && KCamera::of(camera)->isOffsetHalfPixelEnabled();
		opt.camera = camera;
		renderer->onDrawable_draw(node, &opt, drawlist);
	}

	struct ZPred {
		bool operator()(const KNode *node1, const KNode *node2) {
			// 共通の親を持っていること言う前提。
			// node1 と node2 が兄弟関係にあるという前提で、ワールドではなくローカル座標で比較する
			// 描画順で並べるので、Z値が大→小になるようにする
			return node1->getPosition().z > node2->getPosition().z;
		}
	};

	// ノードツリーを描画する
	void renderNodeTree(KNode *node, const KMatrix4 &projection, const KMatrix4 &transform, KNode *camera, KDrawList *drawlist) {
		K__ASSERT(node);
		K__ASSERT(camera);

		// 子要素
		int camera_layer = camera->getLayerInTree();
		int node_layer = node->getLayerInTree();

		m_Tmp2.clear();
		registerDrawables(node, camera_layer, m_Tmp2, true, node_layer);

		// ソートする
		switch (node->getLocalRenderOrder()) {
		case KLocalRenderOrder_DEFAULT:
			// カメラに設定された順番で描画する
			sortByRenderingOrder(KCamera::of(camera)->getRenderingOrder(), m_Tmp2);
			break;

		case KLocalRenderOrder_TREE:
			// ツリーの巡回順で描画する
			// 親→子の順になる
			// 子→親にしたい場合は KNode::getRenderAfterChildren を設定する
			// _RegisterForRender でノードを追加した時点でツリー巡回順になっているので
			// ここでソートする必要はない
			break;
		}
		// 描画する
		for (auto it=m_Tmp2.begin(); it!=m_Tmp2.end(); ++it) {
			KNode *child = *it;
			if (child->getEnable() && child->getVisible()) {
				renderSingleNode(child, projection, transform, camera, drawlist);
			}
		}
		m_Tmp2.clear();
	}

	void nodes_render(const KNodeArray &input, const KMatrix4 &projection, const KMatrix4 &transform, KNode *camera) {
		bool depth_test = KCamera::of(camera)->isZEnabled();
		if (depth_test) {
			KVideo::setDepthTestEnabled(true);
		}
		if (USING_DRAWLIST) {
			// 描画リスト使う
			m_DrawList.clear();
			int i = 0;
			for (auto it=input.begin(); it!=input.end(); ++it) {
				KNode *node = *it;
				if (node->getRenderAtomic()) {
					// 不可分描画
					// 子ツリーが node 登録されていないので、いまここで子ツリーを描画させる
					renderNodeTree(node, projection, transform, camera, &m_DrawList);

				} else {
					renderSingleNode(node, projection, transform, camera, &m_DrawList);
				}
				i++;
			}
			m_DrawList.endList();
			m_DrawList.draw();
			m_NumDrawList += m_DrawList.size(); // on_manager_renderworld はカメラごとに呼ばれるので、代入ではなく加算する
		
		} else {
			// 描画リスト使わない
			for (auto it=input.begin(); it!=input.end(); ++it) {
				KNode *node = *it;
				renderSingleNode(node, projection, transform, camera, &m_DrawList);
			}
		}
		if (depth_test) {
			KVideo::setDepthTestEnabled(false);
		}
	}
}; // CRenderImpl



#pragma region KDrawable
static CRenderMgr *g_RenderMgr = nullptr;

void KDrawable::install() {
	K__ASSERT(g_RenderMgr == nullptr);
	g_RenderMgr = new CRenderMgr();
}
void KDrawable::uninstall() {
	if (g_RenderMgr) {
		g_RenderMgr->drop();
		g_RenderMgr = nullptr;
	}
}
void KDrawable::setConfig(EConfig c, bool value) {
	K__ASSERT(g_RenderMgr);
	g_RenderMgr->setConfig(c, value);
}
bool KDrawable::getConfig(EConfig c) {
	K__ASSERT(g_RenderMgr);
	return g_RenderMgr->getConfig(c);
}
void KDrawable::setCallback(KRenderCallback *cb) {
	K__ASSERT(g_RenderMgr);
	g_RenderMgr->setCallback(cb);
}
KDrawable * KDrawable::of(KNode *node) {
	K__ASSERT(g_RenderMgr);
	return g_RenderMgr->getDrawable(node);
}
void KDrawable::_attach(KNode *node, KDrawable *drawable) {
	K__ASSERT(node);
	K__ASSERT(drawable);
	K__ASSERT(g_RenderMgr);
	g_RenderMgr->addDrawable(node, drawable);
}


KDrawable::KDrawable() {
	static int s_index = 0;
	m_tmp_group_rentex = nullptr;
	m_group_rentex_w = 256;
	m_group_rentex_h = 256;
	m_group_rentex_pivot = KVec3();
	m_group_rentex_autosize = true;
	m_group_enabled = false;
	m_group_rentex_name = K::str_sprintf("_group_%d.tex", ++s_index);
	m_adj_snap = MO_VIDEO_ENABLE_HALF_PIXEL;
	m_adj_half = MO_VIDEO_ENABLE_HALF_PIXEL;
	m_node = nullptr;
}
KDrawable::~KDrawable() {
	if (KBank::getTextureBank()) {
		KBank::getTextureBank()->removeTexture(m_group_rentex_name);
	}
}
KNode * KDrawable::getNode() {
	return m_node;
}
void KDrawable::_setNode(KNode *node) {
	// KDrawable は KNode によって保持される。
	// 循環ロック防止のために KNode の参照カウンタは変更しない
	m_node = node;
}
void KDrawable::on_node_render_aabb(KVec3 *aabb_min, KVec3 *aabb_max) {
	KNode *self = getNode();
	K__ASSERT(self);

	// AABB
	KVec3 lmin, lmax;
	onDrawable_getBoundingAabb(&lmin, &lmax);

	// ユーザーによるAABB操作
	KAction *act = self->getAction();
	if (act) {
		act->onAabbCulling(&lmin, &lmax);
	}
	// ワールド座標化
#if 1
	KVec3 wmin = self->localToWorldPoint(lmin);
	KVec3 wmax = self->localToWorldPoint(lmax);
#else
	KVec3 wpos = self->getWorldPosition();
	KVec3 wmin = wpos * lmin;
	KVec3 wmax = wpos * lmax;
#endif

	// マージンを加える
	// （これはワールド座標に対して行う。そうしないとマージンがスケーリングに影響されてしまう）
	KVec3 margin(8, 8, 8);
	wmin -= margin;
	wmax += margin;

	*aabb_min = wmin;
	*aabb_max = wmax;
}
void KDrawable::onDrawable_inspector() {
	// ローカル座標ぴったりのAABB
	KVec3 lmin, lmax;
	onDrawable_getBoundingAabb(&lmin, &lmax);
	KVec3 lsize = lmax - lmin;

	// ワールド座標かつ補正後のAABB
	KVec3 wmin, wmax;
	on_node_render_aabb(&wmin, &wmax);
	KVec3 wsize = wmax - wmin;

	KDebugGui::K_DebugGui_Aabb("AABB (Raw)", lmin, lmax);
	KDebugGui::K_DebugGui_Aabb("AABB (World)", wmin, wmax);
}
void KDrawable::onDrawable_register(int layer, std::vector<KNode*> &list) {
	K__ASSERT_RETURN(m_node);
	int this_layer = m_node->getLayerInTree();
	if (layer < 0 || this_layer==layer) {
		list.push_back(m_node);
	}
}
void KDrawable::onDrawable_willDraw(const KNode *camera) {
	K__ASSERT_RETURN(m_node);
	KAction *act = m_node->getAction();
	if (act) {
		act->onWillRender();
	}
}
void KDrawable::setGroupingImageSize(int w, int h, const KVec3 &pivot) {
	if (w > 0 && h > 0) {
		// レンダーターゲットのサイズが奇数になっていると、ドットバイドットでの描画が歪む
		K__ASSERT(w % 2 == 0);
		K__ASSERT(h % 2 == 0);
		m_group_rentex_w = w;
		m_group_rentex_h = h;
		m_group_rentex_pivot = pivot;
		m_group_rentex_autosize = false;
	} else {
		m_group_rentex_autosize = true;
	}
}
void KDrawable::getGroupingImageSize(int *w, int *h, KVec3 *pivot) {
	// ※pivotはビットマップ原点（＝レンダーターゲット画像左上）を基準にしている
	if (m_group_rentex_autosize) {
		onDrawable_getGroupImageSize(w, h, pivot);
	} else {
		if (w) *w = m_group_rentex_w;
		if (h) *h = m_group_rentex_h;
		if (pivot) *pivot = m_group_rentex_pivot;
	}
}
std::string KDrawable::getWarningString() {
	K__ASSERT_RETURN_VAL(m_node, "");
	KNode *camera = KCamera::findCameraFor(m_node);
	if (camera == nullptr) {
		return "No camera for this node";
	}
	return "";
}
void KDrawable::setGrouping(bool value) {
	m_group_enabled = value;
}
bool KDrawable::isGrouping() const {
	return m_group_enabled;
}
KMaterial * KDrawable::getGroupingMaterial() {
	return &m_group_material;
}
void KDrawable::setGroupingMatrix(const KMatrix4 &matrix) {
	m_group_matrix = matrix;
}
void KDrawable::updateGroupRenderTextureAndMesh() {
	// グループ描画に必要なレンダーテクスチャサイズを得る
	int img_w = 256;
	int img_h = 256;
	getGroupingImageSize(&img_w, &img_h, nullptr);
	if (img_w <= 0 || img_h <= 0) {
		// テクスチャなし
		return;
	}
	int ren_w = img_w;
	int ren_h = img_h;
	// レンダーテクスチャ
	KTexture *target_tex = KVideo::findTexture(KBank::getTextureBank()->findTextureRaw(m_group_rentex_name, false));
	if (target_tex && !target_tex->isRenderTarget()) {
		// レンダーテクスチャではない
		K__ERROR("'%s' is not a render texture", m_group_rentex_name.c_str());
		return;
	}
	if (target_tex == nullptr) {
		// まだ存在しない
		target_tex = KVideo::findTexture(KBank::getTextureBank()->addRenderTexture(m_group_rentex_name, ren_w, ren_h));

	} else if (target_tex->getWidth() != ren_w || target_tex->getHeight() != ren_h) {
		K__ASSERT(target_tex->isRenderTarget());
		// サイズが違う
		// 一度削除して再生成する
		KBank::getTextureBank()->removeTexture(m_group_rentex_name);
		target_tex = KVideo::findTexture(KBank::getTextureBank()->addRenderTexture(m_group_rentex_name, ren_w, ren_h));
	}
	setGroupRenderTexture(target_tex ? target_tex->getId() : nullptr);

	// レンダーターテクスチャを描画するためのメッシュ
	bool remake = false;
	if (m_group_mesh.getSubMeshCount() == 0) {
		remake = true;
	} else {
		KVec3 p0, p1;
		m_group_mesh.getAabb(&p0, &p1);
		int mesh_w = (int)(p1.x - p0.x);
		int mesh_h = (int)(p1.y - p0.y);
		if (mesh_w != ren_w || mesh_h != ren_h) {
			// メッシュサイズが描画画像とあっていない。頂点を再設定する
			remake = true;
		}
	}
	if (remake) {
		float u = (float)img_w / ren_w;
		float v = (float)img_h / ren_h;
		MeshShape::makeRect(&m_group_mesh, KVec2(0, 0), KVec2(img_w, img_h), KVec2(0, 0), KVec2(u, v), KColor::WHITE);
	}
}
KTEXID KDrawable::getGroupRenderTexture() {
	updateGroupRenderTextureAndMesh();
	return m_tmp_group_rentex;
}
void KDrawable::setGroupRenderTexture(KTEXID rentexid) {
	KTexture *rentex = KVideo::findTexture(rentexid);
	if (rentex && rentex->isRenderTarget()) {
		m_tmp_group_rentex = rentexid;
	} else {
		m_tmp_group_rentex = nullptr;
	}
}
KMesh * KDrawable::getGroupRenderMesh() {
	updateGroupRenderTextureAndMesh();
	return &m_group_mesh;
}
void KDrawable::setGroupRenderMesh(const KMesh &mesh) {
	m_group_mesh = mesh;
}
void KDrawable::updateInspector() {
#ifndef NO_IMGUI
	if (1) {
		ImGui::DragFloat3("Offset", (float*)&m_offset, 1);
	}
	if (1) {
		if (ImGui::TreeNode(u8"グループ")) {
			ImGui::Checkbox(u8"グループ化する", &m_group_enabled);

			ImGui::Separator();
			if (ImGui::TreeNode(u8"マテリアル")) {
				KBank::getVideoBank()->guiMaterialInfo(&m_group_material);
				ImGui::TreePop();
			}

			ImGui::Separator();
			if (ImGui::TreeNode(u8"テクスチャ")) {
				// グループテクスチャ情報
				int ren_w = 256;
				int ren_h = 256;
				KVec3 piv;
				getGroupingImageSize(&ren_w, &ren_h, &piv);

				KTexture *gtex = KVideo::findTexture(getGroupRenderTexture());
				ImGui::Text("Group Tex: %s", m_group_rentex_name.c_str());
				if (gtex) {
					int tex_w = gtex->getWidth();
					int tex_h = gtex->getHeight();
					ImGui::Text("Tex size: %d x %d", tex_w, tex_h);
					ImGui::Text("Tex pivot: %.1f, %.1f", piv.x, piv.y);
					KBank::getTextureBank()->guiTexture(m_group_rentex_name, 256);
				} else {
					// グループ化描画用のテクスチャが存在しない。エラー
					ImGui::Text("Tex size: N/A (No texture found)");
				}

				// グループテクスチャのサイズを自動設定する？
				ImGui::Checkbox("Tex auto sizing", &m_group_rentex_autosize);

				// グループテクスチャのサイズ
				{
					int ii[] = {m_group_rentex_w, m_group_rentex_h};
					if (ImGui::DragInt2("Group Tex size", ii, 1, 1, 1024)) {
						m_group_rentex_w = ii[0];
						m_group_rentex_h = ii[1];
					}
				}

				// グループテクスチャ描画時のピボット
				ImGui::DragFloat2("Pivot for output texture", (float*)&m_group_rentex_pivot);

				// グループテクスチャ描画時の変形行列
				KDebugGui::K_DebugGui_InputMatrix("Matrixc for output texture", &m_group_matrix);
				ImGui::TreePop();
			}

			ImGui::Separator();
			if (ImGui::TreeNode(u8"メッシュ")) {
				KBank::getVideoBank()->guiMeshInfo(&m_group_mesh);
				ImGui::TreePop();
			}
			ImGui::TreePop();
		}
	}
	if (1) {
		ImGui::Separator();
		ImGui::Checkbox("Snap to integer coordinate", &m_adj_snap);
		ImGui::Checkbox("Offset half pixel", &m_adj_half);
		KImGui::PushTextColor(KImGui::COLOR_WARNING);
		if (!g_RenderMgr->getConfig(C_MASTER_ADJ_SNAP)) {
			ImGui::Text("'adj_snap' has been disabled by KRenderManager!");
		}
		if (!g_RenderMgr->getConfig(C_MASTER_ADJ_HALF)) {
			ImGui::Text("'adj_half' has been disabled by KRenderManager!");
		}
		KImGui::PopTextColor();
	}
	if (1) {
		KDebugGui::K_DebugGui_InputMatrix("Local transform", &m_local_transform);
	}
	if (ImGui::TreeNode(KImGui::ID(-2), "Test")) {
		ImGui::Text("SkewTest");
		if (ImGui::Button("-20")) {
			m_local_transform = KMatrix4::fromSkewX(-20);
		}
		ImGui::SameLine();
		if (ImGui::Button("-10")) {
			m_local_transform = KMatrix4::fromSkewX(-10);
		}
		ImGui::SameLine();
		if (ImGui::Button("0")) {
			m_local_transform = KMatrix4::fromSkewX(0);
		}
		ImGui::SameLine();
		if (ImGui::Button("10")) {
			m_local_transform = KMatrix4::fromSkewX(10);
		}
		ImGui::SameLine();
		if (ImGui::Button("20")) {
			m_local_transform = KMatrix4::fromSkewX(20);
		}
		ImGui::TreePop();
	}
	
	ImGui::Separator();
	if (ImGui::CollapsingHeader(typeid(*this).name(), ImGuiTreeNodeFlags_DefaultOpen)) {
		onDrawable_inspector();
	}
#endif // !NO_IMGUI
}







#pragma endregion // KDrawable


} // namespace
