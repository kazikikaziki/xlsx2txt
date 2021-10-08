/// @file
/// Copyright (c) 2020 Heliodor
/// This software is released under the MIT License.
/// http://opensource.org/licenses/mit-license.php

#pragma once
#include "KCollisionShape.h"
#include "keng_game.h"

namespace Kamilo {

class KRayCallback;
struct KRayHit;


class KSolidBodyCallback {
public:
	/// 衝突判定処理の開始時に一度だけ呼ばれる
	virtual void on_collision_update_start() {}

	/// 衝突判定処理の終了時に一度だけ呼ばれる
	virtual void on_collision_update_end() {}

	/// 移動物体同士の衝突判定時に呼ばれる。
	/// 判定を無視するなら deny に true をセットする
	virtual void on_collision_update_body_each(KNode *e1, KNode *e2, bool *deny) {}

	/// 特定の組み合わせにおける衝突応答
	/// e1 衝突物体1
	/// e2 衝突物体2
	/// response1: 衝突の結果、物体1が押し戻される量（物体１を押し戻す向きに正の値）。書き換えることができる
	/// response2: 衝突の結果、物体2が押し戻される量（物体２を押し戻す向きに正の値）。書き換えることができる
	virtual void on_collision_response(KNode *e1, KNode *e2, float *response1, float *response2) {}

	/// 判定を無視するなら deny に true をセットする
	virtual void on_collision_wall(KNode *stNode, KNode *dyNode, const KCollisionTest &test, const KVec3 &oldpos, KVec3 &newpos, bool *deny) {} 

	/// 判定を無視するなら deny に true をセットする
	virtual void on_collision_ground(KNode *stNode, KNode *dyNode, bool *deny) {}
};



class KSolidBody: public KRef {
public:
	static void install();
	static void uninstall();
	static bool getGroundPoint(const KVec3 &pos, float max_penetration, float *out_ground_y, KNode **out_ground=nullptr);
	static bool getDynamicBodyAltitude(KNode *node, float *alt);
	static float getAltitudeAtPoint(const KVec3 &point, float max_penetration=1.0f, KNode **out_ground=nullptr);
	static bool rayCast(const KVec3 &pos, const KVec3 &dir, float maxdist, KRayHit *out_hit);
	static void setDebugLineVisible(bool static_lines, bool dynamic_lines);
	static void setDebug_alwaysShowDebug(bool value);
	static bool getDebug_alwaysShowDebug();
	static void snapToGround(KNode *node);
	static void setCallback(KSolidBodyCallback *cb);
	static void setGroupName(uint32_t group_bit, const char *name_literal);
	static bool rayCastEnumerate(const KVec3 &start, const KVec3 &dir, float maxdist, KRayCallback *cb);
	static bool getSurfaceType(const KVec3 &normalizedNormal, bool *isGround, bool *isWall); ///< 法線を指定して、その面の属性を得る（地面として扱うか、壁としてあつかうかなど）
	static void attachVelocity(KNode *node);
	static bool isAttached(KNode *node);
	static KSolidBody * of(KNode *node);
public:
	KSolidBody();
	virtual ~KSolidBody();
	void setCollisionGroupBits(uint32_t group_bits);
	void setCollisionMaskBits(uint32_t mask_bits);
	void setShapeCenter(const KVec3 &pos);
	KVec3 getShapeCenter();
	KVec3 getShapeCenterInWorld();
	bool getAabbLocal(KVec3 *out_min, KVec3 *out_max);
	bool getAabbWorld(KVec3 *out_min, KVec3 *out_max);
	void setBodyEnabled(bool value);
	bool getBodyEnabled();
	void _setNode(KNode *node);
	KNode * getNode();
	void setShape(KCollider *co);
	KCollider * getShape();

	struct Desc {
		Desc() {
			_gravity = 0.0f;
			_penetratin_response = 0.0f;
			_skin_width = 0.0f;
			_snap_height = 0.0f;
			_climb_height = 0.0f;
			_bounce = 0.0f;
			_bounce_horz = 0.0f;
			_bounce_min_speed = 0.0f;
			_bounce_count = 0;
			_sliding_friction = 0.0f;
			_altitude = 0;
			_has_altitude = false;
			_ground_node = nullptr;
			_sleep_time = 0;
			_mask_bits  = 0xFFFFFFFF;
			_is_static = true;
			_slip_control = false;
			_slip_limit_speed = 0;
			_slip_limit_degrees = 0;
			knode = nullptr;
		}
		// 前フレームでの速度から算出した、実際の加速度
		KVec3 get_actual_accel() const {
			KVec3 curr_pos = knode->getWorldPosition();
			KVec3 last_spd = _prev_pos - _prev_prev_pos;
			KVec3 curr_spd = curr_pos - _prev_pos;
			return curr_spd - last_spd;
		}
		// 前フレームでの位置から算出した、実際の移動量
		KVec3 get_actual_speed() const {
			KVec3 curr_pos = knode->getWorldPosition();
			return curr_pos - _prev_pos;
		}

