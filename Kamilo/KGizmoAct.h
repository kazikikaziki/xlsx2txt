#pragma once
#include "keng_game.h"

namespace Kamilo {

class KMeshDrawable;

class KGizmoAct: public KComp {
public:
	static void install();
	static void uninstall();
	static void attach(KNode *node);
	static KGizmoAct * of(KNode *node);
public:
	KGizmoAct();
	void clear();

	// 直線
	void addLine(const KVec3 &a, const KVec3 &b);
	void addLine(const KVec3 &a, const KVec3 &b, const KColor32 &color);
	void addLine(const KVec3 &a, const KVec3 &b, const KColor32 &color_a, const KColor32 &color_b);

	// 正多角形
	void addRegularPolygon(const KVec3 &pos, float radius, int count, float start_degrees=0, const KColor32 &outline_color=KColor32::WHITE, const KColor32 &fill_color=KColor32::ZERO);

private:
	KMeshDrawable * getGizmoMeshDrawable();
	CMeshBuf m_MeshBuf;
};

} // namespace
