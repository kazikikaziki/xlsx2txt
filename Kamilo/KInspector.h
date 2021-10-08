#pragma once
#include "KVideo.h"
#include "KNode.h"
#include "KMath.h"

namespace Kamilo {

class KEngine;

/// インスペクター内でデータの表示やパラメータの編集をできるようにするためのコールバック。
/// KEngine::addInspectorCallback で登録して使う
class KInspectorCallback {
public:
	virtual ~KInspectorCallback() {}

	/// システム用のインスペクターを更新する時に呼ばれる
	/// 
	/// ゲームエンジンのインスペクターを開いたときに独自の設定項目を表示したい場合は、
	/// KEngine::addInspectorCallback でコールバックを登録し、このメソッド内で Dear ImGui を利用する。
	/// Dead ImGui のライブラリは既にソースツリーに同梱されているので、改めてダウンロード＆インストールする必要はない。
	/// ImGui の関数などは imgui/imgui.h を参照すること。使い方などは imgui/imgui.cpp に書いてある。
	/// @see KScreenCallback::on_manager_gui
	virtual void onInspectorGui() {}
};

struct KInspectableDesc {
	KInspectableDesc() {
		sort_primary = 0;
		default_open = false;
		is_open = false;
		always_show_debug = false;
		label_u8[0] = '\0';
	}

	/// インスペクター上での表示名 (utf8)
	char label_u8[256];

	/// インスペクター上にシステムペインを表示する時、
	/// 並び順の決定に使う値。プライマリキー
	int sort_primary;

	/// インスペクター上で、最初からペインが開いているかどうか
	bool default_open;

	/// インスペクター上で、現在ペインが開いているかどうか
	bool is_open;

	/// インスペクターが表示されていなくてもデバッグ情報を描画するかどうか
	bool always_show_debug;
};

/// インスペクター。
/// ゲームシステムやゲームエンティティの状態を閲覧または編集するためクラス
class KInspector {
public:
	static void install();
	static void uninstall();
	static bool isInstalled();

	static void onGameStart();
	static void onGameEnd();
	static void onEntityRemoving(KNode *node);

	/// インスペクターにアイテムを登録する
	static void addInspectable(KInspectorCallback *cb, const char *label="");

	/// インスペクターからアイテムを削除する
	static void removeInspectable(KInspectorCallback *cb);

	/// インスペクターにアイテムが登録されているか調べる
	static bool hasInspectable(const KInspectorCallback *ins);

	static const KInspectableDesc * getDesc(const KInspectorCallback *cb);
	static KInspectableDesc * getDesc(KInspectorCallback *cb);

	static bool isDebugInfoVisible(KInspectorCallback *ins);

	/// インスペクターGUIを描画する
	/// @param game_display 描画先のレンダーテクスチャー。
	static void render(KTEXID game_display);

	/// インスペクターの表示状態を指定
	static void setVisible(bool value);

	/// インスペクターが表示されているか
	static bool isVisible();

	/// インスペクターで使うフォントを指定する。
	/// @param font_index 使用するフォント番号。
	///                   これは ImGui に登録されているフォント配列のインデックスで、
	///                   ImGui::GetIO().Fonts->Fonts[index] のフォントを表す。
	///                   @see KImGui_AddFontFromFileTTF()
	static void setFont(int imgui_font_index);

	/// インスペクター上で選択されているエンティティ数
	static int getSelectedEntityCount();

	/// インスペクター上で選択されているノードを返す
	static KNode * getSelectedEntity(int index);

	/// ノードをゲーム画面上で強調表示させる
	static void setHighlightedEntity(KNode *node);

	/// ノードがゲーム画面上で強調表示されているかどうか
	static bool isEntityHighlighted(const KNode *node, bool in_hierarchy);

	/// ノードを選択する
	static void selectEntity(const KNode *node);

	/// 全ノードの選択状態を解除する
	static void unselectAllEntities();

	/// 　EID の表示が許可されているかどうか。
	/// Hierarchy の isVisible とは別に、一時的に表示が ON/OFF されている場合は
	/// その値を返す（SOLO, MUTE を操作した場合など）
	static bool isEntityVisible(const KNode *node);

	/// ノードがインスペクター上で選択されているかどうか
	static bool isEntitySelected(const KNode *node);

	/// ノードがインスペクター上で選択されているエンティティツリーに含まれているかどうか
	static bool isEntitySelectedInHierarchy(const KNode *node);

	/// エンティティを選択状態として描画するべきかどうか
	static bool entityShouldRednerAsSelected(const KNode *node);

	/// インスペクター上で選択されているオブジェクトを、ゲーム画面内で強調表示する
	static bool isHighlightSelectionsEnabled();
	static bool isAxisSelectionsEnabled();

	/// ウィンドウ内で実際にゲーム画面が描画されている領域を得る。
	/// ゲームビューが表示されていれば rectInPixel にクライアントウィンドウ座標系（左上が原点）で矩形をセットして true を返す。
	/// デバッグ用GUIを経由せずにウィンドウに直接描画されている場合は何もせずに false を返す
	static KRecti getWorldViewport();

	static bool isStatusTipVisible();
	static void setStatusTipVisible(bool visible);
};

class KDebugSoloMute {
public:
	KDebugSoloMute();
	~KDebugSoloMute();
	void update_gui(KNode *node);

