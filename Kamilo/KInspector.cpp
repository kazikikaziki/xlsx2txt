#include "KInspector.h"

#include <algorithm>
#include "KAction.h"
#include "KScreen.h"
#include "KImGui.h"
#include "KInternal.h"
#include "KCamera.h"
#include "KHitbox.h"
#include "KMouse.h"
#include "KScreen.h"
#include "KSig.h"
#include "KSolidBody.h"
#include "KSpriteDrawable.h"
#include "KTextNode.h"
#include "KWindow.h"
#include "keng_game.h"

namespace Kamilo {

static const float GIZMO_AXIS_LEN = 32;

static ImVec2 _ImVec2i(int x, int y) {
	return ImVec2((float)x, (float)y);
}


class CNdIcons {
public:
	enum Type {
		TP_VISIBLE,
		TP_PAUSED,
		TP_ENABLED,
		TP_NODE,
		TP_VIEWPORT,
		TP_SYSTEM,
		TP_CAMERA,
		TP_SPRITE,
		TP_MESH,
		TP_TEXT,
		TP_STATIC_BODY,
		TP_DYNAMIC_BODY,
		TP_HITBOX,
		TP_ENUM_MAX
	};

	static const int ICON_W = 16;
	static const int ICON_H = 12;
	static const int ICON_TEX_W = 16;
	static const int ICON_TEX_H = 256; // >= ICON_H * ENUM_MAX

	static KTEXID makeIconTexture() {
		/// [aa_test] <--- Doxgen @snippet コマンドからの参照用なので削除してはいけない
		const int ICON_BMP_W = ICON_W;
		const int ICON_BMP_H = ICON_H * TP_ENUM_MAX;
		const char *dat = {
			// TP_VISIBLE
			"                "
			"                "
			"                "
			"    o######o    "
			" o##  o##o  ##o "
			"#    o####o    #"
			"#    o####o    #"
			"#    o####o    #"
			" o##  o##o  ##o "
			"    o######o    "
			"                "
			"                "

			// TP_PAUSED
			"     ######     "
			"       #o       "
			"     o####o     "
			"    #      #    "
			"   #   #o   #   "
			"  o    #o    o  "
			"  #    #o    #  "
			"  #    ##### #  "
			"  o          o  "
			"   #        #   "
			"    #      #    "
			"     o####o     "

			// TP_ENABLED
			"       #o       "
			"       #o       "
			"    #  #o  #    "
			"   #   #o   #   "
			"  o    #o    o  "
			"  #    #o    #  "
			"  #    #o    #  "
			"  o    #o    o  "
			"   #        #   "
			"    #      #    "
			"     o####o     "
			"                "

			// TP_NODE
			"                "
			"      o##o      "
			"     #    #     "
			"    #      #    "
			"   o        o   "
			"   #        #   "
			"   #        #   "
			"   o        o   "
			"    #      #    "
			"     #    #     "
			"      o##o      "
			"                "

			// TP_VIEWPORT
			"                "
			"                "
			"  o##########o  "
			"  #          #  "
			"  #          #  "
			"  #          #  "
			"  #          #  "
			"  #          #  "
			"  #          #  "
			"  o##########o  "
			"      #  #      "
			"    ########    "

			// TP_SYSTEM
			"                "
			"      o#o       "
			"    #o###o#     "
			"   #########    "
			"   o##   ##o    "
			"  o##  #  ##o   "
			"  ### ### ###   "
			"  o##  #  ##o   "
			"   o##   ##o    "
			"   #########    "
			"    #o###o#     "
			"      o#o       "

			// TP_CAMERA
			"                "
			"                "
			"             ## "
			"  ########  ### "
			"  ######## #### "
			"  ############# "
			"  ############# "
			"  ######## #### "
			"  ########  ### "
			"             ## "
			"                "
			"                "

			// TP_SPRITE
			"      ####      "
			"     ######     "
			"     # ## #     "
			"     ######     "
			"      ####      "
			"       oo       "
			"     #######    "
			"   ##########   "
			"  ############  "
			"  ##o######o##  "
			"  ##o######o##  "
			"  ##o######o##  "

			// TP_MESH
			"                "
			"  ###########   "
			"  #   ##   ##   "
			"  #  # #  # #   "
			"  # #  # #  #   "
			"  ##   ##   #   "
			"  ###########   "
			"  #   ##   ##   "
			"  #  # #  # #   "
			"  # #  # #  #   "
			"  ##   ##   #   "
			"  ###########   "

			// TP_TEXT
			"  ############  "
			"  ############  "
			"  ##   ##   ##  "
			"  #    ##    #  "
			"  #    ##    #  "
			"       ##       "
			"       ##       "
			"       ##       "
			"       ##       "
			"       ##       "
			"       ##       "
			"     ######     "

			// TP_STATIC_BODY
			"                "
			"                "
			"     ########   "
			"    #      ##   "
			"   #      #o#   "
			"  ########oo#   "
			"  #      #oo#   "
			"  #      #oo#   "
			"  #      #o#    "
			"  #      ##     "
			"  ########      "
			"                "

			// TP_DYNAMIC_BODY
			"                "
			"     o####o     "
			"    ##ooo###    "
			"   #o   oo###   "
			"  o#      o###  "
			"  #o      o###  "
			"  #o      o###  "
			"  ##o   oo####  "
			"  o##oooo####o  "
			"   ##########   "
			"    ########    "
			"     #####o     "

			// TP_HITBOX
			//"##########      "
			//"#        #      "
			//"#        #      "
			//"#        #      "
			//"#    ###########"
			//"#    #ooo#     #"
			//"#    #ooo#     #"
			//"#    #ooo#     #"
			//"##########     #"
			//"     #         #"
			//"     #         #"
			//"     ###########"
			"                "
			"                "
			"  # # # # # # # "
			"                "
			"  #           # "
			"                "
			"  #           # "
			"                "
			"  #           # "
			"                "
			"  # # # # # # # "
			"                "
		};
		K__ASSERT(strlen(dat) == ICON_BMP_W * ICON_BMP_H);
		K__ASSERT(ICON_BMP_W <= ICON_TEX_W);
		K__ASSERT(ICON_BMP_H <= ICON_TEX_H);
		std::unordered_map<char, KColor32> map;
		if (1) {
			// Blue
			map['#'] = KColor32(0xAA, 0x66, 0xBB, 0xFF);
			map['o'] = KColor32(0xAA, 0x66, 0xBB, 0x99);
		} else {
			// Mono
			map['#'] = KColor32(0xFF, 0xFF, 0xFF, 0xFF);
			map['o'] = KColor32(0xFF, 0xFF, 0xFF, 0x99);
		}
		KImage img = KImageUtils::makeImageFromAscii(ICON_TEX_W, ICON_TEX_H, ICON_BMP_W, ICON_BMP_H, map, dat);
		/// [aa_test] <--- Doxgen @snippet コマンドからの参照用なので削除してはいけない
	#ifdef _DEBUG
		if (0) {
			std::string png = img.saveToMemory();
			KOutputStream file;
			file.openFileName("~gui_icons.png");
			file.write(png.data(), png.size());
		}
	#endif
		KTEXID texid = KVideo::createTexture(ICON_TEX_W, ICON_TEX_H);
		KTexture *tex = KVideo::findTexture(texid);
		if (tex) tex->writeImageToTexture(img, 0.0f, 0.0f);
		return texid;
	}

	static void getUV(Type t, KVec2 *uv0, KVec2 *uv1) {
		uv0->x = 0.0f;
		uv0->y = (float)ICON_H * t     / ICON_TEX_H;
		uv1->x = (float)ICON_W         / ICON_TEX_W;
		uv1->y = (float)ICON_H * (t+1) / ICON_TEX_H;
	}
};


class CDebugRenderNodes: public KTraverseCallback {
public:
	int mCameraLayer;
	KNodeArray mNodes;

	CDebugRenderNodes() {
		mCameraLayer = 0;
	}
	void clear() {
		mCameraLayer = 0;
		mNodes.clear();
	}
	virtual void onVisit(KNode *node) override {
		// カメラのレイヤーと一致しないと描画対象にならない
		int this_layer = node->getLayerInTree();
		if (this_layer != mCameraLayer) {
			return;
		}

		// インスペクター上で選択またはハイライト状態になっていないとダメ
		if (KInspector::isInstalled()) {
			if (!KInspector::isEntitySelected(node) && !KInspector::isEntityHighlighted(node, false)) {
				return;
			}
		}
		mNodes.push_back(node);
	}
};


class CTreeInspector: public KManager, public KDebugTree {
	KTEXID m_icon_tex;
	CDebugRenderNodes m_rendernodes;
public:
	CTreeInspector() {
		m_icon_tex = nullptr;
	}
	void start() {
		m_icon_tex = CNdIcons::makeIconTexture();
		KEngine::addManager(this);
		KEngine::setManagerRenderDebugEnabled(this, true); // on_manager_renderdebug
		init();
	}
	void end() {
	}
	// エンティティ e を選択状態にする
	virtual void on_select(KNode *node) {
		if (KInspector::isInstalled()) {
			KInspector::unselectAllEntities();
			KInspector::selectEntity(node);
		}
	}
	virtual void on_unselect(KNode *node) {
	}
	void onGuiEntity(KNode *node) {
		KDebugGui::K_DebugGui_NodeState(node);

		ImGui::Separator();
		KDebugGui::K_DebugGui_NodeNameId(node);
		KDebugGui::K_DebugGui_NodeTag(node);
		KDebugGui::K_DebugGui_NodeLayer(node);
		KDebugGui::K_DebugGui_NodePriority(node);
		KDebugGui::K_DebugGui_NodeAtomic(node);
		KDebugGui::K_DebugGui_NodeRenderAfterChildren(node);
		{
			bool b = node->getLocalRenderOrder() == KLocalRenderOrder_TREE;
			if (ImGui::Checkbox("Use Local Render Order", &b)) {
				node->setLocalRenderOrder(b ? KLocalRenderOrder_TREE : KLocalRenderOrder_DEFAULT);
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(
					u8"このノードと子をツリー巡回順で描画する。False の場合はカメラの描画準設定を使う\n"
					u8"True にした場合はカメラの設定とは関係なくツリー巡回順（親→子）の順番で描画する\n"
					u8"子→親の順番にしたい場合は Render after children を有効にする"
				);
			}
		}
		KDebugGui::K_DebugGui_NodeRenderNoCulling(node);
		KDebugGui::K_DebugGui_NodeWarning(node);
		ImGui::Dummy(ImVec2(0, 8));

		ImGui::Separator();
		if (ImGui::TreeNode("Scene")) {
			KDebugGui::K_DebugGui_NodeParent(node, this);
			KDebugGui::K_DebugGui_NodeChildren(node, this);
			KNode *camera = KCamera::findCameraFor(node);
			KDebugGui::K_DebugGui_NodeCamera(node, camera, this);
			ImGui::Dummy(ImVec2(0, 8));
			ImGui::TreePop();
		}

		ImGui::Separator();
		ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
		if (ImGui::TreeNode("Transform")) {
			KDebugGui::K_DebugGui_NodeTransform(node);
			ImGui::Dummy(ImVec2(0, 8));
			ImGui::TreePop();
		}

		ImGui::Separator();
		ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
		if (ImGui::TreeNode("Color")) {
			KDebugGui::K_DebugGui_NodeColor(node);
			ImGui::Dummy(ImVec2(0, 8));
			ImGui::TreePop();
		}

		ImGui::Separator();
		ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
		if (ImGui::TreeNode("Specular")) {
			KDebugGui::K_DebugGui_NodeSpecular(node);
			ImGui::Dummy(ImVec2(0, 8));
			ImGui::TreePop();
		}
	}
	void onGuiMain() {
		gui_tree_tools();
		update_gui();

		// GUI 上でマウスポインタ直下にあるエンティティをゲーム画面内で強調表示させる
		KNode *node = get_mouse_hovered();
		KInspector::setHighlightedEntity(node);
	}

