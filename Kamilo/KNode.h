/// @file
/// Copyright (c) 2020 Heliodor
/// This software is released under the MIT License.
/// http://opensource.org/licenses/mit-license.php

#pragma once
#include <unordered_map>
#include <mutex>
#include "KRef.h"
#include "KString.h"
#include "KVec.h"
#include "KColor.h"
#include "KMatrix.h"
#include "KQuat.h"
#include "KSig.h"

#define NODE_NAME 0


namespace Kamilo {

class KNode;
class KQuat;
class KAction;
class KSig;

K_DECL_SIGNAL(K_SIG_NODE_UNBINDTREE);

#pragma region EID
/// @internal
struct K__EID { int not_used; };

/// エンティティの識別子
///
/// @note
/// エンティティ識別子 EID は構造体 K__EID のポインタとして定義されるが、
/// これは型チェックのために K__EID という型を導入しているだけで、
/// コンパイル時に型チェックさせる以上の使い道はない。
/// __EID 自体は全く使われていないし、使ってはいけない。
/// そもそも EID が K__EID へのポインタを保持しない。
typedef K__EID *EID;

/// EID --> int の変換
inline uintptr_t EID_toint(EID x) { return (uintptr_t)x; }

/// int --> EID の変換
inline EID EID_from(uintptr_t x) { return (EID)x; }
inline EID EID_from(void *x) { return (EID)x; }

typedef std::vector<EID> KEntityList;

#pragma endregion // EID







typedef KName KTag;


struct KRenderArgs {
	KMatrix4 camera_projection;
	KMatrix4 camera_transform;
	void *userdata;

	KRenderArgs() {
		userdata = nullptr;
	}
};


class KTraverseCallback {
public:
	virtual void onVisit(KNode *node) = 0;
};

class KNodeRemovingCallback {
public:
	virtual void onRemoveNodes(KNode *nodes[], int size) = 0;
};

enum KNodeTickFlag {
	KNodeTick_DONTCARE_ENABLE = 1, // Enabled 状態に関係なく更新する
	KNodeTick_DONTCARE_PAUSED = 2, // Paused 状態に関係なく更新する
	KNodeTick_ENTERONLY = 4, // 新しいアクションが設定されている場合のみ、enter と最初の step だけを実行する
};
typedef int KNodeTickFlags;

class CNodeTreeImpl; // internal


struct STransformData {
	KVec3 m_Position; // ノード座標。independent_ の値により絶対座標か相対座標かが異なる
	KVec3 m_Scale;    // ノードのローカルスケール
	KQuat m_Rotation; // 回転
	KVec3 m_RotationEuler;
	KMatrix4 m_CustomMatrix;    // pos, scale, rotation に加えて、独自に行う変形行列。
	KMatrix4 m_CustomMatrixInv; // m_CustomTransform の逆行列
	mutable KMatrix4 m_LocalMatrix;    // pos, scale, rotation, m_more_transform によって決まる変形行列
	mutable KMatrix4 m_LocalMatrixInv; // m_LocalMatrix の逆行列
	mutable KMatrix4 m_WorldMatrix;    // ワールド内での行列
	mutable bool m_DirtyLocalMatrix;
	mutable bool m_DirtyWorldMatrix;
	bool m_UsingEuler;
	bool m_UsingCustom;
	bool m_InheritTransform;
	KNode *m_Node;

