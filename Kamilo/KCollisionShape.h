/// @file
/// Copyright (c) 2020 Heliodor
/// This software is released under the MIT License.
/// http://opensource.org/licenses/mit-license.php

#pragma once
#include "KMath.h"
#include "KRef.h"

namespace Kamilo {

class KNode;
class KCollider;



struct KCollisionTest {
	KCollisionTest() {
		ball_radius = 0;
		ball_climb = 0;
		ball_skin = 0.5f;
		bitmask = 0;
		result_collider = nullptr;
	}
	KVec3 ball_pos;
	float ball_radius;
	KVec3 ball_speed;
	float ball_climb;
	float ball_skin = 0.5f; // 接触許容範囲
	uint32_t bitmask;
	KVec3 result_newpos;
	KVec3 result_hitpos;
	KVec3 result_normal;
	KCollider *result_collider;
};

class KCollider: public virtual KRef {
public:
	KCollider();
	void _setNode(KNode *node);
	KNode * getNode() const;
	void set_bitflag(uint32_t value);
	uint32_t get_bitflag() const;
	void setEnable(bool value);
	bool getEnable() const;
	void set_offset(const KVec3 &value);
	const KVec3 & get_offset() const;
	KVec3 get_offset_world() const;

	// AABBとの衝突判定
	bool is_collide_with_aabb(const KVec3 &aabb_min, const KVec3 &aabb_max, uint32_t bitmask) const;

	/// コライダーに球（ワールド座標）が衝突した時、その球の新しい座標（めり込み状態を解消するための座標）と衝突点における法線を得る
	/// ball_wpos 球座標（ワールド座標系）
	/// ball_wspeed 球速度（ワールド座標系）
	virtual bool get_collision_result(KCollisionTest &args) const;

	/// aabb 範囲を得る（ワールド座標）
	virtual void get_aabb(uint32_t bitmask, KVec3 *out_minpoint, KVec3 *out_maxpoint) const;

	/// ローカル（オフセット済み）でのAABBを得る
	virtual void get_aabb_local(uint32_t bitmask, KVec3 *out_minpoint, KVec3 *out_maxpoint) const;

	/// ローカル（オフセットなし）でのAABBを得る
	virtual void get_aabb_raw(uint32_t bitmask, KVec3 *out_minpoint, KVec3 *out_maxpoint) const = 0;

	/// レイとコライダー表面との衝突判定を行う（ワールド座標）。衝突するなら交点の座標と法線を得る
	virtual bool get_ray_collision_point(const KVec3 &ray_pos, const KVec3 &ray_dir, uint32_t bitmask, KVec3 *out_point, KVec3 *out_normal, KCollider **out_collider) const = 0;

	// 球との衝突判定を行う
	// out_dsit: 球の中心から衝突面までの距離
	// out_nromal: 衝突面の法線
	virtual bool get_sphere_collision(const KVec3 &ball_wpos, float ball_radius, uint32_t bitmask, float *out_dist, KVec3 *out_normal) const = 0;

	virtual void update_inspector() = 0;

protected:
	KNode *m_node;
	KVec3 m_offset;
	uint32_t m_bitflag;
	bool m_enable;
};

class KSphereCollider: public KCollider {
public:
	static KSphereCollider * create(const KVec3 &pos, float radius);
	virtual void set_radius(float value) = 0;
	virtual float get_radius() const = 0;
};

class KAabbCollider: public KCollider {
public:
	static KAabbCollider * create(const KVec3 &pos, const KVec3 &halfsize);
	virtual void set_halfsize(const KVec3 &value) = 0;
	virtual const KVec3 & get_halfsize() const = 0;
};

class KCapsuleCollider: public KCollider {
public:
	static KCapsuleCollider * create(const KVec3 &pos, float radius, float halfheight, bool is_cylinder);
	virtual void set_halfheight(float value) = 0;
	virtual void set_radius(float value) = 0;
	virtual float get_halfheight() const = 0;
	virtual float get_radius() const = 0;
	virtual bool get_cylinder() const = 0;
};

class KPlaneCollider: public KCollider {
public:
	static KPlaneCollider * create(const KVec3 &pos, const KVec3 &normal, const KVec3 &trim_min, const KVec3 &trim_max);
	virtual void set_normal(const KVec3 &value) = 0;
	virtual const KVec3 & get_normal() const = 0;
	virtual void set_trim_box(const KVec3 &trim_min, const KVec3 &trim_max) = 0;
	virtual void get_trim_box(KVec3 *trim_min, KVec3 *trim_max) const = 0;
};

class KQuadCollider: public KCollider {
public:
	static KQuadCollider * create(const KVec3 &pos, const KVec3 *points_cw4);
	virtual const KVec3 & get_normal() const = 0;
	virtual void set_points(const KVec3 *points_cw4) = 0;
	virtual void get_points(KVec3 *points_cw4) const = 0;
};

class KCharacterCollider: public KCollider {
public:
	static 	KCharacterCollider * create(const KVec3 &pos, float radius, float halfheight);
	virtual void set_halfheight(float value) = 0;
	virtual void set_radius(float value) = 0;
	virtual float get_halfheight() const = 0;
	virtual float get_radius() const = 0;
};

class KShearedBoxCollider: public KCollider {
public:
	enum _FLAG {
		LEFT   = 1,
		RIGHT  = 2,
		TOP    = 4,
		BOTTOM = 8,
		FRONT  = 16,
		BACK   = 32,

		LEFTRIGHT = LEFT|RIGHT,
		TOPBOTTOM = TOP|BOTTOM,
		FRONTBACK = FRONT|BACK,
		ALL       = LEFTRIGHT|TOPBOTTOM|FRONTBACK,
	};
	typedef int FLAGS;
	static KShearedBoxCollider * create(const KVec3 &pos, const KVec3 &halfsize, float shearx, FLAGS flags);
	virtual void set_halfsize(const KVec3 &value) = 0;
	virtual const KVec3 & get_halfsize() const = 0;
	virtual void set_shearx(float value) = 0;
	virtual float get_shearx() const = 0;
};

class KFloorCollider: public KAabbCollider {
public:
	static KFloorCollider * create(const KVec3 &pos, const KVec3 &halfsize, float shearx, const float *heights);
	virtual void set_halfsize(const KVec3 &value) = 0;
	virtual const KVec3 & get_halfsize() const = 0;
	virtual void set_shearx(float value) = 0;
	virtual float get_shearx() const = 0;
	virtual void set_heights(const float *heights) = 0;
	virtual void get_heights(float *heights) const = 0;
};

namespace Test {
void Test_collisionshape();
}

} // namespace
