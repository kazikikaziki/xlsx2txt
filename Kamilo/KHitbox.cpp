#include "KHitbox.h"
//
#include "KCamera.h"
#include "KImGui.h"
#include "KInspector.h"
#include "KInternal.h"
#include "KSig.h"
#include "KScreen.h"
#include "keng_game.h"

namespace Kamilo {



static bool _IsDebugInfoVisible(KNode *target, KNode *camera) {
	if (target == nullptr) return false;
	K__ASSERT(camera);
	if (! target->getEnableInTree()) return false;
	if (! target->getVisibleInTree()) return false;

	if (!KInspector::isEntityVisible(target)) {
		return false;
	}
	// 対応するカメラ
	KNode *cameraOfTarget = KCamera::findCameraFor(target);
	if (cameraOfTarget == nullptr) {
		return false; // オブジェクトに対応するカメラが存在しない
	}

	if (cameraOfTarget != camera) {
		return false; // オブジェクトとは関係のないカメラが指定されている
	}
	return true;
}



#pragma region CHitboxManagerImpl
class CHitboxManagerImpl: public KManager, public KInspectorCallback {
	std::vector<KHitPair> m_HitPairs;
	std::unordered_map<KNode*, KHitbox*> m_Nodes;
	std::vector<std::vector<KHitbox*>> m_GroupedEntries;
	std::vector<KHitboxGroup> m_Groups;
	KHitboxCallback *m_Callback;
	int m_Clock;
	bool m_Highlight;
	bool m_AlwaysShowHitboxes;
public:
	CHitboxManagerImpl() {
		m_Highlight = true;
		m_Callback = nullptr;
		m_Clock = 0;
		m_AlwaysShowHitboxes  = false;

		KEngine::addManager(this);
		KEngine::addInspectorCallback(this, u8"ヒットボックス"); // KInspectorCallback
		KEngine::setManagerRenderDebugEnabled(this, true); // on_manager_renderdebug
	}
	virtual ~CHitboxManagerImpl() {
		clearHitboxNodes();
	}


	#pragma region KManager
	virtual bool on_manager_isattached(KNode *node) override {
		if (getThisHitbox(node) != nullptr) {
			return true;
		}
		return false;
	}
	virtual void on_manager_detach(KNode *node) override {
		delHitbox(node);
	}
	virtual void on_manager_beginframe() override {
		tickHitboxClock();
	}
	virtual void on_manager_frame() override {
		// 前フレームで衝突が解消されたペアを削除する
		removeEndedPairs();

		// ヒットボックス判定の衝突処理
		updateHitboxes();

		// 衝突解消の検出
		// ただし、衝突中にエンティティ自体が消えた場合は EXIT イベントが発生しないことに注意
		detectHitboxExit();
	}
	virtual void on_manager_nodeinspector(KNode *node) override {
		KHitbox *hb = getTrulyThisHitbox(node);
		if (hb) {
			update_hitbox_inspector_item(hb);
		}
	}
	virtual void on_manager_renderdebug(KNode *cameranode, KNode *start, KEntityFilterCallback *filter) override {
		bool show_debug = KInspector::isDebugInfoVisible(this);
		if (m_AlwaysShowHitboxes || show_debug) {
			drawHitboxes(cameranode);
		}
	}
	#pragma endregion // KManager


	#pragma region KInspectorCallback
	virtual void onInspectorGui() override {
		ImGui::Checkbox("Always show sensor gizmos",  &m_AlwaysShowHitboxes);
		updateHitboxGroupGui();
		updateHitboxHitPairGui();
	}
	#pragma endregion // KInspectorCallback

	void setDebugLineVisible(bool sensor_lines) {
		m_AlwaysShowHitboxes  = sensor_lines;
	}
	void setDebug_alwaysShowDebug(bool val) {
		KInspectableDesc *desc = KInspector::getDesc(this);
		K__ASSERT_RETURN(desc);
		desc->always_show_debug = val;
	}
	bool getDebug_alwaysShowDebug() {
		KInspectableDesc *desc = KInspector::getDesc(this);
		K__ASSERT_RETURN_ZERO(desc);
		return desc->always_show_debug;
	}