		int get_bounce_count() const { return _bounce_count; }
		void set_bounce_count(int value) { _bounce_count = value; }
		float get_bounce() const { return _bounce; }
		void set_bounce(float value) { _bounce = value; }
		float get_bounce_horz() const { return _bounce_horz; }
		void set_bounce_horz(float value) { _bounce_horz = value; }
		float get_bounce_min_speed() const { return _bounce_min_speed; }
		void set_bounce_min_speed(float value) { _bounce_min_speed = value; }
		float get_sliding_friction() const { return _sliding_friction; }
		void set_sliding_friction(float value) { _sliding_friction = value; }
		float get_snap_height() const { return _snap_height; }
		void set_snap_height(float value) { _snap_height = value; }
		float get_climb_height() const { return _climb_height; }
		void set_climb_height(float value) { _climb_height = value; }
		float get_gravity() const { return _gravity; }
		void set_gravity(float value) { _gravity = value; }
		float get_penetratin_response() const { return _penetratin_response; }
		void set_penetratin_response(float value) { _penetratin_response = value; }
		float get_skin_width() const { return _skin_width; }
		void set_skin_width(float value) { _skin_width = value; }
		const KVec3 & get_prev_pos() const { return _prev_pos; }
		const KVec3 & get_prev_prev_pos() const { return _prev_prev_pos; }
		int get_sleep_time() const { return _sleep_time; }
		void set_sleep_time(int value) { _sleep_time = value; }
		uint32_t get_mask_bits() const { return _mask_bits; }
		void set_mask_bits(uint32_t value) { _mask_bits = value; }
		bool is_static() const { return _is_static; }
		void set_static(bool value) { _is_static = value; }
		const KVec3 & get_velocity() const { return _velocity; }
		void set_velocity(const KVec3 &vel) { _velocity = vel; }
		void set_altitude(float alt, KNode *ground) {
			_altitude = alt;
			_has_altitude = true;
			_ground_node = ground;
		}
		void clear_altitude() {
			_altitude = 0;
			_has_altitude = false;
			_ground_node = nullptr;
		}
		/// 物理判定同士が衝突したときに、どれだけ押し返されるか。
		/// 0.0 だと全く押し返されない（相手を押しのける）（重量物のような挙動）
		/// 1.0 だと完全に押し返される（相手に道を譲る）（軽量物のような挙動）
		float _penetratin_response;

		float _gravity; /// 重力加速度（下向きに正の値）
		float _skin_width; // 左右、奥行き方向の接触誤差。相手表面との距離が範囲内だった場合は接触状態とみなす
		float _snap_height; // 地面の接触誤差。地面までの距離の絶対値がこの範囲内にあるなら、着地状態とみなす
		float _climb_height; // 引っかからずに登ることのできる段差の最大高さ。これ以上高い段差だった場合は壁への衝突と同じ扱いになる
		float _bounce; // 高さ方向の反発係数（正の値）
		float _bounce_horz; // 水平方向の反発係数（正の値）
		float _bounce_min_speed; // この速度以下で衝突した場合は反発せずに停止する
		float _sliding_friction; // 物体と接しているときの速度減衰。（１フレーム当たりの速度低下値）
		int _bounce_count; // バウンド回数
		KVec3 _prev_pos;
		KVec3 _prev_prev_pos;
		KNode *knode;
		uint32_t _mask_bits; // 衝突対象のグループ（ビット合成）
		KVec3 _velocity;
		float _altitude; // 高度。形状の底面から地面までの距離（地面存在せず高度が定義できない場合は altitude=0, has_altitude=false になる）
		bool _has_altitude; // altitude に有効な値が入っているなら true になる。 地面が全く存在しない場合は false になる

		bool _slip_control; // 進行方向と軸がずれている法線面に衝突したときに滑る処理 (slip) を制御するか？
		float _slip_limit_speed; // 衝突時の速度によるスリップの有無。この速度未満だったら滑り、以上だったら滑らずに停止する（_slip_control が true の場合のみ）
		float _slip_limit_degrees; // 衝突時の角度によるスリップの有無。速度と衝突面法線がこの角度未満だったら正面衝突とみなし、滑らずに停止する（_slip_control が true の場合のみ）
		KNode *_ground_node;
		KNodeArray _hits_with; // 他の Body との接触
		int _sleep_time;
		bool _is_static; // 動かない
	};
	KNode *m_Node;
	KCollider *m_Shape;
	Desc m_Desc;
};

class KStaticSolidBody: public KSolidBody {
public:
	static void attach(KNode *node);
	static bool isAttached(KNode *node);
	static KStaticSolidBody * of(KNode *node);
public:
	KStaticSolidBody();
	void setShapeGround() ;
	void setShapePlane(const KVec3 &normal);
	void setShapeBox(const KVec3 &halfsize);
	void setShapeShearedBox(const KVec3 &halfsize, float shearx, KShearedBoxCollider::FLAGS flags);
	void setShapeCapsule(float radius, float halfheight);
	void setShapeCylinder(float radius, float halfheight);
	void setShapeFloor(const KVec3 &halfsize, float shearx, const float *heights);
	void setShapeWall(float x0, float z0, float x1, float z1);
};


class KDynamicSolidBody: public KSolidBody {
public:
	static void attach(KNode *node);
	static void attachEx(KNode *node, bool with_default_shape);
	static bool isAttached(KNode *node);
	static KDynamicSolidBody * of(KNode *node);
public:
	explicit KDynamicSolidBody(bool with_default_shape);
	void setShapeSphere(float r);
	void setShapeCapsule(float radius, float halfheight);
	void setVelocity(const KVec3 &value);
	KVec3 getVelocity() const;
	void setBounce(float value);
	void setGravity(float value);
	void setSlidingFriction(float value);
	void setPenetratinResponse(float value);
	void setClimbHeight(float value);
	void setSnapHeight(float value);
};

struct KRayHit {
	KRayHit() {
		dist = 0;
		collider = nullptr;
	}
	float dist;   // レイ始点からの距離
	KVec3 pos;    // 交点座標
	KVec3 normal; // 交点座標における対象面の法線ベクトル（正規化済み）
	KCollider *collider; // 衝突したコライダー
};

class KRayCallback {
public:
	virtual void on_ray_hit(const KVec3 &start, const KVec3 &dir, float maxdist, const KRayHit &hit) = 0;
};




namespace Test {
void Test_collision();
}


} // namespace