	// KScreenCallback
	virtual void on_manager_renderdebug(KNode *cameranode, KNode *start, KEntityFilterCallback *filter) override {
		bool show_debug = KInspector::isVisible();
		KGizmo *gizmo = KScreen::getGizmo();
		if (gizmo && show_debug) {

			m_rendernodes.clear();
			m_rendernodes.mCameraLayer = cameranode->getLayerInTree();
			start->traverse_children(&m_rendernodes); // KTraverseCallback

			if (m_rendernodes.mNodes.size() > 0) {
				KMatrix4 cam_tr;
				cameranode->getWorld2LocalMatrix(&cam_tr);
				for (auto it=m_rendernodes.mNodes.begin(); it!=m_rendernodes.mNodes.end(); ++it) {
					const KNode *node = *it;

					// 座標軸を描画する
					KMatrix4 local2world;
					node->getLocal2WorldMatrix(&local2world);

					KMatrix4 m = local2world * cam_tr;
					gizmo->addAxis(m, KVec3(), GIZMO_AXIS_LEN);
				}
			}
		}
	}
private:
	bool gui_icon(CNdIcons::Type icon, bool val, const char *hint_u8) const {
		KVec2 uv0, uv1;
		CNdIcons::getUV(icon, &uv0, &uv1);

		KDebugGui::K_DebugGui_Image(
			m_icon_tex,
			CNdIcons::ICON_W, CNdIcons::ICON_H,
			uv0, uv1,
			val ? KColor::WHITE : KColor(0.3f, 0.3f, 0.3f, 1.0f),
			KColor::ZERO, // border
			false,
			false
		);
		if (ImGui::IsItemClicked()) {
			return true;
		}
		if (hint_u8 && hint_u8[0]) {
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(hint_u8);
			}
		}
		return false;
	}

	// ツリーノードの Enabled, Paused, Visibled ボタン
	virtual void on_gui_node_buttons(KNode *node) {
		K__ASSERT(node);

		{
			bool val = node->getEnable();
			if (gui_icon(CNdIcons::TP_ENABLED, val, "Enable/Disable")) {
				node->setEnable(!val);
			}
		}
		ImGui::SameLine();
		{
			bool val = node->getPause();
			if (gui_icon(CNdIcons::TP_PAUSED, val, "Tick/Pause")) {
				node->setPause(!val);
			}
		}
		ImGui::SameLine();
		{
			bool val = node->getVisible();
			if (gui_icon(CNdIcons::TP_VISIBLE, val, "Show/Hide")) {
				node->setVisible(!val);
			}
		}
	}
	virtual int on_query_frame_number() const {
		return KEngine::getStatus(KEngine::ST_FRAME_COUNT_APP);
	}
	virtual void on_gui_node_icon(KNode *node) {
		float curStep = 14;
		float curX = ImGui::GetCursorPosX();
		if (node->hasFlag(KNode::FLAG_SYSTEM)) {
			gui_icon(CNdIcons::TP_SYSTEM, true, "System");
			curX += curStep;
			ImGui::SameLine();
			ImGui::SetCursorPosX(curX);
		}
		if (KCamera::isAttached(node)) {
			gui_icon(CNdIcons::TP_CAMERA, true, "Camera");
			curX += curStep;
			ImGui::SameLine();
			ImGui::SetCursorPosX(curX);
		}
		if (KTextDrawable::isAttached(node)) {
			gui_icon(CNdIcons::TP_TEXT, true, "Text");
			curX += curStep;
			ImGui::SameLine();
			ImGui::SetCursorPosX(curX);
		}
		if (KSpriteDrawable::isAttached(node)) {
			gui_icon(CNdIcons::TP_SPRITE, true, "Sprite");
			curX += curStep;
			ImGui::SameLine();
			ImGui::SetCursorPosX(curX);
		}
		if (KMeshDrawable::isAttached(node)) {
			gui_icon(CNdIcons::TP_MESH, true, "Mesh");
			curX += curStep;
			ImGui::SameLine();
			ImGui::SetCursorPosX(curX);
		}
		if (KStaticSolidBody::isAttached(node)) {
			gui_icon(CNdIcons::TP_STATIC_BODY, true, "Static Body");
			curX += curStep;
			ImGui::SameLine();
			ImGui::SetCursorPosX(curX);
		}
		if (KDynamicSolidBody::isAttached(node)) {
			gui_icon(CNdIcons::TP_DYNAMIC_BODY, true, "Dynamic Body");
			curX += curStep;
			ImGui::SameLine();
			ImGui::SetCursorPosX(curX);
		}
		if (KHitbox::isAttached(node)) {
			gui_icon(CNdIcons::TP_HITBOX, true, "Hitbox");
			curX += curStep;
			ImGui::SameLine();
			ImGui::SetCursorPosX(curX);
		}
		if (0) {
			gui_icon(CNdIcons::TP_NODE, true, "An object");
			curX += curStep;
			ImGui::SameLine();
			ImGui::SetCursorPosX(curX);
		}
		ImGui::NewLine();
	}
	void gui_tree_tools() {
		int num_nodes = KNodeTree::getNodeList(nullptr);
		ImGui::Text("Entities: %d", num_nodes);

		// IDを指定して選択
		{
			// 現在選択されているエンティティ名またはID
			std::string curr = get_selected_node_name();
			char text[256] = {0};
			strcpy(text, curr.c_str());
			if (ImGui::InputText("##find", text, sizeof(text))) {
				KNode *node = nullptr;
				if (text[0] == '#') {
					// # で始まっていれば ID 番号が入力されたものとする
					int i = 0;
					K::strToInt(text+1, &i);
					EID e = EID_from(i);
					node = KNodeTree::findNodeById(e);
				} else {
					// 該当するエンティティ名を探す
					node = KNodeTree::findNodeByName(nullptr, text);
				}
				if (node) {
					// 見つかった場合だけ再選択する
					clear_selection(true);
					select(node);
				} else {
					// 見つからない場合は選択状態を維持
				}
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(u8"エンティティ名または # に続いてID番号を指定する。\n※エンティティ名は大小文字を区別する");
			}
			ImGui::SameLine();
			if (ImGui::Button("P")) {
				KNode *node = KNodeTree::findNodeByName(nullptr, "Player");
				clear_selection(true);
				select(node);
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(u8"'Player' を選択");
			}

			ImGui::SameLine();
			if (ImGui::Button("M")) {
				KNode *node = KNodeTree::findNodeByName(nullptr, "MainCamera");
				clear_selection(true);
				select(node);
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(u8"'MainCamera' を選択");
			}
		}

		// 削除ボタン
		if (1) {
			update_sel();
			if (ImGui::Button("Del")) {
				remove_selections();
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Delete selected entity and its tree");
			}
		}

		// 前に移動ボタン
		if (1) {
			ImGui::SameLine();
			if (ImGui::Button("Up")) {
				move_prev_sibling();
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Move selected entity to previous sibling");
			}
		}
		// 後ろに移動ボタン
		if (1) {
			ImGui::SameLine();
			if (ImGui::Button("Down")) {
				move_next_sibling();
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Move selected entity to next sibling");
			}
		}
		if (1) {
			ImGui::SameLine();
			ImGui::Checkbox("Show checkboxes", &m_enable_mini_ctrl);
		}
	}
};



