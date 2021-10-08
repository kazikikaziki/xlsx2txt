#include "KCollisionShape.h"
#include "KImGui.h"
#include "KInternal.h"
#include "keng_game.h" // KNode

namespace Kamilo {


#pragma region Utils


static bool _get_dist_quad(const KVec3 &pos, const KVec3 &q0, const KVec3 &q1, const KVec3 &q2, const KVec3 &q3, float *out_dist) {
	KVec3 hitpos;
	if (KGeom::K_GeomPerpendicularToTriangle(pos, q0, q1, q2, &hitpos)) {
		*out_dist = (hitpos - pos).getLength();
		return true;
	}
	if (KGeom::K_GeomPerpendicularToTriangle(pos, q2, q3, q0, &hitpos)) {
		*out_dist = (hitpos - pos).getLength();
		return true;
	}
	return false;
}
static bool _raycast_quad(const KRay &ray, const KVec3 &p0, const KVec3 &p1, const KVec3 &p2, const KVec3 &p3, KRayDesc *out_hit) {
	KTriangle tri1(p0, p1, p2);
	KTriangle tri2(p2, p3, p0);
	if (tri1.rayTest(ray, out_hit)) {
		return true;
	}
	if (tri2.rayTest(ray, out_hit)) {
		return true;
	}
	return false;
}

#pragma endregion // Utils




#pragma region KCollider
KCollider::KCollider() {
	m_bitflag = (uint32_t)(-1);
	m_enable = true;
	m_node = nullptr;
}
void KCollider::_setNode(KNode *node) {
	m_node = node;
}
KNode * KCollider::getNode() const {
	return m_node;
}
void KCollider::set_bitflag(uint32_t value) {
	m_bitflag = value;
}
uint32_t KCollider::get_bitflag() const {
	return m_bitflag;
}
void KCollider::setEnable(bool value) {
	m_enable = value;
}
bool KCollider::getEnable() const {
	return m_enable;
}
void KCollider::set_offset(const KVec3 &value) {
	m_offset = value;
}
const KVec3 & KCollider::get_offset() const {
	return m_offset;
}
KVec3 KCollider::get_offset_world() const {
	if (m_node) {
		return m_node->localToWorldPoint(m_offset);
	} else {
		return m_offset;
	}
}
bool KCollider::is_collide_with_aabb(const KVec3 &aabb_min, const KVec3 &aabb_max, uint32_t bitmask) const {
	KVec3 minp, maxp;
	get_aabb(bitmask, &minp, &maxp);
	return KGeom::K_GeomIntersectAabb(minp, maxp, aabb_min, aabb_max, nullptr, nullptr);
}
void KCollider::get_aabb(uint32_t bitmask, KVec3 *out_minpoint, KVec3 *out_maxpoint) const {
	KVec3 wpos = get_offset_world(); // 回転とスケーリングを考慮しないでワールド座標でのAABBを求めるため、中心点をワールド座標に持っていくだけで良い
	get_aabb_raw(bitmask, out_minpoint, out_maxpoint);
	if (out_minpoint) *out_minpoint += wpos;
	if (out_maxpoint) *out_maxpoint += wpos;
}
void KCollider::get_aabb_local(uint32_t bitmask, KVec3 *out_minpoint, KVec3 *out_maxpoint) const {
	get_aabb_raw(bitmask, out_minpoint, out_maxpoint);
	if (out_minpoint) *out_minpoint += m_offset;
	if (out_maxpoint) *out_maxpoint += m_offset;
}
bool KCollider::get_collision_result(KCollisionTest &args) const {
	// まず静止状態で判定
	bool collide = false;
	float depth = -1;
	float dist = -1;
	KVec3 norm;
	if (get_sphere_collision(args.ball_pos, args.ball_radius+args.ball_skin, args.bitmask, &dist, &norm)) {
		// 距離が球半径よりも近い。接触またはめり込み状態にある。
		// めり込みの深さ（正の値）
		// これが負の値の場合、隙間があるということ
		depth = args.ball_radius - dist;

		// めり込み深さの絶対値が skin 未満であれば「表面接触」である。
		// 「深さ < -skin」なら確実に離れている。
		// 「-skin <= 深さ < skin」なら接触している（2体間の隙間が skin 未満になっているということ）
		// 「skin <= 深さ」ならば確実にめり込んでいる。
		if (depth < -args.ball_skin) {
			// 確実に離れている
			collide = false;
		
		} else if (depth <= args.ball_skin) {
			// 表面が接触しているだけで互いにめり込んではいないが、
			// 「表面に接触したままとどまる」ことを表現するために表面にぴったり吸い付くようにする
			collide = true;
		
		} else {
			// 深くめり込んでいる
			collide = true;
		}
	}
	/*
	// 移動方向と衝突面の法線が同一向きの場合は、衝突を無視する
	if (collide) {
		float dot = args.ball_speed.dot(norm);
		if (dot > 0) {
			collide = false;
		}
	}
	*/
	if (collide && args.ball_climb > 0) {
		// 乗り上げ高さが指定されている
		// Y軸上側の面と ball 底面の高さを比べる。
		// 相手の高さが climb 未満ならば段差を無視してよい

		// AABBを調べる
		KVec3 aabbMax;
		get_aabb(args.bitmask, nullptr, &aabbMax);

		if (aabbMax.y <= args.ball_pos.y - args.ball_radius + args.ball_climb) {
			// 段差が climb よりも低いので接触判定を無視する（→上に登ることができる）
			collide = false;
		}
	}

	if (collide) {
		// めり込みを解決するために球を衝突面の法線方向に押し戻す
		args.result_newpos = args.ball_pos + norm * depth;
		args.result_hitpos = args.ball_pos - norm * dist;
		args.result_normal = norm;
		args.result_collider = const_cast<KCollider*>(this);
		return true;
	}

	// 球とコライダーは接触していない。
	// 球が静止しているなら接触無しで確定する
	if (args.ball_speed.isZero()) {
		return false;
	}
		
	// 球は移動している。
	// すり抜け防止のため、球が前回の位置から現在の位置まで移動している最中にコライダーと衝突していないか調べる。
	// ※本来なら球のスウィープ形状で判定するべきだが、簡単化のため球中心の軌跡との衝突だけを調べる
	{
		// 前回の位置から進行方向に向かってレイを飛ばす
		KVec3 ray_pos = args.ball_pos - args.ball_speed;
		KVec3 ray_dir = args.ball_speed;
		KVec3 hit_pos, hit_nor;

		// この時、レイの中心は球の底面から climb だけ上にあげた位置にしておく。
		// このレイがどこにも衝突しなければ、climb 以上の段差は存在しない
		// ただし球の中心よりも下げないようにする
		if (args.ball_climb <= args.ball_radius) {
			// レイの位置変更しない
		} else {
			ray_pos.y += (args.ball_climb - args.ball_radius);
		}
		if (get_ray_collision_point(ray_pos, ray_dir, args.bitmask, &hit_pos, &hit_nor, nullptr)) {
			// レイがコライダーと衝突している。
			// まだ距離を考慮していない（もしかしたら遥か先で衝突しているかもしれない）ので、さらに調べる
			float speed_len = args.ball_speed.getLength(); // 球の進行距離
			float hit_dist = (hit_pos - args.ball_pos).getLength(); // レイ始点から衝突点までの距離
			if (hit_dist < speed_len) {
	
				// 移動方向と衝突面の法線が同一向きの場合は、衝突を無視する
				if (collide) {
					float dot = args.ball_speed.dot(hit_nor);
					if (dot > 0) {
						return false;
					}
				}
				
				// 前回から今回までの移動距離よりも近い位置でレイが衝突している。
				// これで球の移動中にコライダーと衝突したことが確定した。球を衝突点まで押し戻す
				args.result_newpos = hit_pos + hit_nor * args.ball_radius; // 衝突点から radius だけ離れた位置に戻す
				args.result_normal = hit_nor;
				args.result_collider = const_cast<KCollider*>(this);
				return true;
			}
		}
	}

	// 衝突なし
	return false;
}
#pragma endregion // KCollider




class CSphereCollider: public KSphereCollider {
	float m_radius;
public:
	CSphereCollider(const KVec3 &pos, float radius) {
		m_offset = pos;
		m_radius = radius;
	}
	virtual void update_inspector() {
		ImGui::Text("SphereCollider");
		ImGui::DragFloat3("Center", m_offset.floats());
		ImGui::DragFloat("Radius", &m_radius, 1.0f, 0.0f, 1000.0f);
	}
	virtual void set_radius(float value) {
		K__ASSERT(value >= 0);
		m_radius = value;
	}
	virtual float get_radius() const {
		return m_radius;
	}
	virtual void get_aabb_raw(uint32_t bitmask, KVec3 *out_minpoint, KVec3 *out_maxpoint) const {
		KVec3 half(m_radius, m_radius, m_radius);
		if (out_minpoint) *out_minpoint = -half;
		if (out_maxpoint) *out_maxpoint =  half;
	}
	virtual bool get_ray_collision_point(const KVec3 &ray_pos, const KVec3 &ray_dir, uint32_t bitmask, KVec3 *out_point, KVec3 *out_normal, KCollider **out_collider) const {
		KRay ray(ray_pos, ray_dir);
		KVec3 wpos = get_offset_world();
		KSphere sphere(wpos, m_radius);
		KRayDesc hit;
		if (sphere.rayTest(ray, &hit, nullptr)) {
			if (out_point) *out_point = hit.pos;
			if (out_normal) *out_normal = hit.normal;
			if (out_collider) *out_collider = const_cast<CSphereCollider*>(this);
			return true;
		}
		return false;
	}
	virtual bool get_sphere_collision(const KVec3 &ball_wpos, float ball_radius, uint32_t bitmask, float *out_dist, KVec3 *out_normal) const {
		KVec3 this_wpos = get_offset_world();
		KVec3 delta = ball_wpos - this_wpos;
		float len = delta.getLength();
		if (len < ball_radius) {
			if (out_dist) *out_dist = len;
			if (out_normal) {
				if (!delta.getNormalizedSafe(out_normal)) {
					*out_normal = KVec3(); // zero
				}
			}
			return true;
		}
		return false;
	}
};
KSphereCollider * KSphereCollider::create(const KVec3 &pos, float radius) {
	return new CSphereCollider(pos, radius);
}




class CAabbCollider: public KAabbCollider {
	KVec3 m_halfsize;
public:
	CAabbCollider(const KVec3 &pos, const KVec3 &halfsize) {
		m_offset = pos;
		m_halfsize = halfsize;
	}
	virtual void update_inspector() {
		ImGui::Text("AABBCollider");
		ImGui::DragFloat3("Center", m_offset.floats());
		ImGui::DragFloat3("HalfSize", m_halfsize.floats());
	}
	virtual void set_halfsize(const KVec3 &value) {
		K__ASSERT(value.x >= 0);
		K__ASSERT(value.y >= 0);
		K__ASSERT(value.z >= 0);
		m_halfsize = value;
	}
	virtual const KVec3 & get_halfsize() const {
		return m_halfsize;
	}
	virtual void get_aabb_raw(uint32_t bitmask, KVec3 *out_minpoint, KVec3 *out_maxpoint) const {
		if (out_minpoint) *out_minpoint = -m_halfsize;
		if (out_maxpoint) *out_maxpoint =  m_halfsize;
	}
	virtual bool get_ray_collision_point(const KVec3 &ray_pos, const KVec3 &ray_dir, uint32_t bitmask, KVec3 *out_point, KVec3 *out_normal, KCollider **out_collider) const {
		KRay ray(ray_pos, ray_dir);
		KVec3 wpos = get_offset_world();
		KAabb aabb(wpos, m_halfsize);
		KRayDesc hit;
		if (aabb.rayTest(ray, &hit, nullptr)) {
			if (out_point) *out_point = hit.pos;
			if (out_normal) *out_normal = hit.normal;
			if (out_collider) *out_collider = const_cast<CAabbCollider*>(this);
			return true;
		}
		return false;
	}
	virtual bool get_sphere_collision(const KVec3 &ball_wpos, float ball_radius, uint32_t bitmask, float *out_dist, KVec3 *out_normal) const {
		KVec3 this_wpos = get_offset_world();
		KVec3 delta = ball_wpos - this_wpos;
		if (fabs(delta.x) > m_halfsize.x + ball_radius) return false;
		if (fabs(delta.y) > m_halfsize.y + ball_radius) return false;
		if (fabs(delta.z) > m_halfsize.z + ball_radius) return false;

		if (fabs(delta.y) <= m_halfsize.y && fabs(delta.z) <= m_halfsize.z) {
			// a または b の位置にいる = X軸方向にめり込んでいる。左右にずらして衝突を解決する
			//
			//   z
			//   |       |
			//---+-------+---
			// a | AABB  | b
			//---0-------+---> x
			//   |       |
			//
			if (out_dist) *out_dist = fabs(delta.x) - m_halfsize.x;
			if (out_normal) {
				out_normal->x = (delta.x >= 0) ? 1.0f : -1.0f;
				out_normal->y = 0;
				out_normal->z = 0;
			}
			return true;
		}
		if (fabs(delta.x) <= m_halfsize.x && fabs(delta.z) <= m_halfsize.z) {
			// a  の位置にいる（上面または下面） = Y軸方向にめり込んでいる。上下にずらして衝突を解決する
			//
			//   z
			//   |       |
			//---+-------+---
			//   |AA(a)BB|  
			//---0-------+---> x
			//   |       |
			//
			if (out_dist) *out_dist = fabs(delta.y) - m_halfsize.y;
			if (out_normal) {
				out_normal->x = 0;
				out_normal->y = (delta.y >= 0) ? 1.0f : -1.0f;
				out_normal->z = 0;
			}
			return true;
		}
		if (fabs(delta.x) <= m_halfsize.x && fabs(delta.y) <= m_halfsize.y) {
			// a または b の位置にいる = Z軸方向にめり込んでいる。前後にずらして衝突を解決する
			//
			//   z
			//   |       |
			//---+-------+---
			// a | AABB  | b
			//---0-------+---> x
			//   |       |
			//
			if (out_dist) *out_dist = fabs(delta.z) - m_halfsize.z;
			if (out_normal) {
				out_normal->x = 0;
				out_normal->y = 0;
				out_normal->z = (delta.z >= 0) ? 1.0f : -1.0f;
			}
			return true;
		}
		return false;
	}
};
KAabbCollider * KAabbCollider::create(const KVec3 &pos, const KVec3 &halfsize) {
	return new CAabbCollider(pos, halfsize);
}




class CCapsuleCollider: public KCapsuleCollider {
	float m_radius;
	float m_halfheight;
	bool m_cylinder;
public:
	CCapsuleCollider(const KVec3 &pos, float radius, float halfheight, bool is_cylinder) {
		m_offset = pos;
		m_radius = radius;
		m_halfheight = halfheight;
		m_cylinder = is_cylinder;
	}
	virtual void update_inspector() {
		ImGui::Text("CapsuleCollider");
		ImGui::DragFloat3("Center", m_offset.floats());
		ImGui::Checkbox("As cylinder", &m_cylinder);
		bool chg = false;
		chg |= ImGui::DragFloat("Radius", &m_radius, 0, 1000);
		chg |= ImGui::DragFloat("HalfHeight", &m_halfheight, 0, 1000);
		if (chg) { adj(); }
	}
	void adj() {
		if (m_halfheight < m_radius) {
			m_halfheight = 0;
			m_radius = m_halfheight;
		}
	}
	virtual void set_halfheight(float value) {
		K__ASSERT(value >= 0);
		m_halfheight = value;
		adj();
	}
	virtual void set_radius(float value) {
		K__ASSERT(value >= 0);
		m_radius = value;
		adj();
	}
	virtual bool get_cylinder() const {
		return m_cylinder;
	}
	virtual float get_halfheight() const {
		return m_halfheight;
	}
	virtual float get_radius() const {
		return m_radius;
	}
	virtual void get_aabb_raw(uint32_t bitmask, KVec3 *out_minpoint, KVec3 *out_maxpoint) const {
		KVec3 half(m_radius, m_halfheight, m_radius);
		if (out_minpoint) *out_minpoint = -half;
		if (out_maxpoint) *out_maxpoint =  half;
	}
	virtual bool get_ray_collision_point(const KVec3 &ray_pos, const KVec3 &ray_dir, uint32_t bitmask, KVec3 *out_point, KVec3 *out_normal, KCollider **out_collider) const {
		KRay ray(ray_pos, ray_dir);
		KVec3 wpos = get_offset_world();
		KCapsule capsule = {
			wpos + KVec3(0.0f, -m_halfheight+m_radius, 0.0f),
			wpos + KVec3(0.0f,  m_halfheight-m_radius, 0.0f),
			m_radius,
		};
		KRayDesc hit;
		if (capsule.rayTest(ray, &hit, nullptr)) {
			if (out_point) *out_point = hit.pos;
			if (out_normal) *out_normal = hit.normal;
			if (out_collider) *out_collider = const_cast<CCapsuleCollider*>(this);
			return true;
		}
		return false;
	}
	virtual bool get_sphere_collision(const KVec3 &ball_wpos, float ball_radius, uint32_t bitmask, float *out_dist, KVec3 *out_normal) const {
		KVec3 this_wpos = get_offset_world();
		KVec3 a = this_wpos + KVec3(0.0f, -m_halfheight+m_radius, 0.0f);
		KVec3 b = this_wpos + KVec3(0.0f,  m_halfheight-m_radius, 0.0f);
		int zone = 0;
		float dist = -1;
		
		if (m_cylinder) {
			zone = 0;
			dist = KGeom::K_GeomDistancePointToLine(ball_wpos, a, b);
		} else {
			dist = KGeom::K_GeomDistancePointToLineSegment(ball_wpos, a, b, &zone);
		}
		if (dist < 0) return false;

		if (zone < 0) {
			if (dist < m_radius + ball_radius) {
				// a 半球側に衝突
				if (out_dist) *out_dist = dist - m_radius;
				if (out_normal) {
					KVec3 delta = ball_wpos - a;
					if (!delta.getNormalizedSafe(out_normal)) {
						*out_normal = KVec3();
					}
				}
				return true;
			}
		}
		if (zone < 0) {
			if (dist < m_radius + ball_radius) {
				// b 半球側に衝突
				if (out_dist) *out_dist = dist - m_radius;
				if (out_normal) {
					KVec3 delta = ball_wpos - b;
					if (!delta.getNormalizedSafe(out_normal)) {
						*out_normal = KVec3();
					}
				}
				return true;
			}
		}
		if (zone == 0) {
			if (dist < m_radius + ball_radius) {
				// 円筒部分に衝突
				if (out_dist) *out_dist = dist - m_radius;
				if (out_normal) {
					KVec3 delta = KGeom::K_GeomPerpendicularToLineVector(ball_wpos, a, b);
					delta = -delta; // 逆向きに
					if (!delta.getNormalizedSafe(out_normal)) {
						*out_normal = KVec3();
					}
				}
				return true;
			}
		}
		return false;
	}
};
KCapsuleCollider * KCapsuleCollider::create(const KVec3 &pos, float radius, float halfheight, bool is_cylinder) {
	return new CCapsuleCollider(pos, radius, halfheight, is_cylinder);
}


class CCharacterCollider: public KCharacterCollider {
	float m_radius;
	float m_halfheight;
public:
	CCharacterCollider(const KVec3 &pos, float radius, float halfheight) {
		m_offset = pos;
		m_radius = radius;
		m_halfheight = halfheight;
	}
	virtual void update_inspector() {
		ImGui::Text("CapsuleCollider");
		ImGui::DragFloat3("Center", m_offset.floats());
		bool chg = false;
		chg |= ImGui::DragFloat("Radius", &m_radius, 0, 1000);
		chg |= ImGui::DragFloat("HalfHeight", &m_halfheight, 0, 1000);
		if (chg) { adj(); }
	}
	void adj() {
		if (m_halfheight < m_radius) {
			m_halfheight = 0;
			m_radius = m_halfheight;
		}
	}
	virtual void set_halfheight(float value) {
		K__ASSERT(value >= 0);
		m_halfheight = value;
		adj();
	}
	virtual void set_radius(float value) {
		K__ASSERT(value >= 0);
		m_radius = value;
		adj();
	}
	virtual float get_halfheight() const {
		return m_halfheight;
	}
	virtual float get_radius() const {
		return m_radius;
	}
	virtual void get_aabb_raw(uint32_t bitmask, KVec3 *out_minpoint, KVec3 *out_maxpoint) const {
		KVec3 half(m_radius, m_halfheight, m_radius);
		if (out_minpoint) *out_minpoint = -half;
		if (out_maxpoint) *out_maxpoint =  half;
	}
	virtual bool get_ray_collision_point(const KVec3 &ray_pos, const KVec3 &ray_dir, uint32_t bitmask, KVec3 *out_point, KVec3 *out_normal, KCollider **out_collider) const {
		KRay ray(ray_pos, ray_dir);
		KVec3 wpos = get_offset_world();
		KCapsule capsule = {
			wpos + KVec3(0.0f, -m_halfheight+m_radius, 0.0f),
			wpos + KVec3(0.0f,  m_halfheight-m_radius, 0.0f),
			m_radius,
		};
		KRayDesc hit;
		if (capsule.rayTest(ray, &hit, nullptr)) {
			if (out_point) *out_point = hit.pos;
			if (out_normal) *out_normal = hit.normal;
			if (out_collider) *out_collider = const_cast<CCharacterCollider*>(this);
			return true;
		}
		return false;
	}
	virtual bool get_sphere_collision(const KVec3 &ball_wpos, float ball_radius, uint32_t bitmask, float *out_dist, KVec3 *out_normal) const {
		KVec3 this_wpos = get_offset_world();
		KVec3 a = this_wpos + KVec3(0.0f, -m_halfheight+m_radius, 0.0f);
		KVec3 b = this_wpos + KVec3(0.0f,  m_halfheight-m_radius, 0.0f);
		int zone = 0;
		float dist = KGeom::K_GeomDistancePointToLineSegment(ball_wpos, a, b, &zone);
		if (dist < 0) return false;

		if (zone < 0) {
			if (dist < m_radius + ball_radius) {
				// a 半球側に衝突
				if (out_dist) *out_dist = dist - ball_radius;
				if (out_normal) {
					KVec3 delta = ball_wpos - a;
					if (!delta.getNormalizedSafe(out_normal)) {
						*out_normal = KVec3();
					}
				}
				return true;
			}
		}
		if (zone < 0) {
			if (dist < m_radius + ball_radius) {
				// b 半球側に衝突
				if (out_dist) *out_dist = dist - ball_radius;
				if (out_normal) {
					KVec3 delta = ball_wpos - b;
					if (!delta.getNormalizedSafe(out_normal)) {
						*out_normal = KVec3();
					}
				}
				return true;
			}
		}
		if (zone == 0) {
			if (dist < m_radius + ball_radius) {
				// 円筒部分に衝突
				if (out_dist) *out_dist = dist - ball_radius;
				if (out_normal) {
					KVec3 delta = KGeom::K_GeomPerpendicularToLineVector(ball_wpos, a, b);
					delta = -delta; // 逆向きに
					if (!delta.getNormalizedSafe(out_normal)) {
						*out_normal = KVec3();
					}
				}
				return true;
			}
		}
		return false;
	}
};
KCharacterCollider * KCharacterCollider::create(const KVec3 &pos, float radius, float halfheight) {
	return new CCharacterCollider(pos, radius, halfheight);
}




class CPlaneCollider: public KPlaneCollider {
	KVec3 m_normal;

