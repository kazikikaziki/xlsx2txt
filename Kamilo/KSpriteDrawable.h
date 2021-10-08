/// @file
/// Copyright (c) 2020 Heliodor
/// This software is released under the MIT License.
/// http://opensource.org/licenses/mit-license.php

#pragma once
#include "KDrawable.h"
#include "KRes.h"

namespace Kamilo {

class KNode;

class KSpriteDrawable: public KDrawable {
public:
	static void attach(KNode *node);
	static bool isAttached(KNode *node);
	static KSpriteDrawable * of(KNode *node);

public:
	struct Layer {
		Layer() {
			visible = true;
		}
		KPath sprite;
		KMaterial material;
		KPath label;
		KVec3 offset;
		bool visible;
	};

	KSpriteDrawable();

	void copy_sprite_state(const KSpriteDrawable *co);

	virtual bool copyFrom(const KDrawable *co) {
		// 描画設定をコピーする
		// （コピーするのはレンダラーの設定だけで、色などのノード付随情報はコピーしない）
		const KSpriteDrawable *mco = dynamic_cast<const KSpriteDrawable*>(co);
		if (mco) {
			copy_sprite_state(mco);
			return true;
		}
		return false;
	}


	void setLayerCount(int count);
	int  getLayerCount() const;
	int  getLayerCapacity() const;
	bool getLayerAabb(int index, KVec3 *minpoint, KVec3 *maxpoint);
	void setLayerLabel(int index, const KPath &label);
	const KPath & getLayerLabel(int index) const;
	void setLayerOffset(int index, const KVec3 &value);
	KVec3 getLayerOffset(int index) const;
	const KPath & getLayerSpritePath(int index) const;
	KSpriteAuto getLayerSprite(int index=0);
	void setLayerSprite(int index, const KPath &asset_path);
	void setLayerVisible(int index, bool value);
	bool isLayerVisible(int index) const;
	void clearLayerFilters();
	void setLayerVisibleFilter(const KPath &label, bool visible);
	void copyLayerVisibleFilters(const KSpriteDrawable *src);
	bool layerWillBeRendered(int index);
	KMaterial * getLayerMaterial(int index=0, KMaterial *def=NULL);
	const KMaterial * getLayerMaterial(int index=0, const KMaterial *def=NULL) const;
	KMaterial * getLayerMaterialAnimatable(int index);
	void setLayerMaterial(int index, const KMaterial &material);
	bool getLayerMeshAndTexture(KNode *node, int index, KTEXID *tex, KMesh *mesh);
	KTEXID getBakedSpriteTexture(int layer);

	/// 指定されたインデクスのスプライトの元画像ＢＭＰでの座標を、ローカル座標に変換する（テクスチャ座標ではない）
	/// 例えばスプライトが player.png から作成されているなら、player.png 画像内の座標に対応するエンティティローカル座標を返す。
	///
	/// png などから直接生成したスプライト、アトラスにより切り取ったスプライト、それらをブロック化圧縮したスプライトに対応する。
	/// それ以外の方法で作成されたスプライトでは正しく計算できない可能性がある。
	///
	/// index スプライト番号
	/// bmp_x, bmp_y スプライトの作成元ビットマップ上の座標。原点は画像左上でＹ軸下向き。ピクセル単位。
	///
	KVec3 bitmapToLocalPoint(int index, const KVec3 &p);
	KVec3 localToBitmapPoint(int index, const KVec3 &p);

	virtual void onDrawable_inspector() override;
	virtual bool onDrawable_getBoundingAabb(KVec3 *min_point, KVec3 *max_point) override;
	virtual void onDrawable_getGroupImageSize(int *w, int *h, KVec3 *pivot) override;
	virtual void onDrawable_draw(KNode *node, const RenderArgs *opt, KDrawList *drawlist) override;

	void unpackInTexture(KTEXID target, const KMatrix4 &transform, const KMesh &mesh, KTEXID texid);

	/// スプライト sprite を、この KSpriteDrawable の設定で描画するときに必要になるテクスチャやメッシュを準備する。
	/// 新しいアセットが必要なら構築し、アセットが未ロード状態ならばロードする。
	/// テクスチャアトラス、ブロック化圧縮、パレットの有無などが影響する
	///
	/// tex: NULL でない値を指定した場合、スプライトを描画するために実際に使用するテクスチャを取得できる
	/// mesh: NULL でない値をを指定した場合、スプライトを描画するために実際に使用するメッシュを取得できる
	///
	bool preparateMeshAndTextureForSprite(KNode *node, const KSpriteAuto &sprite, const KPath &sprite_name, int modifier, KTEXID *tex, KMesh *mesh);

	/// modifier によるテクスチャの動的生成は描画時に行われるが、それを事前に生成しておく
	void preparateModifiedTexture(KNode *node, const char *spritename);


	/// インデックス index のレイヤーが存在することを保証する
	/// 存在しなければ index+1 枚のレイヤーになるようレイヤーを追加する。
	/// 存在する場合は何もしない
	/// インデックス index が利用可能な状態になったら true を返す。
	/// 不正な index が指定された場合は false を返す
	bool guaranteeLayerIndex(int index);

	/// スプライトで使用するテクスチャに対する加工番号 modifier を指定する。
	/// ここで 0 以外の値を指定しておくと、スプライト描画のためのテクスチャを取得する際に
	/// KTextureBank::findTexture を経由して KTextureBankCallback::on_videobank_modifier が呼ばれるようになる。
	/// modifier の番号に応じて何かの加工を行いたい場合は、この KTextureBankCallback::on_videobank_modifier を実装する。
	void setModifier(int index);
	int getModifier() const;

	/// ブロック化テクスチャーを使っている場合、復元したテクスチャーに対してマテリアルを適用するかどうか
	/// false の場合、画像復元とマテリアル適用を同時に行うため、正しく描画されない場合がある。
	/// true の場合、いったん元画像を復元してからマテリアルを適用する。
	/// 特に、処理対象外のピクセルを参照するようなエフェクトを使う場合は、復元されたテクスチャーに対して適用しないといけない
	void setUseUnpackedTexture(bool b);

	bool isVisibleLayerLabel(const KPath &label) const;

private:
	struct RenderLayerDesc {
		RenderLayerDesc() {
			texid = NULL;
			index = 0;
		}
		void clear() {
			texid = NULL;
			index = 0;
			mesh.clear();
			material.clear();
		}
		int index;
		KVec3 offset;
		KMesh mesh;
		KTEXID texid;
		KMaterial material;
	};
	void drawInTexture(const RenderLayerDesc *nodes, int num_nodes, KTEXID target, int w, int h, const RenderArgs *opt) const;
	void getRenderLayers(KNode *node, std::vector<RenderLayerDesc> &out_render_layers);
	std::unordered_map<KPath, bool> m_sprite_filter_layer_labels;
	std::vector<Layer> m_sprite_layers;
	std::vector<RenderLayerDesc> m_render_layers;
	RenderLayerDesc m_tmp_renderlayer;
	int m_layer_count;
	int m_modifier;
	int m_gui_max_layers; // インスペクターのための変数
	bool m_use_unpacked_texture;
};

} // namespace