#pragma region CEntityInspector
// EID のためのインスペクター
class CEntityInspector {
	std::vector<EID> m_selected_entities;
	CTreeInspector *m_tree_inspector;
	KDebugSoloMute m_solomute;
public:
	CEntityInspector() {
		m_tree_inspector = nullptr;
	}
	void init(CTreeInspector *ti) {
		m_tree_inspector = ti;
	}
	// EID の描画がインスペクターによって許可されているか。
	// Mute や Solo フラグの組み合わせによって決まる
	bool isVisible(const KNode *node) const {
		return m_solomute.is_truly_visible(node);
	}
	// 選択中のエンティティ数
	int getSelectedCount() const {
		return (int)m_selected_entities.size();
	}
	// 選択中のエンティティ
	EID getSelected(int index) {
		if (index < 0 || (int)m_selected_entities.size() <= index) {
			return nullptr;
		}
		return m_selected_entities[index];
	}
	// エンティティを選択状態にする
	void select(const KNode *node) {
		if (node) {
			m_selected_entities.push_back(node->getId());
		}
	}
	// すべての選択を解除する
	void unselectAll(){
		m_selected_entities.clear();
	}
	// エンティティが選択されているか？
	bool isSelected(const KNode *node) const {
		EID e = node ? node->getId() : nullptr;
		if (e == nullptr) return false;
		auto it = std::find(m_selected_entities.begin(), m_selected_entities.end(), e);
		if (it == m_selected_entities.end()) return false;
		return true;
	}
	// エンティティまたはエンティティを含むツリーが選択されているか？
	bool isSelectedInHierarchy(const KNode *node) const {
		while (node) {
			if (isSelected(node)) {
				return true;
			}
			node = node->getParent();
		}
		return false;
	}
	// GUIを更新
	void updateGui() {
		EID sel_id = getSelected(0);
		if (sel_id == nullptr) {
			ImGui::Text("** No entity selected");
			return;
		}

		// Solo や Mute として指定されている EID が削除された場合は、フラグを解除する
		KNode *solo_node = m_solomute.get_solo();
		KNode *mute_node = m_solomute.get_mute();

		// 選択中の EID が削除された場合はメッセージを出しておく
		KNode *node = KNodeTree::findNodeById(sel_id);
		if (node == nullptr) {
			ImGui::TextColored(
				KImGui::COLOR_WARNING,
				"** Entiy %d(0x%X) does not exist.",
				EID_toint(sel_id),
				EID_toint(sel_id)
			);
			return;
		}

		if (1) {
			KSig sig(K_SIG_INSPECTOR_ENTITY);
			sig.setNode("node", node);
			KEngine::broadcastSignal(sig);
		}

		if (ImGui::CollapsingHeader("Node (Basic)", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Indent();
			m_solomute.update_gui(node);
			m_tree_inspector->onGuiEntity(node);
			ImGui::Unindent();
			ImGui::Dummy(ImVec2(0, 6));
		}
		if (ImGui::CollapsingHeader("Node (Extends)", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Indent();
			node->on_node_inspector();
			ImGui::Dummy(ImVec2(0, 8));
			ImGui::Unindent();
		}
		gui_attached_systems(sel_id);
	}
private:
	void gui_attached_systems(EID e) {
		KNode *node = KNodeTree::findNodeById(e);

		// Action
		ImGui::PushID(1000);
		if (ImGui::CollapsingHeader("Action", ImGuiTreeNodeFlags_DefaultOpen)) {
			KAction *act = node ? node->getAction() : nullptr;
			ImGui::Indent();
			KDebugGui::K_DebugGui_NodeAction(node);
			ImGui::Dummy(ImVec2(0, 6));
			if (act) act->onInspector();
			ImGui::Unindent();
		}
		ImGui::PopID();

		// このエンティティをアタッチしているシステムを列挙
		int n = KEngine::getManagerCount();
		for (int i=0; i<n; i++) {
			KManager *s = KEngine::getManager(i);
			if (s->isAttached(node)) {
				ImGui::PushID(s);
				if (ImGui::CollapsingHeader(typeid(*s).name(), ImGuiTreeNodeFlags_DefaultOpen)) {
					ImGui::Indent();
					s->on_manager_nodeinspector(node);
					ImGui::Unindent();
					ImGui::Dummy(ImVec2(0, 6));
				}
				ImGui::PopID();
			}
		}
	}
}; // CEntityInspector
#pragma endregion // CEntityInspector



#pragma region CSubSystemInspector
// サブシステムのインスペクター
class CSubSystemInspector {
	std::unordered_map<KInspectorCallback*, KInspectableDesc> m_items;
	std::vector<KInspectorCallback*> m_visible_list;

	// インスペクター上でのシステムの並び順
	class CPred {
	public:
		const CSubSystemInspector &m_inspector;
		CPred(const CSubSystemInspector &ins): m_inspector(ins) {
		}
		bool operator()(KInspectorCallback *a, KInspectorCallback *b) const {
			const KInspectableDesc &desca = *m_inspector.getDesc(a);
			const KInspectableDesc &descb = *m_inspector.getDesc(b);
			if (desca.sort_primary == descb.sort_primary) {
				return strcmp(typeid(*a).name(), typeid(*b).name()) < 0;
		
			} else {
				return desca.sort_primary < descb.sort_primary;
			}
		}
	};

public:
	CSubSystemInspector() {
	}
	void init() {
		m_items.clear();
		m_visible_list.clear();
	}
	void addInspectable(KInspectorCallback *ins, const char *label) {
		if (ins == nullptr) return;
		removeInspectable(ins);
		KInspectableDesc desc;

		// 表示名を決める
		if (label && label[0]) {
			strcpy_s(desc.label_u8, sizeof(desc.label_u8), label);

		} else {
			const char *classname = typeid(*ins).name();

			// クラス名は "class Hoge" のように空白を含んでいるかもしれない。空白よりも後を使う
			const char *classname_space = strrchr(classname, ' ');

			// 空白が見つかったら、その右隣の文字を先頭とする。空白が見つからなかった場合は元文字列を使う
			const char *name = classname_space ? (classname_space+1) : classname;

			strcpy_s(desc.label_u8, sizeof(desc.label_u8), name);
		}

		m_items[ins] = desc;
	}
	void removeInspectable(KInspectorCallback *ins) {
		auto it = m_items.find(ins);
		if (it != m_items.end()) {
			m_items.erase(it);
		}
	}
	bool hasInspectable(const KInspectorCallback *ins) const {
		return getDesc(ins) != nullptr;
	}
	const KInspectableDesc * getDesc(const KInspectorCallback *ins) const {
		auto it = m_items.find(const_cast<KInspectorCallback *>(ins));
		return (it!=m_items.end()) ? &it->second : nullptr;
	}
	KInspectableDesc * getDesc(const KInspectorCallback *ins) {
		auto it = m_items.find(const_cast<KInspectorCallback *>(ins));
		return (it!=m_items.end()) ? &it->second : nullptr;
	}
	void gui_subsystem_tip(KInspectorCallback *ins) {
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Type: %s", typeid(*ins).name());
		}
	}

	void updatePanel() {
		if (m_items.empty()) return;

		// インスペクター上での表示順でソート
		m_visible_list.clear();
		for (auto it=m_items.begin(); it!=m_items.end(); ++it) {
			m_visible_list.push_back(it->first);
		}
		std::sort(m_visible_list.begin(), m_visible_list.end(), CPred(*this));

		// インスペクターで最初に表示されるシステム
		K__ASSERT(m_visible_list.size() > 0);
		KInspectorCallback *top_item = m_visible_list[0];

		// ソートキー
		int lastkey = getDesc(top_item)->sort_primary;

		for (auto it=m_visible_list.begin(); it!=m_visible_list.end(); ++it) {
			KInspectorCallback *ins = *it;

			// エディタ上での表示設定
			KInspectableDesc *desc = getDesc(ins);

			ImGui::PushID(KImGui::ID(ins));
			if (lastkey != desc->sort_primary) {
				ImGui::Separator();
			}
			if (ImGui::CollapsingHeader(desc->label_u8, desc->default_open ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
				gui_subsystem_tip(ins);
				ImGui::Indent();
				ins->onInspectorGui();
				ImGui::Unindent();
				desc->is_open = true;
			} else {
				gui_subsystem_tip(ins);
				desc->is_open = false;
			}
			lastkey = desc->sort_primary;
			ImGui::PopID();
		}
	}
};
#pragma endregion // CSubSystemInspector





#pragma region CInspectorImpl
static const int DEFAULT_PANE_WIDTH = 260;
static const int MIN_PANE_SIZE = 32;

struct SDrawCallbackData {
	KTEXID tex;
	int w, h;
};
static SDrawCallbackData s_drawcallbackdata;

class CInspectorImpl: public KInspector {
	CSubSystemInspector m_subsystem_inspector;
	CEntityInspector m_entity_inspector;
	CTreeInspector *m_tree_inspector;
	KRecti m_world_viewport_in_window;
	EID m_highlighted_entity;
	int m_system_pane_w;
	int m_entity_pane_w;
	int m_font_index;
	bool m_highlight_selections_enabled;
	bool m_axis_selections_enabled;
	bool m_status_visible;
	bool m_visibled;
	bool m_demo;
	bool m_styleedit;
	static const int splitter_thickness = 4;
public:
	CInspectorImpl() {
		m_highlight_selections_enabled = true;
		m_axis_selections_enabled = true;
		m_visibled = false;
		m_demo = false;
		m_styleedit = false;
		m_highlighted_entity = nullptr;
		m_system_pane_w = DEFAULT_PANE_WIDTH;
		m_entity_pane_w = DEFAULT_PANE_WIDTH;
		m_tree_inspector = new CTreeInspector();
		m_subsystem_inspector.init();
		m_entity_inspector.init(m_tree_inspector);
		m_status_visible = false;
		m_font_index = 0;
	}
	~CInspectorImpl() {
		K__DROP(m_tree_inspector);
	}
	void onGameStart() {
		m_tree_inspector->start();
	}
	void onGameEnd() {
		m_tree_inspector->end();
	}
	void onEntityRemoving(KNode *node) {
		m_tree_inspector->unselect(node);
	}
	void setFont(int font_index) {
		m_font_index = font_index;
	}
	void render(KTEXID game_display) {
		KImGui::PushFont(m_font_index);
		guiMain(game_display);
		KImGui::PopFont();
	}
	void setHighlightedEntity(KNode *node) {
		m_highlighted_entity = node ? node->getId() : nullptr;
	}
	bool isEntityHighlighted(const KNode *node, bool in_hierarchy) const {
		if (node == nullptr) return false;
		if (in_hierarchy) {
			// エンティティ自身または親がハイライトされていればＯＫ
			while (node) {
				if (node->getId() == m_highlighted_entity) {
					return true;
				}
				node = node->getParent();
			}
			return false;
		
		} else {
			// エンティティ自身がハイライトされてるならＯＫ
			return (node->getId() == m_highlighted_entity);
		}
	}
	void selectEntity(const KNode *node) {
		m_entity_inspector.select(node);
	}
	void unselectAllEntities() {
		m_entity_inspector.unselectAll();
	}
	void setVisible(bool value) {
		m_visibled = value;
		if (m_visibled) {
			if (getSelectedEntityCount() == 0) {
				const KNode *player = KNodeTree::findNodeByName(nullptr, "Player");
				selectEntity(player);
			}
		}
	}
	bool isVisible() const {
		return m_visibled;
	}
	bool isEntityVisible(const KNode *node) const {
		return m_entity_inspector.isVisible(node);
	}
	int getSelectedEntityCount() const {
		return m_entity_inspector.getSelectedCount();
	}
	KNode * getSelectedEntity(int index) {
		EID e = m_entity_inspector.getSelected(index);
		return KNodeTree::findNodeById(e);
	}
	bool isEntitySelected(const KNode *node) const {
		return m_entity_inspector.isSelected(node);
	}
	bool isEntitySelectedInHierarchy(const KNode *node) const {
		return m_entity_inspector.isSelectedInHierarchy(node);
	}
	bool isStatusTipVisible() const {
		return m_status_visible;
	}
	void setStatusTipVisible(bool visible) {
		m_status_visible = visible;
	}
	bool isHighlightSelectionsEnabled() const {
		return m_highlight_selections_enabled;
	}
	bool isAxisSelectionsEnabled() const {
		return m_axis_selections_enabled;
	}
	bool entityShouldRednerAsSelected(const KNode *node) const {
		bool a = isEntitySelected(node);
		bool b = isEntityHighlighted(node, true);
		return a || b;
	}
	KRecti getWorldViewport() const {
		return m_world_viewport_in_window;
	}
	void addInspectable(KInspectorCallback *ins, const char *label) {
		m_subsystem_inspector.addInspectable(ins, label);
	}
	void removeInspectable(KInspectorCallback *ins) {
		m_subsystem_inspector.removeInspectable(ins);
	}
	bool hasInspectable(const KInspectorCallback *ins) const {
		return m_subsystem_inspector.hasInspectable(ins);
	}
	const KInspectableDesc * getDesc(const KInspectorCallback *ins) const {
		return m_subsystem_inspector.getDesc(ins);
	}
	KInspectableDesc * getDesc(const KInspectorCallback *ins) {
		return m_subsystem_inspector.getDesc(ins);
	}
	bool isDebugInfoVisible(KInspectorCallback *ins) const {
		if (ins == nullptr) return false;

		const KInspectableDesc *desc = getDesc(ins);
		if (desc == nullptr) return false;

		// 「常に表示」が ON ならばインスペクターの状態に関係なく
		// 常にデバッグ描画を許可する
		if (desc->always_show_debug) {
			return true;
		}

		// インスペクターが表示されていて該当ページが開かれているなら
		// デバッグ描画を許可する
		if (isVisible() && desc->is_open) {
			return true;
		}

		return false;
	}

private:
	void gui_screen_menu() {
		ImGui::Checkbox(u8"選択オブジェクトを強調表示する", &m_highlight_selections_enabled);
		ImGui::Checkbox(u8"選択オブジェクトの座標軸を表示する", &m_axis_selections_enabled);
	}
	void gui_system_browser() {
		// ゲームエンジンの基本情報
		if (ImGui::CollapsingHeader(u8"一般", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Indent();
			{
				KDebugGui::K_DebugGui_FrameInfo();
				KDebugGui::K_DebugGui_TimeCtrl();
				if (ImGui::TreeNodeEx(u8"マウス")) {
					KDebugGui::K_DebugGui_Mouse();
					ImGui::TreePop();
				}
				if (ImGui::TreeNodeEx(u8"ウィンドウ")) {
					KDebugGui::K_DebugGui_Window();
					ImGui::TreePop();
				}
			}
			// スクリーン設定
			if (ImGui::TreeNodeEx(u8"表示と描画")) {
				gui_screen_menu();
				ImGui::TreePop();
			}
			guiInspectorPreference();
			ImGui::Unindent();
		}
		if (ImGui::CollapsingHeader(u8"シーンツリー", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Indent();
			m_tree_inspector->onGuiMain();
			ImGui::Unindent();
		}
		// システム
		m_subsystem_inspector.updatePanel();
	}
	bool get_world_view_size(int *w, int *h, KTEXID game_texid) const {
		K__ASSERT(w);
		K__ASSERT(h);
		if (! m_visibled) return false;

		KTexture *game_tex = KVideo::findTexture(game_texid);
		if (game_tex == nullptr) return false;
		
		ImVec2 pane_size = ImGui::GetContentRegionAvail();
		// 元画像サイズ
		KTexture::Desc desc;
		game_tex->getDesc(&desc);
		int tex_w = desc.w;
		int tex_h = desc.h;
		switch (2) {
		case 0:
			// 常に原寸
			*w = tex_w;
			*h = tex_h;
			break;
		case 1:
			// 整数倍かつ最大サイズ
			{
				int si = (int)KMath::min(pane_size.x / tex_w, pane_size.y / tex_h);
				si = (int)KMath::max(1, si);
				*w = tex_w * si;
				*h = tex_h * si;
				break;
			}
		case 2:
		default:
			if (1) {
				// アスペクト比を保ちながら最大サイズ
				float sf = KMath::min((float)pane_size.x / tex_w, (float)pane_size.y / tex_h);
				*w = (int)(tex_w * sf);
				*h = (int)(tex_h * sf);
			} else {
				// FIT
				*w = (int)pane_size.x;
				*h = (int)pane_size.y;
			}
			break;
		}
		return true;
	}
	void gui_world_browser(KTEXID game_tex) {
		if (game_tex) {
			int w, h;
			get_world_view_size(&w, &h, game_tex);
			ImVec2 p = ImGui::GetCursorScreenPos();
			m_world_viewport_in_window.x = (int)p.x;
			m_world_viewport_in_window.y = (int)p.y;
			m_world_viewport_in_window.w = w;
			m_world_viewport_in_window.h = h;
			KImGui::Image(game_tex, _ImVec2i(w, h));
		}
	}
	void gui_status_tip() {
		ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
		ImGui::Begin("##window", nullptr, ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoSavedSettings);
		auto color = KEngine::isPaused() ? ImColor(1.0f, 0.0f, 0.0f, 1.0f) : ImColor(1.0f, 1.0f, 1.0f, 1.0f);
		int fpsreq = KEngine::getStatus(KEngine::ST_FPS_REQUIRED);
		int ac = KEngine::getStatus(KEngine::ST_FRAME_COUNT_APP);
		int gc = KEngine::getStatus(KEngine::ST_FRAME_COUNT_GAME);
		ImGui::TextColored(color, "Game %d:%02d.%02d", gc/fpsreq/60, gc/fpsreq%60, gc%fpsreq);// ImGui::SameLine();
		ImGui::Text("Time  %d:%02d.%02d", ac/fpsreq/60, ac/fpsreq%60, ac%fpsreq);// ImGui::SameLine();
		ImGui::Text("FPS  %.1f (GUI)", ImGui::GetIO().Framerate);
		//
		int fps_update = KEngine::getStatus(KEngine::ST_FPS_UPDATE);
		int fps_render = KEngine::getStatus(KEngine::ST_FPS_RENDER);
		ImGui::Text("FPS  %d/%d (Game render/update)", fps_render, fps_update);
		//
		ImGui::End();
	}
	void guiMain(KTEXID game_tex) {
		if (! m_visibled) {
			if (m_status_visible) {
				gui_status_tip();
			}
			return;
		}

		const int wnd_padding_x = (int)ImGui::GetStyle().WindowPadding.x;
		const int wnd_padding_y = (int)ImGui::GetStyle().WindowPadding.y;
		const int item_spacing_x = (int)ImGui::GetStyle().ItemSpacing.x;

		int menubar_h = 0;
		{
			// メニューバーのサイズを知る
			// ※副作用として空のメニューバーが作られてしまう事に注意
			// measure menu bar height
			// https://github.com/ocornut/imgui/issues/252
			ImGui::BeginMainMenuBar();
			menubar_h = (int)ImGui::GetWindowSize().y;
			if (ImGui::GetCursorPosX() <= wnd_padding_x) {
				// padding を超えていなければ、カーソルが初期位置のままになっている（＝まだアイテムが追加されていない）とみなす。
				// このまま ENdMainMenuBar してしまうと何もないメニューバーが表示されるだけなので、適当に文字を入れておく
				ImGui::Text("** Welcome to debug menu **");
			} else {
				// 既にメニューバーには何らかのアイテムが配置されている。
				// 例えばゲーム側で独自のメニューバーが設定されている。
				// 既に意味のあるメニューバーが表示されているという事なので、ここではなにもしない
			}
			ImGui::EndMainMenuBar();
		}

		// ウィンドウのクライアント領域にフィットする Imgui ウィンドウを作成する
		int window_w = KEngine::getStatus(KEngine::ST_WINDOW_CLIENT_SIZE_W);
		int window_h = KEngine::getStatus(KEngine::ST_WINDOW_CLIENT_SIZE_H);

		{
			const int lwidth = 
				wnd_padding_x + 
				m_system_pane_w + 
				item_spacing_x +
				splitter_thickness + 
				wnd_padding_x;
			ImGui::SetNextWindowPos(_ImVec2i(0, menubar_h), ImGuiCond_Always);
			ImGui::SetNextWindowSize(_ImVec2i(lwidth, window_h-menubar_h), ImGuiCond_Always);
			ImGui::Begin("##window1", nullptr, ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize);
			{
				// 左ペイン
				ImGui::BeginChild("##systems", _ImVec2i(m_system_pane_w, 0));
				gui_system_browser();
				ImGui::EndChild();
				ImGui::SameLine();

				// 左スプリッター
				KDebugGui::K_DebugGui_VSplitter("vsplitter1", &m_system_pane_w, 1, MIN_PANE_SIZE, splitter_thickness);
				ImGui::SameLine();
			}
			ImGui::End();

			const int rwidth =
				wnd_padding_x + 
				splitter_thickness + 
				item_spacing_x +
				m_entity_pane_w +
				wnd_padding_x;
			ImGui::SetNextWindowPos(_ImVec2i(window_w - rwidth, menubar_h), ImGuiCond_Always);
			ImGui::SetNextWindowSize(_ImVec2i(rwidth, window_h-menubar_h), ImGuiCond_Always);
			ImGui::Begin("##window2", nullptr, ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize);
			{
				// 右スプリッター
				KDebugGui::K_DebugGui_VSplitter("vsplitter2", &m_entity_pane_w, -1, MIN_PANE_SIZE, splitter_thickness);
				ImGui::SameLine();

				// 右ペイン
				ImGui::BeginChild("##ent", _ImVec2i(m_entity_pane_w, 0));
				m_entity_inspector.updateGui();
				ImGui::EndChild();
				ImGui::SameLine();
			}
			ImGui::End();
		}

		if (m_demo) {
			ImGui::ShowDemoWindow(&m_demo);
		}
		if (m_styleedit) {
			ImGui::ShowStyleEditor();
		}
	}
	void guiInspectorPreference() {
		if (ImGui::TreeNode(u8"ログ")) {
			{
				bool b = KLog::getState(KLog::STATE_LEVEL_INFO) != 0;
				if (ImGui::Checkbox(u8"情報", &b)) {
					KLog::setLevelVisible(KLog::LEVEL_INF, b);
				}
			}
			{
				bool b = KLog::getState(KLog::STATE_LEVEL_DEBUG) != 0;
				if (ImGui::Checkbox(u8"デバッグ", &b)) {
					KLog::setLevelVisible(KLog::LEVEL_DBG, b);
				}
			}
			{
				bool b = KLog::getState(KLog::STATE_LEVEL_VERBOSE) != 0;
				if (ImGui::Checkbox(u8"詳細", &b)) {
					KLog::setLevelVisible(KLog::LEVEL_VRB, b);
				}
			}
			{
				bool b = KLog::getState(KLog::STATE_HAS_OUTPUT_CONSOLE) != 0;
				if (ImGui::Checkbox(u8"コンソール", &b)) {
					KLog::setOutputConsole(b, true);
				}
			}
			ImGui::TreePop();
		}
		if (ImGui::TreeNode("ImGui/Color")) {
			ImGui::Checkbox("ImGui Demo", &m_demo);
			ImGui::Checkbox("ImGui Style Editor", &m_styleedit);
			ImGui::ShowFontSelector("Fonts");
			ImGui::Text("ImGui::IsAnyWindowFocused: %d", ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow));
			ImGui::Text("ImGui::IsAnyWindowHovered: %d", ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow));
			ImGui::Text("ImGui::IsAnyItemHovered: %d", ImGui::IsAnyItemHovered());
			ImGui::Text("ImGui::IsAnyItemActive: %d", ImGui::IsAnyItemActive());
			ImGui::Text("ImGui::IsAnyItemFocused: %d", ImGui::IsAnyItemFocused());
			ImGui::TreePop();
		}
	}
}; // CInspectorImpl
#pragma endregion // CInspectorImpl


static CInspectorImpl *g_Inspector = nullptr;

void KInspector::install() {
	g_Inspector = new CInspectorImpl();
}
void KInspector::uninstall() {
	if (g_Inspector) {
		delete g_Inspector;
		g_Inspector = nullptr;
	}
}
bool KInspector::isInstalled() {
	return g_Inspector != nullptr;
}
void KInspector::onGameStart() {
	g_Inspector->onGameStart();
}
void KInspector::onGameEnd() {
	g_Inspector->onGameEnd();
}
void KInspector::onEntityRemoving(KNode *node) {
	g_Inspector->onEntityRemoving(node);
}
void KInspector::addInspectable(KInspectorCallback *cb, const char *label) {
	g_Inspector->addInspectable(cb, label);
}
void KInspector::removeInspectable(KInspectorCallback *cb) {
	g_Inspector->removeInspectable(cb);
}
bool KInspector::hasInspectable(const KInspectorCallback *cb) {
	return g_Inspector->hasInspectable(cb);
}
const KInspectableDesc * KInspector::getDesc(const KInspectorCallback *cb) {
	return g_Inspector->getDesc(cb);
}
KInspectableDesc * KInspector::getDesc(KInspectorCallback *cb) {
	return g_Inspector->getDesc(cb);
}
bool KInspector::isDebugInfoVisible(KInspectorCallback *cb) {
	return g_Inspector->isDebugInfoVisible(cb);
}
void KInspector::render(KTEXID game_display) {
	g_Inspector->render(game_display);
}
void KInspector::setVisible(bool value) {
	g_Inspector->setVisible(value);
}
bool KInspector::isVisible() {
	return g_Inspector->isVisible();
}
void KInspector::setFont(int imgui_font_index) {
	g_Inspector->setFont(imgui_font_index);
}
int KInspector::getSelectedEntityCount() {
	return g_Inspector->getSelectedEntityCount();
}
KNode * KInspector::getSelectedEntity(int index) {
	return g_Inspector->getSelectedEntity(index);
}
void KInspector::setHighlightedEntity(KNode *node) {
	g_Inspector->setHighlightedEntity(node);
}
bool KInspector::isEntityHighlighted(const KNode *node, bool in_hierarchy) {
	return g_Inspector->isEntityHighlighted(node, in_hierarchy);
}
void KInspector::selectEntity(const KNode *node) {
	g_Inspector->selectEntity(node);
}
void KInspector::unselectAllEntities() {
	g_Inspector->unselectAllEntities();
}
bool KInspector::isEntityVisible(const KNode *node) {
	return g_Inspector->isEntityVisible(node);
}
bool KInspector::isEntitySelected(const KNode *node) {
	return g_Inspector->isEntitySelected(node);
}
bool KInspector::isEntitySelectedInHierarchy(const KNode *node) {
	return g_Inspector->isEntitySelectedInHierarchy(node);
}
bool KInspector::entityShouldRednerAsSelected(const KNode *node) {
	return g_Inspector->entityShouldRednerAsSelected(node);
}
bool KInspector::isHighlightSelectionsEnabled() {
	return g_Inspector->isHighlightSelectionsEnabled();
}
bool KInspector::isAxisSelectionsEnabled() {
	return g_Inspector->isAxisSelectionsEnabled();
}
KRecti KInspector::getWorldViewport() {
	return g_Inspector->getWorldViewport();
}
bool KInspector::isStatusTipVisible() {
	return g_Inspector->isStatusTipVisible();
}
void KInspector::setStatusTipVisible(bool visible) {
	g_Inspector->setStatusTipVisible(visible);
}








#pragma region K_DebugGui
void KDebugGui::K_DebugGui_Image(KTEXID texid, int _w, int _h, KVec2 uv0, KVec2 uv1, KColor color, KColor border, bool keep_aspect, bool linear_filter) {
	KTexture *tex = KVideo::findTexture(texid);
	if (tex == nullptr) return;

	int tex_w = tex->getWidth();
	int tex_h = tex->getHeight();
	
	int w = (_w > 0) ? _w : tex_w;
	int h = (_h > 0) ? _h : tex_h;
	if (keep_aspect) {
		if (tex_w >= tex_h) {
			h = (int)tex_h * w / tex_w;
		} else {
			w = (int)tex_w * h / tex_h;
		}
	}
	if (!linear_filter) {
		KImGui::SetFilterPoint(); // フィルターを POINT に変更
	}
	if (1) {
		// ここで渡したテクスチャが具体的にどのように描画されるかについては
		// imgui_impl_dx9.cpp: ImGui_ImplDX9_RenderDrawData の
		// const LPDIRECT3DTEXTURE9 texture = (LPDIRECT3DTEXTURE9)pcmd->TextureId;
		// という行付近を参照せよ
		ImGui::Image(
			(ImTextureID)tex->getDirect3DTexture9(),
			ImVec2((float)w, (float)h),
			ImVec2(uv0.x, uv0.y),
			ImVec2(uv1.x, uv1.y),
			ImVec4(color.r, color.g, color.b, color.a),
			ImVec4(border.r, border.g, border.b, border.a)
		);
	}
	if (!linear_filter) {
		KImGui::SetFilterLinear(); // 元のフィルター (LINEAR) に戻す
	}
}
void KDebugGui::K_DebugGui_ImageExportButton(const char *label, KTEXID texid, const char *filename, bool alphamask) {
	// テクスチャ画像を png でエクスポートする
	if (ImGui::Button(label)) {
		KTexture *tex = KVideo::findTexture(texid);
		if (tex) {
			KImage img = tex->exportTextureImage(alphamask ? 3 : -1);
			if (!img.empty()) {
				img.saveToFileName(filename);
				KLog::printInfo("Export texture image %s", filename);
			}
		}
	}

	// エクスポートしたファイルを開く
	if (K::pathExists(filename)) {
		ImGui::SameLine();
		char s[256];
		sprintf_s(s, sizeof(s), "Open: %s", filename);
		if (ImGui::Button(s)) {
			K::fileShellOpen(filename);
		}
	}
}
bool KDebugGui::K_DebugGui_InputMatrix(const char *label, KMatrix4 *m) {
	bool b = false;
	ImGui::Text("%s", label);
	ImGui::Indent();
	b |= ImGui::DragFloat4("##0", m->m +  0, 0.1f);
	b |= ImGui::DragFloat4("##1", m->m +  4, 0.1f);
	b |= ImGui::DragFloat4("##2", m->m +  8, 0.1f);
	b |= ImGui::DragFloat4("##3", m->m + 12, 0.1f);
	if (ImGui::Button("E")) {
		*m = KMatrix4();
		b = true;
	}
	ImGui::Unindent();
	return b;
}
bool KDebugGui::K_DebugGui_InputBlend(const char *label, KBlend *blend) {
	static const char * s_table[KBlend_ENUM_MAX+1] = {
		"One",    // KBlend_ONE
		"Add",    // KBlend_ADD
		"Alpha",  // KBlend_ALPHA
		"Sub",    // KBlend_SUB
		"Mul",    // KBlend_MUL
		"Screen", // KBlend_SCREEN
		"Max",    // KBlend_MAX
		nullptr,     // (sentinel)
	};
	K__ASSERT(blend);
	K__ASSERT(s_table[KBlend_ENUM_MAX] == nullptr);
	int val = *blend;
	if (ImGui::Combo(label, &val, s_table, KBlend_ENUM_MAX)) {
		*blend = (KBlend)val;
		return true;
	}
	return false;
}
bool KDebugGui::K_DebugGui_InputFilter(const char *label, KFilter *filter) {
	static const char * s_table[KFilter_ENUM_MAX+1] = {
		"None",   // KFilter_NONE,
		"Linear", // KFilter_LINEAR
		nullptr,     // (sentinel)
	};
	K__ASSERT(filter);
	K__ASSERT(s_table[KFilter_ENUM_MAX] == nullptr);
	int val = *filter;
	if (ImGui::Combo(label, &val, s_table, KFilter_ENUM_MAX)) {
		*filter = (KFilter)val;
		return true;
	}
	return false;
}

// 左右を区切るための縦線スプリッター
// id: 識別子
// x: このスプリッターを左右にドラッグすることによって変化させる値
// sign: 右にドラッグしたときに x を増加させるなら 1 を、逆ならば -1 を指定する
// min_x: x の最小値
// thickness: スプリッターの太さ
bool KDebugGui::K_DebugGui_VSplitter(const char *id, int *x, int sign, int min_x, int thickness) {
	// 実際はただの縦長の棒状ボタンで、左右にドラッグすることにより x の値を変化させる機能を持つ。
	//
	// 使い方
	// // 左フレーム
	// ImGui::BeginChild("Left", ImVec2(w, 0));
	//  ...
	// ImGui::EndChild();
	// ImGui::SameLine();
	// // スプリッター
	// ImGui_VSplitter("vsp", &w, 1); // <-- スプリッターをドラッグすると w が変化する
	// ImGui::SameLine();
	// // 右フレーム
	// ImGui::BeginChild("Right", ImVec2(ImGui::GetContentRegionAvail().x, 0));
	//  ...
	// ImGui::EndChild();
	//
	// この例ではスプリッターで左側のサイズを決めていたが、右側のサイズを決めるときは sign 引数を -1 にしておく
	//
	bool changed = false;
	// 棒状のボタンをスプリッターとして使う
	float area_h = ImGui::GetContentRegionAvail().y;
	ImGui::PushID(id);
	ImGui::Button("##splitter", ImVec2((float)thickness, area_h));
	// 左右にドラッグした分だけ x を増減する
	if (ImGui::IsItemActive()) {
		int dx = (int)ImGui::GetIO().MouseDelta.x;
		if (dx != 0) {
			*x += dx * sign;
			if (*x < min_x) {
				*x = min_x;
			}
			changed = true;
		}
	}
	// マウスカーソル形状を設定する
	if (ImGui::IsItemHovered()) {
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
	}
	ImGui::PopID();
	return changed;
}

// 左右を区切るための横線スプリッター
// id: 識別子
// y: このスプリッターを左右にドラッグすることによって変化させる値
// sign: 下にドラッグしたときに y を増加させるなら 1 を、逆ならば -1 を指定する
// min_y: y の最小値
// thickness: スプリッターの太さ
bool KDebugGui::K_DebugGui_HSplitter(const char *id, int *y, int sign, int min_y, int thickness) {
	/// 上下を区切るための水平スプリッター
	bool changed = false;
	// 棒状のボタンをスプリッターとして使う
	float area_w = ImGui::GetContentRegionAvail().x;
	ImGui::PushID(id);
	ImGui::Button("##splitter", ImVec2(area_w, (float)thickness));
	// 上下にドラッグした分だけ y を増減する
	if (ImGui::IsItemActive()) {
		int dy = (int)ImGui::GetIO().MouseDelta.y;
		if (dy != 0) {
			*y += dy * sign;
			if (*y < min_y) {
				*y = min_y;
			}
			changed = true;
		}
	}
	// マウスカーソル形状を設定する
	if (ImGui::IsItemHovered()) {
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
	}
	ImGui::PopID();
	return changed;
}

void KDebugGui::K_DebugGui_NodeParent(KNode *node, KDebugTree *tree) {
	if (node == nullptr) return;

	KNode *parent = node->getParent();
	if (parent == nullptr) {
		ImGui::Text("Parent: (none)");
		return;
	}

	ImGui::Text("Parent: #0x%X '%s'", parent->getId(), parent->getName().c_str());
	
	if (ImGui::BeginPopupContextItem(__FUNCTION__)) {
		char s[256] = {0};
		ImGui::Text("Parent:");
		sprintf_s(s, sizeof(s), "#0x%x: %s", EID_toint(parent->getId()), parent->getName().c_str());
		if (ImGui::MenuItem(s)) {
			if (tree) {
				tree->clear_selection(true);
				tree->select(parent);
			}
		}
		ImGui::EndPopup();
	}
}
void KDebugGui::K_DebugGui_NodeChildren(KNode *node, KDebugTree *tree) {
	if (node == nullptr) return;

	int num = node->getChildCount();
	ImGui::Text("Num children: %d", num);

	if (ImGui::BeginPopupContextItem(__FUNCTION__)) {
		ImGui::Text("Children:");
		{
			for (int i=0; i<num; i++) {
				char s[256] = {0};
				KNode *child = node->getChild(i);
				sprintf_s(s, sizeof(s), "#0x%x: %s", EID_toint(child->getId()), child->getName().c_str());
				if (ImGui::MenuItem(s)) {
					if (tree) {
						tree->clear_selection(true);
						tree->select(child);
					}
				}
			}
		}
		ImGui::EndPopup();
	}
}
void KDebugGui::K_DebugGui_NodeCamera(KNode *node, KNode *camera, KDebugTree *tree) {
	if (node == nullptr) return;
	if (camera == nullptr) {
		ImGui::Text("Camera: (none)");
		return;
	}

	std::string path = camera->getNameInTree();
	ImGui::Text("Camera: %s (#%p)", path.c_str(), camera->getId());

	if (ImGui::BeginPopupContextItem(__FUNCTION__)) {
		char s[256] = {0};
		ImGui::Text("Camera:");
		sprintf_s(s, sizeof(s), "#0x%x: %s", EID_toint(camera->getId()), camera->getName().c_str());
		if (ImGui::MenuItem(s)) {
			if (tree) {
				tree->clear_selection(true);
				tree->select(camera);
			}
		}
		ImGui::EndPopup();
	}
}
void KDebugGui::K_DebugGui_NodeNameId(KNode *node) {
	if (node == nullptr) return;
	const std::string &name = node->getName();
	ImGui::Text("ID: 0x%X", node->getId());
	ImGui::Text("Name: %s", name.c_str());
	ImGui::Text("Type: %s", typeid(*node).name());
	if (ImGui::IsItemHovered()) {
		std::string path = node->getNameInTree();
		ImGui::SetTooltip("%s", path.c_str());
	}
}
void KDebugGui::K_DebugGui_NodeTag(KNode *node) {
	if (node == nullptr) return;

	{
		const KNameList &tags = node->getTagList();
		if (tags.size() > 0) {
			ImGui::Text("Tags: "); ImGui::SameLine();
			for (size_t i=0; i<tags.size(); i++) {
				const char *name = tags[i].c_str();
				ImGui::Text("[%s]", name);
				ImGui::SameLine();
			}
			ImGui::NewLine();
		} else {
			ImGui::Text("Tags: (NO TAGS)");
		}
	}

	{
		const KNameList &tags = node->getTagListInherited();
		if (tags.size() > 0) {
			ImGui::Text("Tags Inherited: "); ImGui::SameLine();
			for (size_t i=0; i<tags.size(); i++) {
				const char *name = tags[i].c_str();
				ImGui::Text("[%s]", name);
				ImGui::SameLine();
			}
			ImGui::NewLine();
		} else {
			ImGui::Text("Tags Inherited: (NO TAGS)");
		}
	}


	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
			u8"このエンティティにつけられたタグ番号と、その名前\n"
		);
	}
}
void KDebugGui::K_DebugGui_NodeLayer(KNode *node) {
	if (node == nullptr) return;
	{
		int self_layer = node->getLayer();
		ImGui::Text("Layer:");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(100);
		if (ImGui::InputInt("Layer", &self_layer)) {
			node->setLayer(self_layer);
		}
	}
	{
		int tree_layer = node->getLayerInTree();
		const char *name = KNodeTree::getGroupName(KNode::Category_LAYER, tree_layer);
		ImGui::Text("Layer in tree: %d (%s)", tree_layer, name ? name : "(none)");
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
			u8"このエンティティが所属するレイヤー番号。描画順などに影響する。\n"
			u8"[in tree] と表示されている場合は、親のレイヤーを継承していることを示す。\n"
		);
	}
}
void KDebugGui::K_DebugGui_NodeState(KNode *node) {
	if (node == nullptr) return;
	if (1) {
		bool b = node->getEnable();
		if (ImGui::Checkbox("Enabled", &b)) {
			node->setEnable(b);
		}
	}
	if (1) {
		ImGui::SameLine();
		bool b = node->getPause();
		if (ImGui::Checkbox("Paused", &b)) {
			node->setPause(b);
		}
	}
	if (1) {
		ImGui::SameLine();
		bool b = node->getVisible();
		if (ImGui::Checkbox("Visible", &b)) {
			node->setVisible(b);
		}
	}
}
static bool K_DebugGui__Quaternion(const char *label, KQuat *q) {
#define HSPACE  ImGui::SameLine(0, 8);
	bool mod = false;
	if (ImGui::DragFloat4(label, (float *)q, 0.01f)) { mod = true; }
	float deg = 5;
	ImGui::PushButtonRepeat(true);
	if (ImGui::Button("X+")) { *q *= KQuat::fromXDeg( deg); mod=true; } ImGui::SameLine();
	if (ImGui::Button("X-")) { *q *= KQuat::fromXDeg(-deg); mod=true; } ImGui::SameLine();
	HSPACE; ImGui::SameLine();
	if (ImGui::Button("Y+")) { *q *= KQuat::fromYDeg( deg); mod=true; } ImGui::SameLine();
	if (ImGui::Button("Y-")) { *q *= KQuat::fromYDeg(-deg); mod=true; } ImGui::SameLine();
	HSPACE; ImGui::SameLine();
	if (ImGui::Button("Z+")) { *q *= KQuat::fromZDeg( deg); mod=true; } ImGui::SameLine();
	if (ImGui::Button("Z-")) { *q *= KQuat::fromZDeg(-deg); mod=true; } ImGui::SameLine();
	HSPACE; ImGui::SameLine();
	if (ImGui::Button("0") ) { *q = KQuat(); mod=true; }
	ImGui::PopButtonRepeat();
	if (mod) {
		*q = q->normalized();
	}
	return mod;
#undef HSPACE
}
void KDebugGui::K_DebugGui_NodePosition(KNode *node) {
	if (node == nullptr) return;
	KVec3 pos = node->getPosition();

	// ドラッグでの数値変更速度は、このノードを映しているカメラの設定に依存したものにする
	float speed = 1.0f;
	if (1) {
		KNode *cam = KCamera::findCameraFor(node);
		if (cam) {
			// ウィンドウ上で１ドットずれている点は、ワールド座標でどのくらいずれる？
			KVec3 v0 = KScreen::windowClientToScreenPoint(KVec3(0, 0, 0));
			KVec3 v1 = KScreen::windowClientToScreenPoint(KVec3(1, 0, 0));
			KVec3 w0 = KCamera::of(cam)->getView2WorldPoint(v0);
			KVec3 w1 = KCamera::of(cam)->getView2WorldPoint(v1);
			KVec3 delta = w1 - w0;
			/*
			float projW = KCamera::of(cam)->getProjectionW();
			float projH = KCamera::of(cam)->getProjectionH();
			float projVal = KMath::min(projW, projH);
			speed = projVal * 0.002f;
			*/
			speed = delta.getLength() * 4.0f;
		}
	}
	if (ImGui::DragFloat3("Pos", (float*)&pos, speed)) {
		node->setPosition(pos);
	}
}
void KDebugGui::K_DebugGui_NodeScale(KNode *node) {
	if (node == nullptr) return;
	KVec3 scale = node->getScale();
	if (ImGui::DragFloat3("Scale", (float*)&scale, 0.01f)) {
		node->setScale(scale);
	}
}
void KDebugGui::K_DebugGui_NodeRotate(KNode *node) {
	if (node == nullptr) return;
	KVec3 degrees;
	if (node->getRotationEuler(&degrees)) {
		// 回転がオイラー角によって指定されている
		if (ImGui::DragFloat3("Rotation", (float*)&degrees, 1)) {
			node->setRotationEuler(degrees);
		}

		// クォータニオンモードへの切り替えスイッチ
		ImGui::SameLine();
		if (ImGui::Button("Qt")) {
			KQuat q = KQuat::fromXDeg(degrees.x) * KQuat::fromYDeg(degrees.y) * KQuat::fromZDeg(degrees.z); 
			node->setRotation(q);
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Switch to quaternion mode");
		}

	} else {
		// 回転がクォータニオンによって指定されている
		KQuat q = node->getRotation();
		if (K_DebugGui__Quaternion("Rotation", &q)) {
			node->setRotation(q);
		}

		// オイラーモードへの切り替えスイッチ
		ImGui::SameLine();
		if (ImGui::Button("Eu")) {
			node->setRotationEuler(KVec3());
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Switch to quaternion mode");
		}
	}
}
void KDebugGui::K_DebugGui_NodeWorldPosition(KNode *node) {
	if (node == nullptr) return;
	KMatrix4 wmat;
	node->getLocal2WorldMatrix(&wmat);
	KVec3 wpos = wmat.transformZero();
//	ImGui::Text("Position in world: %.1f, %.1f, %.1f", wpos.x, wpos.y, wpos.z);
	ImGui::InputFloat3("Pos in World", (float*)&wpos, "%.1f", ImGuiInputTextFlags_ReadOnly);
}
void KDebugGui::K_DebugGui_NodeCustomTransform(KNode *node) {
	if (node == nullptr) return;
	KMatrix4 m;
	node->getCustomTransform(&m, nullptr);
	if (KDebugGui::K_DebugGui_InputMatrix("Custom transform", &m)) {
		node->setCustomTransform(m, nullptr);
	}
}
void KDebugGui::K_DebugGui_NodeTransform(KNode *node) {
	if (node == nullptr) return;
	KDebugGui::K_DebugGui_NodePosition(node);
	KDebugGui::K_DebugGui_NodeScale(node);
	KDebugGui::K_DebugGui_NodeRotate(node);
	if (ImGui::TreeNode("Custom transform")) {
		KDebugGui::K_DebugGui_NodeCustomTransform(node);
		ImGui::TreePop();
	}
	KDebugGui::K_DebugGui_NodeInheritTransform(node);
	KDebugGui::K_DebugGui_NodeWorldPosition(node);
}
void KDebugGui::K_DebugGui_NodeColor(KNode *node) {
	if (node == nullptr) return;
	KColor local = node->getColor();
	KColor world = node->getColorInTree();
	bool inherit = node->getColorInherit();
	if (ImGui::ColorEdit4("Color", (float*)&local)) {
		node->setColor(local);
	}
	if (ImGui::Checkbox("Inherit", &inherit)) {
		node->setColorInherit(inherit);
	}
	ImGui::SameLine();
	ImGui::ColorEdit4("Color in tree", (float*)&world, ImGuiColorEditFlags_NoInputs);
}
void KDebugGui::K_DebugGui_NodeSpecular(KNode *node) {
	if (node == nullptr) return;
	KColor local = node->getSpecular();
	KColor world = node->getSpecularInTree();
	bool inherit = node->getSpecularInherit();
	if (ImGui::ColorEdit4("Specular", (float*)&local)) {
		node->setSpecular(local);
	}
	if (ImGui::Checkbox("Inherit", &inherit)) {
		node->setSpecularInherit(inherit);
	}
	ImGui::SameLine();
	ImGui::ColorEdit4("Specular in tree", (float*)&world, ImGuiColorEditFlags_NoInputs);
}
void KDebugGui::K_DebugGui_NodePriority(KNode *node) {
	if (node == nullptr) return;

	ImGui::Text("Priority:"); ImGui::SameLine();
	int value = node->getPriority();
	ImGui::SetNextItemWidth(100);
	if (ImGui::InputInt("##priority", &value)) {
		node->setPriority(value);
	}
	ImGui::Text("Priority in tree: %d", node->getPriorityInTree());
}
void KDebugGui::K_DebugGui_NodeAtomic(KNode *node) {
	if (node == nullptr) return;

	ImGui::Text("Atomic:"); ImGui::SameLine();
	bool value = node->getRenderAtomic();
	if (ImGui::Checkbox("##atomic", &value)) {
		node->setRenderAtomic(value);
	}
}
void KDebugGui::K_DebugGui_NodeInheritTransform(KNode *node) {
	if (node == nullptr) return;

	ImGui::Text("Inherit transform:"); ImGui::SameLine();
	bool value = node->getTransformInherit();
	if (ImGui::Checkbox("##transform", &value)) {
		node->setTransformInherit(value);
	}
}
void KDebugGui::K_DebugGui_NodeRenderAfterChildren(KNode *node) {
	if (node == nullptr) return;

	ImGui::Text("Render after children:"); ImGui::SameLine();
	bool value = node->getRenderAfterChildren();
	if (ImGui::Checkbox("##afterchildren", &value)) {
		node->setRenderAfterChildren(value);
	}
}
void KDebugGui::K_DebugGui_NodeRenderNoCulling(KNode *node) {
	if (node == nullptr) return;

	ImGui::Text("View culling:"); ImGui::SameLine();
	bool value = node->getViewCulling();
	if (ImGui::Checkbox("##viewculling", &value)) {
		node->setViewCulling(value);
	}
}
void KDebugGui::K_DebugGui_NodeAction(KNode *node) {
	if (node == nullptr) return;
	KAction *act = node->getAction();
	ImGui::Text("Action: %s", act ? typeid(*act).name() : "(null)");
}

