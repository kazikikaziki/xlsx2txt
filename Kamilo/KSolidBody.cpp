#include "KSolidBody.h"
//
#include "KImGui.h"
#include "KInspector.h"
#include "KInternal.h"
#include "KDrawable.h"
#include "KScreen.h"
#include "KCamera.h"

namespace Kamilo {


static const float WALL_TALL   = 200;
static const float HUGE_LENGTH = 1024 * 8;
static const float PLANE_LIMIT = 10000.0f;


typedef std::vector<KSolidBody *> KBodyList;


#pragma region Functions
static bool _IsDebugInfoVisible(KSolidBody *body, KNode *camera) {
	if (body == nullptr) return false;
	
	KNode *node = body->getNode();
	if (node == nullptr) return false;

	K__ASSERT(camera);
	if (node->getEnableInTree() == false) {
		return false; // ノードが無効になっている
	}
	if (node->getVisibleInTree() == false) {
		return false; // ノードが非表示になっている
	}
	if (body->getBodyEnabled() == false) {
		return false; // ノードの Body が無効になっている
	}

	/*
	if (KInspector::isInstalled()) {
		if (KInspector::isEntityVisible(target)) {
			return false;
		}
	}
	*/

	// このノードを映しているカメラ
	KNode *camera_for_node = KCamera::findCameraFor(node);
	if (camera_for_node == nullptr) {
		return false; // ノードに対応するカメラが存在しない
	}

	if (camera_for_node != camera) {
		return false; // ノードは関係のないカメラが指定されている
	}
	return true;
}
static float _GetGizmoBlinkingAlpha(KNode *knode) {
	if (knode == nullptr) return 0;
#ifndef NO_IMGUI
	float alpha;
	if (knode->getEnableInTree() && knode->getVisibleInTree()) {
		alpha = 1.0f;
	} else {
		alpha = 0.2f;
	}
	return alpha;
#else // !NO_IMGUI
	return 0.0f;
#endif
}
static bool _PassBodyFilter(KSolidBody *bodynode1, KSolidBody *bodynode2) {
	K__ASSERT(bodynode1);
	K__ASSERT(bodynode2);
	const KCollider *co1 = bodynode1->getShape();
	const KCollider *co2 = bodynode2->getShape();
	// https://www.iforce2d.net/b2dtut/collision-filtering
	if ((co1->get_bitflag() & bodynode2->m_Desc.get_mask_bits()) == 0) {
		return false; // node1 のグループは node2 の判定対象ではない
	}
	if ((co2->get_bitflag() & bodynode1->m_Desc.get_mask_bits()) == 0) {
		return false; // node2 のグループは node1 の判定対象ではない
	}

	// 親子関係にあるノード同士は衝突判定しない
	if (bodynode1->getNode()->isParentOf(bodynode2->getNode())) {
		return false;
	}
	if (bodynode2->getNode()->isParentOf(bodynode1->getNode())) {
		return false;
	}
	return true;
}
#pragma endregion // Functions




#pragma region Collision Gizmo
#define GIZMO_ARROW_SIZE 6
static void _DrawGround(KGizmo *gizmo, const KMatrix4 &matrix, const KColor &color, const KVec3 &pos) {
	// 地面を上向き矢印（法線）で表す。矢印先端の点が地面中心に一致する
	KVec3 p = pos;
	KVec3 from(p.x, p.y - 32, p.z);
	KVec3 to  (p.x, p.y, p.z);
	gizmo->addArrow(matrix, from, to, GIZMO_ARROW_SIZE, color);
}
static void _DrawLine(KGizmo *gizmo, const KMatrix4 &matrix, const KColor &color, const KVec3 &pos, const KVec3 &a, const KVec3 &b) {
	gizmo->addLine(matrix, pos+a, pos+b);
}
static void _DrawQuad(KGizmo *gizmo, const KMatrix4 &matrix, const KColor &color, const KVec3 &pos, const KVec3 &nor, const KVec3 *p, bool cross) {
	if (1) {
		float lenA = (p[1] - p[0]).getLength();
		float lenB = (p[2] - p[1]).getLength();
		if (lenA * 3 < lenB) {
			// lenB が長すぎる
			KVec3 pa = (p[1] + p[2]) / 2;
			KVec3 pb = (p[0] + p[3]) / 2;
			KVec3 Q[] = {p[0], p[1], pa, pb};
			KVec3 R[] = {pb, pa, p[2], p[3]};
			_DrawQuad(gizmo, matrix, color, pos, nor, Q, cross);
			_DrawQuad(gizmo, matrix, color, pos, nor, R, cross);
			return;

		} else if (lenB * 3 < lenA) {
			// lenA が長すぎる
			KVec3 pa = (p[0] + p[1]) / 2;
			KVec3 pb = (p[2] + p[3]) / 2;
			KVec3 Q[] = {pa, p[1], p[2], pb};
			KVec3 R[] = {p[0], pa, pb, p[3]};
			_DrawQuad(gizmo, matrix, color, pos, nor, Q, cross);
			_DrawQuad(gizmo, matrix, color, pos, nor, R, cross);
			return;
		}
	}
	// 平面を法線矢印で表す。矢印起点ではなく、矢印先端の点が平面中心に一致する
	KVec3 n = nor;
	gizmo->addArrow(matrix, pos+p[0], pos+p[0]+nor*16, GIZMO_ARROW_SIZE, color);
	gizmo->addArrow(matrix, pos+p[1], pos+p[1]+nor*16, GIZMO_ARROW_SIZE, color);
	gizmo->addArrow(matrix, pos+p[2], pos+p[2]+nor*16, GIZMO_ARROW_SIZE, color);
	gizmo->addArrow(matrix, pos+p[3], pos+p[3]+nor*16, GIZMO_ARROW_SIZE, color);

	#ifdef _DEBUG
	if (cross) {
		gizmo->addLine(matrix, pos+p[0], pos+p[2], color*KColor(1.0f, 1.0f, 1.0f, 0.3f));
		gizmo->addLine(matrix, pos+p[1], pos+p[3], color*KColor(1.0f, 1.0f, 1.0f, 0.3f));
	}
	#else
	if (cross) {
		gizmo->addLineDash(matrix, pos+p[0], pos+p[2], color*KColor(1.0f, 1.0f, 1.0f, 0.3f));
		gizmo->addLineDash(matrix, pos+p[1], pos+p[3], color*KColor(1.0f, 1.0f, 1.0f, 0.3f));
	}
	#endif
	gizmo->addLine(matrix, pos+p[0], pos+p[1], color);
	gizmo->addLine(matrix, pos+p[1], pos+p[2], color);
	gizmo->addLine(matrix, pos+p[2], pos+p[3], color);
	gizmo->addLine(matrix, pos+p[3], pos+p[0], color);
}
static void _DrawPlane(KGizmo *gizmo, const KMatrix4 &matrix, const KColor &color, const KVec3 &pos, const KVec3 &nor, float halfwidth) {
	if (fabsf(nor.y) <= 0.001f) {
		// XZ平面に垂直な平面の場合は、Ｙ＝０な場所にラインを引く
		// 範囲付き平面の場合は、平面幅が定義されていることに注意
		float r = halfwidth;
		float len = (r > 0) ? r : HUGE_LENGTH;
		KVec3 vec(-nor.z, 0.0f, nor.x);
		KVec3 a = pos + vec * len;
		KVec3 b = pos - vec * len;
		KVec3 p[] = {
			KVec3(a.x, -WALL_TALL*0.5f, a.z),
			KVec3(a.x,  WALL_TALL*0.5f, a.z),
			KVec3(b.x,  WALL_TALL*0.5f, b.z),
			KVec3(b.x, -WALL_TALL*0.5f, b.z),
		};
		_DrawQuad(gizmo, matrix, color, pos, nor, p, true);
	} else {
		// 平面を法線矢印で表す。矢印起点ではなく、矢印先端の点が平面中心に一致する
		KVec3 n = nor;
		KVec3 p = pos;
		KVec3 from = p - n;
		KVec3 to   = p;
		gizmo->addArrow(matrix, from, to, GIZMO_ARROW_SIZE, color);
	}
}
static void _DrawAabb(KGizmo *gizmo, const KMatrix4 &matrix, const KColor &color, const KVec3 &pos, const KVec3 &halfsize, float xshear) {
	if (xshear) {
		gizmo->addAabbSheared(matrix,
			pos - halfsize,
			pos + halfsize,
			xshear,
			color
		);
	} else {
		gizmo->addAabb(matrix,
			pos - halfsize,
			pos + halfsize,
			color
		);
	}
}
static void _DrawShearedBox(KGizmo *gizmo, const KMatrix4 &matrix, const KColor &color, const KVec3 &pos, const KVec3 &halfsize, float xshear) {
	KVec3 c = pos;

	// 上側の平行四辺形
	KVec3 a0(-halfsize.x+xshear, halfsize.y, halfsize.z), a1(halfsize.x+xshear, halfsize.y, halfsize.z);
	KVec3 a3(-halfsize.x-xshear, halfsize.y,-halfsize.z), a2(halfsize.x-xshear, halfsize.y,-halfsize.z);
	gizmo->addLine(matrix, c+a0, c+a1, color);
	gizmo->addLine(matrix, c+a1, c+a2, color);
	gizmo->addLine(matrix, c+a2, c+a3, color);
	gizmo->addLine(matrix, c+a3, c+a0, color);

	// 下側の平行四辺形
	KVec3 b0(-halfsize.x+xshear, -halfsize.y, halfsize.z), b1(halfsize.x+xshear, -halfsize.y, halfsize.z);
	KVec3 b3(-halfsize.x-xshear, -halfsize.y,-halfsize.z), b2(halfsize.x-xshear, -halfsize.y,-halfsize.z);
	gizmo->addLineDash(matrix, c+b0, c+b1, color);
	gizmo->addLine(matrix, c+b1, c+b2, color);
	gizmo->addLine(matrix, c+b2, c+b3, color);
	gizmo->addLineDash(matrix, c+b3, c+b0, color);

	// 上と下を結ぶ垂直線
	gizmo->addLineDash(matrix, c+a0, c+b0, color);
	gizmo->addLine(matrix, c+a1, c+b1, color);
	gizmo->addLine(matrix, c+a2, c+b2, color);
	gizmo->addLine(matrix, c+a3, c+b3, color);
}
static void _DrawFloor(KGizmo *gizmo, const KMatrix4 &matrix, const KColor &color, const KVec3 &pos, const KVec3 &halfsize, float xshear, const float *heights) {
	KVec3 c = pos;

	KVec3 a0(-halfsize.x+xshear, heights[1],  halfsize.z), a1(halfsize.x+xshear, heights[2],  halfsize.z);
	KVec3 a3(-halfsize.x-xshear, heights[0], -halfsize.z), a2(halfsize.x-xshear, heights[3], -halfsize.z);
	gizmo->addLine(matrix, c+a0, c+a1, color);
	gizmo->addLine(matrix, c+a1, c+a2, color);
	gizmo->addLine(matrix, c+a2, c+a3, color);
	gizmo->addLine(matrix, c+a3, c+a0, color);

	// スロープの高さ表現の点線
	KVec3 b0(-halfsize.x+xshear, 0.0f,  halfsize.z), b1(halfsize.x+xshear, 0.0f,  halfsize.z);
	KVec3 b3(-halfsize.x-xshear, 0.0f, -halfsize.z), b2(halfsize.x-xshear, 0.0f, -halfsize.z);
	gizmo->addLineDash(matrix, c+a0, c+b0, color);
	gizmo->addLineDash(matrix, c+a1, c+b1, color);
	gizmo->addLineDash(matrix, c+a2, c+b2, color);
	gizmo->addLineDash(matrix, c+a3, c+b3, color);
}
static void _DrawCapsule(KGizmo *gizmo, const KMatrix4 &matrix, const KColor &color, const KVec3 &pos, float radius, float halfheight) {
	if (radius < 0) radius = 0;
	if (halfheight < 0) halfheight = 0;
	KVec3 top(pos.x, pos.y+halfheight-radius, pos.z); // halfheight は半球を含んだサイズなので、半径を差し引いた場所を半球の中心とする
	KVec3 btm(pos.x, pos.y-halfheight+radius, pos.z);
	gizmo->addCircle(matrix, top, radius, 1/*Y-Axis*/, color);
	gizmo->addCircle(matrix, btm, radius, 1/*Y-Axis*/, color);
	gizmo->addLine(matrix, top+KVec3(-radius, 0.0f, 0.0f), btm+KVec3(-radius, 0.0f, 0.0f), color);
	gizmo->addLine(matrix, top+KVec3( radius, 0.0f, 0.0f), btm+KVec3( radius, 0.0f, 0.0f), color);
}
static void _DrawCylinder(KGizmo *gizmo, const KMatrix4 &matrix, const KColor &color, const KVec3 &pos, float radius, float halfheight) {
	if (radius < 0) radius = 0;
	if (halfheight < 0) halfheight = 0;
	KVec3 top(pos.x, pos.y+halfheight, pos.z);
	KVec3 btm(pos.x, pos.y-halfheight, pos.z);
	gizmo->addCircle(matrix, top, radius, 1/*Y-Axis*/, color);
	gizmo->addCircle(matrix, btm, radius, 1/*Y-Axis*/, color);
	gizmo->addLine(matrix, top+KVec3(-radius, 0.0f, 0.0f), btm+KVec3(-radius, 0.0f, 0.0f), color);
	gizmo->addLine(matrix, top+KVec3( radius, 0.0f, 0.0f), btm+KVec3( radius, 0.0f, 0.0f), color);
}
static void _DrawCylinderWithAltitude(KGizmo *gizmo, const KMatrix4 &matrix, const KColor &color, const KVec3 &pos, float radius, float halfheight, float altitude) {
	if (radius < 0) radius = 0;
	if (halfheight < 0) halfheight = 0;
	_DrawCylinder(gizmo, matrix, color, pos, radius, halfheight);

	// 高度を示す矢印の描画
	KVec3 p0 = pos; // 矢印の始点は判定の中心に
	KVec3 p1 = p0 + KVec3(0.0f, -altitude, 0.0f); // 矢印の終点はオブジェクト真下の地面に
	gizmo->addArrow(matrix, p0, p1, GIZMO_ARROW_SIZE, color);
}
static void _DrawCapsuleWithAltitude(KGizmo *gizmo, const KMatrix4 &matrix, const KColor &color, const KVec3 &pos, float radius, float halfheight, float altitude) {
	if (radius < 0) radius = 0;
	if (halfheight < 0) halfheight = 0;
	_DrawCapsule(gizmo, matrix, color, pos, radius, halfheight);

	// 高度を示す矢印の描画
	KVec3 p0 = pos; // 矢印の始点は判定の中心に
	KVec3 p1 = p0 + KVec3(0.0f, -halfheight - altitude, 0.0f); // 矢印の終点はオブジェクト真下の地面に
	gizmo->addArrow(matrix, p0, p1, GIZMO_ARROW_SIZE, color);
}
static void _DrawSphere(KGizmo *gizmo, const KMatrix4 &matrix, const KColor &color, const KVec3 &pos, float radius) {
	KVec3 btm(pos.x, pos.y-radius, pos.z);
	gizmo->addCircle(matrix, pos, radius, 1/*Y-Axis*/, color);
}
static void _DrawSphereWithAltitude(KGizmo *gizmo, const KMatrix4 &matrix, const KColor &color, const KVec3 &pos, float radius, float altitude) {
	KVec3 btm(pos.x, pos.y-radius, pos.z);
	gizmo->addCircle(matrix, pos, radius, 1/*Y-Axis*/, color);

	// 高度を示す矢印の描画
	KVec3 p0 = pos; // 矢印の始点は判定の中心に
	KVec3 p1 = p0 + KVec3(0.0f, - radius - altitude, 0.0f); // 矢印の終点はオブジェクト真下の地面に
	gizmo->addArrow(matrix, p0, p1, GIZMO_ARROW_SIZE, color);
}
#pragma endregion // Collision Gizmo







const float EXT_D = 4;
const KVec3 EXTENDS(EXT_D, EXT_D, EXT_D); // 最低でも D の厚みで判定するようにするための調整用

struct COL_INFO {
	KVec3 aabb_min, aabb_max; // AABB（ワールド座標）
	KVec3 prev_aabb_min, prev_aabb_max;   // 前回の位置におけるAABB（ワールド座標）
	KVec3 curr_aabb_min, curr_aabb_max;   // 今回の位置におけるAABB（ワールド座標）
	KVec3 whole_aabb_min, whole_aabb_max; // 前回と今回のAABBを両方含むAABB（ワールド座標）
	KVec3 extended_whole_aabb_min, extended_whole_aabb_max; // AABBの最低サイズを保証するために拡大したAABB（ワールド座標）