	// 平面のトリミング範囲。このAxisAlignedボックス内にある平面だけが処理対象になる
	KVec3 m_trim_min;
	KVec3 m_trim_max;

	KVec3 m_aabb_min;
	KVec3 m_aabb_max;
public:
	CPlaneCollider(const KVec3 &pos, const KVec3 &normal, const KVec3 &trim_min, const KVec3 &trim_max) {
		m_offset = pos;
		m_trim_min = trim_min;
		m_trim_max = trim_max;
		if (!normal.getNormalizedSafe(&m_normal)) {
			K__ERROR("");
		}
		update_aabb();
	}
	virtual void update_inspector() {
		ImGui::Text("PlaneCollider");
		ImGui::DragFloat3("Center", m_offset.floats());
		ImGui::DragFloat3("Normal", m_normal.floats());
		ImGui::DragFloat3("Trim min", m_trim_min.floats());
		ImGui::DragFloat3("Trim max", m_trim_max.floats());
		ImGui::InputFloat3("AABB min", m_aabb_min.floats(), "%.1f", ImGuiInputTextFlags_ReadOnly);
		ImGui::InputFloat3("AABB max", m_aabb_max.floats(), "%.1f", ImGuiInputTextFlags_ReadOnly);
	}
	void update_aabb() {
		get_intersected_aabb(m_trim_min, m_trim_max, &m_aabb_min, &m_aabb_max);
	}
	void get_intersected_aabb(const KVec3 &aabb_min, const KVec3 &aabb_max, KVec3 *out_minpoint, KVec3 *out_maxpoint) const {
		KVec3 dir, pos;
		KVec3 minp = aabb_min;
		KVec3 maxp = aabb_max;

		// 平面と AABB のX+側の面との交線を得る
		if (KGeom::K_GeomIntersectPlane(m_offset, m_normal, aabb_min, KVec3(1, 0, 0), &dir, &pos)) {
			K__ASSERT(KGeom::K_GEOM_ALMOST_ZERO(dir.x)); // AABB の X+　側平面との交線なのだから、直線の X 方向成分は 0 になっていないとおかしい
			if (fabsf(dir.z) < fabsf(dir.y)) {
			//	minp.y = aabb_min.y;
			//	maxp.y = aabb_max.y;
				float z0 = pos.z + (aabb_min.y - pos.y) * dir.z / dir.y;
				float z1 = pos.z + (aabb_max.y - pos.y) * dir.z / dir.y;
				minp.z = KMath::max3(minp.z, z0, z1);
				maxp.z = KMath::min3(maxp.z, z0, z1);

			} else {
			//	minp.z = aabb_min.z;
			//	maxp.z = aabb_max.z;
				float y0 = pos.y + (aabb_min.z - pos.z) * dir.y / dir.z;
				float y1 = pos.y + (aabb_max.z - pos.z) * dir.y / dir.z;
				minp.y = KMath::max3(minp.y, y0, y1);
				maxp.y = KMath::min3(maxp.y, y0, y1);
			}
		}

		// 平面と AABB のY+側の面との交線を得る
		if (KGeom::K_GeomIntersectPlane(m_offset, m_normal, aabb_min, KVec3(0, 1, 0), &dir, &pos)) {
			K__ASSERT(KGeom::K_GEOM_ALMOST_ZERO(dir.y)); // AABB の Y+　側平面との交線なのだから、直線の Y 方向成分は 0 になっていないとおかしい
			if (fabsf(dir.z) < fabsf(dir.x)) {
			//	minp.x = aabb_min.x;
			//	maxp.x = aabb_max.x;
				float z0 = pos.z + (aabb_min.x - pos.x) * dir.z / dir.x;
				float z1 = pos.z + (aabb_min.x - pos.x) * dir.z / dir.x;
				minp.z = KMath::max3(minp.z, z0, z1);
				maxp.z = KMath::min3(maxp.z, z0, z1);
			} else {
			//	minp.z = aabb_min.z;
			//	maxp.z = aabb_max.z;
				float x0 = pos.x + (aabb_min.z - pos.z) * dir.x / dir.z;
				float x1 = pos.x + (aabb_min.z - pos.z) * dir.x / dir.z;
				minp.x = KMath::max3(minp.x, x0, x1);
				maxp.x = KMath::min3(maxp.x, x0, x1);
			}
		}
		if (out_minpoint) *out_minpoint = minp;
		if (out_maxpoint) *out_maxpoint = maxp;
	}
	bool is_point_in_trim(const KVec3 &p) const {
		return (
			(m_trim_min.x <= p.x && p.x < m_trim_max.x) &&
			(m_trim_min.y <= p.y && p.y < m_trim_max.y) &&
			(m_trim_min.z <= p.z && p.z < m_trim_max.z)
		);
	}
	virtual void set_trim_box(const KVec3 &trim_min, const KVec3 &trim_max) {
		m_trim_min = trim_min;
		m_trim_max = trim_max;
		update_aabb();
	}
	virtual void get_trim_box(KVec3 *trim_min, KVec3 *trim_max) const {
		if (trim_min) *trim_min = m_trim_min;
		if (trim_max) *trim_max = m_trim_max;
	}
	virtual void set_normal(const KVec3 &value) {
		if (value.getNormalizedSafe(&m_normal)) {
			update_aabb();
		} else {
			K__ERROR("");
		}
	}
	virtual const KVec3 & get_normal() const {
		return m_normal;
	}
	virtual void get_aabb_raw(uint32_t bitmask, KVec3 *out_minpoint, KVec3 *out_maxpoint) const {
		if (out_minpoint) *out_minpoint = m_aabb_min;
		if (out_maxpoint) *out_maxpoint = m_aabb_max;
	}
	virtual bool get_ray_collision_point(const KVec3 &ray_pos, const KVec3 &ray_dir, uint32_t bitmask, KVec3 *out_point, KVec3 *out_normal, KCollider **out_collider) const {
		KRay ray(ray_pos, ray_dir);
		KVec3 wpos = get_offset_world();
		KPlane plane(wpos, m_normal);
		KRayDesc hit;
		if (m_normal.dot(ray.dir) >= 0) {
			return false; // レイと面法線が同じ向き（平行の場合も含む）の場合は裏側から当たっていることになるので判定しないい
		}
		if (!plane.rayTest(ray, &hit)) {
			return false;
		}
		if (!is_point_in_trim(hit.pos)) {
			return false;
		}
		if (out_point) *out_point = hit.pos;
		if (out_normal) *out_normal = hit.normal;
		if (out_collider) *out_collider = const_cast<CPlaneCollider*>(this);
		return true;
	}
	virtual bool get_sphere_collision(const KVec3 &ball_wpos, float ball_radius, uint32_t bitmask, float *out_dist, KVec3 *out_normal) const {
		KVec3 this_wpos = get_offset_world();

		// wpos からこの平面への垂線の交点
		KVec3 foot_wpos = KGeom::K_GeomPerpendicularToPlane(ball_wpos, this_wpos, m_normal); // 回転を考慮しないので法線は変わらない

		if (is_point_in_trim(foot_wpos - this_wpos)) {
			KVec3 delta = ball_wpos - foot_wpos; // 平面から球への垂線ベクトル
			float dist = delta.getLength();

			if (delta.dot(m_normal) < 0) {
				// 球は平面の裏側にある。負の距離を使う
				dist = -dist;
			}

			if (dist <= ball_radius) {
				if (out_dist) *out_dist = dist;
				if (out_normal) *out_normal = m_normal;
				return true;
			}
		}

		// TrimAABB のエッジ部分との衝突は考慮しない
		return false;
	}
};
KPlaneCollider * KPlaneCollider::create(const KVec3 &pos, const KVec3 &normal, const KVec3 &trim_min, const KVec3 &trim_max) {
	return new CPlaneCollider(pos, normal, trim_min, trim_max);
}




class CQuadCollider: public KQuadCollider {
	KVec3 m_points[4];
	KVec3 m_aabb_min;
	KVec3 m_aabb_max;
	KVec3 m_normal;
public:
	CQuadCollider(const KVec3 &pos, const KVec3 *points_cw4) {
		m_offset = pos;
		set_points(points_cw4);
	}
	virtual void update_inspector() {
		bool chg = false;
		ImGui::Text("QuadCollider");
		ImGui::DragFloat3("Center", m_offset.floats());
		chg |= ImGui::DragFloat3("Point A", m_points[0].floats());
		chg |= ImGui::DragFloat3("Point B", m_points[1].floats());
		chg |= ImGui::DragFloat3("Point C", m_points[2].floats());
		chg |= ImGui::DragFloat3("Point D", m_points[3].floats());
		ImGui::InputFloat3("Normal", m_normal.floats(), "%.1f", ImGuiInputTextFlags_ReadOnly);
		ImGui::InputFloat3("AABB min", m_aabb_min.floats(), "%.1f", ImGuiInputTextFlags_ReadOnly);
		ImGui::InputFloat3("AABB max", m_aabb_max.floats(), "%.1f", ImGuiInputTextFlags_ReadOnly);
		if (chg) update_aabb();

	}
	void get_intersected_aabb(const KVec3 &aabb_min, const KVec3 &aabb_max, KVec3 *out_minpoint, KVec3 *out_maxpoint) const {
		KGeom::K_GeomIntersectAabb(m_aabb_min, m_aabb_max, aabb_min, aabb_max, out_minpoint, out_maxpoint);
	}
	void update_aabb() {
		m_aabb_min = m_points[0];
		m_aabb_max = m_points[0];
		for (int i=1; i<4; i++) {
			m_aabb_min = m_aabb_min.getmin(m_points[i]);
			m_aabb_max = m_aabb_max.getmax(m_points[i]);
		}
		KGeom::K_GeomTriangleNormal(m_points[0], m_points[1], m_points[2], &m_normal);
	}
	bool is_point_in_quad(const KVec3 &p) const {
		if (KGeom::K_GeomPointInTriangle(p, m_points[0], m_points[1], m_points[2])) {
			return true;
		}
		if (KGeom::K_GeomPointInTriangle(p, m_points[2], m_points[3], m_points[0])) {
			return true;
		}
		return false;
	}
	virtual void set_points(const KVec3 *points_cw4) {
		for (int i=0; i<4; i++) {
			m_points[i] = points_cw4[i];
		}
		update_aabb();
	}
	virtual void get_points(KVec3 *points_cw4) const {
		if (points_cw4) {
			memcpy(points_cw4, m_points, sizeof(m_points));
		}
	}
	virtual const KVec3 & get_normal() const {
		return m_normal;
	}
	virtual void get_aabb_raw(uint32_t bitmask, KVec3 *out_minpoint, KVec3 *out_maxpoint) const {
		if (out_minpoint) *out_minpoint = m_aabb_min;
		if (out_maxpoint) *out_maxpoint = m_aabb_max;
	}
	virtual bool get_ray_collision_point(const KVec3 &ray_pos, const KVec3 &ray_dir, uint32_t bitmask, KVec3 *out_point, KVec3 *out_normal, KCollider **out_collider) const {
		KRay ray = {ray_pos, ray_dir};
		KVec3 wpos = get_offset_world();
		KPlane plane(wpos, m_normal);
		KRayDesc hit;
		if (m_normal.dot(ray.dir) >= 0) {
			return false; // レイと面法線が同じ向き（平行の場合も含む）の場合は裏側から当たっていることになるので判定しないい
		}
		if (!plane.rayTest(ray, &hit)) {
			return false;
		}
		KVec3 lpos = hit.pos - wpos;
		if (!is_point_in_quad(lpos)) {
			return false;
		}
		if (out_point) *out_point = hit.pos;
		if (out_normal) *out_normal = hit.normal;
		if (out_collider) *out_collider = const_cast<CQuadCollider*>(this);
		return true;
	}
	virtual bool get_sphere_collision(const KVec3 &ball_wpos, float ball_radius, uint32_t bitmask, float *out_dist, KVec3 *out_normal) const {
		KVec3 this_wpos = get_offset_world();
		KVec3 foot_wpos = KGeom::K_GeomPerpendicularToPlane(ball_wpos, this_wpos, m_normal);
		KVec3 lpos = foot_wpos - this_wpos;
		if (is_point_in_quad(lpos)) {
			KVec3 delta = ball_wpos - foot_wpos; // 平面から球への垂線ベクトル
			float dist = delta.getLength();

			if (delta.dot(m_normal) < 0) {
				// 球は平面の裏側にある。負の距離を使う
				dist = -dist;
			}

			if (fabsf(dist) <= ball_radius) {
				if (out_dist) *out_dist = dist;
				if (1) {
					// 法線方向にだけ押し出す（裏側から来た場合はすり抜ける）
					if (out_normal) {
						*out_normal = m_normal;
					}
				} else {
					// 球のある方向に押し出す（裏表関係なく衝突する）
					if (out_normal) {
						if (!delta.getNormalizedSafe(out_normal)) {
							*out_normal = KVec3();
						}
					}
				}
				return true;
			}
		}
		// エッジ部分との衝突は考慮しない
		return false;
	}
};
KQuadCollider * KQuadCollider::create(const KVec3 &pos, const KVec3 *points_cw4) {
	return new CQuadCollider(pos, points_cw4);
}


class CShearedBoxCollider: public KShearedBoxCollider {
	KVec3 m_halfsize;
	float m_shearx;
	KVec3 m_aabb_min;
	KVec3 m_aabb_max;
	FLAGS m_flags; // KShearedBoxCollider::FLAGS
public:
	CShearedBoxCollider(const KVec3 &pos, const KVec3 &halfsize, float shearx, FLAGS flags) {
		m_offset = pos;
		m_halfsize = halfsize;
		m_shearx = shearx;
		m_flags = flags;
		update_aabb();
	}
	virtual void update_inspector() {
		bool chg = false;
		ImGui::Text("ShearedBoxCollider");
		ImGui::DragFloat3("Center", m_offset.floats());
		chg |= ImGui::DragFloat3("HalfSize", m_halfsize.floats());
		chg |= ImGui::DragFloat("ShearX", &m_shearx);
		ImGui::InputFloat3("AABB min", m_aabb_min.floats(), "%.1f", ImGuiInputTextFlags_ReadOnly);
		ImGui::InputFloat3("AABB max", m_aabb_max.floats(), "%.1f", ImGuiInputTextFlags_ReadOnly);
		{
			ImGui::Text("Flags: "); ImGui::SameLine();
			if (m_flags & TOP   ) ImGui::Text("TOP ");    ImGui::SameLine();
			if (m_flags & BOTTOM) ImGui::Text("BOTTOM "); ImGui::SameLine();
			if (m_flags & LEFT  ) ImGui::Text("LEFT ");   ImGui::SameLine();
			if (m_flags & RIGHT ) ImGui::Text("RIGHT ");  ImGui::SameLine();
			if (m_flags & FRONT ) ImGui::Text("FRONT ");  ImGui::SameLine();
			if (m_flags & BACK  ) ImGui::Text("BACK ");   ImGui::SameLine();
			ImGui::NewLine();
		}
		if (chg) update_aabb();
	}
	void update_aabb() {
		m_aabb_min.x = -m_halfsize.x - m_shearx;
		m_aabb_min.y = -m_halfsize.y;
		m_aabb_min.z = -m_halfsize.z;
		m_aabb_max.x = m_halfsize.x + m_shearx;
		m_aabb_max.y = m_halfsize.y;
		m_aabb_max.z = m_halfsize.z;
	}