	STransformData();
	const KVec3 & getPosition() const;
	const KVec3 & getScale() const;
	const KQuat & getRotation() const;
	void setPosition(const KVec3 &value);
	void setPositionDelta(const KVec3 &delta);
	void setScale(const KVec3 &value);
	void setRotation(const KQuat &value);
	void setRotationDelta(const KQuat &delta);
	void setRotationEuler(const KVec3 &euler_degrees); ///< 回転をオイラー角 (XYZ order)で設定する
	bool getRotationEuler(KVec3 *out_euler_degrees) const;
	void setCustomTransform(const KMatrix4 &matrix, const KMatrix4 *matrix_inv);
	void getCustomTransform(KMatrix4 *out_matrix, KMatrix4 *out_matrix_inv) const;
	const KMatrix4 & getLocalMatrix() const; ///<　移動、拡大、回転、カスタム変形をすべて適用させたときの行列
	const KMatrix4 & getLocalMatrixInversed() const;
	void getWorld2LocalMatrix(KMatrix4 *out) const; ///< ワールド座標からローカル座標へ変換するための行列
	KMatrix4 getWorld2LocalMatrix() const;
	void getLocal2WorldMatrix(KMatrix4 *out) const; ///< ローカル座標からワールド座標へ変換するための行列
	KMatrix4 getLocal2WorldMatrix() const;
	void getLocal2WorldRotation(KQuat *out) const; ///< ローカル回転からワールド回転へ変換するためのクォータニオン
	KQuat getLocal2WorldRotation() const;
	void setTransformInherit(bool value);
	bool getTransformInherit() const;
	KVec3 getWorldPosition() const;
	KVec3 localToWorldPoint(const KVec3 &local) const; ///< ローカル座標をワールド座標にする @see getLocal2WorldMatrix
	KVec3 worldToLocalPoint(const KVec3 &world) const; ///< ワールド座標をローカル座標にする @see getWorld2LocalMatrix
	void copyTransform(const KNode *other, bool copy_independent_flag);
	void _setDirtyWorldMatrix();
	void _updateLocalMatrix() const; // mutable 変数を扱うので const 属性にしてある
	void _updateWorldMatrix(const KMatrix4 &parent) const; // mutable 変数を扱うので const 属性にしてある
	void _updateTree();
};


struct STagData {
	KNameList m_SelfTags; // 自分自身のタグ
	KNameList m_ParentTags; // 親から継承したタグ
	KNameList m_TagsInTree; // 自分自身のタグと継承したタグを合成したもの
	KNode *m_Node;

	STagData();
	bool hasTag(const KTag &tag) const;
	bool hasTagInTree(const KTag &tag) const;
	const KNameList & getTagList() const;
	const KNameList & getTagListInTree() const;
	const KNameList & getTagListInherited() const;
	void setTag(const KTag &tag);
	void removeTag(const KTag &tag);
	void _setTagEx(const KTag &tag, bool is_inherited); 
	void _removeTagEx(const KTag &tag, bool is_inherited);
	void _updateNodeTreeTags(bool add);
	void _beginParentChange();
	void _endParentChange();
};


// 継承可能な論理値
struct SFlagData {
	uint32_t m_Bits; // 自分自身のフラグ
	uint32_t m_BitsInTreeAll; // 自分と親ツリーのフラグビットを AND 結合したもの
	uint32_t m_BitsInTreeAny; // 自分と親ツリーのフラグビットを OR 結合したもの
	KNode *m_Node;

	SFlagData();
	bool hasFlag(uint32_t flag) const;
	bool hasFlagInTreeAll(uint32_t flag) const;
	bool hasFlagInTreeAny(uint32_t flag) const;
	uint32_t getFlagBits() const;
	uint32_t getFlagBitsInTreeAll() const;
	uint32_t getFlagBitsInTreeAny() const;
	void setFlag(uint32_t flag, bool value);
	void _updateTree();
	void _updateTree(const SFlagData *parent);
};



enum KLocalRenderOrder {
	KLocalRenderOrder_DEFAULT,
	KLocalRenderOrder_TREE,
};

struct SRenderData {
	KColor m_Diffuse;
	KColor m_Specular;
	KColor m_DiffuseInTree;
	KColor m_SpecularInTree;
	bool m_InheritDiffuse;
	bool m_InheritSpecular;
	bool m_RenderAtomic;
	bool m_RenderAfterChildren;
	bool m_ViewCulling;
	int m_Layer; // 描画単位によるグループ化。同じレイヤー番号を持つエンティティがまとめて描画される
	int m_LayerInTree;
	int m_Priority; // 描画の優先順位。番号が若いほど手前になる
	int m_PriorityInTree;
	KNode *m_Node;
	KLocalRenderOrder m_LocalRenderOrder;