	void update(KCollider *collider, uint32_t bitmask, const KVec3 &speed) {
		collider->get_aabb(bitmask, &aabb_min, &aabb_max);
		prev_aabb_min = aabb_min - speed; // 前回の位置におけるAABB
		prev_aabb_max = aabb_max - speed;
		curr_aabb_min = aabb_min; // 今回の位置におけるAABB（ワールド座標）
		curr_aabb_max = aabb_max;
		whole_aabb_min = prev_aabb_min.getmin(curr_aabb_min); // 前回と今回のAABBを両方含むAABB（ワールド座標）
		whole_aabb_max = prev_aabb_max.getmax(curr_aabb_max);
		extended_whole_aabb_min = whole_aabb_min - EXTENDS;
		extended_whole_aabb_max = whole_aabb_max + EXTENDS;
	}
};


static void _GUI_ColliderAABB(KCollider *coll) {
	if (coll) {
		KVec3 wmin, wmax;
		KVec3 lmin, lmax;
		coll->get_aabb(0, &wmin, &wmax);
		coll->get_aabb_local(0, &lmin, &lmax);
		KDebugGui::K_DebugGui_Aabb("AABB (World)", wmin, wmax);
		KDebugGui::K_DebugGui_Aabb("AABB (Local)", lmin, lmax);
	}
}

// Returns N: 2^N == group_bit
static int _GetGroupBitPosition(uint32_t group_bit) {
	if (group_bit == 0) {
		K__ERROR("E_SETGROUPNAME: group_bit cannot be zero");
		return -1;
	}

	// 一番右のビットが 1 になるまで右シフトする。
	int i = 0;
	while ((group_bit & 1) == 0) {
		group_bit >>= 1;
		i++;
	}

	// この時点で i にはビットシフト数が入っている
	// group_bit は 0x0001 になっているはず。それ以外だった場合、ほかにも 1 になっているビットを含んでいる
	if (group_bit != 1) {
		K__ERROR("E_SETGROUPNAME: invalid group_bit: 0x%08x (contains multiple 1)", group_bit);
		return -1;
	}

	return i;
}

static void _BodyList_erase(KBodyList &bodynodes, KNode *knode) {
	for (auto it=bodynodes.begin(); it!=bodynodes.end(); /*none*/) {
		if ((*it)->getNode() == knode) {
			it = bodynodes.erase(it);
		} else {
			it++;
		}
	}
}


#pragma region CCollisionMgr
class CCollisionMgr: public KManager, public KInspectorCallback {
	KBodyList m_TmpTable;
	KBodyList m_TmpMovingNodes;
	KBodyList m_TmpDynamicNodes;
	std::unordered_map<KNode*, KSolidBody*> m_Nodes;
	KSolidBodyCallback *m_Callback;
	int m_Frame;
	bool m_AlwaysShowStaticCollider;
	bool m_AlwaysShowDynamicCollider;
	const char *m_GroupNames[sizeof(uint32_t) * 8];
	mutable std::recursive_mutex m_Mutex;
	mutable KBodyList m_TmpBodyList_Ground;
	mutable KBodyList m_TmpBodyList_Altitude;
	mutable KBodyList m_TmpBodyList_Ray;
	mutable KBodyList m_TmpBodyList_SphereCast;
	mutable KBodyList m_TmpBodyList_SphereCollide;
	mutable KBodyList m_TmpStaticBody_Collision;

	void lock() const {
	#if K_THREAD_SAFE
		m_Mutex.lock();
	#endif
	}
	void unlock() const {
	#if K_THREAD_SAFE
		m_Mutex.unlock();
	#endif
	}

public:
	CCollisionMgr() {
		m_Frame = -1;
		m_AlwaysShowStaticCollider  = false;
		m_AlwaysShowDynamicCollider = false;
		m_Callback = nullptr;
		memset(m_GroupNames, 0, sizeof(m_GroupNames));

		KEngine::addManager(this);
		KEngine::addInspectorCallback(this, u8"衝突/物理"); // KInspectorCallback
		KEngine::setManagerRenderDebugEnabled(this, true); // on_manager_renderdebug
		m_Callback = nullptr;
	}
	virtual ~CCollisionMgr() {
		on_manager_end();
	}

	#pragma region KManager
	virtual void on_manager_end() override {
		K__ASSERT(m_Nodes.empty()); // 正しく on_manager_detach が呼ばれていれば、この時点でノード数はゼロのはず
	}
	virtual bool on_manager_isattached(KNode *node) override {
		return getBody(node) != nullptr;
	}
	virtual void on_manager_detach(KNode *node) override {
		detachBody(node);

		// ※ tmp はあくまでも一時的な計算用であるため
		// ここで更新する必要はないはずだが、
		// Debug 情報を表示しつつ該当エンティティをインスペクターから削除した場合など
		// 削除済みエンティティの gizmo を描画しようとしてエラーになる場合がある。
		_BodyList_erase(m_TmpTable, node);
		_BodyList_erase(m_TmpMovingNodes, node);
		_BodyList_erase(m_TmpDynamicNodes, node);

		lock();
		{
			auto it = m_Nodes.find(node);
			if (it != m_Nodes.end()) {
				KSolidBody *body = it->second;
				body->_setNode(nullptr);
				body->drop();
				m_Nodes.erase(it);
			}
		}
		unlock();
	}
	virtual void on_manager_beginframe() override {
		m_Frame++;
	}
	virtual void on_manager_frame() override {
		lock();
		{
			for (auto it=m_Nodes.begin(); it!=m_Nodes.end(); ++it) {
				KSolidBody *bodynode = it->second;
				if (bodynode->getNode()->getEnableInTree()) {
					if (bodynode->m_Desc.get_sleep_time() > 0) {
						bodynode->m_Desc._sleep_time--;
					}
				}
				bodynode->m_Desc._hits_with.clear();
			}
			if (1) {
				if (m_Callback) m_Callback->on_collision_update_start();
				// 衝突処理を行う必要があるエンティティだけをリストアップする
				update_dynamicbody_list_unsafe(&m_TmpMovingNodes, &m_TmpDynamicNodes);

				// 速度コンポーネントにしたがって位置更新（衝突考慮しない）
				update_dynamicbody_positions_unsafe();

				// 物理判定同士の衝突処理
				update_dynamicbody_collision_unsafe();

				// 地形判定と物理判定の衝突処理
				update_staticbody_collision_unsafe();

				if (m_Callback) m_Callback->on_collision_update_end();
			} else {
				// 座標だけ更新する。衝突判定しない
				update_dynamicbody_list_unsafe(&m_TmpMovingNodes, &m_TmpDynamicNodes);
				update_dynamicbody_positions_unsafe();
			}
		}
		unlock();
	}
	virtual void on_manager_nodeinspector(KNode *node) override {
	#ifndef NO_IMGUI
		// 物理属性
		lock();
		{
			KSolidBody *bodynode = getBody(node);
			if (bodynode) {
				ImGui::Separator();
				if (bodynode->m_Desc.is_static()) {
					if (ImGui::TreeNode("StaticBody")) {
						update_staticbody_inspector(bodynode);
						ImGui::TreePop();
					}
				} else {
					if (ImGui::TreeNode("DynamicBody")) {
						update_dynamicbody_inspector(bodynode);
						ImGui::TreePop();
					}
				}
			}
		}
		unlock();
	#endif // !NO_IMGUI
	}
	virtual void on_manager_renderdebug(KNode *cameranode, KNode *start, KEntityFilterCallback *filter) override {
		bool show_debug = KInspector::isDebugInfoVisible(this);
		if (m_AlwaysShowStaticCollider || show_debug) {
			draw_staticbody_gizmo_list(cameranode);
		}
		if (m_AlwaysShowDynamicCollider || show_debug) {
			draw_dynamicbody_gizmo_list(cameranode);
		}
	}
	#pragma endregion // Manager