void KDebugGui::K_DebugGui_NodeWarning(KNode *node) {
	if (node == nullptr) return;
	KImGui::PushTextColor(KImGui::COLOR_WARNING);
	{
		std::string warn = node->getWarningString();
		if (warn.size() > 0) {
			ImGui::Text(warn.c_str());
		}
	}
	KImGui::PopTextColor();
}
void KDebugGui::K_DebugGui_Mouse() {
	// スクリーン座標系でのマウス
	int gpos_x = KMouse::getGlobalX();
	int gpos_y = KMouse::getGlobalY();

	// ウィンドウ座標系でのマウス
	int lpos_x = 0;
	int lpos_y = 0;
	KWindow::getMouseCursorPos(&lpos_x, &lpos_y);
	{
		KRecti viewport = KScreen::getGameViewportRect();
		ImGui::Text("In screen: (%d, %d)", gpos_x, gpos_y);
		ImGui::Text("In window: (%d, %d)", lpos_x, lpos_y);

		// ビューポート左上を (0, 0) 右下を (w, h) とする座標系
		int mouse_vp_x = lpos_x - viewport.x;
		int mouse_vp_y = lpos_y - viewport.y;
		ImGui::Text("In viewport: (%d, %d)", mouse_vp_x, mouse_vp_y);

		// ビューポート中心を (0, 0) 右下を(w/2, -h/2) とする座標系
		int ox = viewport.x + viewport.w/2;
		int oy = viewport.y + viewport.h/2;
		int nx = (int)(lpos_x - ox);
		int ny = (int)(lpos_y - oy);
		ImGui::Text("In view: (%d, %d)", nx, -ny);
	}
}
void KDebugGui::K_DebugGui_Window() {
	if (1) {
		int x = KEngine::getStatus(KEngine::ST_WINDOW_POSITION_X);
		int y = KEngine::getStatus(KEngine::ST_WINDOW_POSITION_Y);
		ImGui::Text("Pos: %d, %d", x, y);
	}
	if (1) {
		int cw = KEngine::getStatus(KEngine::ST_WINDOW_CLIENT_SIZE_W);
		int ch = KEngine::getStatus(KEngine::ST_WINDOW_CLIENT_SIZE_H);
		ImGui::Text("Client size %d x %d", cw, ch);
	}
	if (1) {
		// 基本ゲーム画面サイズ
		int game_w = 0;
		int game_h = 0;
		KScreen::getGameSize(&game_w, &game_h);

		if (ImGui::Button("Wnd x1")) {
			KEngine::resizeWindow(game_w, game_h);
		}
		ImGui::SameLine();
		if (ImGui::Button("x2")) {
			KEngine::resizeWindow(game_w*2, game_h*2);
		}
		ImGui::SameLine();
		if (ImGui::Button("x3")) {
			KEngine::resizeWindow(game_w*3, game_h*3);
		}
		ImGui::SameLine();
		if (ImGui::Button("Full")) {
			KEngine::setWindowMode(KEngine::WND_FULLSCREEN);
		}
		ImGui::SameLine();
		if (ImGui::Button("WinFull")) {
			KEngine::setWindowMode(KEngine::WND_WINDOWED_FULLSCREEN);
		}
	}
	if (1) {
		// 背景色
		float rgba[4] = {0, 0, 0, 0};
		KEngine::command("get_bgcolor", rgba);
		ImGui::ColorEdit4("BG Color", rgba);
		KEngine::command("set_bgcolor", rgba);
	}
}
void KDebugGui::K_DebugGui_TimeCtrl() {
	if (KEngine::isPaused()) {
		// 一時停止中
		if (ImGui::Button("Play")) {
			KEngine::play();
		}
		ImGui::SameLine();
		if (KImGui::ButtonRepeat("Step")) {
			KEngine::playStep();
		}

	} else {
		// 実行中
		if (ImGui::Button("Pause")) {
			KEngine::pause();
		}
		ImGui::SameLine();
		if (KImGui::ButtonRepeat("Step")) {
			KEngine::pause();
		}
	}
	{
		KEngine::Flags flags = KEngine::getFlags();
		bool b = (flags & KEngine::FLAG_NOWAIT) != 0;
		if (ImGui::Checkbox("No Wait", &b)) {
			if (b) {
				flags |= KEngine::FLAG_NOWAIT;
			} else {
				flags &= ~KEngine::FLAG_NOWAIT;
			}
			KEngine::setFlags(flags);
		}
	}
	if (ImGui::TreeNode(u8"フレームスキップ")) {
		int skip = KEngine::getStatus(KEngine::ST_MAX_SKIP_FRAMES);

		const int N = 5;
		const char *names[N] = {
			"No Skip",
			"Skip1", 
			"Skip2", 
			"Skip3", 
			"Skip4", 
		};
		if (ImGui::Combo("##FrameSkips", &skip, names, N)) {
			KEngine::setFps(-1, skip);
		}
		ImGui::TreePop();
	}
}
void KDebugGui::K_DebugGui_FrameInfo() {
	int fpsreq = KEngine::getStatus(KEngine::ST_FPS_REQUIRED);
	int ac = KEngine::getStatus(KEngine::ST_FRAME_COUNT_APP);
	int gc = KEngine::getStatus(KEngine::ST_FRAME_COUNT_GAME);
	ImGui::Text("[App ] %d:%02d:%02d(%d)", ac/fpsreq/60, ac/fpsreq%60, ac%fpsreq, ac);
	if (KEngine::isPaused()) {
		KImGui::PushTextColor(KImGui::COLOR_WARNING);
		ImGui::Text("[Game] %d:%02d:%02d(%d) [PAUSED]", gc/fpsreq/60, gc/fpsreq%60, gc%fpsreq, gc);
		KImGui::PopTextColor();
	} else {
		ImGui::Text("[Game] %d:%02d:%02d(%d)", gc/fpsreq/60, gc/fpsreq%60, gc%fpsreq, gc);
	}
	//
	int fps_update = KEngine::getStatus(KEngine::ST_FPS_UPDATE);
	int fps_render = KEngine::getStatus(KEngine::ST_FPS_RENDER);
	ImGui::Text("GAME FPS: %d (%d)", fps_render, fps_update);

	float guif = ImGui::GetIO().Framerate;
	ImGui::Text("GUI FPS: %.1f (%.3f ms/f)", guif, 1000.0f / guif);
}
void KDebugGui::K_DebugGui_Aabb(const char *label, const KVec3 &aabb_min, const KVec3 &aabb_max) {
	KVec3 aabb_size = aabb_max - aabb_min;
	ImGui::Text("%s", label);
	ImGui::Indent();
	ImGui::Text("Min: %.2f,  %.2f,  %.2f",  aabb_min.x, aabb_min.y, aabb_min.z);
	ImGui::Text("Max: %.2f,  %.2f,  %.2f",  aabb_max.x, aabb_max.y, aabb_max.z);
	ImGui::Text("Size: %.2f,  %.2f,  %.2f", aabb_size.x, aabb_size.y, aabb_size.z);
	ImGui::Unindent();
}

