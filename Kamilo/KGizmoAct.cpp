#include "KGizmoAct.h"
#include "KInternal.h"
#include "KMeshDrawable.h"

namespace Kamilo {

static KCompNodes<KGizmoAct> g_GizmoCompNodes;


#pragma region KGizmoAct
void KGizmoAct::install() {
	g_GizmoCompNodes.clear();
}
void KGizmoAct::uninstall() {
	g_GizmoCompNodes.clear();
}
KGizmoAct * KGizmoAct::of(KNode *node) {
	return g_GizmoCompNodes.get(node);
}
void KGizmoAct::attach(KNode *node) {
	KGizmoAct *giz = new KGizmoAct();
	{
		KNode *gizmo = KNode::create();
		gizmo->setParent(node);
		gizmo->setName("$gizmo");
		KMeshDrawable::attach(gizmo);
		g_GizmoCompNodes.attach(node, giz);
		gizmo->drop();
	}
	giz->drop();
}

KGizmoAct::KGizmoAct() {
}
KMeshDrawable * KGizmoAct::getGizmoMeshDrawable() {
	KNode *self = getNode();
	KNode *mesh_node = self->findChild("$gizmo");
	return KMeshDrawable::of(mesh_node);
}
void KGizmoAct::clear() {
	KMeshDrawable *co = getGizmoMeshDrawable();
	KMesh *mesh = co ? co->getMesh() : nullptr;
	if (mesh) {
		mesh->clear();
	}
}
void KGizmoAct::addLine(const KVec3 &a, const KVec3 &b) {
	addLine(a, b, KColor32::WHITE, KColor32::WHITE);
}
void KGizmoAct::addLine(const KVec3 &a, const KVec3 &b, const KColor32 &color) {
	addLine(a, b, color, color);
}
void KGizmoAct::addLine(const KVec3 &a, const KVec3 &b, const KColor32 &color_a, const KColor32 &color_b) {
	KMeshDrawable *co = getGizmoMeshDrawable();
	KMesh *mesh = co ? co->getMesh() : nullptr;
	if (mesh) {
		m_MeshBuf.resize2(mesh, 2, 0);
		m_MeshBuf.setPosition(0, a);
		m_MeshBuf.setPosition(1, b);
		m_MeshBuf.setColor32(0, color_a);
		m_MeshBuf.setColor32(1, color_b);
		m_MeshBuf.addToMesh(mesh, KPrimitive_LINES);
	}
}
void KGizmoAct::addRegularPolygon(const KVec3 &pos, float radius, int count, float start_degrees, const KColor32 &fill_color, const KColor32 &outline_color) {
	KMeshDrawable *co = getGizmoMeshDrawable();
	KMesh *mesh = co ? co->getMesh() : nullptr;
	if (mesh && count >= 2) {
		m_MeshBuf.resize2(mesh, count+1, 0);
		for (int i=0; i<count+1; i++) { // 図形を閉じるため i=count までループする
			float rad = KMath::degToRad(start_degrees + i * 360.0f / count);
			KVec3 p;
			p.x = pos.x + radius * cosf(rad);
			p.y = pos.y + radius * sinf(rad);
			m_MeshBuf.setPosition(i, p);
			m_MeshBuf.setColor32(i, KColor32::WHITE);
		}
		if (fill_color != KColor32::ZERO) {
			KMaterial m;
			m.color = fill_color;
			m_MeshBuf.addToMesh(mesh, KPrimitive_TRIANGLE_FAN, &m);
		}
		if (outline_color != KColor32::ZERO) {
			KMaterial m;
			m.color = outline_color;
			m_MeshBuf.addToMesh(mesh, KPrimitive_LINE_STRIP, &m);
		}
	}
};
#pragma endregion // KGizmoAct

} // namespace
