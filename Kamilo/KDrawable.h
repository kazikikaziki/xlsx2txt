/// @file
/// Copyright (c) 2020 Heliodor
/// This software is released under the MIT License.
/// http://opensource.org/licenses/mit-license.php

#pragma once
#include "KRef.h"
#include "KMath.h"
#include "KVideo.h"
#include "keng_game.h"

namespace Kamilo {

class KRenderCallback;
class KDrawList;
class KNode;
class KEntityFilterCallback;









class KDrawable: public KRef {
public:
	enum EConfig {
		C_MASTER_ADJ_SNAP,
		C_MASTER_ADJ_HALF,
	};
	static void install();
	static void uninstall();
	static KDrawable * of(KNode *node);
	static bool isAttached(KNode *node) { return of(node) != nullptr; }
	static void _attach(KNode *node, KDrawable *drawable);
	static void setConfig(EConfig c, bool value);
	static bool getConfig(EConfig c);
	static void setCallback(KRenderCallback *cb);

public:
	struct RenderArgs {
		RenderArgs() {
			cb = NULL;
			adj_snap = false;
			adj_half = false;
			camera = NULL;
		}
		KMatrix4 projection;
		KMatrix4 transform;
		KRenderCallback *cb;
		KNode *camera;
		bool adj_snap;
		bool adj_half;
	};

	KDrawable();
	virtual ~KDrawable();
	virtual bool copyFrom(const KDrawable *co) = 0;

	/// カリングで利用する描画範囲AABB（ローカル座標系）を得る。
	/// aabb_min, aabb_max にはメッシュから計算済みのAABBがあらかじめ入っている。
	/// 「on_node_will_render でメッシュを更新する」という使い方をしたい場合、素直に実装すると期待通り動かない
	/// （メッシュを更新よりも前にAABBチェックするため）
	/// その場合は、onDrawable_getBoundingAabb をオーバーライドし、実際の AABB ではなく見積りサイズを返すようにする
	virtual void on_node_render_aabb(KVec3 *out_aabb_min, KVec3 *out_aabb_max);

	/// グループ化されているときに使う一時レンダーテクスチャのサイズを自動計算に設定してある場合、
	/// サイズ取得するために呼ばれる
	/// pivot 一時レンダーテクスチャ「を」描画する時に使う原点座標
	virtual void onDrawable_getGroupImageSize(int *w, int *h, KVec3 *pivot) = 0;

	/// 描画を実行する
	virtual void onDrawable_draw(KNode *node, const RenderArgs *args, KDrawList *drawlist) = 0;

	/// 描画オブジェクトのAABB
	/// 描画するべきものが存在しない場合は false を返す
	virtual bool onDrawable_getBoundingAabb(KVec3 *minpoint, KVec3 *maxpoint) = 0;

	virtual void onDrawable_register(int layer, std::vector<KNode*> &list);
	virtual void onDrawable_willDraw(const KNode *camera);
	virtual void onDrawable_inspector();

	void updateInspector();
	KNode * getNode();
	void _setNode(KNode *node);
	std::string getWarningString();

	/// グループ化を設定する。
	/// グループ化を有効にすると、一度内部レンダーテクスチャに描画してから
	/// その結果をあらためて描画するようになる。
	/// サブメッシュやスプライトレイヤーごとに個別のマテリアルを設定するのではなく、
	/// 描画後の画像に対してマテリアルを適用したい時に使う（アルファ設定など）
	/// ※ここでグループ化されるのは、他のエンティティの描画ではなく、このレンダーコンポーネント内で描画される複数のメッシュである
	///   複数のサブメッシュ、複数のレイヤーなど。
	void setGrouping(bool value);
	bool isGrouping() const;

	/// グループ化されている場合、一時レンダーテクスチャへの描画結果を
	/// 改めて出力するときに使うマテリアルを得る
	KMaterial * getGroupingMaterial();

	/// グループ化されている場合、レンダーテクスチャに描画するときの変形行列を指定する
	void setGroupingMatrix(const KMatrix4 &matrix);

	/// グループ化されているときに使う一時レンダーテクスチャのサイズを指定する。
	/// w, h どちらかに 0 に指定すると、自動的にサイズを計算する
	/// pivot メッシュの原点に対応する、一時レンダーテクスチャ上の座標（テクセル座標ではない）
	void setGroupingImageSize(int w, int h, const KVec3 &pivot);
	void getGroupingImageSize(int *w, int *h, KVec3 *pivot);

	KTEXID getGroupRenderTexture();
	void setGroupRenderTexture(KTEXID rentex);

	KMesh * getGroupRenderMesh();
	void setGroupRenderMesh(const KMesh &mesh);

	void updateGroupRenderTextureAndMesh();

	void setRenderOffset(const KVec3 &offset) {
		m_offset = offset;
	}
	const KVec3 & getRenderOffset() const {
		return m_offset;
	}
	const std::string & getGroupRenderTextureName() const {
		return m_group_rentex_name;
	}
	void setGroupRenderTextureName(const std::string &name) {
		m_group_rentex_name = name;
	}
	/// このレンダラーにだけ適用させる変形行列。
	/// 子の Transform には影響しない
	void setLocalTransform(const KMatrix4 &matrix) {
		m_local_transform = matrix;
	}
	const KMatrix4 & getLocalTransform() const {
		return m_local_transform;
	}

	void setAdjustSnapEnable(bool value) {
		m_adj_snap = value;
	}
	bool getAdjustSnapEnable() const {
		return m_adj_snap;
	}
	void setAdjustHalfEnable(bool value) {
		m_adj_half = value;
	}
	bool getAdjustHalfEnable() const {
		return m_adj_half;
	}
	void setRenderGroupImageAutoSizing(bool value) {
		m_group_rentex_autosize = value;
	}
	bool getRenderGroupImageAutoSizing() const {
		return m_group_rentex_autosize;
	}

protected:
	KMatrix4 m_local_transform; // このレンダラーにだけ適用させる変形行列。子の Transform には影響しない
	KVec3 m_offset; // 描画位置のオフセット
	bool m_adj_snap; // 座標を整数化する。ただし、微妙な伸縮アニメを伴う場合はアニメがガタガタになってしまうことがある。
	bool m_adj_half;
	bool m_group_rentex_autosize;

	KTEXID m_tmp_group_rentex;

	std::string m_group_rentex_name;
	KMaterial m_group_material;
	KMatrix4 m_group_matrix;
	KVec3 m_group_rentex_pivot;
	int m_group_rentex_w;
	int m_group_rentex_h;
	KMesh m_group_mesh;
	KNode *m_node;
	bool m_group_enabled;
};



class KRenderCallback {
public:
	/// ノードを描画する直前に呼ばれる。deny に true をセットしておくと描画をキャンセルできる
	virtual void on_render_query(KNode *node, const KDrawable::RenderArgs *opt, KDrawable *renderer, bool *deny) {}

	/// ノードを描画するための色を決定する最終段階で呼ばれる。
	/// color, specular を書き換えることにより e の描画で最終的に適用される色を変えることができる
	virtual void on_render_color(KNode *node, KColor *color, KColor *specular) {};

	/// ソート方法を KCamera::ORDER_CUSTOM にしたときに呼ばれる
	virtual void on_render_custom_sort(KNodeArray &nodes) {}
};




} // namespace