	void get_points(KVec3 *points8, bool local) const {
		/*
		   1------------2
		  /|           /|
		 / |          / |
		0------------3  |
		|  |         |  |
		|  |         |  |
		|  5---------|--6
		| /          | /
		|/           |/
		4------------7
		*/
		// 上面
		KVec3 wpos = local ? KVec3() : get_offset_world();
		points8[0] = wpos + KVec3(-m_halfsize.x - m_shearx, m_halfsize.y, -m_halfsize.z);
		points8[1] = wpos + KVec3(-m_halfsize.x + m_shearx, m_halfsize.y,  m_halfsize.z);
		points8[2] = wpos + KVec3( m_halfsize.x + m_shearx, m_halfsize.y,  m_halfsize.z);
		points8[3] = wpos + KVec3( m_halfsize.x - m_shearx, m_halfsize.y, -m_halfsize.z);

		// 底面
		points8[4] = wpos + KVec3(-m_halfsize.x - m_shearx, -m_halfsize.y, -m_halfsize.z);
		points8[5] = wpos + KVec3(-m_halfsize.x + m_shearx, -m_halfsize.y,  m_halfsize.z);
		points8[6] = wpos + KVec3( m_halfsize.x + m_shearx, -m_halfsize.y,  m_halfsize.z);
		points8[7] = wpos + KVec3( m_halfsize.x - m_shearx, -m_halfsize.y, -m_halfsize.z);
	}
	virtual void get_aabb_raw(uint32_t bitmask, KVec3 *out_minpoint, KVec3 *out_maxpoint) const {
		if (out_minpoint) *out_minpoint = m_aabb_min;
		if (out_maxpoint) *out_maxpoint = m_aabb_max;
	}
	virtual void set_halfsize(const KVec3 &value) {
		m_halfsize = value;
		update_aabb();
	}
	virtual const KVec3 & get_halfsize() const {
		return m_halfsize;
	}
	virtual void set_shearx(float value) {
		m_shearx = value;
		update_aabb();
	}
	virtual float get_shearx() const {
		return m_shearx;
	}
	virtual bool get_ray_collision_point(const KVec3 &ray_pos, const KVec3 &ray_dir, uint32_t bitmask, KVec3 *out_point, KVec3 *out_normal, KCollider **out_collider) const {
		// 普通のAABBで近似
		KRay ray(ray_pos, ray_dir);
		KVec3 wpos = get_offset_world();
		KRayDesc hitresult;

		// 頂点座標を得る
		KVec3 p[8];
		get_points(p, false); // points in world

		bool found = false;
		hitresult.dist = FLT_MAX;

		// 左
		if (m_flags & LEFT) {
			KRayDesc hit;
			if (_raycast_quad(ray, p[1], p[0], p[4], p[5], &hit)) {
				if (hit.dist < hitresult.dist) {
					hitresult = hit;
					found = true;
				}
			}
		}
		// 右
		if (m_flags & RIGHT) {
			KRayDesc hit;
			if (_raycast_quad(ray, p[3], p[2], p[6], p[7], &hit)) {
				if (hit.dist < hitresult.dist) {
					hitresult = hit;
					found = true;
				}
			}
		}
		// 前面
		if (m_flags & FRONT) {
			KRayDesc hit;
			if (_raycast_quad(ray, p[0], p[3], p[7], p[4], &hit)) {
				if (hit.dist < hitresult.dist) {
					hitresult = hit;
					found = true;
				}
			}
		}
		// 背面
		if (m_flags & BACK) {
			KRayDesc hit;
			if (_raycast_quad(ray, p[2], p[1], p[5], p[6], &hit)) {
				if (hit.dist < hitresult.dist) {
					hitresult = hit;
					found = true;
			}
			}
		}
		// 上面
		if (m_flags & TOP) {
			KRayDesc hit;
			if (_raycast_quad(ray, p[0], p[1], p[2], p[3], &hit)) {
				if (hit.dist < hitresult.dist) {
					hitresult = hit;
					found = true;
				}
			}
		}
		// 底面
		if (m_flags & BOTTOM) {
			KRayDesc hit;
			if (_raycast_quad(ray, p[7], p[6], p[5], p[4], &hit)) {
				if (hit.dist < hitresult.dist) {
					hitresult = hit;
					found = true;
				}
			}
		}
		if (found) { // 見つかった？
			if (out_point) *out_point = hitresult.pos;
			if (out_normal) *out_normal = hitresult.normal;
			if (out_collider) *out_collider = const_cast<CShearedBoxCollider*>(this);
			return true;
		}
		return false;
	}
	virtual bool get_sphere_collision(const KVec3 &ball_wpos, float ball_radius, uint32_t bitmask, float *out_dist, KVec3 *out_normal) const {
		KVec3 this_wpos = get_offset_world();
		KVec3 ball_lpos = ball_wpos - this_wpos;

		KVec3 p[8];
		get_points(p, true); // points in local

		const int extend = 0;
		KVec3 minkow; // ミンコフスキー差のAABB
		minkow.x =  m_halfsize.x + ball_radius + extend + m_shearx;
		minkow.y =  m_halfsize.y + ball_radius + extend;
		minkow.z =  m_halfsize.z + ball_radius + extend;

		// AABB check
		if (fabsf(ball_lpos.x) > minkow.x) return false;
		if (fabsf(ball_lpos.y) > minkow.y) return false;
		if (fabsf(ball_lpos.z) > minkow.z) return false;

		bool in_x = fabsf(ball_lpos.x) < m_halfsize.x + m_shearx; // ボールの中心が範囲内にある？
		bool in_y = fabsf(ball_lpos.y) < m_halfsize.y; // ボールの中心が範囲内にある？
		bool in_z = fabsf(ball_lpos.z) < m_halfsize.z; // ボールの中心が範囲内にある？

		int hasL = m_flags & LEFT;
		int hasR = m_flags & RIGHT;
		int hasF = m_flags & FRONT;
		int hasB = m_flags & BACK;
		int hasTop = m_flags & TOP;
		int hasBtm = m_flags & BOTTOM;

		// 左側面（左前と左奥を結ぶ壁）
		if (hasL && ball_lpos.x < 0 && in_y && in_z) {
			float dist;
			if (_get_dist_quad(ball_lpos, p[1], p[0], p[4], p[5], &dist)) {
				if (dist <= ball_radius) {
					if (out_dist) *out_dist = dist;
					if (out_normal) KGeom::K_GeomTriangleNormal(p[1], p[0], p[4], out_normal);
					return true;
				}
			}
		}
		// 右側面（右前と右奥を結ぶ壁）
		if (hasR && ball_lpos.x > 0 && in_y && in_z) {
			float dist;
			if (_get_dist_quad(ball_lpos, p[3], p[2], p[6], p[7], &dist)) {
				if (dist <= ball_radius) {
					if (out_dist) *out_dist = dist;
					if (out_normal) KGeom::K_GeomTriangleNormal(p[3], p[2], p[6], out_normal);
					return true;
				}
			}
		}
		// 前面（左前と右前を結ぶ壁）
		if (hasF && in_x && in_y && ball_lpos.z < 0) {
			float dist;
			if (_get_dist_quad(ball_lpos, p[0], p[3], p[7], p[4], &dist)) {
				if (dist <= ball_radius) {
					if (out_dist) *out_dist = dist;
					if (out_normal) *out_normal = KVec3(0, 0, -1);
					return true;
				}
			}
		}
		// 背面（左奥と右奥を結ぶ壁）
		if (hasB && in_x && in_y && ball_lpos.z > 0) {
			float dist;
			if (_get_dist_quad(ball_lpos, p[2], p[1], p[5], p[6], &dist)) {
				if (dist <= ball_radius) {
					if (out_dist) *out_dist = dist;
					if (out_normal) *out_normal = KVec3(0, 0, 1);
					return true;
				}
			}
		}
		// 上面
		if (hasTop && in_x && ball_lpos.y > 0 && in_z) {
			float dist;
			if (_get_dist_quad(ball_lpos, p[0], p[1], p[2], p[3], &dist)) {
				if (dist <= ball_radius) {
					if (out_dist) *out_dist = dist;
					if (out_normal) *out_normal = KVec3(0, 1, 0);
					return true;
				}
			}
		}
		// 底面
		if (hasBtm && in_x && ball_lpos.y < 0 && in_z) {
			float dist;
			if (_get_dist_quad(ball_lpos, p[7], p[6], p[5], p[4], &dist)) {
				if (dist <= ball_radius) {
					if (out_dist) *out_dist = dist;
					if (out_normal) *out_normal = KVec3(0, -1, 0);
					return true;
				}
			}
		}
#if 1
		// 左前角（左前の鋭角な垂直線）
		if (hasL && hasF && (ball_lpos.x < -m_halfsize.x || ball_lpos.z < -m_halfsize.z)) {
			KVec3 edgeTop = KVec3(-m_halfsize.x,  m_halfsize.y, -m_halfsize.z);
			KVec3 edgeBtm = KVec3(-m_halfsize.x, -m_halfsize.y, -m_halfsize.z);
			float dist = KGeom::K_GeomDistancePointToLine(ball_lpos, edgeTop, edgeBtm);
			if (dist <= ball_radius) {
				if (out_dist) *out_dist = dist;
				if (out_normal) {
					KVec3 delta = KVec3(ball_lpos.x - edgeBtm.x, 0.0f, ball_lpos.z - edgeBtm.z);
					if (!delta.getNormalizedSafe(out_normal)) {
						*out_normal = KVec3();
					}
				}
				return true;
			}
		}
		// 右奥角（右奥の鋭角な垂直線）
		if (hasR && hasB && (ball_lpos.x >= m_halfsize.x || ball_lpos.z >= -m_halfsize.z)) {
			KVec3 edgeTop = KVec3(m_halfsize.x,  m_halfsize.y, m_halfsize.z);
			KVec3 edgeBtm = KVec3(m_halfsize.x, -m_halfsize.y, m_halfsize.z);
			float dist = KGeom::K_GeomDistancePointToLine(ball_lpos, edgeTop, edgeBtm);
			if (dist <= ball_radius) {
				if (out_dist) *out_dist = dist;
				if (out_normal) {
					KVec3 delta = KVec3(ball_lpos.x - edgeBtm.x, 0.0f, ball_lpos.z - edgeBtm.z);
					if (!delta.getNormalizedSafe(out_normal)) {
						*out_normal = KVec3();
					}
				}
				return true;
			}
		}
#endif
		return false;
	}
};
KShearedBoxCollider * KShearedBoxCollider::create(const KVec3 &pos, const KVec3 &halfsize, float shearx, KShearedBoxCollider::FLAGS flags) {
	return new CShearedBoxCollider(pos, halfsize, shearx, flags);
}




class CFloorCollider: public KFloorCollider {
	KVec3 m_halfsize;
	float m_shearx;
	float m_heights[4];
	KVec3 m_aabb_min;
	KVec3 m_aabb_max;
public:
	CFloorCollider(const KVec3 &pos, const KVec3 &halfsize, float shearx, const float *heights) {
		m_offset = pos;
		m_halfsize = halfsize;
		m_shearx = shearx;
		if (heights) {
			m_heights[0] = heights[0];
			m_heights[1] = heights[1];
			m_heights[2] = heights[2];
			m_heights[3] = heights[3];
		} else {
			m_heights[0] = 0;
			m_heights[1] = 0;
			m_heights[2] = 0;
			m_heights[3] = 0;
		}
		update_aabb();
	}
	virtual void update_inspector() {
		bool chg = false;
		ImGui::Text("FloorCollider");
		ImGui::DragFloat3("Center", m_offset.floats());
		chg |= ImGui::DragFloat3("HalfSize", m_halfsize.floats());
		chg |= ImGui::DragFloat("ShearX", &m_shearx);
		chg |= ImGui::DragFloat("Height A", m_heights+0);
		chg |= ImGui::DragFloat("Height B", m_heights+1);
		chg |= ImGui::DragFloat("Height C", m_heights+2);
		chg |= ImGui::DragFloat("Height D", m_heights+3);
		ImGui::InputFloat3("AABB min", m_aabb_min.floats(), "%.1f", ImGuiInputTextFlags_ReadOnly);
		ImGui::InputFloat3("AABB max", m_aabb_max.floats(), "%.1f", ImGuiInputTextFlags_ReadOnly);
		if (chg) update_aabb();
	}
	void update_aabb() {
		m_aabb_min.x = -m_halfsize.x - m_shearx;
		m_aabb_min.y = KMath::min4(m_heights[0], m_heights[1], m_heights[2], m_heights[3]);
		m_aabb_min.z = -m_halfsize.z;
		m_aabb_max.x = m_halfsize.x + m_shearx;
		m_aabb_max.y = KMath::max4(m_heights[0], m_heights[1], m_heights[2], m_heights[3]);
		m_aabb_max.z = m_halfsize.z;
	}
	virtual void get_aabb_raw(uint32_t bitmask, KVec3 *out_minpoint, KVec3 *out_maxpoint) const {
		if (out_minpoint) *out_minpoint = m_aabb_min;
		if (out_maxpoint) *out_maxpoint = m_aabb_max;
	}
	virtual void set_halfsize(const KVec3 &value) {
		m_halfsize = value;
		update_aabb();
	}
	virtual const KVec3 & get_halfsize() const {
		return m_halfsize;
	}
	virtual void set_shearx(float value) {
		m_shearx = value;
		update_aabb();
	}
	virtual float get_shearx() const {
		return m_shearx;
	}
	virtual void set_heights(const float *heights) {
		if (heights) {
			for (int i=0; i<4; i++) {
				m_heights[i] = heights[i];
			}
			update_aabb();
		}
	}
	virtual void get_heights(float *heights) const {
		if (heights) {
			for (int i=0; i<4; i++) {
				heights[i] = m_heights[i];
			}
		}
	}
	virtual bool get_ray_collision_point(const KVec3 &ray_pos, const KVec3 &ray_dir, uint32_t bitmask, KVec3 *out_point, KVec3 *out_normal, KCollider **out_collider) const {
		const KVec3 p[] = {
			KVec3(-m_halfsize.x, m_heights[0],-m_halfsize.z),
			KVec3(-m_halfsize.x, m_heights[1], m_halfsize.z),
			KVec3( m_halfsize.x, m_heights[2], m_halfsize.z),
			KVec3( m_halfsize.x, m_heights[3],-m_halfsize.z),
		};

		KRay ray(ray_pos, ray_dir);
		KVec3 this_wpos = get_offset_world();
		KRayDesc hit;
		KTriangle tri0(this_wpos+p[0], this_wpos+p[1], this_wpos+p[2]);
		KTriangle tri1(this_wpos+p[2], this_wpos+p[3], this_wpos+p[0]);
		if (tri0.rayTest(ray, &hit)) {
			if (out_point) *out_point = hit.pos;
			if (out_normal) *out_normal = hit.normal;
			if (out_collider) *out_collider = const_cast<CFloorCollider*>(this);
			return true;
		}
		if (tri1.rayTest(ray, &hit)) {
			if (out_point) *out_point = hit.pos;
			if (out_normal) *out_normal = hit.normal;
			if (out_collider) *out_collider = const_cast<CFloorCollider*>(this);
			return true;
		}
		return false;
	}
	virtual bool get_sphere_collision(const KVec3 &ball_wpos, float ball_radius, uint32_t bitmask, float *out_dist, KVec3 *out_normal) const {
		KVec3 this_wpos = get_offset_world();
		KVec3 ball_lpos = ball_wpos - this_wpos;
		const KVec3 p[] = {
			KVec3(-m_halfsize.x, m_heights[0],-m_halfsize.z),
			KVec3(-m_halfsize.x, m_heights[1], m_halfsize.z),
			KVec3( m_halfsize.x, m_heights[2], m_halfsize.z),
			KVec3( m_halfsize.x, m_heights[3],-m_halfsize.z),
		};

		float dist;
		if (_get_dist_quad(ball_lpos, p[0], p[1], p[2], p[3], &dist)) {
			if (dist <= ball_radius) {
				if (out_dist) *out_dist = dist;
				if (out_normal) KGeom::K_GeomTriangleNormal(p[0], p[1], p[2], out_normal);
				return true;
			}
		}
		// エッジ部分との衝突は考慮しない
		return false;
	}
};
KFloorCollider * KFloorCollider::create(const KVec3 &pos, const KVec3 &halfsize, float shearx, const float *heights) {
	return new CFloorCollider(pos, halfsize, shearx, heights);
}


namespace Test {
void Test_collisionshape() {
	{
		float X = 100;
		float Y = 100;
		float R = 20;
		KCollider *co = KSphereCollider::create(KVec3(X, Y, 0.0f), R);
		KVec3 pos; bool b;

		// 原点（円の左下）から真上にレイを飛ばす
		b = co->get_ray_collision_point(KVec3(), KVec3(0, 1, 0), 0, &pos, nullptr, nullptr);
		K__ASSERT(!b); // 衝突しない

		// 原点（円の左下）から右上にレイを飛ばす
		float maxerr = 0.0001f;
		b = co->get_ray_collision_point(KVec3(), KVec3(1, 1, 0), 0, &pos, nullptr, nullptr);
		float p = (hypotf(X,Y) - R) * 1/sqrtf(2);
		K__ASSERT(b && pos.equals(KVec3(p, p, 0.0f), maxerr)); // 円の左下に衝突

		// 円の真下から真上にレイを飛ばす
		b = co->get_ray_collision_point(KVec3(X, 0.0f, 0.0f), KVec3(0, 1, 0), 0, &pos, nullptr, nullptr);
		K__ASSERT(b && pos.equals(KVec3(X, Y-R, 0.0f), maxerr)); // 円の真下に衝突

		// 円の真左から右にレイを飛ばす
		b = co->get_ray_collision_point(KVec3(0.0f, Y, 0.0f), KVec3(1, 0, 0), 0, &pos, nullptr, nullptr);
		K__ASSERT(b && pos.equals(KVec3(X-R, Y, 0.0f), maxerr)); // 円の左に衝突

		co->drop();
	}

	{
		int HalfW = 200;
		KCollider *co = KAabbCollider::create(KVec3(1000, 1000, 0), KVec3(HalfW, 40, 30));
		KVec3 pos, nor; bool b;

		// 原点（AABBの左下）から真上にレイを飛ばす
		b = co->get_ray_collision_point(KVec3(0, 0, 0), KVec3(0, 1, 0), 0, &pos, &nor, nullptr);
		K__ASSERT(!b); // 衝突しない

		// AABBの下側（遠方）から右上にレイを飛ばす
		b = co->get_ray_collision_point(KVec3(1000, 0, 0), KVec3(1, 1, 0), 0, &pos, &nor, nullptr);
		K__ASSERT(!b); // 衝突しない

		// AABBの下側（近距離）から右上にレイを飛ばす
		b = co->get_ray_collision_point(KVec3(1000, 900, 0), KVec3(1, 1, 0), 0, &pos, &nor, nullptr);
		K__ASSERT(b && pos==KVec3(1060, 960, 0) && nor==KVec3(0, -1, 0)); // AABB底面に衝突

		// AABBの左から右にレイを飛ばす
		b = co->get_ray_collision_point(KVec3(0, 1000, 0), KVec3(1, 0, 0), 0, &pos, &nor, nullptr);
		K__ASSERT(b && pos==KVec3(1000-HalfW, 1000, 0) && nor==KVec3(-1, 0, 0)); // AABB左に衝突

		// AABBの右から左にレイを飛ばす
		b = co->get_ray_collision_point(KVec3(2000, 1000, 0), KVec3(-1, 0, 0), 0, &pos, &nor, nullptr);
		K__ASSERT(b && pos==KVec3(1000+HalfW, 1000, 0) && nor==KVec3( 1, 0, 0));

		co->drop();
	}
}
} // Test



} // namespace