	#pragma region KInspectorCallback
	virtual void onInspectorGui() override {
	#ifndef NO_IMGUI
		ImGui::Text("Frames: %d", m_Frame);
		ImGui::Checkbox("Always show static gizmos",  &m_AlwaysShowStaticCollider);
		ImGui::Checkbox("Always show dynamic gizmos", &m_AlwaysShowDynamicCollider);
	#endif // !NO_IMGUI
	}
	#pragma endregion // KInspectorCallback

	void setCallback(KSolidBodyCallback *cb) {
		m_Callback = cb;
	}
	void attachVelocity(KNode *node) {
		// 速度コンポーネントは剛体と共有する。
		addDynamicBody(node, false);
	}

	void addStaticBody(KNode *node) {
		lock();
		{
			// static body がアタッチ済みの場合は何もしない。
			// それ以外がアタッチされている場合はエラー
			KSolidBody *body = getBody(node);
			if (body) {
				if (dynamic_cast<KStaticSolidBody *>(body)) {
					return; // アタッチ済み
				}
				K__ERROR("Incompatible collision-body attached");
				return;
			}
		}
		{	// static body を追加
			KStaticSolidBody *e = new KStaticSolidBody();
			e->_setNode(node);
			m_Nodes[node] = e;
		}
		unlock();
	}
	void addDynamicBody(KNode *node, bool with_default_shape) {
		lock();
		{
			// dynamic body がアタッチ済みの場合は何もしない。
			// それ以外がアタッチされている場合はエラー
			KSolidBody *body = getBody(node);
			if (body) {
				if (dynamic_cast<KDynamicSolidBody *>(body)) {
					return; // アタッチ済み
				}
				K__ERROR("Incompatible collision-body attached");
				return;
			}
		}
		{	// dynamic body を追加
			KDynamicSolidBody *e = new KDynamicSolidBody(with_default_shape);
			e->_setNode(node);
			m_Nodes[node] = e;
		}
		unlock();
	}
	KSolidBody * getBody(KNode *node) {
		auto it = m_Nodes.find(node);
		if (it != m_Nodes.end()) {
			return it->second;
		}
		return nullptr;
	}
	KStaticSolidBody * getStaticBody(KNode *node) {
		return dynamic_cast<KStaticSolidBody *>(getBody(node));
	}
	KDynamicSolidBody * getDynamicBody(KNode *node) {
		return dynamic_cast<KDynamicSolidBody *>(getBody(node));
	}
	void detachBody(KNode *node) {
		lock();
		{
			auto it = m_Nodes.find(node);
			if (it != m_Nodes.end()) {
				it->second->drop();
			}
			m_Nodes.erase(node);
		}
		unlock();
	}
	float getAltitudeAtPoint_unsafe(const KVec3 &point, float max_penetration, KNode **out_ground) {
		float ret = -1;
		{
			KSolidBody *node = nullptr;
			KBodyList &bodylist = m_TmpBodyList_Altitude;
			bodylist.clear();
			get_active_static_body_list_unsafe(bodylist);
			float alt = -1;
			if (get_altitude(bodylist, point, max_penetration, &alt, &node)) {
				if (out_ground) *out_ground = node->getNode();
				ret = alt;
			}
		}
		return ret;
	}

	/// 指定された地点 point の真下にある地面を探し、そこからの高度を得る。
	/// 点の真下に地面が存在しない場合は -1 を返す。（それ以外にこの関数が負の値を返すことはない）
	/// ただし、max_penetration 距離までならば指定地点の「上側」の地面も検出する。つまり地面から max_penetration までならば「めり込んで」いても良い。
	/// max_penetration には 0 以上の値を指定する。
	float getAltitudeAtPoint(const KVec3 &point, float max_penetration, KNode **out_ground) {
		float ret = -1;
		lock();
		{
			ret = getAltitudeAtPoint_unsafe(point, max_penetration, out_ground);
		}
		unlock();
		return ret;
	}
	bool getGroundPoint_unsafe(const KVec3 &pos, float max_penetration, float *out_ground_y, KNode **out_ground) {
		KBodyList &bodylist = m_TmpBodyList_Ground;
		bodylist.clear();
		get_active_static_body_list_unsafe(bodylist);

		KSolidBody *node = nullptr;
		if (get_ground_point(bodylist, pos, max_penetration, out_ground_y, &node)) {
			if (out_ground) *out_ground = node->getNode();
			return true;
		}
		return false;
	}
	/// 指定された地点 point の真下にある地面を探す。
	/// 地面が存在すれば、その地面の y 座標を out_ground_y にセットして true を返す。
	/// 点の真下に地面が存在しない場合は何もせずに false を返す。
	/// この関数は pos よりも「下側」の地面を探すが、max_penetration 距離までならば指定地点の「上側」の地面も検出する。
	/// つまり地面から max_penetration までならば「めり込んで」いても良い。
	/// max_penetration には 0 以上の値を指定する。
	bool getGroundPoint(const KVec3 &pos, float max_penetration, float *out_ground_y, KNode **out_ground) {
		bool ret = false;
		lock();
		{
			ret = getGroundPoint_unsafe(pos, max_penetration, out_ground_y, out_ground);
		}
		unlock();
		return ret;
	}
	/// Body を持っているなら、その高度を得る。
	/// ※高度とは、剛体の底面高さと、その直下にある地面高さの差を表す
	/// Bodyが存在しないか、Bodyの真下に地面が全く存在せずに高度を定義できない場合、false を返す
	bool getDynamicBodyAltitude(KNode *node, float *alt) {
		bool ret = false;
		lock();
		{
			KSolidBody *body = getBody(node);
			KVec3 pmin, pmax;
			if (body && body->getAabbWorld(&pmin, &pmax)) {
				// 底面の中心
				KVec3 bottom;
				bottom.x = (pmin.x + pmax.x) / 2;
				bottom.z = (pmin.z + pmax.z) / 2;
				bottom.y = pmin.y;
				float altval = getAltitudeAtPoint_unsafe(bottom, 1.0f, nullptr);
				if (altval >= 0.0f) {
					if (alt) *alt = altval;
					ret = true;
				}
				// 底なし
			}
		}
		unlock();
		return ret;
	}
	/// 真下の地面に着地させる
	void snapToGround(KNode *node) {
		K__ASSERT_RETURN(node);
		lock();
		{
			KVec3 pos = node->getPosition();
			// pos の真下にある地面座標と地面エンティティを得る
			KNode *ground_e = nullptr;
			float ground_y = 0;
			if (getGroundPoint_unsafe(pos, 10, &ground_y, &ground_e)) {
				// 接地させる
				pos.y = ground_y;
				node->setPosition(pos);

				// 接地フラグを設定する
				KSolidBody *dyNode = getBody(node);
				if (dyNode) {
					dyNode->m_Desc.set_altitude(0, ground_e);
				}
			}
		}
		unlock();
	}
	void setDebugLineVisible(bool static_lines, bool dynamic_lines) {
		m_AlwaysShowStaticCollider  = static_lines;
		m_AlwaysShowDynamicCollider = dynamic_lines;
	}
	void setDebug_alwaysShowDebug(bool val) {
		KInspectableDesc *desc = KInspector::getDesc(this);
		if (desc) {
			desc->always_show_debug = val;
		}
	}
	bool getDebug_alwaysShowDebug() {
		KInspectableDesc *desc = KInspector::getDesc(this);
		if (desc) {
			return desc->always_show_debug;
		}
		return false;
	}
	bool rayCastEnumerate(const KVec3 &start, const KVec3 &dir, float maxdist, KRayCallback *cb) {
		if (cb == nullptr) return false;

		KVec3 nDir;
		if (!dir.getNormalizedSafe(&nDir)) return false; // レイの向きを定義できない

		// maxDist が指定されているなら、レイの始点と終点を含むAABBを得る
		KVec3 rayAabbMin;
		KVec3 rayAabbMax;
		if (maxdist > 0) {
			KVec3 end = start + nDir * maxdist;
			rayAabbMin = start.getmin(end);
			rayAabbMax = start.getmax(end);
		}
		uint32_t bitmask = 0;

		lock();
		{
			KBodyList &bodylist = m_TmpBodyList_Ray;
			bodylist.clear();
			get_active_static_body_list_unsafe(bodylist); // 地形用オブジェクトリスト

			for (auto it=bodylist.begin(); it!=bodylist.end(); ++it) {
				KSolidBody *bodynode = *it;
				KCollider *collider = bodynode->getShape();
				if (maxdist > 0) {
					if (!collider->is_collide_with_aabb(rayAabbMin, rayAabbMax, bitmask)) {
						continue; // AABBの交差なし
					}
				}
				KVec3 pos, nor;
				KCollider *coll = nullptr;
				if (collider->get_ray_collision_point(start, nDir, bitmask, &pos, &nor, &coll)) {
					float dist = (pos - start).getLength();
					if (maxdist < 0 || dist < maxdist) {
						KRayHit hit;
						hit.pos = pos;
						hit.dist = dist;
						hit.normal = nor;
						hit.collider = coll;
						cb->on_ray_hit(start, dir, maxdist, hit);
					}
				}
			}
		}
		unlock();
		return true;
	}
	
	class CRayCB: public KRayCallback {
	public:
		float mDist;
		KVec3 mPos;
		KVec3 mNormal;
		KCollider *mCollider;

		CRayCB() {
			mDist = FLT_MAX;
			mCollider = nullptr;
		}
		virtual void on_ray_hit(const KVec3 &start, const KVec3 &dir, float maxdist, const KRayHit &hit) override {
			if (hit.dist < mDist) {
				mPos = hit.pos;
				mDist = hit.dist;
				mNormal = hit.normal;
				mCollider = hit.collider;
			}
		}
	};

	/// pos から dir 方向に向かって判定レイを飛ばす
	/// KRayHit::data にはヒットした場所にある KCollider が入る
	bool rayCast(const KVec3 &start, const KVec3 &dir, float maxdist, KRayHit *out_point) {
		CRayCB cb;
		rayCastEnumerate(start, dir, maxdist, &cb);
		if (cb.mCollider) {
			if (out_point) {
				out_point->pos = cb.mPos;
				out_point->dist = cb.mDist;
				out_point->normal = cb.mNormal;
				out_point->collider = cb.mCollider;
			}
			return true;
		}
		return false;
	}

	/// pos から dir 方向に向かって判定球を飛ばす
	bool sphereCast(const KVec3 &start, float radius, const KVec3 &dir, float maxdist, KRayHit *out_point) {
		bool ret = false;
		lock();
		{
			ret = sphereCastUnsafe(start, radius, dir, maxdist, out_point);
		}
		unlock();
		return ret;
	}
	bool sphereCastUnsafe(const KVec3 &start, float radius, const KVec3 &dir, float maxdist, KRayHit *out_point) {
		KVec3 nDir;
		if (!dir.getNormalizedSafe(&nDir)) return false; // レイの向きを定義できない

		KBodyList &bodylist = m_TmpBodyList_SphereCast;
		bodylist.clear();
		get_active_static_body_list_unsafe(bodylist); // 地形用オブジェクトリスト

		KVec3 aabb_min, aabb_max;
		if (bodylist.size() > 0) {
			bodylist[0]->getShape()->get_aabb(0, &aabb_min, &aabb_max);
			for (int i=1; i<(int)bodylist.size(); i++) {
				KVec3 minp, maxp;
				bodylist[i]->getShape()->get_aabb(0, &minp, &maxp);
				aabb_min = aabb_min.getmin(minp);
				aabb_max = aabb_max.getmax(maxp);
			}
		}

		float step = radius;
		if (step < 8) step = 8;
		if (maxdist > 1000) maxdist = 1000;

		for (float dist=0; dist<maxdist; dist+=step) {
			KVec3 pos = start + nDir * dist;
			if (pos.x < aabb_min.x || aabb_max.x < pos.x) return false;
			if (pos.y < aabb_min.y || aabb_max.y < pos.y) return false;
			if (pos.z < aabb_min.z || aabb_max.z < pos.z) return false;
			KCollider *coll = get_sphere_collide(pos, radius, bodylist);
			if (coll) {
				if (out_point) {
					out_point->pos = pos;
					out_point->dist = dist;
					out_point->normal = -nDir;
					out_point->collider = coll;
				}
				return true;
			}
		}
		return false;
	}