	SRenderData();
	const KColor & getColor() const;
	const KColor & getSpecular() const;
	const KColor & getColorInTree() const;
	const KColor & getSpecularInTree() const;
	float getAlpha() const;
	void setColor(const KColor &color);
	void setSpecular(const KColor &specular);
	void setAlpha(float alpha);
	void setColorInherit(bool value);
	bool getColorInherit() const;
	void setSpecularInherit(bool value);
	bool getSpecularInherit() const;
	void setRenderAtomic(bool value);///< 不可分。子ツリーを一つの塊として扱う（描画時に子ノードの間に別のノードが挟まらない）
	bool getRenderAtomic() const;
	void setRenderAfterChildren(bool value);  ///< 通常は親（自分）→子の順番で描画するが、子→親（自分）の順番で描画したい場合に使う（描画順序がツリー順になっている場合のみ）
	bool getRenderAfterChildren() const;
	void setViewCulling(bool value); ///< カメラの描画範囲外だった場合は描画プロセスを実行しない（描画結果だけ欲しい場合など、カメラとの位置関係と無関係に描画したい場合は false にする）
	bool getViewCulling() const;
	bool getViewCullingInTree() const;
	KLocalRenderOrder getLocalRenderOrder() const;
	void setLocalRenderOrder(KLocalRenderOrder lro);
	void setLayer(int value);
	int  getLayer() const;
	int  getLayerInTree() const;
	void setPriority(int value); // 描画の優先順位。番号が若いほど手前になる
	int  getPriority() const;
	int  getPriorityInTree() const;
	void _updateTree();
	void _updateTree(const SRenderData *parent);
};




class KNode: public KRef {
public:
	static KNode * create();

	KNode();
	virtual ~KNode();
	enum Category {
		Category_LAYER,    // 描画単位によるグループ化。同じレイヤー番号を持つエンティティがまとめて描画される
		Category_PRIORITY, // 描画の優先順位。番号が若いほど手前になる
		Category_TAG,      // タグ付けによるグループ化
		Category_ENUM_MAX
	};
	enum Flag {
		FLAG_NO_ENABLE    = 0x0001,
		FLAG_NO_UPDATE    = 0x0002,
		FLAG_NO_RENDER    = 0x0004, // Invisible
		FLAG_SYSTEM       = 0x0040, // システムノード（デバッグ用のポーズ中でも動作する）
		FLAG__MARK_REMOVE = 0x4000, // 削除マーク (internal flag)
		FLAG__INVALD      = 0x8000, // 削除済み (internal flag)
	};
	typedef int Flags;

	virtual void on_node_inspector() {}
	virtual void on_node_ready() {}
	virtual void on_node_start() {} // call before first step() 
	virtual void on_node_end() {}
	virtual void on_node_step() {}
	virtual void on_node_gui() {}
	virtual void on_node_systemstep() {}

	/// すべての描画チェックが完了し、実際にノードを描画する直前に呼ばれる。
	/// 描画の前処理が重い場合、ここにその処理を書いておくと無駄が少なくなる
	virtual void on_node_will_render(KNode *camera) {}

	virtual void on_node_render(KRenderArgs *args) {} // ノードを描画する
	virtual void on_node_signal(KSig &sig) {}

	#pragma region KRef
	virtual const char * getReferenceDebugString() const override;
	virtual std::string getWarningString();
	#pragma endregion // KRef


	#pragma region Signal
	void broadcastSignalToChildren(KSig &sig); // 子ツリーに対して送信する
	void broadcastSignalToParents(KSig &sig); // 親に対して送信する
	void processSignal(KSig &sig); // このノードにシグナルを処理させる
	#pragma endregion // Signal

	void sendActionCommand(KSig &cmd);

	// Name and ID
	void setName(const std::string &name);
	bool hasName(const std::string &name) const;
	const std::string & getName() const;
	const std::string & getNameInTree() const;
	EID getId() const;
	void _updateNames();

	#pragma region Tree
	CNodeTreeImpl * get_tree() const;
	void _set_tree(CNodeTreeImpl *tree);
	void _propagate_enter_tree();
	void _propagate_exit_tree();
	void _propagate_ready();
	#pragma endregion // Tree


	#pragma region Parent/Children/Sibling
	void markAsRemove();
	void setParent(KNode *new_parent);
	KNode * getRoot();
	KNode * getParent() const;
	KNode * getChild(int index) const;
	KNode * getChildFast(int index) const;
	int getChildCount() const;
	void setChildIndex(KNode *child, int new_index);
	int getChildIndex(const KNode *child) const;
	bool isParentOf(const KNode *child) const;
	template <class T> T getChildT() const {
		int num = getChildCount();
		for (int i=0; i<num; i++) {
			KNode *node = getChild(i);
			T nodeT = dynamic_cast<T>(node);
			if (nodeT) return nodeT;
		}
		return nullptr;
	}
	#pragma endregion // Parent/Children/Sibling