#pragma endregion // K_DebugGui






#pragma region KDebugSoloMute
// デバッグ用の設定 Solo, Mute を管理する。
// あくまでも描画するべきかどうか判断するだけで、
// 実際に表示非表示の切り替えを行ったりはしない。
KDebugSoloMute::KDebugSoloMute() {
	m_solo = nullptr;
	m_mute = nullptr;
	m_tree = false;
}
KDebugSoloMute::~KDebugSoloMute() {
	if (m_solo) m_solo->drop();
	if (m_mute) m_mute->drop();
}
KNode * KDebugSoloMute::get_solo() const {
	if (m_solo && m_solo->isInvalid()) {
		m_solo->drop();
		m_solo = nullptr;
	}
	return m_solo;
}
KNode * KDebugSoloMute::get_mute() const {
	if (m_mute && m_mute->isInvalid()) {
		m_mute->drop();
		m_mute = nullptr;
	}
	return m_mute;
}
bool KDebugSoloMute::get_tree() const {
	return m_tree;
}
void KDebugSoloMute::set_solo(KNode *node) {
	if (m_solo) m_solo->drop();
	m_solo = node;
	if (m_solo) m_solo->grab();
}
void KDebugSoloMute::set_mute(KNode *node) {
	if (m_mute) m_mute->drop();
	m_mute = node;
	if (m_mute) m_mute->grab();
}
void KDebugSoloMute::set_tree(bool value) {
	m_tree = value;
}
bool KDebugSoloMute::is_solo(const KNode *node) const {
	if (node && m_solo==node) {
		return true;
	}
	return false;
}
bool KDebugSoloMute::is_solo_in_tree(const KNode *node) const {
	if (m_solo) {
		while (node) {
			if (m_solo == node) {
				return true;
			}
			node = node->getParent();
		}
	}
	return false;
}
bool KDebugSoloMute::is_mute(const KNode *node) const {
	if (node && m_mute==node) {
		return true;
	}
	return false;
}
bool KDebugSoloMute::is_mute_in_tree(const KNode *node) const {
	if (m_mute) {
		while (node) {
			if (m_mute == node) {
				return true;
			}
			node = node->getParent();
		}
	}
	return false;
}
bool KDebugSoloMute::is_truly_visible(const KNode *node) const {
	if (node == nullptr) return false;
	if (m_solo) {
		// solo に指定されているなら表示
		if (m_solo == node) {
			return true;
		}
		// ツリー単位で指定されているなら solo の子ツリーも表示
		if (m_tree && is_solo_in_tree(node)) {
			return true;
		}
		return false;
	}
	if (m_mute) {
		// mute に指定されているなら隠す
		if (m_mute == node) {
			return false;
		}
		// ツリー単位で指定されているなら mute の子ツリーも隠す
		if (m_tree && is_mute_in_tree(node)) {
			return false;
		}
		return true;
	}
	return true;
}
void KDebugSoloMute::update_gui(KNode *node) {
	gui_solo(node); ImGui::SameLine();
	gui_mute(node); ImGui::SameLine();
	gui_tree(node);
}
void KDebugSoloMute::gui_solo(KNode *node) {
	if (node == nullptr) return;
	bool solo = is_solo(node);
	if (ImGui::Checkbox("Solo", &solo)) {
		if (solo) {
			set_mute(nullptr);
			set_solo(node);
		} else {
			set_solo(nullptr);
		}
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Solo: Hide all nodes except this");
	}
}
void KDebugSoloMute::gui_mute(KNode *node) {
	if (node == nullptr) return;
	bool mute = is_mute(node);
	if (ImGui::Checkbox("Mute", &mute)) {
		if (mute) {
			set_mute(node);
			set_mute(nullptr);
		} else {
			set_mute(nullptr);
		}
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Mute: Hide this node");
	}
}
void KDebugSoloMute::gui_tree(KNode *node) {
	ImGui::Checkbox("In Tree", &m_tree);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
			u8"SOLO または MUTE の効果を子ツリーにも適用する\n"
			u8"TREE + SOLO: このノードの子ツリーだけを表示し、他のノードをすべて隠す\n"
			u8"TREE + MUTE: このノードを子ツリーだけを隠し、他のノードは表示したままにする"
		);
	}
}
#pragma endregion // KDebugSoloMute