	/// pos に半径 radius の球を置いたときに static 形状に接触するかどうか
	bool getSphereCollide(const KVec3 &pos, float radius) {
		bool ret = false;
		lock();
		{
			KBodyList &bodylist = m_TmpBodyList_SphereCollide;
			bodylist.clear();
			get_active_static_body_list_unsafe(bodylist); // 地形用オブジェクトリスト
			ret = get_sphere_collide(pos, radius, bodylist);
		}
		unlock();
		return ret;
	}
	void setCollisionGroupBits(KNode *node, uint32_t group_bits) {
		K__ASSERT(node);
		lock();
		{
			KCollider *coll = getBody(node)->getShape();
			if (coll) coll->set_bitflag(group_bits);
		}
		unlock();
	}
	void setCollisionMaskBits(KNode *node, uint32_t mask_bits) {
		lock();
		{
			KSolidBody *body = getBody(node);
			if (body) {
				body->m_Desc.set_mask_bits(mask_bits);
			}
		}
		unlock();
	}
	// グループ名を設定する。デバッグ用
	void setGroupName(uint32_t group_bit, const char *name) {
		int i = _GetGroupBitPosition(group_bit);
		if (i >= 0) {
			m_GroupNames[i] = name;
		}
	}
	const char * getGroupName(uint32_t group_bit) const {
		int i = _GetGroupBitPosition(group_bit);
		if (i >= 0) {
			return m_GroupNames[i];
		}
		return nullptr;
	}
	// 法線を指定して、その面の属性を得る（地面として扱うか、壁としてあつかうかなど）
	// 法線がほとんど上を向いていて、地面や床として扱ってよい（キャラクターがその上に立つことができる）のなら isGround に true をセットして true を返す
	// 法線が水平方向を向いていて、壁として扱う必要があるなら isWall に true をセットして true を返す
	// そのどちらでもない場合は両方に false をセットして true を返す
	// 判定できない場合は何もせずに false を返す
	bool getSurfaceType(const KVec3 &normalized_normal, bool *is_ground, bool *is_wall) const {
		K__ASSERT(is_ground);
		K__ASSERT(is_wall);
		if (normalized_normal.isZero()) {
			return false;
		}
		*is_ground = false;
		*is_wall = false;

		if (normalized_normal.y < -0.7f) {
			// 法線がだいたい下向き（天井）

		} else if (normalized_normal.y < 0.7f) {
			// 法線がだいたい垂直（壁）
			*is_wall = true;

		} else {
			// 法線がだいたい上向き（地面）
			*is_ground = true;
		}
		return true;
	}

private:
	void update_bitmask_inspector(KSolidBody *bodynode) {
		KCollider *coll = bodynode->getShape();
		if (coll == nullptr) {
			ImGui::Text("No collider");
			return;
		}
		if (ImGui::TreeNode((void*)1, "Group Bits: 0x08%X", coll->get_bitflag())) {
			uint32_t bitmask = coll->get_bitflag();
			bool chg = false;
			for (int i=0; i<32; i++) {
				char s[256] = {0};
				sprintf_s(s, sizeof(s), "0x%08X: %s", 1<<i, m_GroupNames[i] ? m_GroupNames[i] : "(no name)");
				chg |= ImGui::CheckboxFlags(s, &bitmask, 1<<i);
			}
			if (chg) {
				coll->set_bitflag(bitmask);
			}
			ImGui::TreePop();
		}
		if (ImGui::TreeNode((void*)2, "Mask Bits: 0x08%X", bodynode->m_Desc.get_mask_bits())) {
			for (int i=0; i<32; i++) {
				char s[256] = {0};
				sprintf_s(s, sizeof(s), "0x%08X: %s", 1<<i, m_GroupNames[i] ? m_GroupNames[i] : "(no name)");
				uint32_t bits = bodynode->m_Desc.get_mask_bits();
				if (ImGui::CheckboxFlags(s, &bits, 1<<i)) {
					bodynode->m_Desc.set_mask_bits(bits);
				}
			}
			ImGui::TreePop();
		}
	}
	void update_staticbody_inspector(KSolidBody *bodynode) {
		if (bodynode && bodynode->getNode()) {
			KCollider *coll = bodynode->getShape();
			if (coll) {
				// Enabled
				{
					bool b = bodynode->getBodyEnabled();
					if (ImGui::Checkbox("Body Enabled", &b)) {
						bodynode->setBodyEnabled(b);
					}
				}

				// Collider
				{
					update_bitmask_inspector(bodynode);
					_GUI_ColliderAABB(coll);
					if (coll) {
						KVec3 value = coll->get_offset_world();
						ImGui::Text("Center in world: %g, %g, %g", value.x, value.y, value.z);
						coll->update_inspector();
					}
				}
			}
		}
	}
	void update_dynamicbody_inspector(KSolidBody *bodynode) {
	#ifndef NO_IMGUI
		if (bodynode == nullptr) return;
		
		KCollider *coll = bodynode->getShape();
		
		// Enabled
		{
			bool b = bodynode->getBodyEnabled();
			if (ImGui::Checkbox("Body Enabled", &b)) {
				bodynode->setBodyEnabled(b);
			}
		}
		// Collider
		{
			update_bitmask_inspector(bodynode);
			_GUI_ColliderAABB(coll);
			if (coll) {
				KVec3 value = coll->get_offset_world();
				ImGui::Text("Center in world: %g, %g, %g", value.x, value.y, value.z);
				coll->update_inspector();
			}
		}

		KVec3 velocity = bodynode->m_Desc.get_velocity();
		if (ImGui::DragFloat3("Speed", (float*)&velocity)) {
			bodynode->m_Desc.set_velocity(velocity);
		}
		ImGui::Text("Speed length: %g", bodynode->m_Desc.get_velocity().getLength());

		KVec3 prev_prev_pos = bodynode->m_Desc.get_prev_prev_pos();
		KVec3 prev_pos = bodynode->m_Desc.get_prev_pos();
		ImGui::InputFloat3("Prev Prev pos", (float*)&prev_prev_pos, "%.1f", ImGuiInputTextFlags_ReadOnly);
		ImGui::InputFloat3("Prev pos", (float*)&prev_pos, "%.1f", ImGuiInputTextFlags_ReadOnly);

		KVec3 act_speed = bodynode->m_Desc.get_actual_speed();
		KVec3 act_accel = bodynode->m_Desc.get_actual_accel();
		ImGui::Text("Actual speed: %g", act_speed.getLength());
		ImGui::Text("Actial accel: %g", act_accel.getLength());
		ImGui::InputFloat3("Actial speed", (float*)&act_speed, "%.1f", ImGuiInputTextFlags_ReadOnly);
		ImGui::InputFloat3("Actial accel", (float*)&act_accel, "%.1f", ImGuiInputTextFlags_ReadOnly);
		{
			float skin_width = bodynode->m_Desc.get_skin_width();
			if (ImGui::DragFloat("Skin width", &skin_width)) {
				bodynode->m_Desc.set_skin_width(skin_width);
			}
		}
		{
			float snap_height = bodynode->m_Desc.get_snap_height();
			if (ImGui::DragFloat("Snap height", &snap_height)) {
				bodynode->m_Desc.set_snap_height(snap_height);
			}
		}
		{
			float sliding_friction = bodynode->m_Desc.get_sliding_friction();
			if (ImGui::DragFloat("Sliding friction", &sliding_friction, 0.1f, 0.0f, 10.0f)) {
				bodynode->m_Desc.set_sliding_friction(sliding_friction);
			}
		}
		{
			float penetratin_response = bodynode->m_Desc.get_penetratin_response();
			if (ImGui::DragFloat("Penetration response", &penetratin_response)) {
				bodynode->m_Desc.set_penetratin_response(penetratin_response);
			}
		}
		{
			float climb_height = bodynode->m_Desc.get_climb_height();
			if (ImGui::DragFloat("Climb height", &climb_height)) {
				bodynode->m_Desc.set_climb_height(climb_height);
			}
		}
		{
			float bounce = bodynode->m_Desc.get_bounce();
			if (ImGui::DragFloat("Bounce (Vert)", &bounce, 0.1f, 0.0f, 1.0f)) {
				bodynode->m_Desc.set_bounce(bounce);
			}

			float bounce_horz = bodynode->m_Desc.get_bounce_horz();
			if (ImGui::DragFloat("Bounce (Horz)", &bounce_horz, 0.1f, 0.0f, 1.0f)) {
				bodynode->m_Desc.set_bounce_horz(bounce_horz);
			}

			float bounce_min_speed = bodynode->m_Desc.get_bounce_min_speed();
			if (ImGui::DragFloat("Bounce min speed", &bounce_min_speed, 0.1f)) {
				bodynode->m_Desc.set_bounce_min_speed(bounce_min_speed);
			}
		}
		{
			float gravity = bodynode->m_Desc.get_gravity();
			if (ImGui::DragFloat("Gravity", &gravity, 0.1f)) {
				bodynode->m_Desc.set_gravity(gravity);
			}
		}
		{
			bool slip_ctrl = bodynode->m_Desc._slip_control;
			if (ImGui::Checkbox("Slip ctrl", &slip_ctrl)) {
				bodynode->m_Desc._slip_control = slip_ctrl;
			}

			float slip_deg = bodynode->m_Desc._slip_limit_degrees;
			if (ImGui::DragFloat("No slip deg (less)", &slip_deg, 0.1f)) {
				bodynode->m_Desc.set_gravity(slip_deg);
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(u8"衝突時の角度によるスリップの有無。進行方向と衝突面法線の角度差がこの値未満だったら正面衝突とみなし、滑らずに停止する");
			}

			float slip_spd = bodynode->m_Desc._slip_limit_speed;
			if (ImGui::DragFloat("No slip spd (more)", &slip_spd, 0.1f)) {
				bodynode->m_Desc.set_gravity(slip_spd);
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(u8" 衝突時の速度によるスリップの有無。この速度未満だったら滑り、以上だったら滑らずに停止する");
			}
		}
		{
			float alt = -1;
			if (getDynamicBodyAltitude(bodynode->getNode(), &alt)) {
				ImGui::Text("Altitude: %g", alt);
			} else {
				ImGui::Text("Altitude: N/A");
			}
		}
		{
			if (bodynode->m_Desc.get_sleep_time() == 0) {
				KImGui::PushTextColor(KImGui::COLOR_DEFAULT());
				ImGui::Text("Sleep Off");
			} else if (bodynode->m_Desc.get_sleep_time() > 0) {
				KImGui::PushTextColor(KImGui::COLOR_WARNING);
				ImGui::Text("Sleep On (%d)", bodynode->m_Desc.get_sleep_time());
			} else {
				KImGui::PushTextColor(KImGui::COLOR_WARNING);
				ImGui::Text("Sleep: On (INF)");
			}
		}
		KImGui::PopTextColor();

		if (bodynode->m_Desc._ground_node) {
			KPath name = bodynode->m_Desc._ground_node->getNameInTree();
			ImGui::Text("Ground: %s", name.c_str());
		} else {
			ImGui::Text("Ground: %s", "(null)");
		}

		ImGui::Text("Hits with: "); ImGui::SameLine();
		if (bodynode->m_Desc._hits_with.size() > 0) {
			for (int i=0; i<(int)bodynode->m_Desc._hits_with.size(); i++) {
				std::string s = bodynode->m_Desc._hits_with[i]->getNameInTree();
				ImGui::Text("  %s", s.c_str());
			}
		} else {
			ImGui::Text("(none)");
		}
	
	#endif // !NO_IMGUI
	}
	bool get_ground_point(const KBodyList &bodylist, const KVec3 &pos, float max_penetration, float *out_ground_y, KSolidBody **out_ground_bodynode) const {
		uint32_t bitmask = 0;

		const float INVALID_Y = -1000000;
		KSolidBody *gnd_bodynode = nullptr;
		float ground_y = INVALID_Y;
		KVec3 raypos = pos + KVec3(0.0f, max_penetration, 0.0f);
		for (auto it=bodylist.begin(); it!=bodylist.end(); ++it) {
			KSolidBody *bodynode = *it;
			KVec3 hit;
			KCollider *coll = bodynode->getShape();
			if (coll && coll->get_ray_collision_point(raypos, KVec3(0, -1, 0), bitmask, &hit, nullptr, nullptr)) {
				if (ground_y < hit.y) {
					ground_y = hit.y;
					gnd_bodynode = (*it);
				}
			}
		}
		if (gnd_bodynode) {
			if (out_ground_y) *out_ground_y = ground_y;
			if (out_ground_bodynode) *out_ground_bodynode = gnd_bodynode;
			return true;
		}
		return false;
	}
	bool get_altitude(const KBodyList &bodylist, const KVec3 &point, float max_penetration, float *out_alt, KSolidBody **out_ground) const {
		float gnd_y;
		if (get_ground_point(bodylist, point, max_penetration, &gnd_y, out_ground)) {
			if (out_alt) *out_alt = point.y - gnd_y;
			return true;
		}
		return false;
	}
	void draw_staticbody_gizmo_list(KNode *cameranode) {
		KGizmo *gizmo = KScreen::getGizmo();

		KMatrix4 tr;
		cameranode->getWorld2LocalMatrix(&tr);

		// オブジェクトリスト
		KBodyList list;
		get_active_static_body_list_unsafe(list);

		for (auto it=list.begin(); it!=list.end(); ++it) {
			KSolidBody *body = *it;
			if (_IsDebugInfoVisible(body, cameranode)) {
				draw_staticbody_gizmo_each(gizmo, body, tr);
			}
		}
	}
	void draw_staticbody_gizmo_each(KGizmo *gizmo, KSolidBody *cNode, const KMatrix4 &transform) const {
		K__ASSERT(cNode);
		if (cNode==nullptr || cNode->getNode()==nullptr) return;
		const float line_alpha = _GetGizmoBlinkingAlpha(cNode->getNode());
		KColor color(0.0f, 1.0f, 1.0f, line_alpha);


		KMatrix4 matrix;
		cNode->getNode()->getLocal2WorldMatrix(&matrix);
		matrix = matrix * transform;

		{
			KQuadCollider *coll = dynamic_cast<KQuadCollider*>(cNode->getShape());
			if (coll) {
				KVec3 p[4], nor;
				coll->get_points(p);
				nor = coll->get_normal();
				// 画面奥向きの壁の場合は簡略表示する
				if (KVec3(0, 0, 1).dot(nor) > 0) {
					_DrawQuad(gizmo, matrix, color * KColor(1.0f, 1.0f, 1.0f, 0.3f), coll->get_offset(), nor, p, false);
				} else {
					_DrawQuad(gizmo, matrix, color, coll->get_offset(), nor, p, true);
				}
				return;
			}
		}
		{
			KShearedBoxCollider *coll = dynamic_cast<KShearedBoxCollider*>(cNode->getShape());
			if (coll) {
				_DrawAabb(gizmo, matrix, color, coll->get_offset(), coll->get_halfsize(), coll->get_shearx());
				return;
			}
		}
		{
			KFloorCollider *coll = dynamic_cast<KFloorCollider*>(cNode->getShape());
			if (coll) {
				float heigths[4];
				coll->get_heights(heigths);
				_DrawFloor(gizmo, matrix, color, coll->get_offset(), coll->get_halfsize(), coll->get_shearx(), heigths);
				return;
			}
		}
		{
			KCapsuleCollider *coll = dynamic_cast<KCapsuleCollider*>(cNode->getShape());
			if (coll) {
				float radius = coll->get_radius();
				float halfheight = coll->get_halfheight();
				if (coll->get_cylinder()) {
					_DrawCylinder(gizmo, matrix, color, coll->get_offset(), radius, halfheight);
				} else {
					_DrawCapsule(gizmo, matrix, color, coll->get_offset(), radius, halfheight);
				}
				return;
			}
		}
		{
			KAabbCollider *coll = dynamic_cast<KAabbCollider*>(cNode->getShape());
			if (coll) {
				_DrawAabb(gizmo, matrix, color, coll->get_offset(), coll->get_halfsize(), 0);
				return;
			}
		}
		{
			KPlaneCollider *coll = dynamic_cast<KPlaneCollider*>(cNode->getShape());
			if (coll) {
				_DrawGround(gizmo, matrix, color, coll->get_offset());
				return;
			}
		}
	}
	void get_active_static_body_list_unsafe(KBodyList &out_bodylist) {
		out_bodylist.clear();

		// 静止オブジェクト側のフィルタリング
		for (auto it=m_Nodes.begin(); it!=m_Nodes.end(); ++it) {
			KSolidBody *bodynode = it->second;
			if (!bodynode->m_Desc.is_static()) {
				continue; // NOT STATIC
			}

			// コライダーが無効になっていたら判定しない
			if (bodynode->getBodyEnabled() == false) {
				continue;
			}

			// Entity がツリー内で disabled だったら判定しない
			// （paused 状態は考慮しない。オブジェクトの動きが停止している時でも、勝手に衝突判定を突き抜けられては困る）
			if (!bodynode->getNode()->getEnableInTree()) {
				continue;
			}

			// CollisionMask が 0 の場合は誰とも衝突しない
			if (bodynode->m_Desc.get_mask_bits() == 0) {
				continue;
			}

			out_bodylist.push_back(bodynode);
		}
	}
	bool collide_ground(const KBodyList &bodylist, KSolidBody *dyBody, const KVec3 &dySpeed, KSolidBodyCallback *cb, KVec3 *out_dySpeed) {
		KCollider *collider = dyBody->getShape();
		if (out_dySpeed) *out_dySpeed = dySpeed;
	
		// ボディ中央座標
		KVec3 dy_center = collider->get_offset_world();

		// ボディ中央を基準にしたときの高度を測る。ボディ底面から測るのはダメ。そこを基準にしてしまうと、
		// 基準点より上の地面がすべて無視される。
		float alt = 0;
		KSolidBody *stNode = nullptr;
		if (!get_altitude(bodylist, dy_center, dyBody->m_Desc.get_snap_height(), &alt, &stNode)) {
			// 地面が存在しない。高度値無効
			dyBody->m_Desc.clear_altitude();
			return false;
		}

		// ボディ底面からの高度値になおす
		{
			uint32_t bitmask = 0;
			KVec3 dy_aabb_min;
			collider->get_aabb_raw(bitmask, &dy_aabb_min, nullptr);
			alt += dy_aabb_min.y;
		}

		// 判定するかどうかを問い合わせる
		if (!_PassBodyFilter(stNode, dyBody)) {
			// 地面への着地判定なし。高度だけ書き換える
			dyBody->m_Desc.set_altitude(alt, nullptr);
			return false;
		}
		bool deny = false;
		if (cb) cb->on_collision_ground(stNode->getNode(), dyBody->getNode(), &deny);
		if (deny) {
			// 地面への着地判定なし。高度だけ書き換える
			dyBody->m_Desc.set_altitude(alt, nullptr);
			return false;
		}

		// 上昇中であれば着地しない
		if (dySpeed.y > 0) {
			dyBody->m_Desc.set_altitude(alt, nullptr);
			return false;
		}

		// 下降中の場合、いきなりスナップすると不自然なので
		// かなり地面ぎりぎりになるまで着地しないようにする
		if (dySpeed.y < 0) {
			if (alt > 0) {
				dyBody->m_Desc.set_altitude(alt, nullptr);
				return false;
			}
		}

		// 高度がスナップ距離以上あれば着地しない
		if (alt >= dyBody->m_Desc.get_snap_height()) {
			dyBody->m_Desc.set_altitude(alt, nullptr);
			return false;
		}

		// 着地した。
		// めり込まないように補正
		{
			KVec3 pos = dyBody->getNode()->getPosition();
			pos.y -= alt;
			dyBody->getNode()->setPosition(pos);
		}

		// 接地処理を行う。バウンドまたは接地状態気をを維持しながら地面の上を滑るように動かす

		// 高度は必ず 0 になる。地面情報を更新する
		dyBody->m_Desc.set_altitude(0, stNode->getNode());

		// 新しい速度を決める。
		// まだバウンドは考慮しない
		KVec3 newspeed(dySpeed.x, 0.0f, dySpeed.z);

		// 摩擦による減速
		{
			float fric = dyBody->m_Desc.get_sliding_friction();
			if (fric > 0) {
				float L = dySpeed.getLength();
				float k = 0.0f;
				if (fric < L) {
					k = (L - fric) / L;
				}
				newspeed *= k;
			}
		}

		// バウンド処理しない場合はここで終了
		if (dyBody->m_Desc.get_bounce() <= 0) {
			if (out_dySpeed) {
				*out_dySpeed = newspeed;
			}
			return true;
		}

		// バウンドするための最低Ｙ速度を上回っている場合だけバウンド考慮する
		float min_bounce = dyBody->m_Desc.get_bounce_min_speed();
		float gravity = dyBody->m_Desc.get_gravity();
		K__ASSERT(min_bounce > 0);
		// dySpeed.y ではなく実際の速度でバウンドするかどうかを決める。
		// 重力下では常に下向きの加速度がかかっているため、dySpeed.y は重力加速度が影響した速度になってしまっている
		// 見た目は静止していても下向きの速度が加算され続けるため、dySpeed.y では正しく判断できない
		if (fabsf(dyBody->m_Desc.get_actual_speed().y) >= min_bounce) {
			newspeed.y = dySpeed.y * -1 * dyBody->m_Desc.get_bounce();
			newspeed.x *= dyBody->m_Desc.get_bounce_horz();
			newspeed.z *= dyBody->m_Desc.get_bounce_horz();
			dyBody->m_Desc._bounce_count++;
		}
		if (out_dySpeed) *out_dySpeed = newspeed;
		return true;
	}
	void node_move(KNode *knode, float dx, float dy, float dz) {
		KVec3 pos = knode->getPosition();
		pos.x += dx;
		pos.y += dy;
		pos.z += dz;
		knode->setPosition(pos);
	}
	void draw_dynamicbody_gizmo_list(KNode *cameranode) {
		KGizmo *gizmo = KScreen::getGizmo();

		KMatrix4 tr;
		cameranode->getWorld2LocalMatrix(&tr);

		// オブジェクトリスト
		KBodyList list;
		update_dynamicbody_list_unsafe(nullptr, &list);

		for (auto it=list.begin(); it!=list.end(); ++it) {
			KSolidBody *body = *it;
			if (_IsDebugInfoVisible(body, cameranode)) {
				draw_dynamicbody_gizmo_each_unsafe(gizmo, body, tr);
			}
		}
	}
	void draw_dynamicbody_gizmo_each_unsafe(KGizmo *gizmo, KSolidBody *dyNode, const KMatrix4 &transform) {
		K__ASSERT(gizmo);
		K__ASSERT(dyNode);

		KMatrix4 matrix;
		dyNode->getNode()->getLocal2WorldMatrix(&matrix);
		matrix = matrix * transform;

		const float line_alpha = _GetGizmoBlinkingAlpha(dyNode->getNode());
		KColor color1(0.0f, 0.0f, 1.0f, line_alpha);
		KColor color2(0.4f, 0.4f, 0.4f, line_alpha);
		{
			KCharacterCollider *coll = dynamic_cast<KCharacterCollider*>(dyNode->getShape());
			if (coll) {
				KVec3 center = coll->get_offset_world(); // 判定中心
				float alt = getAltitudeAtPoint_unsafe(center, 1.0f, nullptr);
				// DynamicBodyの場合、実際にはカプセルというよりも円筒形で判定している
				_DrawCylinderWithAltitude(gizmo, transform, color1, center, coll->get_radius(), coll->get_halfheight(), alt);
			//	_DrawCylinderWithAltitude(gizmo, matrix, color1, center, coll->get_radius(), coll->get_halfheight(), alt);
			//	_DrawCapsuleWithAltitude(gizmo, matrix, color1, center, coll->get_radius(), coll->get_halfheight(), alt);
				return;
			}
		}
	}
	void update_dynamicbody_list_unsafe(KBodyList *movelist, KBodyList *dynamiclist) {
		// 物理衝突処理を行う必要があるエンティティだけをリストアップする
		int now_time = KEngine::getStatus(KEngine::ST_FRAME_COUNT_GAME);

		if (movelist) movelist->clear();
		if (dynamiclist) dynamiclist->clear();

		for (auto it=m_Nodes.begin(); it!=m_Nodes.end(); ++it) {
			KSolidBody *bodynode = it->second;
			if (bodynode->m_Desc.is_static()) continue;

			// Entity がツリー内で disabled だったら判定しない
			// （paused 状態は考慮しない。オブジェクトの動きが停止している時でも、勝手に衝突判定を突き抜けられては困る）
			if (!bodynode->getNode()->getEnableInTree()) {
				continue;
			}

			// CollisionMask が 0 の場合は誰とも衝突しない
			if (bodynode->m_Desc.get_mask_bits() == 0) {
				continue;
			}

			// スリープ中だった場合は判定も移動もしない
			if (bodynode->m_Desc.get_sleep_time() != 0) {
				continue;
			}

			if (bodynode->getBodyEnabled()) {
				// 衝突可能な形状を持っている
				if (dynamiclist) dynamiclist->push_back(bodynode);

			} else {
				// 衝突可能な形状を持っていない。
				// 衝突処理なしで移動する
				if (movelist) movelist->push_back(bodynode); 
			}
		}
	}
	void simple_move(KSolidBody *dyNode) {
		K__ASSERT(dyNode);

		if (dyNode->getNode()->getPauseInTree()) {
			return;
		}
		if (dyNode->m_Desc.get_sleep_time() != 0) {
			return;
		}
		KVec3 pos = dyNode->getNode()->getPosition();
		KVec3 spd = dyNode->m_Desc.get_velocity();

		// 変更前の座標を記録しておく
		dyNode->m_Desc._prev_prev_pos = dyNode->m_Desc.get_prev_pos();
		dyNode->m_Desc._prev_pos = pos;

		// 座標変更
		KVec3 newpos = pos += spd;
		KVec3 newspd = spd;
		if (dyNode && dyNode->getShape()) {
			newspd.y -= dyNode->m_Desc.get_gravity();
		}
		dyNode->getNode()->setPosition(newpos);
		dyNode->m_Desc.set_velocity(newspd);
	}
	void update_dynamicbody_positions_unsafe() {
		// update_dynamicbody_list() で作成されたリストのうち、移動可能物体の位置を更新する
		for (auto it=m_TmpMovingNodes.begin(); it!=m_TmpMovingNodes.end(); ++it) {
			KSolidBody *dyNode = *it;
			simple_move(dyNode);
		}
		for (auto it=m_TmpDynamicNodes.begin(); it!=m_TmpDynamicNodes.end(); ++it) {
			KSolidBody *dyNode = *it;
			simple_move(dyNode);
		}
	}
	KCollider * get_sphere_collide(const KVec3 &pos, float radius, const KBodyList &bodylist) const {
		uint32_t bitmask = 0;
		for (auto it=bodylist.begin(); it!=bodylist.end(); ++it) {
			KSolidBody *bodynode = *it;
			KCollider *collider = bodynode->getShape();
			if (collider->get_sphere_collision(pos, radius, bitmask, nullptr, nullptr)) {
				return collider;
			}
		}
		return nullptr;
	}
	void update_staticbody_collision_unsafe() {
		// 地形用オブジェクトリスト
		KBodyList &bodylist = m_TmpStaticBody_Collision;
		bodylist.clear();
		get_active_static_body_list_unsafe(bodylist);

		// 高度情報を初期化する
		clear_dynamicbody_altitudes();

		for (auto dit=m_TmpDynamicNodes.begin(); dit!=m_TmpDynamicNodes.end(); ++dit) {
			KSolidBody *dyNode = *dit;
			KCollider *dyCollider = dyNode->getShape();
			uint32_t bitmask = 0;

			KCharacterCollider *dyChara = dynamic_cast<KCharacterCollider*>(dyCollider);
			if (dyChara == nullptr) {
				// 運動している物体は KCharacterCollider のみ対応
				continue;
			}

#if 1
			// paused なダイナミックノードの判定をスキップしてしまうと、
			// 高度情報が更新されずに接地しているかどうかが分からなくなってしまうので、
			// スキップしちゃダメ
#else
			// pause 中の場合、地形との判定は行わない
			// ※pause中でも動的剛体同士の判定をするために m_TmpDynamicNodes には paused なノードも入っている.
			//   update_dynamicbody_list を参照せよ
			if (dyNode->knode->getPauseInTree()) {
				continue;
			}
#endif
			COL_INFO info;
			info.update(dyCollider, bitmask, dyNode->m_Desc.get_velocity());

			struct HIT {
				KVec3 delta;
				KVec3 normal;
				KNode *node;
			};
			const int MAX_HITS = 4;
			HIT hitlist[MAX_HITS];
			int num_hits = 0;

			for (auto sit=bodylist.begin(); sit!=bodylist.end(); ++sit) {
				KSolidBody *stBody = *sit;
				KCollider *stCollider = stBody->getShape();

				KVec3 stMinW, stMaxW;
				stCollider->get_aabb(bitmask, &stMinW, &stMaxW);
				stMinW -= EXTENDS;
				stMaxW += EXTENDS;

				// AABB同士の重なりをチェック
				if (!KGeom::K_GeomIntersectAabb(info.extended_whole_aabb_min, info.extended_whole_aabb_max, stMinW, stMaxW, nullptr, nullptr)) {
					continue; // 重なりなし。詳細な衝突判定をスキップ
				}

				KVec3 cur_cpos = dyCollider->get_offset_world();
				KVec3 new_cpos = dyCollider->get_offset_world();
			
				if (1/*dyChara*/) {
					// カプセルの下側半球と重なる球で判定する
					float h = dyChara->get_halfheight() - dyChara->get_radius(); // カプセル中心から、カプセル下側半球中心までの長さ
					KCollisionTest test;
					test.ball_pos = cur_cpos + KVec3(0.0f, -h, 0.0f); // カプセルの下側半球の中心座標。これを中心とする球で判定する
					test.ball_radius = dyChara->get_radius();
					test.ball_speed = dyNode->m_Desc.get_velocity();
					test.ball_climb = dyNode->m_Desc.get_climb_height();
					test.ball_skin = 0.5f;
					if (stCollider->get_collision_result(test)) {
						// 衝突した
						bool isground = false;
						bool iswall = false;
						getSurfaceType(test.result_normal, &isground, &iswall);

						if (isground) {
							// 地面とみなせる部分に衝突している。地面との判定は後で行うので、ここでは無視する
					
						} else if (iswall) {
							// 壁とみなせる部分に衝突している

							KVec3 calcpos = test.result_newpos + KVec3(0.0f, h, 0.0f); // test.result_newpos はカプセル下部半球の位置なので、これをカプセル位置に直す
							KVec3 pos0 = new_cpos; // newpos(変更前)
							KVec3 pos1 = calcpos; // newpos(変更後)
							bool deny = false;
							if (m_Callback) {
								m_Callback->on_collision_wall(stBody->getNode(), dyNode->getNode(), test, pos0, pos1, &deny);
							}
							if (!deny) {
								new_cpos = pos1;
								if (num_hits < MAX_HITS) {
									HIT hit;
									hit.normal = test.result_normal;
									hit.delta = new_cpos - cur_cpos;
									hit.node = stBody->getNode();
									hitlist[num_hits] = hit;
									dyNode->m_Desc._hits_with.push_back(stBody->getNode());
									num_hits++;
								}
							}
						} else {
							// 地面でも壁でもない（天井など）
						}
					}
				}
			}

			if (num_hits > 0) {
				// 衝突した。めりこみ解消位置に移動する

				// ゆっくり移動しているときはスリップするが、高速で衝突したら逸れずにその場で止まる
				bool allow_slip = true; // デフォルトでは常に衝突スリップ可能
				if (dyNode->m_Desc._slip_control) {
					const KVec3 &vel = dyNode->m_Desc.get_velocity();
					if (dyNode->m_Desc._slip_limit_speed <= vel.getLength()) {
						// 現在の速度が dyNode->m_Desc._slip_slower_than 以上になった。衝突してもスリップせずにその場で停止する
						allow_slip = false;
					}
				}

				// 衝突壁が１個だけの場合、裏側からの衝突は無視してよい
				if (num_hits == 1) {
					KVec3 vel = dyNode->m_Desc.get_velocity();
					float dot = vel.dot(hitlist[0].normal); // 速度と壁の法線向きの一致度
					if (dot > 0) {
						// 移動方向と法線向きが同じ。
						// 壁から離れる方向に移動しているので、この衝突を無視する

					} else {
						// 衝突後の座標を計算
						KVec3 pos = dyNode->getNode()->getPosition();
						KVec3 newpos = pos + hitlist[0].delta; // スリップ込みの新座標
						if (!allow_slip) {
							// スリップなし
							// 進行方向と補正方向が30度(cos>=0.5)以上開いている場合はXYを移動前の位置に戻す
							// ※Yはスリップしたままで良い、そうしないと壁にぶつかったときに落下してくれない）
							KVec3 prevpos = pos - vel; // 位置更新前の座標

							// 衝突前速度、衝突後速度を正規化したもの（高低差を無視するので y を 0 にしておく）
							KVec3 nor_vel_xz = KVec3(vel.x, 0.0f, vel.z).getNormalized(); // 正規化速度
							KVec3 nor_adj_xz = KVec3(newpos.x - prevpos.x, 0.0f, newpos.z - prevpos.z).getNormalized(); // 衝突後の速度（移動前と補正後移動先の差）

							// 衝突前速度、衝突後速度の一致度を調べる
							float dot_adj = nor_vel_xz.dot(nor_adj_xz);

							// 閾値角度。衝突前、衝突後の角度がこれ未満だったら正面衝突とみなし、スリップせずに停止する
							float thr_deg = dyNode->m_Desc._slip_limit_degrees;
							float thr_val = cosf(KMath::degToRad(thr_deg));
							if (dot_adj < thr_val) {
								newpos.x = pos.x - vel.x; // 速度適用前の位置. newpos ではなく pos から引くことに注意
								newpos.z = pos.z - vel.z;
							}
						}
						dyNode->getNode()->setPosition(newpos);
					}
				} else {
					// 複数の壁とヒットしている。
					// 法線と速度向きの組み合わせに関係なく、すべての壁との判定を行う
					//
					// 例えば
					//    A
					//   / 
					//  / 〇
					// B-----------C
					//
					// という地形があり、〇の速度がABよりも（BCに対して）垂直に近い速度（決して垂直ではない）設定されていて、〇本体をBCにこすりつけながら左に移動しているとする。
					// 〇は常にBCと衝突し押し返されることにより左に移動しているが、
					// 〇の速度設定は AB よりも垂直に近くなっており、ABの法線と〇の速度が同じ向きになってしまうためABはそのまますり抜けてしまう
					KVec3 pos = dyNode->getNode()->getPosition();
					KVec3 newpos = pos;

					// まずスリップありの新座標を得る
					for (int i=0; i<num_hits; i++) {
						const HIT &hit = hitlist[i];
						newpos += hit.delta;
					}
					if (!allow_slip) {
						// スリップなしの場合はXYを移動前の位置に戻す
						// ※Yはスリップしたままで良い、そうしないと壁にぶつかったときに落下してくれない）
						KVec3 vel = dyNode->m_Desc.get_velocity();
						newpos.x = pos.x - vel.x; // 速度適用前の位置. newpos ではなく pos から引くことに注意
						newpos.z = pos.z - vel.z;
					}
					dyNode->getNode()->setPosition(newpos);
				}
			}

			// 地面との判定
			// 現在地における高度と真下の地面ノードを得る
			KVec3 dySpeed = dyNode->m_Desc.get_velocity();
			KVec3 newSpeed = dySpeed;
			collide_ground(bodylist, dyNode, dySpeed, m_Callback/*KSolidBodyCallback*/, &newSpeed);
			dyNode->m_Desc.set_velocity(newSpeed);
		}
	}
	void update_dynamicbody_collision_unsafe() { // 動的剛体同士で相互作用するものを処理する
		// 相互作用する可能性のある剛体リストを作成
		m_TmpTable.clear();
		m_TmpTable.reserve(m_TmpDynamicNodes.size());
		for (auto it=m_TmpDynamicNodes.begin(); it!=m_TmpDynamicNodes.end(); ++it) {
			KSolidBody *dyNode = *it;
			// 動的衝突に対応しているのは KCharacterCollider のみ
			if (dynamic_cast<KCharacterCollider*>(dyNode->getShape())) {
				m_TmpTable.push_back(dyNode);
			}
		}

		for (int i=0; i<(int)m_TmpTable.size()-1; i++) {
			KSolidBody *dyNode1 = m_TmpTable[i];
			KCollider *dyCollider1 = dyNode1->getShape();
			uint32_t bitmask = 0;

			float radius1 = dynamic_cast<KCharacterCollider*>(dyCollider1)->get_radius();

			COL_INFO info1;
			info1.update(dyCollider1, bitmask, dyNode1->m_Desc.get_velocity());

			for (int j=i+1; j<(int)m_TmpTable.size(); j++) {
				KSolidBody *dyNode2 = m_TmpTable[j];
				KCollider *dyCollider2 = dyNode2->getShape();

				// フィルタリング
				if (!_PassBodyFilter(dyNode1, dyNode2)) {
					continue;
				}
		
				COL_INFO info2;
				info2.update(dyCollider2, bitmask, dyNode2->m_Desc.get_velocity());

				// AABBでの判定
				if (!KGeom::K_GeomIntersectAabb(
					info1.extended_whole_aabb_min, info1.extended_whole_aabb_max, 
					info2.extended_whole_aabb_min, info2.extended_whole_aabb_max,
					nullptr, nullptr)
				) {
					continue;
				}

				float radius2 = dynamic_cast<KCharacterCollider*>(dyCollider2)->get_radius();

				// 2体間の中心距離
				KVec3 wpos1 = dyNode1->getNode()->getWorldPosition();
				KVec3 wpos2 = dyNode2->getNode()->getWorldPosition();
				KVec3 delta = wpos2 - wpos1;
				float delta_r = delta.getLength();

				// 2体がめり込んでいる場合、その深さ。
				// めり込んでいるなら正の値になる
				float penetration_depth = radius1 + radius2 - delta_r;
			
				// 2体の接触許容範囲のうち、大きい方の値
				float skin = KMath::max(dyNode1->m_Desc.get_skin_width(), dyNode2->m_Desc.get_skin_width());

				// めり込み深さの絶対値が skin 未満であれば「表面接触」である。
				// 「深さ < -skin」なら確実に離れている。
				// 「-skin <= 深さ < skin」なら接触している（2体間の隙間が skin 未満になっているということ）
				// 「skin <= 深さ」ならば確実にめり込んでいる。
				if (penetration_depth < -skin) {
					continue;
				}

				// 2体が衝突状態にある。
				// この衝突状態を解決するか、それとも無視するか問い合わせる
				bool deny = false;
				if (m_Callback) m_Callback->on_collision_update_body_each(dyNode1->getNode(), dyNode2->getNode(), &deny);
				if (deny) {
					continue;
				}

				if (penetration_depth <= 0) {
					// 表面が接触しているだけで、互いにめり込んではいない。
					// 座標修正する必要はない
					continue;
				}
				// 2体が互いに真正面から衝突した場合、互いに横にそれることができず、
				// お互いを避けることができない。少しだけ座標をずらしておく
				if (delta.x == 0) delta.x = 0.001f;
				if (delta.z == 0) delta.z = 0.001f;

				// めり込みを解決する
				{
					float response1 = dyNode1->m_Desc.get_penetratin_response();
					float response2 = dyNode2->m_Desc.get_penetratin_response();
					if (m_Callback) m_Callback->on_collision_response(dyNode1->getNode(), dyNode2->getNode(), &response1, &response2);

					const float cos_xz = (delta_r > 0) ? delta.x / delta_r : 1;
					const float sin_xz = (delta_r > 0) ? delta.z / delta_r : 0;
					const float penet_x = penetration_depth * 0.5f * cos_xz;
					const float penet_z = penetration_depth * 0.5f * sin_xz;

					KVec3 pos1 = dyNode1->getNode()->getPosition();
					pos1.x -= penet_x * response1;
					pos1.z -= penet_z * response1;
					dyNode1->getNode()->setPosition(pos1);

					KVec3 pos2 = dyNode2->getNode()->getPosition();
					pos2.x += penet_x * response2;
					pos2.z += penet_z * response2;
					dyNode2->getNode()->setPosition(pos2);
				}

				dyNode1->m_Desc._hits_with.push_back(dyNode2->getNode());
				dyNode2->m_Desc._hits_with.push_back(dyNode1->getNode());
			}
		}
	}
	void clear_dynamicbody_altitudes() {
		// 高度情報を初期化する
		for (int i=0; i<(int)m_TmpDynamicNodes.size(); i++) {
			KSolidBody *dyNode = m_TmpDynamicNodes[i];
			dyNode->m_Desc.clear_altitude();
		}
	}
}; // CCollisionMgr

extern CCollisionMgr *g_CollisionMgr = nullptr; 





#pragma region KSolidBody
void KSolidBody::install() {
	K__ASSERT(g_CollisionMgr == nullptr);
	g_CollisionMgr = new CCollisionMgr();
}
void KSolidBody::uninstall() {
	if (g_CollisionMgr) {
		g_CollisionMgr->drop();
		g_CollisionMgr = nullptr;
	}
}
bool KSolidBody::isAttached(KNode *node) {
	return of(node) != nullptr;
}
KSolidBody * KSolidBody::of(KNode *node) {
	K__ASSERT(g_CollisionMgr);
	return g_CollisionMgr->getBody(node);
}
void KSolidBody::attachVelocity(KNode *node) {
	K__ASSERT(g_CollisionMgr);
	g_CollisionMgr->attachVelocity(node);
}
bool KSolidBody::getGroundPoint(const KVec3 &pos, float max_penetration, float *out_ground_y, KNode **out_ground) {
	K__ASSERT(g_CollisionMgr);
	return g_CollisionMgr->getGroundPoint(pos, max_penetration, out_ground_y, out_ground);
}
bool KSolidBody::getDynamicBodyAltitude(KNode *node, float *alt) {
	K__ASSERT(g_CollisionMgr);
	return g_CollisionMgr->getDynamicBodyAltitude(node, alt);
}
float KSolidBody::getAltitudeAtPoint(const KVec3 &point, float max_penetration, KNode **out_ground) {
	K__ASSERT(g_CollisionMgr);
	return g_CollisionMgr->getAltitudeAtPoint(point, max_penetration, out_ground);
}
bool KSolidBody::rayCast(const KVec3 &pos, const KVec3 &dir, float maxdist, KRayHit *out_hit) {
	K__ASSERT(g_CollisionMgr);
	return g_CollisionMgr->rayCast(pos, dir, maxdist, out_hit);
}
void KSolidBody::setDebugLineVisible(bool static_lines, bool dynamic_lines) {
	K__ASSERT(g_CollisionMgr);
	g_CollisionMgr->setDebugLineVisible(static_lines, dynamic_lines);
}
void KSolidBody::setDebug_alwaysShowDebug(bool value) {
	K__ASSERT(g_CollisionMgr);
	g_CollisionMgr->setDebug_alwaysShowDebug(value);
}
bool KSolidBody::getDebug_alwaysShowDebug() {
	K__ASSERT(g_CollisionMgr);
	return g_CollisionMgr->getDebug_alwaysShowDebug();
}
void KSolidBody::snapToGround(KNode *node) {
	K__ASSERT(g_CollisionMgr);
	g_CollisionMgr->snapToGround(node);
}
void KSolidBody::setCallback(KSolidBodyCallback *cb) {
	K__ASSERT(g_CollisionMgr);
	g_CollisionMgr->setCallback(cb);
}
void KSolidBody::setGroupName(uint32_t group_bit, const char *name_literal) {
	K__ASSERT(g_CollisionMgr);
	g_CollisionMgr->setGroupName(group_bit, name_literal);
}
bool KSolidBody::rayCastEnumerate(const KVec3 &start, const KVec3 &dir, float maxdist, KRayCallback *cb) {
	K__ASSERT(g_CollisionMgr);
	return g_CollisionMgr->rayCastEnumerate(start, dir, maxdist, cb);
}
bool KSolidBody::getSurfaceType(const KVec3 &normalizedNormal, bool *isGround, bool *isWall) {
	K__ASSERT(g_CollisionMgr);
	return g_CollisionMgr->getSurfaceType(normalizedNormal, isGround, isWall);
}
void KSolidBody::setCollisionGroupBits(uint32_t group_bits) {
	K__ASSERT(g_CollisionMgr);
	g_CollisionMgr->setCollisionGroupBits(m_Node, group_bits);
}
void KSolidBody::setCollisionMaskBits(uint32_t mask_bits) {
	K__ASSERT(g_CollisionMgr);
	g_CollisionMgr->setCollisionMaskBits(m_Node, mask_bits);
}

////////////////////////////////////////////////////////////////////

KSolidBody::KSolidBody() {
	m_Node = nullptr;
	m_Shape = nullptr;
}
KSolidBody::~KSolidBody() {
	K_Drop(m_Shape);
}
void KSolidBody::_setNode(KNode *node) {
	m_Node = node;
	m_Desc.knode = node;
	if (m_Shape) m_Shape->_setNode(node);
}
KNode * KSolidBody::getNode() {
	return m_Node;
}
void KSolidBody::setShape(KCollider *co) {
	if (m_Shape) {
		m_Shape->_setNode(nullptr);
		m_Shape->drop();
		m_Shape = nullptr;
	}
	m_Shape = co;
	if (m_Shape) {
		m_Shape->grab();
		m_Shape->_setNode(m_Node);
	}
}
KCollider * KSolidBody::getShape() {
	return m_Shape;
}
void KSolidBody::setShapeCenter(const KVec3 &pos) {
	KCollider *coll = getShape();
	if (coll) coll->set_offset(pos);
}
KVec3 KSolidBody::getShapeCenter() {
	KCollider *coll = getShape();
	return coll ? coll->get_offset() : KVec3();
}
KVec3 KSolidBody::getShapeCenterInWorld() {
	const KCollider *coll = getShape();
	return coll ? coll->get_offset_world() : KVec3();
}
bool KSolidBody::getAabbLocal(KVec3 *out_min, KVec3 *out_max) {
	const KCollider *coll = getShape();
	if (coll) {
		coll->get_aabb_local(0, out_min, out_max);
		return true;
	}
	return false;
}
bool KSolidBody::getAabbWorld(KVec3 *out_min, KVec3 *out_max) {
	const KCollider *coll = getShape();
	if (coll) {
		coll->get_aabb(0, out_min, out_max);
		return true;
	}
	return false;
}
void KSolidBody::setBodyEnabled(bool value) {
	KCollider *coll = getShape();
	coll->setEnable(value);
}
bool KSolidBody::getBodyEnabled() {
	KCollider *coll = getShape();
	return coll ? coll->getEnable() : false;
}
#pragma endregion // KSolidBody



#pragma region KStaticSolidBody
void KStaticSolidBody::attach(KNode *node) {
	K__ASSERT(g_CollisionMgr);
	if (node && !isAttached(node)) {
		g_CollisionMgr->addStaticBody(node);
	}
}
bool KStaticSolidBody::isAttached(KNode *node) {
	return of(node) != nullptr;
}
KStaticSolidBody * KStaticSolidBody::of(KNode *node) {
	K__ASSERT(g_CollisionMgr);
	return g_CollisionMgr->getStaticBody(node);
}

KStaticSolidBody::KStaticSolidBody() {
	m_Desc.set_static(true);
	m_Desc.set_gravity(0.4f);
	m_Desc.set_skin_width(4); // 左右、奥行き方向の接触誤差。相手がこの範囲内にいる場合は接触状態とみなす
	m_Desc.set_snap_height(4); // 地面の接触誤差。地面がこの範囲内にある場合は着地状態とみなす
	m_Desc.set_penetratin_response(0.5f);
	m_Desc.set_climb_height(16);
	m_Desc.set_bounce(0.5f);
	m_Desc.set_bounce_horz(0.8f);
	m_Desc.set_bounce_min_speed(m_Desc.get_gravity() * 2);
	m_Desc.set_sliding_friction(0.0f);
	if (1) {
		KCollider *coll = KSphereCollider::create(KVec3(), 32);
		setShape(coll);
		coll->drop();
	}
}
void KStaticSolidBody::setShapeGround() {
	KCollider *coll = KPlaneCollider::create(
		KVec3(), KVec3(0, 1, 0), 
		KVec3(-PLANE_LIMIT, -PLANE_LIMIT, -PLANE_LIMIT), 
		KVec3( PLANE_LIMIT,  PLANE_LIMIT,  PLANE_LIMIT)
	);
	setShape(coll);
	coll->drop();
}
void KStaticSolidBody::setShapePlane(const KVec3 &_normal) {
	KVec3 normal;
	if (!_normal.getNormalizedSafe(&normal)) {
		K__ERROR("E_NORMALIZE_ZERO_VECTOR: setShapePlane");
		return;
	}
	KCollider *coll = KPlaneCollider::create(
		KVec3(),
		normal,
		KVec3(-PLANE_LIMIT, -PLANE_LIMIT, -PLANE_LIMIT), // trim min
		KVec3( PLANE_LIMIT,  PLANE_LIMIT,  PLANE_LIMIT)  // trim max
	);
	setShape(coll);
	coll->drop();
}
void KStaticSolidBody::setShapeBox(const KVec3 &halfsize) {
	KCollider *coll = KAabbCollider::create(KVec3(), halfsize);
	setShape(coll);
	coll->drop();
}
void KStaticSolidBody::setShapeShearedBox(const KVec3 &halfsize, float shearx, KShearedBoxCollider::FLAGS flags) {
	KCollider *coll = KShearedBoxCollider::create(KVec3(), halfsize, shearx, flags);
	setShape(coll);
	coll->drop();
}
void KStaticSolidBody::setShapeCapsule(float radius, float halfheight) {
	KCollider *coll = KCapsuleCollider::create(KVec3(), radius, halfheight, false);
	setShape(coll);
	coll->drop();
}
void KStaticSolidBody::setShapeCylinder(float radius, float halfheight) {
	KCollider *coll = KCapsuleCollider::create(KVec3(), radius, halfheight, true);
	setShape(coll);
	coll->drop();
}
void KStaticSolidBody::setShapeFloor(const KVec3 &halfsize, float shearx, const float *heights) {
	KCollider *coll = KFloorCollider::create(KVec3(), halfsize, shearx, heights);
	setShape(coll);
	coll->drop();
}
void KStaticSolidBody::setShapeWall(float x0, float z0, float x1, float z1) {
	float delta_x = x1 - x0;
	float delta_z = z1 - z0;

	float y0 = 0;
	float y1 = WALL_TALL;

	// center position in world
	KVec3 center(
		(x0 + x1) * 0.5f,
		(y0 + y1) * 0.5f,
		(z0 + z1) * 0.5f
	);

	// normal vector
	float normal_x =  delta_z;
	float normal_z = -delta_x;
	KVec3 normal(normal_x, 0.0f, normal_z);
	KVec3 norNormal;
	if (!normal.getNormalizedSafe(&norNormal)) {
		K__ERROR("E_NORMALIZE_ZERO_VECTOR: %s", __FUNCTION__);
		return;
	}

	float half_wall_width = hypotf(delta_x, delta_z) * 0.5f;

	if (1) {
		KVec3 pos = getNode()->getPosition();
		pos = center;
		getNode()->setPosition(pos);
	}
	KVec3 p[] = {
		KVec3(x0, y0, z0) - center,
		KVec3(x0, y1, z0) - center,
		KVec3(x1, y1, z1) - center,
		KVec3(x1, y0, z1) - center,
	};
	KCollider *coll = KQuadCollider::create(KVec3(), p);
	setShape(coll);
	coll->drop();
}
#pragma endregion // KStaticSolidBody



#pragma region KDynamicSolidBody
void KDynamicSolidBody::attach(KNode *node) {
	attachEx(node, true);
}
void KDynamicSolidBody::attachEx(KNode *node, bool with_default_shape) {
	K__ASSERT(g_CollisionMgr);
	if (node && !isAttached(node)) {
		g_CollisionMgr->addDynamicBody(node, with_default_shape);
	}
}
bool KDynamicSolidBody::isAttached(KNode *node) {
	return of(node) != nullptr;
}
KDynamicSolidBody * KDynamicSolidBody::of(KNode *node) {
	K__ASSERT(g_CollisionMgr);
	return g_CollisionMgr->getDynamicBody(node);
}
KDynamicSolidBody::KDynamicSolidBody(bool with_default_shape) {
	m_Desc.set_static(false);
	m_Desc.set_gravity(0.4f);
	m_Desc.set_skin_width(4); // 左右、奥行き方向の接触誤差。相手がこの範囲内にいる場合は接触状態とみなす
	m_Desc.set_snap_height(4); // 地面の接触誤差。地面がこの範囲内にある場合は着地状態とみなす
	m_Desc.set_penetratin_response(0.5f);
	m_Desc.set_climb_height(16);
	m_Desc.set_bounce(0.5f);
	m_Desc.set_bounce_horz(0.8f);
	m_Desc.set_bounce_min_speed(m_Desc.get_gravity() * 2);
	m_Desc.set_sliding_friction(0.0f);
	m_Desc.set_mask_bits(0xFFFFFFFF);
	m_Desc.set_sleep_time(0);
	m_Desc.clear_altitude();
	if (with_default_shape) {
		// とりあえずデフォルトのコライダーを付けておく
		KCollider *coll = KSphereCollider::create(KVec3(), 32);
		setShape(coll);
		coll->drop();
	}
}
void KDynamicSolidBody::setShapeSphere(float r) {
	setShapeCapsule(r, r); // 特殊な形のカプセルとして作成する
}
void KDynamicSolidBody::setShapeCapsule(float radius, float halfheight) {
	// halfheight: 体の高さ/2。必ず halfheight >= readius であること
	// halfheight = readius にすると球体になる
	KCollider *coll = KCharacterCollider::create(KVec3(), radius, halfheight);
	setShape(coll);
	coll->drop();
}
void KDynamicSolidBody::setVelocity(const KVec3 &value) {
	m_Desc.set_velocity(value);
}
KVec3 KDynamicSolidBody::getVelocity() const {
	return m_Desc.get_velocity();
}
void KDynamicSolidBody::setBounce(float value) {
	m_Desc.set_bounce(value);
}
void KDynamicSolidBody::setSlidingFriction(float value) {
	m_Desc.set_sliding_friction(value);
}
void KDynamicSolidBody::setGravity(float value) {
	m_Desc.set_gravity(value);
}
void KDynamicSolidBody::setPenetratinResponse(float value) {
	m_Desc.set_penetratin_response(value);
}
void KDynamicSolidBody::setClimbHeight(float value) {
	m_Desc.set_climb_height(value);
}
void KDynamicSolidBody::setSnapHeight(float value) {
	m_Desc.set_snap_height(value);
}

#pragma endregion // KDynamicSolidBody






















namespace Test {

void Test_collision() {
#ifdef _DEBUG
	KVec3 c, s;

	K__ASSERT(!KCollisionMath::isAabbIntersected(KVec3(0, 0, 0), KVec3(10, 10, 0), KVec3( 100, 10, 0), KVec3(5, 5, 0), &c, &s));
	K__ASSERT(!KCollisionMath::isAabbIntersected(KVec3(0, 0, 0), KVec3(10, 10, 0), KVec3(-100, 10, 0), KVec3(5, 5, 0), &c, &s));
	K__ASSERT(!KCollisionMath::isAabbIntersected(KVec3(0, 0, 0), KVec3(10, 10, 0), KVec3( 10, 100, 0), KVec3(5, 5, 0), &c, &s));
	K__ASSERT(!KCollisionMath::isAabbIntersected(KVec3(0, 0, 0), KVec3(10, 10, 0), KVec3( 10,-100, 0), KVec3(5, 5, 0), &c, &s));

	// 片方が片方を完全に内包している
	K__ASSERT(KCollisionMath::isAabbIntersected(KVec3(0, 0, 0), KVec3(10, 10, 10), KVec3(0, 0, 0), KVec3(20, 20, 20), &c, &s));
	K__ASSERT(c == KVec3(0, 0, 0));
	K__ASSERT(s == KVec3(10, 10, 10));

	// めり込み深さ0の場合、つまり接しているだけの場合は衝突とみなす。
	K__ASSERT(KCollisionMath::isAabbIntersected(KVec3(0, 0, 0), KVec3(10, 10, 0), KVec3(15, 15, 0), KVec3(5, 5, 0), &c, &s));
	K__ASSERT(c == KVec3(10, 10, 0));
	K__ASSERT(s == KVec3( 0,  0, 0));

	K__ASSERT(KCollisionMath::isAabbIntersected(KVec3(0, 0, 0), KVec3(10, 10, 0), KVec3(15, 15, 0), KVec3(9, 9, 0), &c, &s));
	K__ASSERT(c == KVec3(8, 8, 0));
	K__ASSERT(s == KVec3(2, 2, 0));
	
	K__ASSERT(KMath::equals(KCollisionMath::getSignedDistanceOfLinePoint2D(0, 0, 0, 100, 100, 0), -hypotf(50, 50))); // 点P(0, 0) と、A(0, 100) B(100, 0) を通る直線の距離。A から B を見た時、点Pは右側にあるので正の距離を返す
	K__ASSERT(KMath::equals(KCollisionMath::getSignedDistanceOfLinePoint2D(0, 0, 100, 0, 0, 100),  hypotf(50, 50))); // 点P(0, 0) と、A(100, 0) B(0, 100) を通る直線の距離。A から B を見た時、点Pは左側にあるので負の距離を返す

	// 円(0, 0, R=80) と点(50, 50) の衝突と解決。
	// 衝突解決には、円を左下に向かって 80-hypotf(50, 50) だけ移動させる
	float adj_x, adj_y;
	K__ASSERT(KCollisionMath::collisionCircleWithPoint2D(0,0,80,  50,50,  1.0f, &adj_x, &adj_y));
	K__ASSERT(KMath::equals(hypot(adj_x, adj_y), 80-hypotf(50, 50)));
	K__ASSERT(adj_x < 0 && adj_y < 0); // 左下に移動させるので両方とも負の値になる

	// 円(0, 0, R=80) と直線点 (90, 0)-(0, 90) の衝突と解決。
	// 衝突解決には、円を左下に向かって 80-hypotf(90, 90)/2 だけ移動させる（直線の向きに対して右側にはじく）
	K__ASSERT(KCollisionMath::collisionCircleWithLine2D(0,0,80,  90,0,  0,90,  1.0f, &adj_x, &adj_y));
	K__ASSERT(KMath::equals(hypot(adj_x, adj_y), 80-hypotf(90, 90)/2));
	K__ASSERT(adj_x < 0 && adj_y < 0); // 左下に移動させるので両方とも負の値になる

	// 円１(0, 0, R=50) と円２(90, 0, R=50) の衝突と解決。
	// 衝突解決には円１を左側に 10 だけ移動させる
	K__ASSERT(KCollisionMath::collisionCircleWithCircle2D(0,0,50,  90,0,50,  1.0f, &adj_x, &adj_y));
	K__ASSERT(KMath::equals(adj_x, -10));
	K__ASSERT(KMath::equals(adj_y,  0));
#endif // _DEBUG
}
} // Test
#pragma endregion // CCollisionMgr



} // namespace