	// Find
	// 指定された名前のノードを探す。tag を指定した場合は名前とタグの両方で探す
	// 名前に nullptr を指定した場合はタグだけで探す。
	KNode * findChild(const std::string &name, const KTag &tag=nullptr) const;
	KNode * findChildInTree(const std::string &name, const KTag &tag=nullptr) const;
	KNode * findChildInTree_unsafe(const std::string &name, const KTag &tag=nullptr) const;
	KNode * findChildPath(const std::string &subpath) const;

	// Traverse
	void traverse_parents(KTraverseCallback *cb);
	void traverse_children(KTraverseCallback *cb, bool recurse=true);

	// Action
	void setAction(KAction *act, bool update_now=true);
	KAction * getAction() const;
	template <class T> T getActionT() const {
		return dynamic_cast<T>(getAction());
	}

	void tick(KNodeTickFlags flags); // ゲーム用の更新。デバッグ用のポーズ中は呼ばれない
	void tick2(KNodeTickFlags flags);

	void sys_tick(); // システム用の更新。デバッグ用のポーズ中でも関係なく呼ばれる

	// Removing
	void remove();
	void remove_children();
	void _remove_all();



	#pragma region Transform
	const KVec3 & getPosition() const;
	const KVec3 & getScale() const;
	const KQuat & getRotation() const;
	void setPosition(const KVec3 &value);
	void setPosition(float x, float y, float z=0.0f) { setPosition(KVec3(x, y, z)); }
	void setPositionDelta(const KVec3 &delta);
	void setScale(const KVec3 &value);
	void setScale(float sx, float sy, float sz=1.0f) { setScale(KVec3(sx, sy, sz)); }
	void setRotation(const KQuat &value);
	void setRotationDelta(const KQuat &delta);
	void setRotationEuler(const KVec3 &euler_degrees); ///< 回転をオイラー角 (XYZ order)で設定する
	void setRotationEuler(float xdeg, float ydeg, float zdeg) { setRotationEuler(KVec3(xdeg, ydeg, zdeg)); }
	bool getRotationEuler(KVec3 *out_euler_degrees) const;
	void setCustomTransform(const KMatrix4 &matrix, const KMatrix4 *matrix_inv=nullptr);
	void getCustomTransform(KMatrix4 *out_matrix, KMatrix4 *out_matrix_inv) const;
	const KMatrix4 & getLocalMatrix() const; ///<　移動、拡大、回転、カスタム変形をすべて適用させたときの行列
	const KMatrix4 & getLocalMatrixInversed() const;
	void getWorld2LocalMatrix(KMatrix4 *out) const; ///< ワールド座標からローカル座標へ変換するための行列
	KMatrix4 getWorld2LocalMatrix() const;
	void getLocal2WorldMatrix(KMatrix4 *out) const; ///< ローカル座標からワールド座標へ変換するための行列
	KMatrix4 getLocal2WorldMatrix() const;
	void getLocal2WorldRotation(KQuat *out) const; ///< ローカル回転からワールド回転へ変換するためのクォータニオン
	KQuat getLocal2WorldRotation() const;
	void setTransformInherit(bool value);
	bool getTransformInherit() const;
	KVec3 getWorldPosition() const;
	KVec3 localToWorldPoint(const KVec3 &local) const; ///< ローカル座標をワールド座標にする @see getLocal2WorldMatrix
	KVec3 worldToLocalPoint(const KVec3 &world) const; ///< ワールド座標をローカル座標にする @see getWorld2LocalMatrix
	void copyTransform(const KNode *other, bool copy_independent_flag);
	#pragma endregion // Transform


