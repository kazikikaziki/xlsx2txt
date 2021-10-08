/// @file
/// Copyright (c) 2020 Heliodor
/// This software is released under the MIT License.
/// http://opensource.org/licenses/mit-license.phpモ
#pragma once
#include "KDrawable.h"

namespace Kamilo {

class KNode;

class KMeshDrawable: public KDrawable {
public:
	static void attach(KNode *node);
	static bool isAttached(KNode *node);
	static KMeshDrawable * of(KNode *node);

public:
	KMeshDrawable();

	KMesh * getMesh();
	const KMesh * getMesh() const;
	void setMesh(const KMesh &mesh);

	KSubMesh * getSubMesh(int index);
	const KSubMesh * getSubMesh(int index) const;
	int getSubMeshCount() const;

	virtual void onDrawable_inspector() override;
	virtual void onDrawable_draw(KNode *node, const RenderArgs *opt, KDrawList *drawlist) override;
	virtual void onDrawable_getGroupImageSize(int *w, int *h, KVec3 *pivot) override;
	virtual bool onDrawable_getBoundingAabb(KVec3 *min_point, KVec3 *max_point) override;

	void drawMesh(const KMatrix4 &projection, const KMatrix4 &transform, const KColor &mul, const KColor &add, KDrawList *drawlist) const;
	void drawSubmesh(const KMesh *mesh, int index, const KMatrix4 &projection, const KMatrix4 &transform, const KColor &mul, const KColor &spe, KDrawList *drawlist) const;

	void copy_mesh_state(const KMeshDrawable *co);

	virtual bool copyFrom(const KDrawable *co) {
		// 描画設定をコピーする
		// （コピーするのはレンダラーの設定だけで、色などのノード付随情報はコピーしない）
		const KMeshDrawable *mco = dynamic_cast<const KMeshDrawable*>(co);
		if (mco) {
			copy_mesh_state(mco);
			return true;
		}
		return false;
	}
	KVideo::StencilFunc m_stencil_func;
	KVideo::StencilOp m_stencil_op;
	int m_stencil_ref;
	bool m_stencil_enabled;
	bool m_color_write_enabled;


	// 描画順を逆順にする
	// （末尾のサブメッシュを最初に描画し、先頭のサブメッシュを最後に描画する）
	bool m_invert_order;

private:
	KMesh m_mesh;
	mutable int m_bound_grouping_rentex_w;
	mutable int m_bound_grouping_rentex_h;
	KDrawList m_tmp_drawlist;
};

} // namespace
