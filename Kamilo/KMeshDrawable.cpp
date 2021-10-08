#include "KMeshDrawable.h"
#include "KDrawable.h"
#include "KImGui.h"
#include "KInternal.h"
#include "KRes.h"

namespace Kamilo {

KMeshDrawable::KMeshDrawable() {
	m_bound_grouping_rentex_w = 0;
	m_bound_grouping_rentex_h = 0;
	m_invert_order = false;
	m_color_write_enabled = true;
	m_stencil_enabled = false;
	m_stencil_func = KVideo::STENCILFUNC_NEVER;
	m_stencil_op = KVideo::STENCILOP_KEEP;
	m_stencil_ref = 0;
}

KMesh * KMeshDrawable::getMesh() {
	return &m_mesh;
}
const KMesh * KMeshDrawable::getMesh() const {
	return &m_mesh;
}
KSubMesh * KMeshDrawable::getSubMesh(int index) {
	return m_mesh.getSubMesh(index);
}
const KSubMesh * KMeshDrawable::getSubMesh(int index) const {
	return m_mesh.getSubMesh(index);
}
int KMeshDrawable::getSubMeshCount() const {
	return m_mesh.getSubMeshCount();
}
void KMeshDrawable::setMesh(const KMesh &mesh) {
	m_mesh = mesh;
}
void KMeshDrawable::onDrawable_inspector() {

	KDrawable::onDrawable_inspector();

#ifndef NO_IMGUI
	ImGui::Text("Mesh Renderer");
	{
		ImGui::Checkbox("Color Write", &m_color_write_enabled);
		ImGui::Checkbox("Stencil", &m_stencil_enabled);
		{
		KImGuiCombo combo;
		combo.begin();
		combo.addItem("NEVER"       , KVideo::STENCILFUNC_NEVER);
		combo.addItem("LESS"        , KVideo::STENCILFUNC_LESS);
		combo.addItem("EQUAL"       , KVideo::STENCILFUNC_EQUAL);
		combo.addItem("LESSEQUAL"   , KVideo::STENCILFUNC_LESSEQUAL);
		combo.addItem("GREATER"     , KVideo::STENCILFUNC_GREATER);
		combo.addItem("NOTEQUAL"    , KVideo::STENCILFUNC_NOTEQUAL);
		combo.addItem("GREATEREQUAL", KVideo::STENCILFUNC_GREATEREQUAL);
		combo.addItem("ALWAYS"      , KVideo::STENCILFUNC_ALWAYS);
		combo.end();
		combo.showGui("Stencil Func", (int*)&m_stencil_func);
		}
		{
		KImGuiCombo combo;
		combo.begin();
		combo.addItem("KEEP",    KVideo::STENCILOP_KEEP);
		combo.addItem("REPLACE", KVideo::STENCILOP_REPLACE);
		combo.addItem("INC",     KVideo::STENCILOP_INC);
		combo.addItem("DEC",     KVideo::STENCILOP_DEC);
		combo.end();
		combo.showGui("Stencil Op", (int*)&m_stencil_op);
		}
		ImGui::InputInt("Stencil Ref", &m_stencil_ref);
	}
	ImGui::Text("Num Vertices  %d", m_mesh.getVertexCount());
	ImGui::Text("Num Indices   %d", m_mesh.getIndexCount());
	ImGui::Text("Num Submeshes %d", m_mesh.getSubMeshCount());
	for (int i=0; i<m_mesh.getSubMeshCount(); i++) {
		ImGui::PushID(i);
		if (ImGui::TreeNode(KImGui::ID(0), "Submesh %d", i)) {
			KSubMesh *subMesh = m_mesh.getSubMesh(i);
			ImGui::Text("Start %d", subMesh->start);
			ImGui::Text("Count %d", subMesh->count);
			KBank::getVideoBank()->guiMaterialInfo(&subMesh->material);
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	ImGui::Separator();
	if (ImGui::TreeNode(u8"メッシュ")) {
		KBank::getVideoBank()->guiMeshInfo(&m_mesh);
		ImGui::TreePop();
	}
#endif // !NO_IMGUI
}
void KMeshDrawable::drawSubmesh(const KMesh *mesh, int submeshIndex, const KMatrix4 &projection_matrix, const KMatrix4 &transform_matrix, const KColor &mul, const KColor &add, KDrawList *drawlist) const {
	if (mesh == nullptr) return;

	const KSubMesh *subMesh = mesh->getSubMesh(submeshIndex);
	if (subMesh == nullptr) return;

	KMaterial dupMaterial = subMesh->material; // COPY

	// マスターカラーと合成する
	dupMaterial.color *= mul; // MUL
	dupMaterial.specular += add; // ADD

	// 描画
	if (drawlist) {
		if (!m_color_write_enabled) {
			drawlist->setColorWrite(m_color_write_enabled);
		}
		if (m_stencil_enabled) {
			drawlist->setStencil(true);
			drawlist->setStencilOpt(m_stencil_func, m_stencil_ref, m_stencil_op);
		}
		drawlist->setProjection(projection_matrix);
		drawlist->setTransform(subMesh->transform * transform_matrix);
		drawlist->setMaterial(dupMaterial);
		drawlist->addMesh(mesh, submeshIndex);
		if (m_stencil_enabled) {
			drawlist->setStencil(false);
		}
		if (!m_color_write_enabled) {
			drawlist->setColorWrite(true);
		}
	} else {
		KVideo::pushRenderState();
		KVideo::setProjection(projection_matrix);
		KVideo::setTransform(subMesh->transform * transform_matrix);
		KVideoUtils::beginMaterial(&dupMaterial);
		KVideoUtils::drawMesh(mesh, submeshIndex);
		KVideoUtils::endMaterial(&dupMaterial);
		KVideo::popRenderState();
	}
}
void KMeshDrawable::drawMesh(const KMatrix4 &projection_matrix, const KMatrix4 &transform_matrix, const KColor &mul, const KColor &add, KDrawList *drawlist) const {
	int num_submesh = m_mesh.getSubMeshCount();
	if (!m_invert_order) {
		// サブメッシュの番号が小さな方から順番に描画する。
		// （サブメッシュ[0]が一番最初＝一番奥に描画される）
		for (int i=0; i<num_submesh; i++) {
			drawSubmesh(&m_mesh, i, projection_matrix, transform_matrix, mul, add, drawlist);
		}

	} else {
		// サブメッシュの番号が大きなほうから順番に描画する。
		// （サブメッシュ[0]が一番最後＝一番手前に描画される）
		for (int i=num_submesh-1; i>=0; i--) {
			drawSubmesh(&m_mesh, i, projection_matrix, transform_matrix, mul, add, drawlist);
		}
	}
}
void KMeshDrawable::onDrawable_draw(KNode *node, const RenderArgs *opt, KDrawList *drawlist) {
	if (node == nullptr) return;
	if (opt == nullptr) return;

	// メッシュ未指定なら描画しない
	const KMesh *mesh = getMesh();
	if (mesh == nullptr) {
		return;
	}

	// サブメッシュ数
	int num_submesh = mesh->getSubMeshCount();
	if (num_submesh == 0) {
		return;
	}

	// 頂点数
	if (mesh->getVertexCount() == 0) {
		return;
	}

	// 動的な描画チェック
	if (opt->cb) {
		bool deny = false;
		opt->cb->on_render_query(node, opt, this, &deny);
		if (deny) {
			return;
		}
	}

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
		if (groupW > 0 && groupH > 0) {
			// 描画
			KMatrix4 my_projection_matrix = KMatrix4::fromOrtho((float)groupW, (float)groupH, (float)(-1000), (float)1000);
			KMatrix4 my_transform_matrix = group_transform;

			KVideo::pushRenderTarget(group_tex);
			KVideo::clearColor(KColor::ZERO);
			KVideo::clearDepth(1.0f);
			KVideo::clearStencil(0);

			if (1) {
				m_tmp_drawlist.clear();
				drawMesh(my_projection_matrix, my_transform_matrix, KColor::WHITE, KColor::ZERO, &m_tmp_drawlist);
				m_tmp_drawlist.endList();
				m_tmp_drawlist.draw();
			} else {
				drawMesh(my_projection_matrix, my_transform_matrix, KColor::WHITE, KColor::ZERO, nullptr);
			}

			KVideo::popRenderTarget();
		}
		// グループ化描画終わり。ここまでで group_tex テクスチャにはスプライトの絵が描画されている。
		// これを改めて描画する
		{
			// オフセット
			KVec3 off;
			{
				off += m_offset;
				off.x += -groupPiv.x; // レンダーターゲット内のピヴォットが、通常描画時の原点と一致するように調整する
				off.y +=  groupPiv.y - groupH;
			}

			// 変形行列
			KMatrix4 my_transform = KMatrix4::fromTranslation(off) * m_local_transform * opt->transform;

			// ピクセルパーフェクト
			//if (MO_VIDEO_ENABLE_HALF_PIXEL) {
				// 前提として、半ピクセルずらす前の座標は整数でないといけない
				if (opt->adj_snap && m_adj_snap) {
					KVideoUtils::translateFloorInPlace(my_transform);
				}
				if (opt->adj_half && m_adj_half) {
					KVideoUtils::translateHalfPixelInPlace(my_transform);
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
				drawlist->setTransform(my_transform);
				drawlist->setMaterial(mat);
				drawlist->addMesh(groupMesh, 0);
			} else {
				KVideo::pushRenderState();
				KVideo::setProjection(opt->projection);
				KVideo::setTransform(my_transform);
				KVideoUtils::beginMaterial(&mat);
				KVideoUtils::drawMesh(groupMesh, 0);
				KVideoUtils::endMaterial(&mat);
				KVideo::popRenderState();
			}
		}

	} else {
		// 変形行列
		KMatrix4 my_transform = KMatrix4::fromTranslation(m_offset) * m_local_transform * opt->transform;

		// ピクセルパーフェクト
		//if (MO_VIDEO_ENABLE_HALF_PIXEL) {
			// 前提として、半ピクセルずらす前の座標は整数でないといけない
			if (opt->adj_snap && m_adj_snap) {
				KVideoUtils::translateFloorInPlace(my_transform);
			}
			if (opt->adj_half && m_adj_half) {
				KVideoUtils::translateHalfPixelInPlace(my_transform);
			}
		//}

		drawMesh(opt->projection, my_transform, master_col, master_spe, drawlist);
	}
}
bool KMeshDrawable::onDrawable_getBoundingAabb(KVec3 *min_point, KVec3 *max_point) {
	KMesh *mesh = getMesh();
	KVec3 m0, m1;
	if (mesh && mesh->getAabb(&m0, &m1)) {
		if (min_point) *min_point = m0 + m_offset;
		if (max_point) *max_point = m1 + m_offset;
	}
	return false;
}
void KMeshDrawable::copy_mesh_state(const KMeshDrawable *co) {
	if (co == nullptr) return;
	m_mesh.copyFrom(&co->m_mesh);
	m_bound_grouping_rentex_w = co->m_bound_grouping_rentex_w;
	m_bound_grouping_rentex_h = co->m_bound_grouping_rentex_h;
	m_invert_order = co->m_invert_order;
}
void KMeshDrawable::onDrawable_getGroupImageSize(int *w, int *h, KVec3 *pivot) {
	KMesh *mesh = getMesh();

	int MARGIN = 2;

	/// メッシュが毎フレーム書き換わるような場合、ここで毎フレームことなるサイズを返すと、
	/// 毎フレームレンダーテクスチャを再生成することになってしまう。
	/// それを防ぐため、現在のレンダーテクスチャよりも大きなサイズが必要になった場合のみ値を更新する
	KVec3 minp, maxp;
	if (mesh && mesh->getAabb(&minp, &maxp)) {
		int aabb_w = (int)ceilf(maxp.x - minp.x);
		int aabb_h = (int)ceilf(maxp.y - minp.y);
		aabb_w += MARGIN*2; // margin
		aabb_h += MARGIN*2; // margin
		if (aabb_w > m_bound_grouping_rentex_w) {
			m_bound_grouping_rentex_w = aabb_w;
		}
		if (aabb_w > m_bound_grouping_rentex_h) {
			m_bound_grouping_rentex_h = aabb_h;
		}
	}
	if (w) *w = m_bound_grouping_rentex_w;
	if (h) *h = m_bound_grouping_rentex_h;
	
	if (pivot) {
		pivot->x = MARGIN - minp.x;
		pivot->y = MARGIN + maxp.y; // テクスチャ上端から基準点までの距離であることに注意。min.yではなくmax.yを使う
	}
}
void KMeshDrawable::attach(KNode *node) {
	if (node && !isAttached(node)) {
		KMeshDrawable *dw = new KMeshDrawable();
		KDrawable::_attach(node, dw);
		dw->drop();
	}
}
bool KMeshDrawable::isAttached(KNode *node) {
	return of(node) != nullptr;
}
KMeshDrawable * KMeshDrawable::of(KNode *node) {
	KDrawable *dw = KDrawable::of(node);
	return dynamic_cast<KMeshDrawable*>(dw ? dw : nullptr);
}

} // namespace
