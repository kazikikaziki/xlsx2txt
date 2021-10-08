#pragma once
#include "KRef.h"
#include "KVec.h"
#include "KColor.h"
#include "KString.h"

namespace Kamilo {


typedef int KHitboxGroupId;
class KHitPair;
class KHitboxGroup;
class KNode;

class KHitbox: public KRef {
public:
	static void install();
	static void uninstall();

	static void attach(KNode *node);
	static KHitbox * attach2(KNode *node);
	static bool isAttached(KNode *node);
	static KHitbox * of(KNode *node);
	static KHitbox * cast(KNode *node);

	static void setGroupCount(int count);
	static KHitboxGroup * getGroup(int index);

	static int getHitboxCount(KNode *node);
	static KHitbox * getHitboxByGroup(KNode *node, int groupindex);
	static KHitbox * getHitboxByIndex(KNode *node, int index);

	static void setDebugLineVisible(bool visible);
	static void setDebug_alwaysShowDebug(bool val);
	static bool getDebug_alwaysShowDebug();

	static int getIntersectHitboxes(KHitbox *hb, std::vector<KHitbox*> &out_list);
	static int getHitboxHitPairCount();
	static const KHitPair * getHitboxHitPairByIndex(int index);
	static const KHitPair * getHitboxHitPairOf(KNode *node1, KNode *node2);

public:
	KHitbox();
	void copyFrom(const KHitbox *source);

	void on_node_inspector();

	/// このヒットボックスの持ち主（親ノードとは限らない）
	KNode * getOwner();
	void _setNode(KNode *node);
	KNode * getNode();

	/// AABBの中心座標（エンティティからの相対座標）
	const KVec3 & getCenter() const;
	void setCenter(const KVec3 &p);

	/// AABBの広がり（AABB直方体の各辺の半分のサイズを指定。半径のようなもの）
	const KVec3 & getHalfSize() const;
	void setHalfSize(const KVec3 &s);

	/// この判定を有効にするかどうか
	bool getEnabled() const;
	void setEnabled(bool b);

	// このヒットボックスに関連付けるユーザー定義の整数値 slot には 0 以上 USERDATA_COUNT 未満の値を指定する
	void setUserData(int slot, intptr_t data);
	intptr_t getUserData(int slot) const;

	// ヒットボックスは１つのノードに対して複数アタッチできる。それぞれのヒットボックスを識別するためのユーザー定義のタグ
	void setUserTag(KName tag);
	KName getUserTag();
	bool hasUserTag(KName tag);

	/// コリジョンレイヤー
	int getGroupIndex() const;
	void setGroupIndex(int index);

	/// ヒットボックスの中心座標をワールド座標系で取得する
	KVec3 getCenterInWorld() const;

	bool getAabbInWorld(KVec3 *minpos, KVec3 *maxpos) const;

	/// ミュート設定する。この値が 0 の場合のみ、判定が有効になる
	void setMute(int value);
	int getMute() const;

	static const int USERDATA_COUNT = 8;
private:
	intptr_t m_UserData[USERDATA_COUNT];
	KVec3 m_HalfSize; // aabb half size
	KNode *m_Node;
	KName m_UserTag;
	int m_GroupIndex;
	int m_Mute; // 一時的に利用不可能にする (enabled が true だったとしても）
};

class KHitboxGroup {
public:
	KHitboxGroup();
	/// 全てのグループとの衝突を無効にする
	void clearMask();

	/// 全てのグループとの衝突を有効にする
	void setCollideWithAny();

	/// 指定グループとの衝突を有効にする
	void setCollideWith(int groupIndex);

	/// 指定グループとの衝突を無効にする
	void setDontCollideWith(int groupIndex);

	/// 指定されたグループとの衝突が有効かどうか
	bool canCollidableWith(int groupIndex) const;

	void setName(const char *n);

	void setNameForInspector(const char *n);

	void setGizmoColor(const KColor &color);

	int m_Bitmask; /// 判定対象のグループ番号目のビットを立てたマスク整数を指定する。つまり現在の時点では 32 グループまで対応可能である
	std::string m_Name; /// グループ名（内部管理のための名前
	std::string m_NameForInspector; /// グループ名（デバッガに表示する時のグループ名
	KColor m_GizmoColor; /// デバッグモードで衝突判定を描画する時の色
	bool m_GizmoVisible;
};

class KHitboxCallback {
public:
	/// ヒットボックス領域同士の判定時に呼ばれる。
	/// e1 の判定 box1 と、e2 の判定 box2 が衝突可能な組み合わせであれば true を返す。
	/// 判定を無視するなら deny に true をセットする
	/// deny の値を true にすると衝突判定を無視する
	virtual void on_hitbox_update(const KHitbox *hitbox1, const KHitbox *hitbox2, bool *deny) {}
};

class KHitPair {
public:
	struct Obj {
		Obj() {
			node = nullptr;
			hitbox = nullptr;
		}
		KNode *node; ///< エンティティ
		KHitbox *hitbox; ///< コライダー
		KVec3 hitbox_pos; ///< 衝突検出の瞬間における hitbox のワールド座標（衝突後の座標修正に影響されない）
	};

	KHitPair();

	/// 衝突した瞬間であれば true を返す
	bool isEnter() const;

	/// 離れた瞬間であれば true を返す
	bool isExit() const;

	/// 接触状態であれば true を返す
	bool isStay() const;

	/// 指定されたエンティティ同士の衝突ならば、
	/// e1 の衝突情報を hit1 に、e2 の衝突情報を hit2 にセットして true を返す。
	/// e1 または e2 に nullptr を指定した場合、それはエンティティが何でもよい事を示す。
	/// e1 も e2 も nullptr の場合は順不同で hit1, hit2 に衝突情報をセットして true を返す。この場合戻り値は必ず true になる。
	/// 衝突情報が不要な場合は hit1, hi2 に nullptr を指定できる
	bool isPairOf(const KNode *node1, const KNode *node2, Obj *hit1=nullptr, Obj *hit2=nullptr) const;

	/// 指定されたグループ同士の衝突ならば true を返す
	/// out_hitbox1  groupIndex1 に対応するヒットボックス
	/// out_hitbox2  groupIndex2 に対応するヒットボックス
	bool isPairOfGroup(int groupIndex1, int groupIndex2, KHitbox **out_hitbox1, KHitbox **out_hitbox2) const;

	const Obj * getObjectFor(const KNode *node) const;

	Obj m_Object1;       ///< 能動的判定側。攻撃側、イベント用ヒットボックス側など
	Obj m_Object2;       ///< 受動的判定側。ダメージ側、ヒットボックスに感知される物体など
	int m_TimestampEnter;      ///< 衝突開始時刻 (KCollisionManager::frame_counter_ の値)
	int m_TimestampExit;       ///< 衝突解消時刻。衝突が継続している場合は -1
	int m_TimestampLastUpdate; ///< 最終更新時刻
};




} // namespace