	#pragma region Render
	const KColor & getColor() const;
	const KColor & getSpecular() const;
	const KColor & getColorInTree() const;
	const KColor & getSpecularInTree() const;
	float getAlpha() const;
	void setColor(const KColor &color);
	void setSpecular(const KColor &specular);
	void setAlpha(float alpha);
	void setColorInherit(bool value);
	bool getColorInherit() const;
	void setSpecularInherit(bool value);
	bool getSpecularInherit() const;
	void setRenderAtomic(bool value);///< 不可分。子ツリーを一つの塊として扱う（描画時に子ノードの間に別のノードが挟まらない）
	bool getRenderAtomic() const;
	void setRenderAfterChildren(bool value);  ///< 通常は親（自分）→子の順番で描画するが、子→親（自分）の順番で描画したい場合に使う（描画順序がツリー順になっている場合のみ）
	bool getRenderAfterChildren() const;
	void setViewCulling(bool value); ///< カメラの描画範囲外だった場合は描画プロセスを実行しない（描画結果だけ欲しい場合など、カメラとの位置関係と無関係に描画したい場合は false にする）
	bool getViewCulling() const;
	bool getViewCullingInTree() const;
	KLocalRenderOrder getLocalRenderOrder() const;
	void setLocalRenderOrder(KLocalRenderOrder lro);
	#pragma endregion // Render


	#pragma region Flags
	bool hasFlag(Flag flag) const;
	bool hasFlagInTreeAll(Flag flag) const;
	bool hasFlagInTreeAny(Flag flag) const;
	Flags getFlagBits() const;
	Flags getFlagBitsInTreeAll() const;
	Flags getFlagBitsInTreeAny() const;
	void setFlag(Flag flag, bool value);
	#pragma endregion // Flags


	#pragma region Tags
	bool hasTag(const KTag &tag) const;
	bool hasTagInTree(const KTag &tag) const;
	const KNameList & getTagList() const;
	const KNameList & getTagListInTree() const;
	const KNameList & getTagListInherited() const;
	void setTag(const KTag &tag);
	void removeTag(const KTag &tag);
	void copyTags(KNode *src);
	void _updateNodeTreeTags(bool add);
	#pragma endregion // Tags


	#pragma region Helper
	bool isInvalid() const;
	void setPause(bool value);
	bool getPause() const;
	bool getPauseInTree() const;
	void setEnable(bool value);
	bool getEnable() const;
	bool getEnableInTree() const;
	void setVisible(bool value);
	bool getVisible() const;
	bool getVisibleInTree() const;
	void setLayer(int value);
	int getLayer() const;
	int getLayerInTree() const;
	void setPriority(int value);
	int getPriority() const;
	int getPriorityInTree() const;
	#pragma endregion // Helper

public:
	void _RegisterForDelete(std::vector<KNode*> &list, bool all);
	void _ExitAction();
	void _DeleteAction();
	void _add_child_nocheck(KNode *node);
	void _remove_child_nocheck(KNode *node);
	void _invalidate_child_tree();
	void _Invalidated();
	STransformData & _getTransformData() { return m_TransformData; }
	const STransformData & _getTransformData() const { return m_TransformData; }
	STagData & _getTagData() { return m_TagData; }
	const STagData & _getTagData() const { return m_TagData; }
	SFlagData & _getFlagData() { return m_FlagData; }
	const SFlagData & _getFlagData() const { return m_FlagData; }
	SRenderData & _getRenderData() { return m_RenderData; }
	const SRenderData & _getRenderData() const { return m_RenderData; }


private:
	void lock() const;
	void unlock() const;
#ifdef _DEBUG
	std::string *m_pName;
	KNode **m_pParent;
#endif

	struct NodeData {
		EID uuid;
		KNode *parent;
		std::string name;
		std::string nameInTree;
		std::vector<KNode *> children;
		CNodeTreeImpl *tree;
		int blocked;
		bool is_ready;
		bool should_call_onstart;