	KNode * get_solo() const;
	KNode * get_mute() const;
	bool get_tree() const;
	void set_solo(KNode *node);
	void set_mute(KNode *node);
	void set_tree(bool value);
	bool is_solo(const KNode *node) const;
	bool is_solo_in_tree(const KNode *node) const;
	bool is_mute(const KNode *node) const;
	bool is_mute_in_tree(const KNode *node) const;
	bool is_truly_visible(const KNode *node) const;
private:
	KNodeTree * mgr() const;
	void gui_solo(KNode *node);
	void gui_mute(KNode *node);
	void gui_tree(KNode *node);
private:
	mutable KNode *m_solo;
	mutable KNode *m_mute;
	bool m_tree;
};


class KDebugTree {
public:
	KDebugTree();
	void init();
	void update_gui();

	bool select(KNode *node);
	bool unselect(KNode *node);
	bool is_selected(KNode *node) const;
	void clear_selection(bool clear_range_start);
	void move_prev_sibling();
	void move_next_sibling();
	void remove_selections();
	std::string get_selected_node_name() const;
	KNode * get_mouse_hovered() const;

	virtual void on_select(KNode *node) = 0;
	virtual void on_unselect(KNode *node) = 0;
	virtual void on_gui_node_icon(KNode *node) = 0;
	virtual void on_gui_node_buttons(KNode *node) = 0;
	virtual int on_query_frame_number() const = 0;

protected:
	void update_sel();
	bool m_enable_mini_ctrl;

private:
	void move_sibling(int step);
	void gui_tree(KNode *node);
	std::vector<EID> m_selected_entities;
	std::vector<EID> m_tmp_visible_tree_entities; // インスペクターのツリービュー上に表示されているノード
	EID m_next_sel_parent; // 次のフレームで選択するべきノードの親（ノード自身はすでに消えている可能性があるので）
	int m_next_sel_subindex; // 次のフレームで選択するべきノードの兄弟インデックス
	int m_next_sel_frame; // 即座にインスペクターを消してしまった場合「次のインスペクター更新のタイミング」が来るのは次にインスペクターを表示された時になってしまうので、具体的なフレーム数を指定しておく
	EID m_range_selection_start;
	EID m_range_selection_end;
	EID m_highlighted;
	bool m_auto_expand; // 選択されているノードまで階層を開く
	bool m_range_selection_required;
};


class KDebugGui {
public:
	static void K_DebugGui_Image(KTEXID tex, int _w, int _h, KVec2 uv0=KVec2(0,0), KVec2 uv1=KVec2(1,1), KColor color=KColor::WHITE, KColor border=KColor::ZERO, bool keep_aspect=true, bool linear_filter=true);
	static void K_DebugGui_ImageExportButton(const char *label, KTEXID tex, const char *filename, bool alphamask=false);
	static bool K_DebugGui_InputMatrix(const char *label, KMatrix4 *m);
	static bool K_DebugGui_InputBlend(const char *label, KBlend *blend);
	static bool K_DebugGui_InputFilter(const char *label, KFilter *filter);
	static bool K_DebugGui_VSplitter(const char *id, int *x, int sign=1, int min_x=10, int thickness=4);
	static bool K_DebugGui_HSplitter(const char *id, int *y, int sign=1, int min_y=10, int thickness=4);
	static void K_DebugGui_NodeNameId(KNode *node);
	static void K_DebugGui_NodeTag(KNode *node);
	static void K_DebugGui_NodeLayer(KNode *node);
	static void K_DebugGui_NodePriority(KNode *node);
	static void K_DebugGui_NodeAtomic(KNode *node);
	static void K_DebugGui_NodeState(KNode *node);
	static void K_DebugGui_NodePosition(KNode *node);
	static void K_DebugGui_NodeScale(KNode *node);
	static void K_DebugGui_NodeRotate(KNode *node);
	static void K_DebugGui_NodeInheritTransform(KNode *node);
	static void K_DebugGui_NodeCustomTransform(KNode *node);
	static void K_DebugGui_NodeTransform(KNode *node);
	static void K_DebugGui_NodeColor(KNode *node);
	static void K_DebugGui_NodeSpecular(KNode *node);
	static void K_DebugGui_NodeAction(KNode *node);
	static void K_DebugGui_NodeParent(KNode *node, KDebugTree *tree=nullptr);
	static void K_DebugGui_NodeChildren(KNode *node, KDebugTree *tree=nullptr);
	static void K_DebugGui_NodeCamera(KNode *node, KNode *camera, KDebugTree *tree=nullptr);
	static void K_DebugGui_NodeRenderAfterChildren(KNode *node);
	static void K_DebugGui_NodeRenderNoCulling(KNode *node);
	static void K_DebugGui_NodeWorldPosition(KNode *node);
	static void K_DebugGui_NodeWarning(KNode *node);
	static void K_DebugGui_Mouse();
	static void K_DebugGui_Window();
	static void K_DebugGui_TimeCtrl();
	static void K_DebugGui_FrameInfo();
	static void K_DebugGui_Aabb(const char *label, const KVec3 &aabb_min, const KVec3 &aabb_max);
};



} // namespace