	KHitbox * getHitboxByGroup(KNode *node, int groupindex) {
		int num = getChildHitboxCount(node);
		for (int i=0; i<num; i++) {
			KHitbox *hb = getChildHitbox(node, i);
			if (hb->getGroupIndex() == groupindex) {
				return hb;
			}
		}
		return nullptr;
	}
	void setHitboxCallback(KHitboxCallback *cb) {
		m_Callback = cb;
	}
	KHitbox * getTrulyThisHitbox(KNode *node) {
		auto it = m_Nodes.find(node);
		if (it != m_Nodes.end()) {
			return it->second;
		}
		return nullptr;
	}
	KHitbox * getThisHitbox(KNode *node) {
		static const std::string name = "$hitbox";
		if (node == nullptr) {
			return nullptr;
		}
		{
			KHitbox *co = getTrulyThisHitbox(node);
			if (co) return co;
		}
		{
			KNode *nd = node->findChild(name);
			KHitbox *co = getTrulyThisHitbox(nd);
			if (co) return co;
		}
		return nullptr;
	}
	KHitbox * getChildHitbox(KNode *node, int index) {
		if (node == nullptr) return nullptr;
		
		int cnt = node->getChildCount();
		for (int i=0; i<cnt; i++) {
			KNode *subnode = node->getChild(i);
			KHitbox *hitbox = getThisHitbox(subnode);
			if (hitbox) {
				if (index == 0) return hitbox;
				index--;
			}
		}
		return nullptr;
	}
	int getChildHitboxCount(KNode *node) {
		if (node == nullptr) return 0;

		int ret = 0;
		int cnt = node->getChildCount();
		for (int i=0; i<cnt; i++) {
			KNode *subnode = node->getChild(i);
			KHitbox *hitbox = getThisHitbox(subnode);
			if (hitbox) ret++;
		}
		return ret;
	}
	void addHitbox(KNode *node) {
		KHitbox *hb = new KHitbox();
		hb->_setNode(node);
		m_Nodes[node] = hb;
	}
	void delHitbox(KNode *node) {
		KHitbox *hb = getThisHitbox(node);
		if (hb == nullptr) return;

		for (auto it=m_HitPairs.begin(); it!=m_HitPairs.end(); /*no expr*/) {
			if (it->m_Object1.node == hb->getNode() || it->m_Object2.node == hb->getNode()) {
				onHitboxExit(*it);
				it = m_HitPairs.erase(it); // 衝突エンティティが消えた場合、EXIT イベントが発生しないことに注意
			} else {
				++it;
			}
		}
		{
			auto it = m_Nodes.find(node);
			if (it != m_Nodes.end()) {
				KHitbox *hb = it->second;
				hb->_setNode(nullptr);
				hb->drop();
				m_Nodes.erase(it);
			}
		}
	}
	void clearHitboxNodes() {
		m_HitPairs.clear();

		for (auto it=m_Nodes.begin(); it!=m_Nodes.end(); ++it) {
			KHitbox *hb = it->second;
			hb->drop();
		}
		m_Nodes.clear();
	}
	/// ヒットボックス同士が接触しているなら、その情報を返す
	const KHitPair * getHitboxHit(KNode *node1, KNode *node2) const {
		K__ASSERT(node1 != node2);
		for (size_t i=0; i<m_HitPairs.size(); i++) {
			const KHitPair *pair = &m_HitPairs[i];
			if (pair->isPairOf(node1, node2, nullptr, nullptr)) {
				return pair;
			}
		}
		return nullptr;
	}
	/// 接触しているヒットボックスのペア数を返す
	int getHitboxHitPairCount() const {
		return (int)m_HitPairs.size();
	}
	/// 接触しているヒットボックスのペアを得る
	const KHitPair * getHitboxHitPair(int index) const {
		return &m_HitPairs[index];
	}
	int getIntersectHitboxes(KHitbox *hb, std::vector<KHitbox*> &out_list) const {
		int num = 0;
		for (auto it=m_HitPairs.begin(); it!=m_HitPairs.end(); ++it) {
			const KHitPair &pair = *it;
			if (pair.m_Object1.hitbox == hb) {
				out_list.push_back(pair.m_Object2.hitbox);
				num++;
			}
			if (pair.m_Object2.hitbox == hb) {
				out_list.push_back(pair.m_Object1.hitbox);
				num++;
			}
		}
		return num;
	}
	/// ヒットボックス範囲を描画する
	void drawHitboxes(KNode *cameranode) const {
		KMatrix4 tr;
		cameranode->getWorld2LocalMatrix(&tr);
		for (auto it=m_Nodes.begin(); it!=m_Nodes.end(); ++it) {
			KHitbox *hb = it->second;
			if (_IsDebugInfoVisible(hb->getNode(), cameranode)) {
				drawHitbox(hb, tr);
			}
		}
	}
	/// 衝突判定のグループ情報をセットする
	/// グループの組み合わせによって衝突判定するかどうかを決めることができる
	void setGroupCount(int count) {
		m_Groups.resize(count);
	}
	int getGroupCount() const {
		return m_Groups.size();
	}
	KHitboxGroup * getGroup(int index) {
		return &m_Groups[index];
	}
	const KHitboxGroup * getGroup(int index) const {
		return &m_Groups[index];
	}

public:
	const KHitboxGroup * getGroupOfHitbox(const KHitbox *hitbox) const {
		int idx = hitbox->getGroupIndex();
		if (0 <= idx && idx < (int)m_Groups.size()) {
			return &m_Groups[idx];
		}
		return nullptr;
	}
	void updateHitboxGroupGui() {
		ImGui::Text("Groups: %d", (int)m_Groups.size());
		ImGui::PushID(KImGui::ID(0));
		for (size_t i=0; i<m_Groups.size(); i++) {
			KHitboxGroup *group = &m_Groups[i];
			ImGui::Separator();
			if (ImGui::TreeNode(KImGui::ID(i), "[%d] %s", i, group->m_NameForInspector.c_str())) {
				ImGui::Text("Name: %s", group->m_Name.c_str());
				ImGui::Checkbox("Gizmo visible", &group->m_GizmoVisible);
				ImGui::ColorEdit4("Gizmo color", reinterpret_cast<float*>(&group->m_GizmoColor));
				ImGui::TreePop();
			}
		}
		ImGui::PopID();
	}
	void updateHitboxHitPairGui() {
		ImGui::Text("HitPairs(Sensor): %d", (int)m_HitPairs.size());
		for (size_t i=0; i<m_HitPairs.size(); i++) {
			KHitPair *pair = &m_HitPairs[i];
			ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Appearing);
			if (ImGui::TreeNode(KImGui::ID(i), "[%d]", i)) {
				std::string name1 = pair->m_Object1.node->getNameInTree();
				std::string name2 = pair->m_Object2.node->getNameInTree();
				ImGui::Text("Node1   : %s", name1.c_str());
				ImGui::Text("Node2   : %s", name2.c_str());
				if (pair->m_TimestampEnter == pair->m_TimestampLastUpdate) {
					ImGui::Text("State     : %s", "Enter");
				} else if (pair->m_TimestampExit == pair->m_TimestampLastUpdate) {
					ImGui::Text("State     : %s", "Exit");
				} else {
					ImGui::Text("State     : %s", "Stay");
				}
				ImGui::Text("Enter time: %d(F)", pair->m_TimestampEnter);
				ImGui::Text("Exit  time: %d(F)", pair->m_TimestampExit);
				ImGui::Text("Stay  time: %d(F)", pair->m_TimestampLastUpdate - pair->m_TimestampEnter);
				ImGui::TreePop();
			}
		}
	}
	void updateHitboxes() {
		updateSensorNodeList();
		updateSensorCollision();
	}
	void removeEndedPairs() {
		// 前フレームで衝突が解消されたペアを削除する
		for (auto it=m_HitPairs.begin(); it!=m_HitPairs.end(); /*++it*/) {
			if (it->m_TimestampExit >= 0) {
				it = m_HitPairs.erase(it);
			} else {
				++it;
			}
		}
	}
	void detectHitboxExit() {
		// 衝突解消の検出
		// ただし、衝突中にエンティティ自体が消えた場合は EXIT イベントが発生しないことに注意
		for (auto it=m_HitPairs.begin(); it!=m_HitPairs.end(); ++it) {
			KHitPair &pair = *it;
			if (pair.m_TimestampLastUpdate < m_Clock) {
				// タイムスタンプが古いままになっているという事は、このフレームで衝突が検出されなかったことを意味する
				// 衝突解消時刻を記録しておく
				pair.m_TimestampExit = m_Clock;
				pair.m_TimestampLastUpdate = m_Clock; // 衝突解消時刻をもって最後の更新時刻とする。これをしないと、KHitPair::isExit() の条件式が正しく動作しない
				onHitboxExit(pair);
			}
		}
	}
	void tickHitboxClock() {
		m_Clock++;
	}