#pragma region KDebugTree
#define NODE_TEXT_COLOR_NORMAL     ImColor(255, 255, 255)
#define NODE_TEXT_COLOR_DISABLED   ImColor( 80,  80,  80)
#define NODE_TEXT_COLOR_INVISIBLE  ImColor(160, 160, 160)
#define NODE_TEXT_COLOR_PAUSED     ImColor(160,  64,  64)
#define NODE_TEXT_COLOR_WARNING    ImColor(255,  64,  64)

KDebugTree::KDebugTree() {
	m_next_sel_parent = nullptr;
	m_range_selection_start = nullptr;
	m_range_selection_end = nullptr;
	m_highlighted = nullptr;
	m_next_sel_subindex = 0;
	m_next_sel_frame = 0;
	m_auto_expand = false;
	m_range_selection_required = false;
	m_enable_mini_ctrl = false;
}
void KDebugTree::init() {
	m_next_sel_parent = nullptr;
	m_range_selection_start = nullptr;
	m_range_selection_end = nullptr;
	m_highlighted = nullptr;
	m_next_sel_subindex = 0;
	m_next_sel_frame = -1;
	m_auto_expand = true;
	m_range_selection_required = false;
	m_enable_mini_ctrl = false;
	m_tmp_visible_tree_entities.clear();
	m_selected_entities.clear();
}
bool KDebugTree::select(KNode *node) {
	if (node && !is_selected(node)) {
		m_selected_entities.push_back(node->getId());
		on_select(node);
		return true;
	}
	return false;
}
bool KDebugTree::unselect(KNode *node) {
	if (node && is_selected(node)) {
		auto it = std::find(m_selected_entities.begin(), m_selected_entities.end(), node->getId());
		m_selected_entities.erase(it);
		if (m_range_selection_start == node->getId()) {
			m_range_selection_start = nullptr;
		}
		on_unselect(node);
		return true;
	}
	return false;
}
bool KDebugTree::is_selected(KNode *node) const {
	return node && std::find(m_selected_entities.begin(), m_selected_entities.end(), node->getId()) != m_selected_entities.end();
}
void KDebugTree::clear_selection(bool clear_range_start) {
	m_selected_entities.clear();
	if (clear_range_start) {
		m_range_selection_start = nullptr;
	}
}
void KDebugTree::move_sibling(int step) {
	if (m_selected_entities.empty()) return;
	EID e = m_selected_entities[0];
	KNode *node = KNodeTree::findNodeById(e);
	if (node == nullptr) return;
	KNode *parent = node->getParent();
	if (parent == nullptr) return;
	int i = parent->getChildIndex(node);
	parent->setChildIndex(node, i+step);
}
void KDebugTree::move_prev_sibling() {
	move_sibling(-1);
}
void KDebugTree::move_next_sibling() {
	move_sibling(1);
}
void KDebugTree::gui_tree(KNode *node) {
	if (node == nullptr) return;

	int child_count = node->getChildCount();

	// ツリーノードのテキスト。
	// エンティティ名と子エンティティ数を表示させる
	char label[256] = {0};
	if (child_count > 0) {
		sprintf_s(label, sizeof(label), "%s (0x%x) [%d]", node->getName().c_str(), EID_toint(node->getId()), child_count);
	} else {
		sprintf_s(label, sizeof(label), "%s (0x%x)", node->getName().c_str(), EID_toint(node->getId()));
	}

	// ノードボタン（Enabled, Paused, Visible)
	if (m_enable_mini_ctrl) {
		ImGui::PushID(node);
		on_gui_node_buttons(node);
		ImGui::SameLine();
		ImGui::PopID();
	}

	bool selected = is_selected(node); // ノードが選択されている？

	// ツリーノードの属性を決める
	ImGuiTreeNodeFlags flags = 0;//ImGuiTreeNodeFlags_OpenOnDoubleClick|ImGuiTreeNodeFlags_OpenOnArrow;
	if (selected) flags |= ImGuiTreeNodeFlags_Selected;
	if (child_count == 0) flags |= ImGuiTreeNodeFlags_Leaf;

	// 自動展開が ON の場合、このノードが選択ノードを含んでいたら展開する
	if (m_auto_expand && !m_selected_entities.empty()) {
		EID sel = m_selected_entities.front();
		KNode *selnode = KNodeTree::findNodeById(sel);
		if (node->isParentOf(selnode)) {
			ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		}
	}

	// ノードのテキスト色
	ImVec4 text_color = NODE_TEXT_COLOR_NORMAL;
	if (node) {
		if (!node->getEnableInTree()) {
			text_color = NODE_TEXT_COLOR_DISABLED;
		} else if (node->getPauseInTree()) {
			text_color = NODE_TEXT_COLOR_PAUSED;
		} else if (!node->getVisibleInTree()) {
			text_color = NODE_TEXT_COLOR_INVISIBLE;
		}
	}

#if 0
	// ツリーノード。エンティティの状態によってラベルの文字色を変える
	ImGui::PushStyleColor(ImGuiCol_Text, text_color);
	bool tree_expanded = ImGui::TreeNodeEx((void*)node, flags, label);
	ImGui::PopStyleColor();
	ImGui::SameLine();

	// ノードアイコン
	on_gui_node_icon(node);
#elif 1
	bool tree_expanded = ImGui::TreeNodeEx((void*)node, flags, "");

	ImGui::SameLine();
	ImGui::BeginGroup();
	ImGui::TextColored(text_color, label); // ノードラベル
	ImGui::SameLine();
	on_gui_node_icon(node); // ノードアイコン
	ImGui::EndGroup();


#else
	// ツリーノード。エンティティの状態によってラベルの文字色を変える
	bool tree_expanded = ImGui::TreeNodeEx((void*)node, flags, "");
	ImGui::SameLine();

	// ノードアイコン
	on_gui_node_icon(node);
	ImGui::SameLine();
	
	// ノードラベル
	ImGui::TextColored(text_color, label);
#endif
	m_tmp_visible_tree_entities.push_back(node->getId());

	if (ImGui::IsItemHovered()) {
		// ノードにマウスポインタが重なったらハイライト
		m_highlighted = node->getId();
	}

	// ノードがクリックされたら選択
	if (ImGui::IsItemClicked()) {
		if (ImGui::GetIO().KeyCtrl) {
			// Ctrl キーとともに押された。選択状態をトグル＆複数選択
			if (selected) {
				unselect(node);
			} else {
				select(node);
			}
			m_range_selection_start = node->getId(); // 次に範囲選択が行われた場合、ここが起点となる
		
		} else if (ImGui::GetIO().KeyShift) {
			// Shift キーとともに押された。範囲選択する
			m_range_selection_end = node->getId();
		
		} else if (ImGui::GetIO().KeyAlt) {
			// Alt キーとともに押された

		} else {
			// 修飾キーなしでクリックされている。単一選択する
			clear_selection(true);
			select(node);
			m_range_selection_start = node->getId(); // 次に範囲選択が行われた場合、ここが起点となる
		}
	}
	if (! tree_expanded) {
		// ツリービューを開いていない
		return;
	}

	// 再帰的に子ノードを表示
	for (int i=0; i<node->getChildCount(); i++) {
		gui_tree(node->getChild(i));
	}
	ImGui::TreePop();
}
void KDebugTree::update_gui() {
	// ツリー上での範囲選択のために、
	// GUI上での表示順を記録しておくためのリストを初期化する
	m_tmp_visible_tree_entities.clear();
//	m_range_selection_start = nullptr; これはクリアしないこと！！
	m_range_selection_end = nullptr;
	m_highlighted = nullptr;

	// ルート要素ごとに、そのツリーをGUIに追加していく
	KNode *root = KNodeTree::getRoot();
	K__ASSERT(root);
	int num_roots = root->getChildCount();
	for (int i=0; i<num_roots; i++) {
		KNode *node = root->getChild(i);
		gui_tree(node);
	}

	// ツリービューでの範囲選択処理。複数の親子にまたがっていてもよい
	if (m_range_selection_start && m_range_selection_end) {
		clear_selection(false); // 範囲選択情報は削除しない
		// 範囲選択が決定された
		size_t i = 0;
		EID end = nullptr;
		// 範囲選択の先頭を探す。範囲選択の先頭よりも終端の方がリスト手前にある可能性に注意
		for (; i<m_tmp_visible_tree_entities.size(); i++) {
			EID e = m_tmp_visible_tree_entities[i];
			if (e == m_range_selection_start) { 
				end = m_range_selection_end; 
				break;
			}
			if (e == m_range_selection_end) {
				end = m_range_selection_start;
				break;
			}
		}
		// 範囲選択の終端までを選択する
		for (; i<m_tmp_visible_tree_entities.size(); i++) {
			EID e = m_tmp_visible_tree_entities[i];
			KNode *node = KNodeTree::findNodeById(e);
			if (node) select(node);
			if (e == end) break;
		}
	}
}
void KDebugTree::update_sel() {
	if (m_next_sel_frame < 0) {
		return; // 何も選択されていない
	}
	int t = on_query_frame_number();
	if (t <= m_next_sel_frame || m_next_sel_frame+10 <= t) {
		// 更新すべきフレームを既に過ぎているか、まだフレームが進んでいない
		// ※きっちり m_next_sel_frame + 1 で判定しないこと。
		// フレームスキップされている可能性を考える
		return;
	}
	m_next_sel_frame = -1;
	KNode *node = KNodeTree::findNodeById(m_next_sel_parent);
	if (node == nullptr) {
		return; // 選択ノードは既に削除されている
	}
	int num = node->getChildCount();
	if (num == 0) {
		// 子がいない。親を選択
		select(node);

	} else if (m_next_sel_subindex < num) {
		// 元のカーソル位置と同じ場所を選択
		KNode *newsel = node->getChild(m_next_sel_subindex);
		select(newsel);

	} else {
		// インデックスが範囲外。最後の子を選択
		select(node->getChild(num-1));
	}
}
void KDebugTree::remove_selections() {
	if (m_selected_entities.empty()) {
		return;
	}
	// 最初に選択されているノードの場所を記録しておく
	EID e = m_selected_entities.front();
	KNode *node = KNodeTree::findNodeById(e);
	if (node == nullptr) {
		return;
	}
	// これは、削除前に選択されていたノードと同じ場所にあるノードを、ノード削除後に再選択するためのもの。
	// これをしないと DEL ボタンを押すたびに選択が解除されてしまうため、
	// DEL を連打して連続でノードを削除することができなくなる
	// ※ただし、この再選択は次のフレームで行う（実際にノードが削除されるのはフレーム最後なので）
	KNode *parent = node->getParent();
	if (parent) {
		m_next_sel_parent = parent->getId();
		m_next_sel_subindex = parent->getChildIndex(node);
		m_next_sel_frame = on_query_frame_number() + 1;
	} else {
		m_next_sel_parent = nullptr;
		m_next_sel_subindex = -1;
		m_next_sel_frame = -1;
	}

	// 選択されたノードを削除する
	for (auto it=m_selected_entities.begin(); it!=m_selected_entities.end(); ++it) {
		EID e = *it;
		KNode *node = KNodeTree::findNodeById(e);
		if (node) {
			node->remove();
		}
	}
}
std::string KDebugTree::get_selected_node_name() const {
	std::string ret;
	EID e = m_selected_entities.empty() ? 0 : m_selected_entities[0];
	KNode *node = KNodeTree::findNodeById(e);
	if (node) {
		ret = node->getNameInTree();
		if (ret.empty()) {
			ret = K::str_sprintf("#%p", e);
		}
	}
	return ret;
}
KNode * KDebugTree::get_mouse_hovered() const {
	return KNodeTree::findNodeById(m_highlighted);
}
#pragma endregion // KDebugTree





} // namespace