		NodeData() {
			uuid = nullptr;
			parent = nullptr;
			tree = nullptr;
			blocked = 0;
			is_ready = false;
			should_call_onstart = true;
		}
	};
	NodeData m_NodeData;
	mutable STransformData m_TransformData;
	mutable SRenderData m_RenderData;
	mutable SFlagData m_FlagData;
	mutable STagData m_TagData;

public:
	mutable std::recursive_mutex m_Mutex;
	KAction *m_ActionNext;
	KAction *m_ActionCurr;
	std::vector<KNode *> m_TmpNodes;
};

typedef std::vector<KNode*> KNodeArray;






class KComp: public KRef {
public:
	KComp() {
		m_Node = nullptr;
	}
	virtual ~KComp() {}
	virtual void on_comp_attach() {}
	virtual void on_comp_detach() {}
	KNode * getNode() {
		return m_Node;
	}
	void _setNode(KNode *node) {
		// KComp は KNode によって保持される可能性がある。
		// 循環ロック防止のために KNode の参照カウンタは変更しないでおく
		if (node) {
			// アタッチしようとしている
			m_Node = node;
			on_comp_attach();
		} else {
			// デタッチしようとしている
			on_comp_detach();
			m_Node = node;
		}
	}
private:
	KNode *m_Node;
};


// Co は KComp の継承であること!!!
template <class Co> class KCompNodes {
	std::unordered_map<KNode*, Co*> m_Nodes;
public:
	typedef typename std::unordered_map<KNode*, Co*>::iterator iterator;

	KCompNodes() {
	}
	virtual ~KCompNodes() {
	//	assert(m_Nodes.empty()); // 正しく解放されていれば、すでにノードは削除済みのはず
		clear();
	}
	iterator begin() {
		return m_Nodes.begin();
	}
	iterator end() {
		return m_Nodes.end();
	}
	int size() const {
		return (int)m_Nodes.size();
	}
	void clear() {
		for (auto it=m_Nodes.begin(); it!=m_Nodes.end(); ++it) {
			Co *comp = it->second;
			comp->_setNode(nullptr); // ここでエラーが起きる場合、KComp を継承していない可能性がある
			comp->drop();
			KNode *node = it->first;
			node->drop();
		}
		m_Nodes.clear();
	}
	void attach(KNode *node, Co *comp) {
	//	assert(node);
	//	assert(comp);
		detach(node);
		m_Nodes[node] = comp;
		node->grab();
		comp->_setNode(node); // ここでエラーが起きる場合、KComp を継承していない可能性がある
		comp->grab();
	}
	void detach(KNode *node) {
		auto it = m_Nodes.find(node);
		if (it != m_Nodes.end()) {
			Co *comp = it->second;
			comp->_setNode(nullptr); // ここでエラーが起きる場合、KComp を継承していない可能性がある
			comp->drop();
			node->drop();
			m_Nodes.erase(it);
		}
	}
	bool contains(KNode *node) const {
		auto it = m_Nodes.find(node);
		return it != m_Nodes.end();
	}
	Co * get(KNode *node) {
		auto it = m_Nodes.find(node);
		if (it != m_Nodes.end()) {
			return it->second;
		}
		return nullptr;
	}
	int exportArray(std::vector<KNode*> &node_array) const {
		for (auto it=m_Nodes.begin(); it!=m_Nodes.end(); ++it ) {
			KNode *node = it->first;
			node_array.push_back(node);
		}
		return node_array.size();
	}
};










class KNodeManagerCallback {
public:
	virtual void on_node_marked_as_remove() {}
	virtual void on_node_remove_start() {}
	virtual void on_node_remove_done() {}
};

class KNodeTree {
public:
	static void install();
	static void uninstall();
	static bool isInstalled();

public:
	static void addCallback(KNodeManagerCallback *cb);
	static void removeCallback(KNodeManagerCallback *cb);
	static void destroyMarkedNodes(KNodeRemovingCallback *cb);
	static KNode * findNodeById(EID id);
	static KNode * findNodeByName(const KNode *start, const std::string &name);
	static KNode * findNodeByPath(const KNode *start, const std::string &path);
	static KNode * getRoot();
	static void setGroupName(KNode::Category category, int group, const std::string &name);
	static const char * getGroupName(KNode::Category category, int group);
	static void tick_nodes(KNodeTickFlags flags);
	static void tick_nodes2(KNodeTickFlags flags);
	static void tick_system_nodes();
	static void broadcastSignal(KSig &sig);
	static void sendSignal(KNode *target, KSig &sig);
	static void sendSignalDelay(KNode *target, KSig &sig, int delay);
	static int getNodeList(KNodeArray *out_nodes);
	static int getNodeListByTag(KNodeArray *out_nodes, const KName &tag); // タグが付いているノードを一括取得する
	static bool makeNodesByPath(const std::string &path, KNode **out_node, bool singleton=true);

	// Internal
	static void _add_tag(KNode *node, const KName &tag);
	static void _del_tag(KNode *node, const KName &tag);
	static void _on_node_enter(KNode *node);
	static void _on_node_exit(KNode *node);
	static void _on_node_marked_as_remove(KNode *node);
	static void _on_node_removing(KNode *node);
};



} // namespace