	void update_hitbox_inspector_item(KHitbox *hitbox) {
	#ifndef NO_IMGUI
		ImGui::Separator();
		ImGui::PushID(hitbox);
		std::string typestr = getGroupOfHitbox(hitbox)->m_NameForInspector;
		ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Once);
		if (ImGui::TreeNode(typestr.c_str())) {
			hitbox->on_node_inspector();
			if (m_HitPairs.size() == 0) {
				ImGui::Text("Hits with: (No hit)");
			} else {
				ImGui::Text("Hits with: %d hitboxes", m_HitPairs.size());
				ImGui::Indent();
				for (size_t i=0; i<m_HitPairs.size(); i++) {
					const KHitPair *hitpair = &m_HitPairs[i];
					if (hitpair->m_Object1.hitbox == hitbox) {
						const std::string &label = getGroupOfHitbox(hitpair->m_Object2.hitbox)->m_NameForInspector;
						const KNode *node = hitpair->m_Object2.node;
						ImGui::BulletText("%s.%s", node->getName().c_str(), label.c_str());
					}
					if (hitpair->m_Object2.hitbox == hitbox) {
						const std::string &label = getGroupOfHitbox(hitpair->m_Object1.hitbox)->m_NameForInspector;
						const KNode *node = hitpair->m_Object1.node;
						ImGui::BulletText("%s.%s", node->getName().c_str(), label.c_str());
					}
				}
				ImGui::Unindent();
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	#endif // !NO_IMGUI
	}

private:
	bool getHitboxHits(const KHitbox *hb, KNode *node) const {
		for (size_t i=0; i<m_HitPairs.size(); i++) {
			auto pair = &m_HitPairs[i];
			if (pair->m_Object1.hitbox == hb && pair->m_Object2.node == node) return true;
			if (pair->m_Object2.hitbox == hb && pair->m_Object1.node == node) return true;
		}
		return false;
	}
	void drawHitbox(KHitbox *hitbox, const KMatrix4 &transform) const {
		K__ASSERT(hitbox);
		if (!hitbox->getNode()->getEnable()) return;
		if (hitbox->getMute()) return;

		// この行列だと、HITBOXの形が回転拡縮の影響を受けたものになってしまう。
		// そうではなく、回転拡縮の影響を受けるのはHITBOXの中心座標だけであり、HITBOX自体のサイズは回転拡縮の影響を受けないようにしたい
		// KMatrix4 matrix;
		// node->getLocal2WorldMatrix(&matrix);
		// matrix = matrix * transform;
		KNode *selnode = KInspector::getSelectedEntity(0);
		const float line_alpha = 1.0f;//kk_GetGizmoBlinkingAlpha(hitbox->getNode());
		{
			const KHitboxGroup *group = getGroupOfHitbox(hitbox);
			K__ASSERT(group);
			if (group == nullptr || !group->m_GizmoVisible) return;
			KColor color = group->m_GizmoColor;
			color.a = line_alpha;
			// インスペクターで選択中のエンティティと接触しているもの色を変える
			if (m_Highlight && selnode && selnode != hitbox->getNode()) {
				if (getHitboxHits(hitbox, selnode)) {
					color = KColor(1, 1, 1, line_alpha);
				}
			}
			KVec3 halfsize = hitbox->getHalfSize();
			KVec3 centerInWorld = hitbox->getCenterInWorld();
			// Hitboxは回転拡縮の影響を受けないので
			// node->getLocal2WorldMatrix を使わないようにする
			// ※Hitboxの中心座標がどこに来るのかという計算にはノードの回転拡縮の影響があるが、
			// Hitboxそのものの大きさは回転角祝の影響を受けない
			KMatrix4 matrix = KMatrix4::fromTranslation(centerInWorld) * transform;
			KScreen::getGizmo()->addAabb(matrix, -halfsize, halfsize, color);
		}
	}
	int get_index_of_hit_pair(const KHitbox *hitbox1, const KHitbox *hitbox2) const {
		for (size_t i=0; i<m_HitPairs.size(); i++) {
			const KHitPair &pair = m_HitPairs[i];
			if (pair.m_Object1.hitbox == hitbox1 && pair.m_Object2.hitbox == hitbox2) {
				return (int)i;
			}
			if (pair.m_Object1.hitbox == hitbox2 && pair.m_Object2.hitbox == hitbox1) {
				return (int)i;
			}
		}
		return -1;
	}

	void updateSensorNodeList() {
		m_GroupedEntries.clear();
		m_GroupedEntries.resize(m_Groups.size());
		for (auto it=m_Nodes.begin(); it!=m_Nodes.end(); ++it) {
			KNode *node = it->first;
			KHitbox *hitbox = it->second;

			// MUTEされていたら判定しない
			if (hitbox->getMute()) continue;
		
			// コライダーが Disabled だったら判定しない
			if (!node->getEnable()) continue;

			// Entity がツリー内で Disabled だったら判定しない
			// （paused 状態は考慮しない。オブジェクトの動きが停止している時でも、勝手に衝突判定を突き抜けられては困る）
			if (!node->getEnableInTree()) continue;
		
			int gindex = hitbox->getGroupIndex();
			if (0 <= gindex && gindex < (int)m_GroupedEntries.size()) {
				m_GroupedEntries[gindex].push_back(hitbox);
			}
		}
	}
	void updateSensorCollision() {
		for (int i=0; i<(int)m_Groups.size(); i++) {
			for (int j=i+1; j<(int)m_Groups.size(); j++) {
				const KHitboxGroup &group1 = m_Groups[i];
				const KHitboxGroup &group2 = m_Groups[j];
				// グループの組み合わせによるフィルタリングを通過したら、それぞれのグループに属するヒットボックス同士で衝突処理を行う
				if (group1.canCollidableWith(j) && group2.canCollidableWith(i)) {
					update_group_nodes(i, j);
				}
			}
		}
	}
	/// グループ group_id1 と group_id2 のそれぞれのグループに属するヒットボックス同士で衝突処理を行う
	void update_group_nodes(int groupIndex1, int groupIndex2) {
		K__ASSERT(groupIndex1 != groupIndex2); // 同一グルーブ同士の比較はダメ
		std::vector<KHitbox*> &groupHitboxes1 = m_GroupedEntries[groupIndex1];
		std::vector<KHitbox*> &groupHitboxes2 = m_GroupedEntries[groupIndex2];
		for (size_t i=0; i<groupHitboxes1.size(); i++) {
			for (size_t j=0; j<groupHitboxes2.size(); j++) {
				KHitbox *hitbox1 = groupHitboxes1[i];
				KHitbox *hitbox2 = groupHitboxes2[j];
				if (hitbox1->getNode() == hitbox2->getNode()) continue; // 同キャラ判定を排除
				if (hitbox1->getOwner() == hitbox2->getOwner()) continue; // 同一オーナー判定を排除
				update_node_pair(hitbox1, hitbox2);
			}
		}
	}
	/// 衝突ノード node1 と node2 の衝突判定と処理を行う
	void update_node_pair(KHitbox *hitbox1, KHitbox *hitbox2) {
		bool deny = false;
		if (m_Callback) m_Callback->on_hitbox_update(hitbox1, hitbox2, &deny);
		if (deny) return;
		const KVec3 hitboxPos1 = hitbox1->getCenterInWorld();
		const KVec3 hitboxPos2 = hitbox2->getCenterInWorld();
		const KVec3 delta = hitboxPos1 - hitboxPos2;
		const KVec3 r = hitbox1->getHalfSize() + hitbox2->getHalfSize();
		if (fabsf(delta.x) < r.x && fabsf(delta.y) < r.y && fabsf(delta.z) < r.z) {
			int pair_index = get_index_of_hit_pair(hitbox1, hitbox2);
			if (pair_index >= 0) {
				// 同じ衝突ペアが見つかった。衝突状態が持続している
				KHitPair &pair = m_HitPairs[pair_index];
				pair.m_TimestampLastUpdate = m_Clock;
				pair.m_Object1.hitbox_pos   = hitboxPos1;
				pair.m_Object2.hitbox_pos   = hitboxPos2;
				onHitboxStay(pair);
			} else {
				// 同じ衝突ペアが見つからなかった。
				// このフレームで新しく衝突した
				KHitPair pair;
				pair.m_Object1.hitbox     = hitbox1;
				pair.m_Object2.hitbox     = hitbox2;
				pair.m_Object1.node       = hitbox1->getNode();
				pair.m_Object2.node       = hitbox2->getNode();
				pair.m_Object1.hitbox_pos = hitboxPos1;
				pair.m_Object2.hitbox_pos = hitboxPos2;
				pair.m_TimestampEnter    = m_Clock;
				pair.m_TimestampExit     = -1;
				pair.m_TimestampLastUpdate = m_Clock;
				m_HitPairs.push_back(pair);
				onHitboxEnter(pair);
			}
		}
	}
	void onHitboxEnter(KHitPair &pair) {
		KSig sig(K_SIG_HITBOX_ENTER);
		sig.setNode("hitbox1", pair.m_Object1.node);
		sig.setNode("hitbox2", pair.m_Object2.node);
		KNodeTree::broadcastSignal(sig);
	}
	void onHitboxStay(KHitPair &pair) {
		KSig sig(K_SIG_HITBOX_STAY);
		sig.setNode("hitbox1", pair.m_Object1.node);
		sig.setNode("hitbox2", pair.m_Object2.node);
		KNodeTree::broadcastSignal(sig);
	}
	void onHitboxExit(KHitPair &pair) {
		KSig sig(K_SIG_HITBOX_EXIT);
		sig.setNode("hitbox1", pair.m_Object1.node);
		sig.setNode("hitbox2", pair.m_Object2.node);
		KNodeTree::broadcastSignal(sig);
	}
}; // CHitboxManagerImpl

#pragma endregion // CHitboxManagerImpl






#pragma region KHitboxGroup
KHitboxGroup::KHitboxGroup() {
	m_Bitmask = 0; // 初期設定では誰とも衝突しない
	m_GizmoVisible = true;
}
void KHitboxGroup::clearMask() {
	m_Bitmask = 0;
}
void KHitboxGroup::setCollideWithAny() {
	m_Bitmask = -1;
}
void KHitboxGroup::setCollideWith(int groupIndex) {
	K__ASSERT(0 <= groupIndex && groupIndex < 32);
	m_Bitmask |= (1 << groupIndex);
}
void KHitboxGroup::setDontCollideWith(int groupIndex) {
	K__ASSERT(0 <= groupIndex && groupIndex < 32);
	m_Bitmask &= ~(1 << groupIndex);
}
bool KHitboxGroup::canCollidableWith(int groupIndex) const {
	K__ASSERT(0 <= groupIndex && groupIndex < 32);
	return (m_Bitmask & (1 << groupIndex)) != 0;
}

void KHitboxGroup::setName(const char *n) {
	m_Name = n;
}

void KHitboxGroup::setNameForInspector(const char *n) {
	m_NameForInspector = n;
}

void KHitboxGroup::setGizmoColor(const KColor &color) {
	m_GizmoColor = color;
}
#pragma endregion // KHitboxGroup




#pragma region KHitPair
KHitPair::KHitPair() {
	m_TimestampEnter = -1;
	m_TimestampExit = -1;
	m_TimestampLastUpdate = -1;
}
bool KHitPair::isEnter() const {
	return m_TimestampEnter == m_TimestampLastUpdate;
}
bool KHitPair::isExit() const {
	return m_TimestampExit == m_TimestampLastUpdate;
}
bool KHitPair::isStay() const {
	return m_TimestampEnter < m_TimestampLastUpdate;
}
bool KHitPair::isPairOf(const KNode *node1, const KNode *node2, KHitPair::Obj *hit1, KHitPair::Obj *hit2) const {
	bool match1, match2;

	// 順番通りでチェック
	match1 = (node1 == nullptr || m_Object1.node == node1);
	match2 = (node2 == nullptr || m_Object2.node == node2);
	if (match1 && match2) {
		if (hit1) *hit1 = m_Object1;
		if (hit2) *hit2 = m_Object2;
		return true;
	}
	// 入れ替えてチェック
	match1 = (node1 == nullptr || m_Object2.node == node1);
	match2 = (node2 == nullptr || m_Object1.node == node2);
	if (match1 && match2) {
		if (hit1) *hit1 = m_Object2;
		if (hit2) *hit2 = m_Object1;
		return true;
	}
	return false;
}
bool KHitPair::isPairOfGroup(int groupIndex1, int groupIndex2, KHitbox **out_hitbox1, KHitbox **out_hitbox2) const {
	if (m_Object1.hitbox && m_Object2.hitbox) {
		int g1 = m_Object1.hitbox->getGroupIndex();
		int g2 = m_Object2.hitbox->getGroupIndex();
		if (g1 == groupIndex1 && g2 == groupIndex2) {
			if (out_hitbox1) *out_hitbox1 = m_Object1.hitbox;
			if (out_hitbox2) *out_hitbox2 = m_Object2.hitbox;
			return true;
		}
		if (g1 == groupIndex2 && g2 == groupIndex1) {
			if (out_hitbox1) *out_hitbox1 = m_Object2.hitbox;
			if (out_hitbox2) *out_hitbox2 = m_Object1.hitbox;
			return true;
		}
	}
	return false;
}
const KHitPair::Obj * KHitPair::getObjectFor(const KNode *node) const {
	if (m_Object1.node == node) return &m_Object1;
	if (m_Object2.node == node) return &m_Object2;
	return nullptr;
}
#pragma endregion // KHitPair




#pragma region KHitbox
static CHitboxManagerImpl *g_HitboxMgr = nullptr;

void KHitbox::install() {
	K__ASSERT(g_HitboxMgr == nullptr);
	g_HitboxMgr = new CHitboxManagerImpl();
}
void KHitbox::uninstall() {
	if (g_HitboxMgr) {
		g_HitboxMgr->drop();
		g_HitboxMgr = nullptr;
	}
}
void KHitbox::attach(KNode *node) {
	K__ASSERT(g_HitboxMgr);
	if (node && !isAttached(node)) {
		g_HitboxMgr->addHitbox(node);
	}
}
KHitbox * KHitbox::attach2(KNode *node) {
	KNode *hbnode = KNode::create();
	hbnode->setParent(node);
	hbnode->setName("$hitbox");
	KHitbox::attach(hbnode);
	KHitbox *ret = KHitbox::of(hbnode);
	hbnode->drop();
	return ret;
}
bool KHitbox::isAttached(KNode *node) {
	return of(node) != nullptr;
}
KHitbox * KHitbox::cast(KNode *node) {
	return dynamic_cast<KHitbox*>(node);
}
KHitbox * KHitbox::of(KNode *node) {
	K__ASSERT(g_HitboxMgr);
	return g_HitboxMgr->getThisHitbox(node);
}


KHitbox::KHitbox() {
	m_HalfSize = KVec3(16, 16, 16);
	m_GroupIndex = 0;
	memset(m_UserData, 0, sizeof(m_UserData));
	m_Mute = 0;
	m_Node = nullptr;
}
KNode * KHitbox::getOwner() {
	return m_Node ? m_Node->getParent() : nullptr;
}
KNode * KHitbox::getNode() {
	return m_Node;
}
void KHitbox::_setNode(KNode *node) {
	m_Node = node;
}
void KHitbox::on_node_inspector() {
	if (m_Mute) {
		KImGui::PushTextColor(KImGui::COLOR_STRONG);
		ImGui::Text("Mute: %d", m_Mute);
		KImGui::PopTextColor();
	} else {
		ImGui::Text("Mute: %d", m_Mute);
	}
	ImGui::InputInt("Group", &m_GroupIndex);
	{
		KVec3 value = getHalfSize();
		if (ImGui::DragFloat3("Half size", (float*)&value)) {
			setHalfSize(value);
		}
	}
	for (int i=0; i<USERDATA_COUNT; i++) {
		char s[256] = {0};
		sprintf_s(s, "UserData%d", i);
		ImGui::InputInt(s, &m_UserData[i]);
	}
}
int KHitbox::getGroupIndex() const {
	return m_GroupIndex;
}
void KHitbox::setGroupIndex(int index) {
	m_GroupIndex = index;
}
void KHitbox::copyFrom(const KHitbox *source) {
	if (source == nullptr) return;
	for (int i=0; i<USERDATA_COUNT; i++) {
		m_UserData[i] = source->m_UserData[i];
	}
	m_HalfSize = source->m_HalfSize;
	m_GroupIndex = source->m_GroupIndex;
}
const KVec3 & KHitbox::getCenter() const {
	static const KVec3 s_zero;
	return m_Node ? m_Node->getPosition() : s_zero;
}
void KHitbox::setCenter(const KVec3 &p) {
	if (m_Node) m_Node->setPosition(p);
}
const KVec3 & KHitbox::getHalfSize() const {
	return m_HalfSize;
}
void KHitbox::setHalfSize(const KVec3 &s) {
	m_HalfSize = s;
}
bool KHitbox::getEnabled() const {
	return m_Node && m_Node->getEnable();
}
void KHitbox::setEnabled(bool b) {
	if (m_Node) m_Node->setEnable(b);
}
void KHitbox::setMute(int value) {
	m_Mute = value;
}
int KHitbox::getMute() const {
	return m_Mute;
}
void KHitbox::setUserData(int slot, intptr_t data) {
	if (0 <= slot && slot < USERDATA_COUNT) {
		m_UserData[slot] = data;
	}
}
intptr_t KHitbox::getUserData(int slot) const {
	if (0 <= slot && slot < USERDATA_COUNT) {
		return m_UserData[slot];
	}
	return 0;
}
void KHitbox::setUserTag(KName tag) {
	m_UserTag = tag;
}
KName KHitbox::getUserTag() {
	return m_UserTag;
}
bool KHitbox::hasUserTag(KName tag) {
	return m_UserTag == tag;
}


KVec3 KHitbox::getCenterInWorld() const {
	return m_Node ? m_Node->getWorldPosition() : KVec3();
}
bool KHitbox::getAabbInWorld(KVec3 *minpos, KVec3 *maxpos) const {
	KVec3 wpos = getCenterInWorld();
	KVec3 half = getHalfSize();
	if (minpos) *minpos = wpos - half;
	if (maxpos) *maxpos = wpos + half;
	return true;
}
int KHitbox::getHitboxCount(KNode *node) {
	K__ASSERT(g_HitboxMgr);
	return g_HitboxMgr->getChildHitboxCount(node);
}
KHitbox * KHitbox::getHitboxByGroup(KNode *node, int groupindex) {
	K__ASSERT(g_HitboxMgr);
	return g_HitboxMgr->getHitboxByGroup(node, groupindex);
}
void KHitbox::setDebugLineVisible(bool visible) {
	K__ASSERT(g_HitboxMgr);
	g_HitboxMgr->setDebugLineVisible(visible);
}
void KHitbox::setDebug_alwaysShowDebug(bool val) {
	K__ASSERT(g_HitboxMgr);
	g_HitboxMgr->setDebug_alwaysShowDebug(val);
}
bool KHitbox::getDebug_alwaysShowDebug() {
	K__ASSERT(g_HitboxMgr);
	return g_HitboxMgr->getDebug_alwaysShowDebug();
}
int KHitbox::getIntersectHitboxes(KHitbox *hb, std::vector<KHitbox*> &out_list) {
	K__ASSERT(g_HitboxMgr);
	return g_HitboxMgr->getIntersectHitboxes(hb, out_list);
}
KHitbox * KHitbox::getHitboxByIndex(KNode *node, int index) {
	K__ASSERT(g_HitboxMgr);
	return g_HitboxMgr->getChildHitbox(node, index);
}
int KHitbox::getHitboxHitPairCount() {
	K__ASSERT(g_HitboxMgr);
	return g_HitboxMgr->getHitboxHitPairCount();
}
const KHitPair * KHitbox::getHitboxHitPairByIndex(int index) {
	K__ASSERT(g_HitboxMgr);
	return g_HitboxMgr->getHitboxHitPair(index);
}
const KHitPair * KHitbox::getHitboxHitPairOf(KNode *node1, KNode *node2) {
	K__ASSERT(g_HitboxMgr);
	return g_HitboxMgr->getHitboxHit(node1, node2);
}
void KHitbox::setGroupCount(int count) {
	K__ASSERT(g_HitboxMgr);
	g_HitboxMgr->setGroupCount(count);
}
KHitboxGroup * KHitbox::getGroup(int index) {
	K__ASSERT(g_HitboxMgr);
	return g_HitboxMgr->getGroup(index);
}

#pragma endregion // KHitbox





} // namespace
