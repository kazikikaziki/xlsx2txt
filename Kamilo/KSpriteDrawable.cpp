#include "KSpriteDrawable.h"

#include "KImGui.h"
#include "keng_game.h"

namespace Kamilo {

static const int MAX_SPRITE_LAYER_COUNT = 16;

static KPath _MakeUnpackedTexturePath(const KPath &sprite) {
	return KPath::fromFormat("%s.[UNPACKED].tex", sprite.u8());
}

KSpriteDrawable::KSpriteDrawable() {
	m_modifier = 0;
	m_gui_max_layers = 0;
	m_layer_count = 0;
	m_use_unpacked_texture = false;
}
void KSpriteDrawable::copy_sprite_state(const KSpriteDrawable *co) {
	if (co == nullptr) return;
	int numLayers = co->getLayerCount();

	// スプライトを全レイヤーコピーする
	setLayerCount(numLayers);
	for (int i=0; i<numLayers; i++) {
		setLayerSprite (i, co->m_sprite_layers[i].sprite);
		setLayerVisible(i, co->m_sprite_layers[i].visible);
		setLayerLabel  (i, co->m_sprite_layers[i].label);
		setLayerOffset (i, co->m_sprite_layers[i].offset);
		const KMaterial *mat_src = co->getLayerMaterial(i);
		if (mat_src) {
			KMaterial *mat_dst = getLayerMaterialAnimatable(i);
			*mat_dst = *mat_src;
		}
	}
	// のこりの未使用レイヤーを初期化しておく
	for (int i=numLayers; i<(int)m_sprite_layers.size(); i++) {
		m_sprite_layers[i] = Layer();
	}

	setModifier(co->getModifier());
	copyLayerVisibleFilters(co);
	m_group_matrix = co->m_group_matrix;
	m_local_transform = co->m_local_transform;
}
int KSpriteDrawable::getLayerCount() const {
	return m_layer_count;
}
int KSpriteDrawable::getLayerCapacity() const {
	return m_sprite_layers.size();
}

void KSpriteDrawable::setModifier(int modifier) {
	m_modifier = modifier;
}
int KSpriteDrawable::getModifier() const {
	return m_modifier;
}
// 指定されたインデックスのレイヤーが存在することを保証する
bool KSpriteDrawable::guaranteeLayerIndex(int index) {
	if (index < 0 || MAX_SPRITE_LAYER_COUNT <= index) {
		KLog::printError("E_INVALID_SPRITE_LAYER_INDEX: %d", index);
		return false;
	}

	// 一度確保したレイヤーは減らさない。
	// （アニメによってレイヤー枚数が変化する場合、レイヤーに割り当てられたマテリアルを
	// 毎回生成・削除しなくてもいいように維持してくため）
	const int n = (int)m_sprite_layers.size();
	if (n < index+1) {
		m_sprite_layers.resize(index+1);

		// 新しく増えた領域を初期化しておく
		for (int i=n; i<index+1; i++) {
			m_sprite_layers[i] = Layer();
		}
	}
	if (m_layer_count < index+1) {
		m_layer_count = index+1;
	}
	// 未使用レイヤーを初期化しておく
	for (int i=m_layer_count; i<(int)m_sprite_layers.size(); i++) {
		m_sprite_layers[i] = Layer();
	}
	return true;
}
void KSpriteDrawable::setUseUnpackedTexture(bool b) {
	m_use_unpacked_texture = b;
}
void KSpriteDrawable::setLayerCount(int count) {
	if (count > 0 && guaranteeLayerIndex(count - 1)) {
		m_layer_count = count;
	} else {
		m_layer_count = 0;
	}
}
bool KSpriteDrawable::getLayerAabb(int index, KVec3 *minpoint, KVec3 *maxpoint) {
	const KSpriteAuto sp = getLayerSprite(index);
	if (sp == nullptr) return false;
	KVec3 p0, p1;
	if (! sp->m_Mesh.getAabb(&p0, &p1)) return false;
	KVec3 sp_offset = sp->getRenderOffset();

	if (minpoint) *minpoint = sp_offset + m_offset+ p0;
	if (maxpoint) *maxpoint = sp_offset + m_offset+ p1;
	return true;
}
const KPath & KSpriteDrawable::getLayerSpritePath(int index) const {
	if (0 <= index && index < getLayerCount()) {
		return m_sprite_layers[index].sprite;
	} else {
		static const KPath s_def;
		return s_def;
	}
}
KTEXID KSpriteDrawable::getBakedSpriteTexture(int index) {
	if (0 <= index && index < getLayerCount()) {
		return KBank::getSpriteBank()->getBakedSpriteTexture(m_sprite_layers[index].sprite, m_modifier, false, KBank::getTextureBank());
	}
	return nullptr;
}
KSpriteAuto KSpriteDrawable::getLayerSprite(int index) {
	const KPath &path = getLayerSpritePath(index);
	if (! path.empty()) {
		bool should_be_found = true;
		return KBank::getSpriteBank()->findSprite(path, should_be_found);
	} else {
		return nullptr;
	}
}
void KSpriteDrawable::setLayerSprite(int index, const KPath &asset_path) {
	if (asset_path.empty()) {
		// 無色透明画像とみなす
	} else if (asset_path.extension().compare(".sprite") != 0) {
		KLog::printError("E_INVALID_FILEPATH_FOR_SPRITE: \"%s\" (NO FILE EXT .sprite)", asset_path.u8());
	}
	if (guaranteeLayerIndex(index)) {
		m_sprite_layers[index].sprite = asset_path;
	}
}
void KSpriteDrawable::setLayerLabel(int index, const KPath &label) {
	if (guaranteeLayerIndex(index)) {
		m_sprite_layers[index].label = label;
	}
}
const KPath & KSpriteDrawable::getLayerLabel(int index) const {
	if (0 <= index && index < getLayerCount()) {
		return m_sprite_layers[index].label;
	}
	return KPath::Empty;
}
void KSpriteDrawable::setLayerOffset(int index, const KVec3 &value) {
	if (guaranteeLayerIndex(index)) {
		m_sprite_layers[index].offset = value;
	}
}
KVec3 KSpriteDrawable::getLayerOffset(int index) const {
	if (0 <= index && index < getLayerCount()) {
		return m_sprite_layers[index].offset;
	}
	return KVec3();
}
void KSpriteDrawable::setLayerVisible(int index, bool value) {
	if (guaranteeLayerIndex(index)) {
		m_sprite_layers[index].visible = value;
	}
}
bool KSpriteDrawable::isLayerVisible(int index) const {
	if (0 <= index && index < getLayerCount()) {
		return m_sprite_layers[index].visible;
	}
	return false;
}
void KSpriteDrawable::clearLayerFilters() {
	m_sprite_filter_layer_labels.clear();
}
void KSpriteDrawable::copyLayerVisibleFilters(const KSpriteDrawable *src) {
	if (src == nullptr) return;
	m_sprite_filter_layer_labels = src->m_sprite_filter_layer_labels;
}
/// labelw ラベル名。空文字列の場合は、どのラベル名ともマッチしない場合に適用される
void KSpriteDrawable::setLayerVisibleFilter(const KPath &label, bool visible) {
	m_sprite_filter_layer_labels[label] = visible;
}
bool KSpriteDrawable::layerWillBeRendered(int index) {
	if (index < 0) return false;
	if (getLayerCount() <= index) return false;
	const Layer &L = m_sprite_layers[index];
	if (! L.visible) return false;
	if (L.sprite.empty()) return false;
	if (! m_sprite_filter_layer_labels.empty()) {
		// フィルターが存在する場合は、その設定に従う
		auto it = m_sprite_filter_layer_labels.find(L.label);
		if (it != m_sprite_filter_layer_labels.end()) {
			if (! it->second) return false; // フィルターによって非表示に設定されている
		} else {
			// フィルターに登録されていないレイヤーの場合は、空文字列 "" をキーとする設定を探し、これをデフォルト値とする
			if (! m_sprite_filter_layer_labels[""]) return false;
		}
	}
	return true;
}
/// アニメーション可能なマテリアルを得る
KMaterial * KSpriteDrawable::getLayerMaterialAnimatable(int index) {
	if (index < 0) {
		return nullptr;
	}
	if (index < (int)m_sprite_layers.size()) {
		return &m_sprite_layers[index].material;
	}
	m_sprite_layers.resize(index + 1);
	return &m_sprite_layers[index].material;
}
KMaterial * KSpriteDrawable::getLayerMaterial(int index, KMaterial *def) {
	if (0 <= index && index < getLayerCount()) {
		return &m_sprite_layers[index].material;
	}
	return def;
}
const KMaterial * KSpriteDrawable::getLayerMaterial(int index, const KMaterial *def) const {
	if (0 <= index && index < getLayerCount()) {
		return &m_sprite_layers[index].material;
	}
	return def;
}
void KSpriteDrawable::setLayerMaterial(int index, const KMaterial &material) {
	if (guaranteeLayerIndex(index)) {
		m_sprite_layers[index].material = material;
	}
}
KVec3 KSpriteDrawable::bitmapToLocalPoint(int index, const KVec3 &p) {
	// スプライト情報を取得
	KSpriteAuto sprite = getLayerSprite(index);
	if (sprite == nullptr) return KVec3();

	// スプライトのpivot
	KVec2 sprite_pivot = sprite->m_Pivot;

	// レイヤーのオフセット量
	KVec3 layer_offset = getLayerOffset(0);

	// 元画像サイズ
	int bmp_h = sprite->m_ImageH;

	// ビットマップ左下基準での座標（Ｙ上向き）
	KVec3 bmp_local(p.x, bmp_h - p.y, p.z);

	// エンティティローカル座標
	KVec3 local(
		layer_offset.x + bmp_local.x - sprite_pivot.x,
		layer_offset.y + bmp_local.y - sprite_pivot.y,
		layer_offset.z + bmp_local.z);
	
	return local;
}
KVec3 KSpriteDrawable::localToBitmapPoint(int index, const KVec3 &p) {
	// スプライト情報を取得
	KSpriteAuto sprite = getLayerSprite(index);
	if (sprite == nullptr) return KVec3();

	// スプライトのpivot
	KVec2 sprite_pivot = sprite->m_Pivot;

	// レイヤーのオフセット量
	KVec3 layer_offset = getLayerOffset(0);

	// 元画像サイズ
	int bmp_h = sprite->m_ImageH;

	// ビットマップ左下基準での座標（Ｙ上向き）
	KVec3 bmp_local(
		p.x - layer_offset.x + sprite_pivot.x,
		p.y - layer_offset.y + sprite_pivot.y,
		p.z - layer_offset.z);
		
	// ビットマップ左上基準での座標（Ｙ下向き）
	KVec3 pos(
		bmp_local.x, 
		bmp_h - bmp_local.y,
		bmp_local.z);

	return pos;
}
bool KSpriteDrawable::onDrawable_getBoundingAabb(KVec3 *min_point, KVec3 *max_point) {
	bool found = false;
	KVec3 m0, m1;
	int n = getLayerCount();
	for (int i=0; i<n; i++) {
		KVec3 off = getLayerOffset(i);
		KSpriteAuto sp = getLayerSprite(i);
		if (sp != nullptr) {
			KVec3 roff = sp->getRenderOffset();
			KVec3 p0, p1;
			if (sp->m_Mesh.getAabb(&p0, &p1)) {
				p0 += roff + off;
				p1 += roff + off;
				m0 = p0.getmin(m0);
				m1 = p1.getmax(m1);
				found = true;
			}
		}
	}
	if (found) {
		if (min_point) *min_point = m0 + m_offset;
		if (max_point) *max_point = m1 + m_offset;
		return true;
	}
	return false;
}
void KSpriteDrawable::onDrawable_getGroupImageSize(int *w, int *h, KVec3 *pivot) {
	// スプライトに使われる各レイヤー画像がすべて同じ大きさであると仮定する
	KSpriteAuto sp = getLayerSprite(0);
	if (sp != nullptr) {
		int img_w = sp->m_ImageW;
		int img_h = sp->m_ImageH;
		// スプライトサイズが奇数でない場合は、偶数に補正する。
		// グループ化描画する場合、レンダーターゲットが奇数だとドットバイドットでの描画が歪む
		if (img_w % 2 == 1) img_w = (int)ceilf(img_w / 2.0f) * 2; // img_w/2*2 はダメ。img_w==1で破綻する
		if (img_h % 2 == 1) img_h = (int)ceilf(img_h / 2.0f) * 2; // img_h/2*2 はダメ。img_h==1で破綻する
		if (w) *w = img_w;
		if (h) *h = img_h;
		if (pivot) {
			KVec2 piv2d = sp->m_Pivot;
			if (sp->m_PivotInPixels) {
				pivot->x = piv2d.x;
				pivot->y = piv2d.y;
				pivot->z = 0;
			} else {
				pivot->x = img_w * piv2d.x;
				pivot->y = img_h * piv2d.y;
				pivot->z = 0;
			}
		}
	}
}
// フィルターの設定を得る
bool KSpriteDrawable::isVisibleLayerLabel(const KPath &label) const {

	if (m_sprite_filter_layer_labels.empty()) {
		// レイヤー名によるフィルターが指定されていない。
		// 無条件で表示可能とする
		return true;
	}

	// フィルターが存在する場合は、その設定に従う
	{
		auto it = m_sprite_filter_layer_labels.find(label);
		if (it != m_sprite_filter_layer_labels.end()) {
			return it->second;
		}
	}

	// 指定されたレイヤー名がフィルターに登録されていなかった。
	// 空文字列 "" をキーとする設定がデフォルト値なので、その設定に従う
	{
		auto it = m_sprite_filter_layer_labels.find(KPath::Empty);
		if (it != m_sprite_filter_layer_labels.end()) {
			return it->second;
		}
	}

	// デフォルト値が指定されていない。
	// 無条件で表示可能とする
	return true;
}


// スプライトレイヤーとして描画される画像を、１枚のテクスチャに収める
// target 描画先レンダーテクスチャ
// transform 描画用変形行列。単位行列の場合、レンダーテクスチャ中央を原点とする、ピクセル単位の座標系になる（Ｙ軸上向き）
void KSpriteDrawable::unpackInTexture(KTEXID targetid, const KMatrix4 &transform, const KMesh &mesh, KTEXID texid) {
	KTexture *targettex = KVideo::findTexture(targetid);
	int texw = targettex->getWidth();
	int texh = targettex->getHeight();

	// レンダーターゲットのサイズが奇数になっていると、ドットバイドットでの描画が歪む
	K__ASSERT(texw>0 && texw%2==0);
	K__ASSERT(texh>0 && texh%2==0);

	const KMatrix4 group_projection = KMatrix4::fromOrtho((float)texw, (float)texh, (float)(-1000), (float)1000);
	const KMatrix4 group_transform = transform;
	
	KVideo::pushRenderTarget(targettex->getId());
	KVideo::clearColor(KColor::ZERO);
	KVideo::clearDepth(1.0f);
	KVideo::clearStencil(0);
	{
		KMatrix4 final_transform = transform; // Copy

		// ピクセルパーフェクト
		//if (MO_VIDEO_ENABLE_HALF_PIXEL) {
			// 前提として、半ピクセルずらす前の座標は整数でないといけない
			if (KDrawable::getConfig(KDrawable::C_MASTER_ADJ_SNAP) && m_adj_snap) {
				KVideoUtils::translateFloorInPlace(final_transform);
			}
			if (KDrawable::getConfig(KDrawable::C_MASTER_ADJ_HALF) && m_adj_half) {
				KVideoUtils::translateHalfPixelInPlace(final_transform);
			}
		//}

		// マテリアル
		// テクスチャの内容をそのまま転送するだけ
		KMaterial mat;
		mat.blend = KBlend_ONE;
		mat.color = KColor32::WHITE;
		mat.specular = KColor32::ZERO;
		mat.texture = texid;

		// 描画
		{
			KVideo::pushRenderState();
			KVideo::setProjection(group_projection);
			KVideo::setTransform(final_transform);
			KVideoUtils::beginMaterial(&mat);
			KVideoUtils::drawMesh(&mesh, 0/*submesh_index*/);
			KVideoUtils::endMaterial(&mat);
			KVideo::popRenderState();
		}
	}
	KVideo::popRenderTarget();
}

void KSpriteDrawable::drawInTexture(const RenderLayerDesc *layers, int num_layers, KTEXID target, int w, int h, const RenderArgs *opt) const {
	if (w <= 0 || h <= 0 || num_layers <= 0 || layers==nullptr) {
		// 描画するものがない。
		// グループ用のレンダーテクスチャをクリアするだけ
		// target->fill(); <-- これでは色しかクリアできない
		KVideo::pushRenderTarget(target);
		KVideo::clearColor(KColor::ZERO);
		KVideo::clearDepth(1.0f);
		KVideo::clearStencil(0);
		KVideo::popRenderTarget();
		return;
	}

	if (w <= 0) return;
	if (h <= 0) return;
	if (num_layers <= 0) return;
	if (layers == nullptr) return;

	// レンダーターゲットのサイズが奇数になっていると、ドットバイドットでの描画が歪む
	K__ASSERT(w>0 && w%2==0);
	K__ASSERT(h>0 && h%2==0);

	const KMatrix4 group_projection_matrix = KMatrix4::fromOrtho((float)w, (float)h, (float)(-1000), (float)1000);
	const KMatrix4 group_transform_matrix = opt->transform;
	
	KVideo::pushRenderTarget(target);
	KVideo::clearColor(KColor::ZERO);
	KVideo::clearDepth(1.0f);
	KVideo::clearStencil(0);
	for (int i=0; i<num_layers; i++) {
		const RenderLayerDesc &layer = layers[i];
		KMatrix4 transform_matrix = group_transform_matrix; // copy

		// オフセット
		// いちどレンダーターゲットを経由するので、どちらの描画時に適用するオフセットなのか注意すること
		if (1) {
			transform_matrix = KMatrix4::fromTranslation(layer.offset) * transform_matrix;
		}

		// ピクセルパーフェクト
		//if (MO_VIDEO_ENABLE_HALF_PIXEL) {
			// 前提として、半ピクセルずらす前の座標は整数でないといけない
			if (opt->adj_snap && m_adj_snap) {
				KVideoUtils::translateFloorInPlace(transform_matrix);
			}
			if (opt->adj_half && m_adj_half) {
				KVideoUtils::translateHalfPixelInPlace(transform_matrix);
			}
		//}

		// 描画
		{
			KVideo::pushRenderState();
			KVideo::setProjection(group_projection_matrix);
			KVideo::setTransform(transform_matrix);
			KVideoUtils::beginMaterial(&layer.material);
			KVideoUtils::drawMesh(&layer.mesh, 0/*submesh_index*/);
			KVideoUtils::endMaterial(&layer.material);
			KVideo::popRenderState();
		}
	}
	KVideo::popRenderTarget();
}
// modifier によるテクスチャの動的生成は描画時に行われるが、それを事前に生成しておく
void KSpriteDrawable::preparateModifiedTexture(KNode *node, const char *spritename) {
	// テクスチャを取得する過程で、動的テクスチャを処理済みにしておく。
	// modifier の値に応じたテクスチャの動的生成については KTextureBankCallback::on_videobank_modifier を参照せよ
	KSpriteAuto sprite = KBank::getSpriteBank()->findSprite(spritename, true);

	// getTextureEx を経由して on_videobank_modifier を呼ぶことが目的なので、戻り値は使わない
	KBank::getTextureBank()->getTextureEx(sprite->m_TextureName, m_modifier, true, node);
}

void KSpriteDrawable::getRenderLayers(KNode *node, std::vector<RenderLayerDesc> &out_render_layers) {
	// レイヤーを番号の大きなほうから順番に描画する
	// なお、sprite_layers は実際に描画する枚数よりも余分に確保されている場合があるため、
	// スプライトレイヤー枚数の取得に m_sprite_layers.size() を使ってはいけない。
	for (int i=m_layer_count-1; i>=0; i--) {
		const Layer &spritelayer = m_sprite_layers[i];

		// 非表示レイヤーを除外
		if (!spritelayer.visible) {
			continue;
		}
		// スプライト未指定なら除外
		if (spritelayer.sprite.empty()) {
			continue;
		}
		// レイヤー名による表示設定を確認
		if (! isVisibleLayerLabel(spritelayer.label)) {
			continue;
		}
		// スプライトを取得
		KSpriteAuto sprite = KBank::getSpriteBank()->findSprite(spritelayer.sprite, true);
		if (sprite == nullptr) {
			continue;
		}

		// 描画に使うテクスチャとメッシュを取得
		m_tmp_renderlayer.clear();
		preparateMeshAndTextureForSprite(node, sprite, spritelayer.sprite, m_modifier, &m_tmp_renderlayer.texid, &m_tmp_renderlayer.mesh);

		if (m_tmp_renderlayer.mesh.getVertexCount() == 0) {
			continue; // メッシュなし
		}

		if (m_use_unpacked_texture) {
			// レイヤーごとにレンダーターゲットに展開する
			KPath tex_path = _MakeUnpackedTexturePath(spritelayer.sprite);
			KTEXID target_tex = KBank::getTextureBank()->findTextureRaw(tex_path, false);

			// レンダーテクスチャーを取得
			int texW = 0;
			int texH = 0;
			getGroupingImageSize(&texW, &texH, nullptr);
			// レンダーテクスチャーにレイヤー内容を書き出す
			if (texW > 0 && texH > 0) {
				if (target_tex == nullptr) {
					target_tex = KBank::getTextureBank()->addRenderTexture(tex_path, texW, texH, KTextureBank::F_OVERWRITE_SIZE_NOT_MATCHED);
				}

				// スプライトのメッシュはY軸上向きに合わせてある。
				// レンダーテクスチャの左下を原点にする
				KVec3 pos;
				pos.x -= texW / 2;
				pos.y -= texH / 2;
				KMatrix4 transform = KMatrix4::fromTranslation(pos); // 描画位置
				unpackInTexture(target_tex, transform, m_tmp_renderlayer.mesh, m_tmp_renderlayer.texid);

				// レンダーテクスチャをスプライトとして描画するためのメッシュを作成する
				KMesh newmesh;
				int w = sprite->m_ImageW;
				int h = sprite->m_ImageH;
				float u0 = 0.0f;
				float u1 = (float)sprite->m_ImageW / texW;
				float v0 = (float)(texH-sprite->m_ImageH) / texH;
				float v1 = 1.0f;
				MeshShape::makeRect(&newmesh, KVec2(0, 0), KVec2(w, h), KVec2(u0, v0), KVec2(u1, v1), KColor::WHITE);

				// 得られたレンダーターゲットをスプライトとして描画する
				m_tmp_renderlayer.mesh = newmesh; //<--新しいメッシュに変更
				m_tmp_renderlayer.texid = target_tex; //<-- レイヤーを描画したレンダーテクスチャに変更
				m_tmp_renderlayer.offset = spritelayer.offset + sprite->getRenderOffset();
				m_tmp_renderlayer.index = i;
				m_tmp_renderlayer.material = spritelayer.material; // COPY
				m_tmp_renderlayer.material.texture = m_tmp_renderlayer.texid;
				out_render_layers.push_back(m_tmp_renderlayer);
			}

		} else {
			// 展開なし。描画と同時に展開する
			m_tmp_renderlayer.offset = spritelayer.offset + sprite->getRenderOffset();
			m_tmp_renderlayer.index = i;
			m_tmp_renderlayer.material = spritelayer.material; // COPY
			m_tmp_renderlayer.material.texture = m_tmp_renderlayer.texid;
			out_render_layers.push_back(m_tmp_renderlayer);
		}
	}
}
void KSpriteDrawable::onDrawable_draw(KNode *node, const RenderArgs *opt, KDrawList *drawlist) {
	if (node == nullptr) return;
	if (opt == nullptr) return;

	// 動的な描画チェック
	if (opt->cb) {
		bool deny = false;
		opt->cb->on_render_query(node, opt, this, &deny);
		if (deny) {
			return;
		}
	}

	// 実際に描画するべきレイヤーを、描画するべき順番で取得する
	m_render_layers.clear();
	getRenderLayers(node, m_render_layers);

	// マスターカラー
	KColor master_col = node->getColorInTree();
	KColor master_spe = node->getSpecularInTree();

	// 描画色決定への介入
	if (opt->cb) {
		opt->cb->on_render_color(node, &master_col, &master_spe);
	}

	if (isGrouping()) {
		// グループ化する。
		// 一旦別のレンダーテクスチャに書き出し、それをスプライトとして描画する
		int groupW = 0;
		int groupH = 0;
		KVec3 groupPiv;
		getGroupingImageSize(&groupW, &groupH, &groupPiv);
		KTEXID group_tex = getGroupRenderTexture();

		// groupPivはビットマップ原点（＝レンダーターゲット画像左上）を基準にしていることに注意
		KVec3 pos;
		pos.x -= groupW / 2; // レンダーテクスチャ左上へ
		pos.y += groupH / 2;
		pos.x += groupPiv.x; // さらに groupPivずらす
		pos.y -= groupPiv.y;
		KMatrix4 group_transform = KMatrix4::fromTranslation(pos) * m_group_matrix; // 描画位置

		// 描画
		RenderArgs gopt = *opt;
		gopt.transform = group_transform;
		drawInTexture(m_render_layers.data(), m_render_layers.size(), group_tex, groupW, groupH, &gopt);
		// グループ化描画終わり。ここまでで render_target テクスチャにはスプライトの絵が描画されている。
		// これを改めて描画する
		{
			// オフセット
			KVec3 off;
			{
				off += m_offset; // 一括オフセット。すべてのレイヤーが影響を受ける
			//	off += layer.offset; // レイヤーごとに指定されたオフセット
			//	off += sprite->offset(); // スプライトに指定された Pivot 座標によるオフセット
				off.x += -groupPiv.x; // レンダーターゲット内のピヴォットが、通常描画時の原点と一致するように調整する
				off.y +=  groupPiv.y - groupH;
			}

			// 変形行列
			KMatrix4 my_transform_matrix = KMatrix4::fromTranslation(off) * m_local_transform * opt->transform;

			// ピクセルパーフェクト
			//if (MO_VIDEO_ENABLE_HALF_PIXEL) {
				// 前提として、半ピクセルずらす前の座標は整数でないといけない
				if (opt->adj_snap && m_adj_snap) {
					KVideoUtils::translateFloorInPlace(my_transform_matrix);
				}
				if (opt->adj_half && m_adj_half) {
					KVideoUtils::translateHalfPixelInPlace(my_transform_matrix);
				}
			//}

			// マテリアル
			KMaterial *p_mat = getGroupingMaterial();
			K__ASSERT(p_mat);
			KMaterial mat = *p_mat; // COPY
			mat.texture = group_tex;

			// マスターカラーと合成する
			mat.color    *= master_col; // MUL
			mat.specular += master_spe; // ADD

			// 描画
			KMesh *groupMesh = getGroupRenderMesh();
			if (drawlist) {
				drawlist->setProjection(opt->projection);
				drawlist->setTransform(my_transform_matrix);
				drawlist->setMaterial(mat);
				drawlist->addMesh(groupMesh, 0);
			} else {
				KVideo::pushRenderState();
				KVideo::setProjection(opt->projection);
				KVideo::setTransform(my_transform_matrix);
				KVideoUtils::beginMaterial(&mat);
				KVideoUtils::drawMesh(groupMesh, 0);
				KVideoUtils::endMaterial(&mat);
				KVideo::popRenderState();
			}
		}
		return;
	}
	
	// グループ化しない。
	// 通常描画する
	for (size_t i=0; i<m_render_layers.size(); i++) {
		const RenderLayerDesc &renderlayer = m_render_layers[i];

		KMatrix4 my_transform_matrix = opt->transform; // Copy

		// 画像用の変形を適用
		my_transform_matrix = m_local_transform * my_transform_matrix;

		// オフセット
		{
			KVec3 off;
			off += m_offset; // 一括オフセット。すべてのレイヤーが影響を受ける
			off += renderlayer.offset; // レイヤーごとのオフセット
			my_transform_matrix = KMatrix4::fromTranslation(off) * my_transform_matrix;
		}

		// ピクセルパーフェクト
		//if (MO_VIDEO_ENABLE_HALF_PIXEL) {
			// 前提として、半ピクセルずらす前の座標は整数でないといけない
			if (opt->adj_snap && m_adj_snap) {
				KVideoUtils::translateFloorInPlace(my_transform_matrix);
			}
			if (opt->adj_half && m_adj_half) {
				KVideoUtils::translateHalfPixelInPlace(my_transform_matrix);
			}
		//}

		// マテリアル
		KMaterial *p_mat = getLayerMaterialAnimatable(renderlayer.index);
		K__ASSERT(p_mat);
		KMaterial mat = *p_mat; // COPY
		mat.texture = renderlayer.texid;

		// マスターカラーと合成する
		mat.color    *= master_col; // MUL
		mat.specular += master_spe; // ADD

		// 描画
		if (drawlist) {
			drawlist->setProjection(opt->projection);
			drawlist->setTransform(my_transform_matrix);
			drawlist->setMaterial(mat);
			drawlist->addMesh(&renderlayer.mesh, 0);
		} else {
			KVideo::pushRenderState();
			KVideo::setProjection(opt->projection);
			KVideo::setTransform(my_transform_matrix);
			KVideoUtils::beginMaterial(&mat);
			KVideoUtils::drawMesh(&renderlayer.mesh, 0/*submesh_index*/);
			KVideoUtils::endMaterial(&mat);
			KVideo::popRenderState();
		}
	}
}
bool KSpriteDrawable::getLayerMeshAndTexture(KNode *node, int index, KTEXID *tex, KMesh *mesh) {
	if (index < 0 || getLayerCount() <= index) {
		return false;
	}
	const Layer &spritelayer = m_sprite_layers[index];

	// 非表示レイヤーを除外
	if (!spritelayer.visible) {
		return false;
	}
	// スプライト未指定なら除外
	if (spritelayer.sprite.empty()) {
		return false;
	}
	// レイヤー名による表示設定を確認
	if (! isVisibleLayerLabel(spritelayer.label)) {
		return false;
	}
	// スプライトを取得
	KSpriteAuto sprite = KBank::getSpriteBank()->findSprite(spritelayer.sprite, true);
	if (sprite == nullptr) {
		return false;
	}

	// 描画に使うテクスチャとメッシュを取得
	if (!preparateMeshAndTextureForSprite(node, sprite, spritelayer.sprite, m_modifier, tex, mesh)) {
		return false;
	}

	return true;
}
bool KSpriteDrawable::preparateMeshAndTextureForSprite(KNode *node, const KSpriteAuto &sprite, const KPath &sprite_name, int modifier, KTEXID *tex, KMesh *mesh) {
	if (node == nullptr) return false;
	if (sprite == nullptr) return false;

	// テクスチャを取得する。
	// modifier の値に応じたテクスチャの動的生成については KTextureBankCallback::on_videobank_modifier を参照せよ
	KTEXID actual_tex = KBank::getTextureBank()->getTextureEx(sprite->m_TextureName, modifier, true, node);

	if (tex) *tex = actual_tex;
	if (mesh) *mesh = sprite->m_Mesh;
	return true;
}
void KSpriteDrawable::onDrawable_inspector() {

	KDrawable::onDrawable_inspector();

#ifndef NO_IMGUI
	// 同一クリップ内でレイヤー枚数が変化する場合、
	// インスペクターの表示レイヤーがフレーム単位で増えたり減ったりして安定しない。
	// 見やすくするために、これまでの最大レイヤー数の表示領域を確保しておく
	m_gui_max_layers = KMath::max(m_gui_max_layers, getLayerCount());
	for (int i=0; i<m_gui_max_layers; i++) {
		ImGui::PushID(0);
		if (i < getLayerCount()) {
			bool disable = !isLayerVisible(i);

			if (disable) {
				KImGui::PushTextColor(KImGui::COLOR_DISABLED());
			}
			if (ImGui::TreeNode(KImGui::ID(i), "Layer[%d]: %s", i, m_sprite_layers[i].sprite.u8())) {
				ImGui::Text("Label: %s", m_sprite_layers[i].label.u8());
				KBank::getSpriteBank()->guiSpriteSelector("Sprite", &m_sprite_layers[i].sprite);

				ImGui::Text("Will be rendered: %s", layerWillBeRendered(i) ? "Yes" : "No");
				bool b = isLayerVisible(i);
				if (ImGui::Checkbox("Visible", &b)) {
					setLayerVisible(i, b);
				}
				KVec3 p = getLayerOffset(i);
				if (ImGui::DragFloat3("Offset", (float*)&p, 1)) {
					setLayerOffset(i, p);
				}
				if (ImGui::TreeNode(KImGui::ID(0), "Sprite: %s", m_sprite_layers[i].sprite.u8())) {
					KBank::getSpriteBank()->guiSpriteInfo(m_sprite_layers[i].sprite, KBank::getTextureBank());
					ImGui::TreePop();
				}
				if (ImGui::TreeNode(KImGui::ID(1), "Material")) {
					KMaterial *mat = getLayerMaterial(i);
					KBank::getVideoBank()->guiMaterialInfo(mat);
					ImGui::TreePop();
				}
				ImGui::TreePop();
			}
			if (disable) {
				KImGui::PopTextColor();
			}
		} else {
			KImGui::PushTextColor(KImGui::COLOR_DISABLED());
			if (ImGui::TreeNode(KImGui::ID(i), "Layer[%d]: (none)", i)) {
				ImGui::TreePop();
			}
			KImGui::PopTextColor();
		}
		ImGui::PopID();
	}
	if (ImGui::TreeNode(KImGui::ID(-1), "Layer filters")) {
		for (auto it=m_sprite_filter_layer_labels.begin(); it!=m_sprite_filter_layer_labels.end(); ++it) {
			if (it->first.empty()) {
				ImGui::Checkbox("(Default)", &it->second);
			} else {
				ImGui::Checkbox(it->first.u8(), &it->second);
			}
		}
		ImGui::TreePop();
	}
	ImGui::Checkbox("Use unpacked texture", &m_use_unpacked_texture);
/*
	ImGui::Checkbox(u8"Unpack/アンパック", &m_use_unpacked_texture);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
			u8"スプライトとして表示するテクスチャーがブロック圧縮されている場合、\n"
			u8"既定の設定では描画と展開を同時に行いますが、この項目にチェックを付けた場合は、\n"
			u8"一度レンダーテクスチャーに画像を展開してから描画します。\n"
			u8"シェーダーやUVアニメが正しく反映されない場合、ここにチェックを入れると改善される場合があります\n"
			u8"ブロック圧縮されていないテクスチャーを利用している場合、このオプションは無視されます"
		);
	}
*/
#endif // !NO_IMGUI
}

void KSpriteDrawable::attach(KNode *node) {
	if (node && !isAttached(node)) {
		KSpriteDrawable *dw = new KSpriteDrawable();
		KDrawable::_attach(node, dw);
		dw->drop();
	}
}
bool KSpriteDrawable::isAttached(KNode *node) {
	return of(node) != nullptr;
}
KSpriteDrawable * KSpriteDrawable::of(KNode *node) {
	KDrawable *dw = KDrawable::of(node);
	return dynamic_cast<KSpriteDrawable*>(dw ? dw : nullptr);
}


} // namespace
