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

	// 矩形
	void addRectangle(const KVec3 &pos, const KVec3 &halfsize, const KColor32 &border_color=KColor32::WHITE, const KColor32 &fill_color=KColor32::ZERO);
	void addConvexPolygon(const KVec3 &pos, const KVec3 points[], int count, const KColor32 &border_color=KColor32::WHITE, const KColor32 &fill_color=KColor32::ZERO);

	// 正多角形
	void addRegularPolygon(const KVec3 &pos, float radius, int count, float start_degrees=0, const KColor32 &border_color=KColor32::WHITE, const KColor32 &fill_color=KColor32::ZERO);

	void addRectangleXY(float x0, float y0, float x1, float y1, const KColor32 &border_color=KColor32::WHITE, const KColor32 &fill_color=KColor32::ZERO) {
		KVec3 o((x0 + x1)/2, (y0 + y1)/2, 0.0f);
		KVec3 r((x1 - x0)/2, (y1 - y0)/2, 0.0f);
		addRectangle(o, r, border_color, fill_color);
	}

private:
	KMeshDrawable * getGizmoMeshDrawable();
	CMeshBuf m_MeshBuf;
};

} // namespace
