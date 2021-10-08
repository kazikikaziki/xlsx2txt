#include "KMath.h"
#include "KVec.h"
#include "KMatrix.h"

// Use stb_perlin.h
#define K_USE_STB_PERLIN


#ifdef K_USE_STB_PERLIN
	// stb_image
	// https://github.com/nothings/stb/blob/master/stb_image.h
	// license: public domain
	#define STB_PERLIN_IMPLEMENTATION
	#include "stb_perlin.h"
#endif

#include "KInternal.h"


namespace Kamilo {

void K__math_error(const char *func) {
	if (K::_IsDebuggerPresent()) {
		K::_break();
		K__ASSERT(0);
		K__ERROR("K__math_error at %s", func);
	} else {
		K__ERROR("K__math_error at %s", func);
	}
}

























#pragma region KMath
int KMath::max(int a, int b) {
	return a > b ? a : b;
}
float KMath::max(float a, float b) {
	return a > b ? a : b;
}
int KMath::min(int a, int b) {
	return a < b ? a : b;
}
float KMath::min(float a, float b) {
	return a < b ? a : b;
}
float KMath::signf(float x) {
	return (x < 0) ? -1.0f : (x > 0) ? 1.0f : 0.0f;
}
int KMath::signi(int x) {
	return (x < 0) ? -1 : (x > 0) ? 1 : 0;
}

/// 角度（度）からラジアンを求める
float KMath::degToRad(float deg) {
	return PI * deg / 180.0f;
}
/// 角度（ラジアン）から度を求める
float KMath::radToDeg(float rad) {
	return 180.0f * rad / PI;
}

/// x が 2 の整数乗であれば true を返す
bool KMath::isPow2(int x) {
	return fmodf(log2f((float)x), 1.0f) == 0.0f;
}

/// パーリンノイズ値を -1.0 ～ 1.0 の値で得る
/// x: ノイズ取得座標。周期は 1.0
/// x_wrap: 変化の周波数。0 だと繰り返さないが、
///         1 以上の値を指定すると境界でノイズがつながるようになる。
///         2のべき乗であること。最大値は256。大きな値を指定すると相対的に細かい変化になる
/// ヒント
/// x, y, z のどれかを 0 に固定すると正弦波のようになるため、あまりランダム感がない
/// wrap に 0 を指定するとかなり平坦な値になる
/// float noise = KMath::perlin(t, 0, z, 8, 0, 0);
float KMath::perlin(float x, float y, float z, int x_wrap, int y_wrap, int z_wrap) {
#ifdef K_USE_STB_PERLIN
	// x, y, z: ノイズ取得座標。周期は 1.0
	// x_wrap, y_wrap, z_wrap: 折り畳み値（周期）
	// 2のべき乗を指定する. 最大値は256
	// -1 以上 1 以下の値を返す
	float xx = (x_wrap > 0) ? (x * x_wrap) : x;
	float yy = (y_wrap > 0) ? (y * x_wrap) : y;
	float zz = (z_wrap > 0) ? (z * x_wrap) : z;
	return stb_perlin_noise3(xx, yy, zz, x_wrap, y_wrap, z_wrap);
#else
	K__WARNING("No perlin support");
	return 0.0f;
#endif
}

/// KMath::perlin と同じだが、戻り値が 0.0 以上 1.0 以下の範囲に射影する
float KMath::perlin01(float x, float y, float z, int x_wrap, int y_wrap, int z_wrap) {
	float p = perlin(x, y, z, x_wrap, y_wrap, z_wrap);
	return linearStep(-1.0f, 1.0f, p);
}

float KMath::linearStepUnclamped(float edge0, float edge1, float x) {
	if (edge1 == edge0) {
		K__MATH_ERROR;
		return 1.0f; // xが既に終端 edge1 に達しているとみなす
	}
	return (x - edge0) / (edge1 - edge0);
}
// smoothstep の線形版。normalize。x の区間 [edge0 .. edge1] を [0..1] に再マッピングする
float KMath::linearStep(float edge0, float edge1, float x) {
	if (edge1 == edge0) {
		K__MATH_ERROR;
		return 1.0f; // xが既に終端 edge1 に達しているとみなす
	}
	return clamp01((x - edge0) / (edge1 - edge0));
}
float KMath::linearStepBumped(float edge0, float edge1, float x) {
	return 1.0f - fabsf(linearStep(edge0, edge1, x) - 0.5f) * 2.0f;
}
float KMath::smoothStep(float edge0, float edge1, float x) {
	x = linearStep(edge0, edge1, x);
	return x * x * (3 - 2 * x);
}
float KMath::smoothStepBumped(float edge0, float edge1, float x) {
	x = linearStepBumped(edge0, edge1, x);
	return x * x * (3 - 2 * x);
}
float KMath::smootherStep(float edge0, float edge1, float x) {
	// smoothstepよりもさらに緩急がつく
	// https://en.wikipedia.org/wiki/Smoothstep
	x = linearStep(edge0, edge1, x);
	return x * x * x * (x * (x * 6 - 15) + 10);
}
float KMath::smootherStepBumped(float edge0, float edge1, float x) {
	x = linearStepBumped(edge0, edge1, x);
	return x * x * x * (x * (x * 6 - 15) + 10);
}

int KMath::clampi(int a, int lower, int upper) {
	return (a < lower) ? lower : (a < upper) ? a : upper;
}
float KMath::clampf(float a, float lower, float upper) {
	return (a < lower) ? lower : (a < upper) ? a : upper;
}
float KMath::clamp01(float a) {
	return clampf(a, 0, 1);
}
float KMath::round(float a) {
	return (a >= 0) ? floorf(a + 0.5f) : -round(fabsf(a));
}
int KMath::repeati(int a, int b) {
	if (b == 0) return 0;
	if (a >= 0) return a % b;
	int B = (-a / b + 1) * b; // b の倍数のうち、-a を超えるもの (B+a>=0を満たすB)
	return (B + a) % b;
}
float KMath::repeatf(float a, float b) {
	if (b == 0) return 0;
	if (a >= 0) return fmodf(a, b);
	float B = ceilf(-a / b + 1.0f) * b; // b の倍数のうち、-a を超えるもの (B+a>=0を満たすB)
	return fmodf(B + a, b);
}
int KMath::delta_repeati(int a, int b, int n) {
	int aa = repeati(a, n);
	int bb = repeati(b, n);
	if (aa == bb) return 0;
	int d1 = bb - aa;
	int d2 = bb - aa + n;
	return (abs(d1) < abs(d2)) ? d1 : d2;
}
float KMath::lerpUnclamped(float a, float b, float t) {
	return a + (b - a) * t;
}
float KMath::lerp(float a, float b, float t) {
	return lerpUnclamped(a, b, clamp01(t));
}
float KMath::normalize_deg(float deg) {
	return repeatf(deg + 180.0f, 360.0f) - 180.0f;
}
float KMath::normalize_rad(float rad) {
	return repeatf(rad + PI, PI*2) - PI;
}
bool KMath::equals(float a, float b, float maxerr) {
	// a と b の差が maxerr 以下（未満ではない）であれば true を返す。
	// maxerr を 0 に設定した場合は a == b と同じことになる
	// maxerr=0の場合でも判定できるように等号を含める
	return fabsf(a - b) <= maxerr;
}
bool KMath::inRange(float a, float lower, float upper) {
	return lower <= a && a <= upper;
}
bool KMath::inRange(int a, int lower, int upper) {
	return lower <= a && a <= upper;
}
bool KMath::isZero(float a, float eps) {
	return equals(a, 0, eps);
}
float KMath::triangle_wave(float t, float cycle) {
	float m = fmodf(t, cycle);
	m /= cycle;
	if (m < 0.5f) {
		return m / 0.5f;
	} else {
		return (1.0f - m) / 0.5f;
	}
}
float KMath::triangle_wave(int t, int cycle) {
	return triangle_wave((float)t, (float)cycle);
}
#pragma endregion // KMath




















#pragma region Math classes










#pragma endregion // Math classes





#pragma region KGeom
static float _SQ(float x) { return x * x; }

float KGeom::K_GeomPointIsLeft2D(float px, float py, float ax, float ay, float bx, float by) {
	// XY平面上で (ax, ay) から (bx, by) を見ている時に点(px, py) が視線の左右どちらにあるか
	// 左なら正の値、右なら負の値、視線上にあるなら 0 を返す
	float Ax = bx - ax;
	float Ay = by - ay;
	float Bx = px - ax;
	float By = py - ay;
	return Ax * By  - Ay * Bx;
}

bool KGeom::K_GeomTriangleNormal(const KVec3 &a, const KVec3 &b, const KVec3 &c, KVec3 *out_cw_normalized_normal) {
	// 三角形 abc の法線ベクトルを得る（時計回りの並びを表とする）
	// out_cw_normalized_normal に正規化された法線をセットする
	KVec3 ab = b - a;
	KVec3 ac = c - a;
	KVec3 no = ab.cross(ac);
	return no.getNormalizedSafe(out_cw_normalized_normal);
}
bool KGeom::K_GeomPointInTriangle(const KVec3 &p, const KVec3 &a, const KVec3 &b, const KVec3 &c) {
	// 点 p が三角形 abc 内にあるか判定する
	KVec3 v1 = (b-a).cross(p-a); // 点 a から b と p を見る
	KVec3 v2 = (c-b).cross(p-b); // 点 b から c と p を見る
	KVec3 v3 = (a-c).cross(p-c); // 点 c から a と p を見る

	if (!v1.isZero()) v1 = v1.getNormalized();
	if (!v2.isZero()) v2 = v2.getNormalized();
	if (!v3.isZero()) v3 = v3.getNormalized();

	// 外積同士の向きが一致しているかどうか確かめるため、外積同士の内積をとる
	float dot12 = v1.dot(v2);
	float dot13 = v1.dot(v3);
	float dot23 = v2.dot(v3);

	// 3個の内積値の正負の組み合わせで点がどの場所にあるかを判定する。
	// 
	// これには必ず３個の値すべてを調べる必要がある。
	// 点が確実に三角形内部にある場合はどれか2組だけ調べればよいが、
	// 点が辺に重なる場合や頂点に重なる場合は外積が 0 になるため、残り2点で判定する必要がある。
	// 以下に正負の組み合わせを示す
	//
	// + + + = 3個の正           = 点 p は abc の内部にある
	// + + 0 = 2個の正と1個のゼロ = 点 p は辺のどれかに重なる
	// + 0 0 = 1個の正と2個のゼロ = 点 p は頂点のどれかに重なる
	// 0 0 0 = 3個のゼロ         = 点 p および三角形が1点に縮退している
	// 0 0 - = 2個のゼロと1個の負 = 点 p は abc の外部にあり、辺の延長線上にある
	// それ以外                  = 点 p は abc の外部にあり、どの辺の延長線上にもない
	// 
	// 以上のことから、負の値を全く含まない場合に三角形内部にあると判定する

	if (dot12>=0 && dot13>=0 && dot23>=0) {
		return true;
	}
	return false;
}
KVec3 KGeom::K_GeomPerpendicularToLineVector(const KVec3 &p, const KVec3 &a, const KVec3 &b) {
	// 点 p から直線 ab におろした垂線のベクトル
	KVec3 dir;
	if ((b - a).getNormalizedSafe(&dir)) {
		float dist_a = dir.dot(p - a); // [点a] から [点pを通る垂線との交点] までの距離
		KVec3 H = a + dir * dist_a; // 交点
		return H - p; // p から H へのベクトル
	} else {
		K__MATH_ERROR;
		return KVec3();
	}
}
bool KGeom::K_GeomPerpendicularToLine(const KVec3 &p, const KVec3 &a, const KVec3 &b, float *out_dist_a, KVec3 *out_pos) {
	// 点 p から直線 ab におろした垂線の交点
	// out_dist_a: aから交点までの距離
	// out_pos: 交点座標

	// 幾何計算（２次元・３次元）の計算公式
	// https://www.eyedeal.co.jp/img/eyemLib_caliper.pdf
	// 指定点を通り，指定直線に垂直な直線
	KVec3 dir;
	if ((b - a).getNormalizedSafe(&dir)) {
		float dist_a = dir.dot(p - a); // [点a] から [点pを通る垂線との交点] までの距離
		if (out_dist_a) *out_dist_a = dist_a;
		if (out_pos) *out_pos = a + dir * dist_a;
		return true;
	}
	return false;
}
KVec3 KGeom::K_GeomPerpendicularToPlane(const KVec3 &p, const KVec3 &plane_pos, const KVec3 &plane_normal) {
	// https://math.keicode.com/vector-calculus/distance-from-plane-to-point.php
	// 点 p と、原点を通る法線 n の平面との距離は
	// dist = |p dot n|
	// で求まる。これで点 p から法線交点までの距離がわかるので、法線ベクトルを利用して
	// 点 p から dist だけ離れた位置の点を得る。
	// dist = p + nor * |p dot n|
	// ただしここで使いたいのは点から面に向かうベクトルであり、面の法線ベクトルとは逆向きなので
	// 符号を反転させておく
	// dist = p - nor * |p dot n|
	KVec3 nor;
	if (plane_normal.getNormalizedSafe(&nor)) {
		KVec3 relpos = p - plane_pos;
		float dist = relpos.dot(nor);
		return p - nor * dist;
	} else {
		K__MATH_ERROR;
		return KVec3();
	}
}

bool KGeom::K_GeomPerpendicularToTriangle(const KVec3 &p, const KVec3 &a, const KVec3 &b, const KVec3 &c, KVec3 *out_pos) {
	// 点 p から三角形 abc におろした垂線の交点
	
	// 三角形の法線
	KVec3 nor;
	if (!K_GeomTriangleNormal(a, b, c, &nor)) {
		return false;
	}

	// 三角形を含む平面に降ろした垂線
	KVec3 q = K_GeomPerpendicularToPlane(p, a, nor);

	// 交点 q は三角形の内部にあるか？
	if (!K_GeomPointInTriangle(q, a, b, c)) {
		return false;
	}
	if (out_pos) *out_pos = q;
	return true;
}

float KGeom::K_GeomDistancePointToLine(const KVec3 &p, const KVec3 &a, const KVec3 &b) {
	// 点 p と直線 ab の最短距離
	KVec3 H;
	if (K_GeomPerpendicularToLine(p, a, b, nullptr, &H)) {
		return (H - p).getLength();
	} else {
		K__MATH_ERROR;
		return 0;
	}
}

float KGeom::K_GeomDistancePointToAabb(const KVec3 &p, const KVec3 &aabb_min, const KVec3 &aabb_max) {
	// AABBと点の最短距離
	// http://marupeke296.com/COL_3D_No11_AABBvsPoint.html
	float sqlen = 0;
	if (p.x < aabb_min.x) sqlen += _SQ(p.x - aabb_min.x);
	if (p.x > aabb_max.x) sqlen += _SQ(p.x - aabb_max.x);
	if (p.y < aabb_min.y) sqlen += _SQ(p.y - aabb_min.y);
	if (p.y > aabb_max.y) sqlen += _SQ(p.y - aabb_max.y);
	if (p.z < aabb_min.z) sqlen += _SQ(p.z - aabb_min.z);
	if (p.z > aabb_max.z) sqlen += _SQ(p.z - aabb_max.z);
	return sqrtf(sqlen);
}
float KGeom::K_GeomDistancePointToObb(const KVec3 &p, const KVec3 &obb_pos, const KVec3 &obb_halfsize, const KMatrix4 &obb_matrix) {
	// OBBと点の最短距離
	// http://marupeke296.com/COL_3D_No12_OBBvsPoint.html
	KVec3 v;
	if (obb_halfsize.x > 0) {
		KVec3 axisdir = obb_matrix.transform(KVec3(1, 0, 0));
		float s = (p - obb_pos).dot(axisdir);
		if (fabsf(s) > 1.0f) {
			v += axisdir * (1 - s) * obb_halfsize.x;
		}
	}
	if (obb_halfsize.y > 0) {
		KVec3 axisdir = obb_matrix.transform(KVec3(0, 1, 0));
		float s = (p - obb_pos).dot(axisdir);
		if (fabsf(s) > 1.0f) {
			v += axisdir * (1 - s) * obb_halfsize.y;
		}
	}
	if (obb_halfsize.z > 0) {
		KVec3 axisdir = obb_matrix.transform(KVec3(0, 0, 1));
		float s = (p - obb_pos).dot(axisdir);
		if (fabsf(s) > 1.0f) {
			v += axisdir * (1 - s) * obb_halfsize.z;
		}
	}
	return v.getLength();
}
float KGeom::K_GeomDistancePointToLineSegment(const KVec3 &p, const KVec3 &a, const KVec3 &b, int *zone) {
	// p から直線abに垂線を下したときの点 Q を求める
	float Q_from_a = FLT_MAX; // p から垂線交点 Q までの距離
	KVec3 Q_pos; // 垂線交点 Q の座標
	if (!K_GeomPerpendicularToLine(p, a, b, &Q_from_a, &Q_pos)) {
		return -1;
	}

	float len = (b - a).getLength();

	if (Q_from_a < 0) {
		// Q が a よりも外側にある
		if (zone) *zone = -1;
		return (p - a).getLength();

	} else if (Q_from_a < len) {
		// Q が a と b の間にある
		if (zone) *zone = 0;
		return (p - Q_pos).getLength();

	} else {
		// Q が b よりも外側にある
		if (zone) *zone = 1;
		return (p - b).getLength();
	}
}
float KGeom::K_GeomDistancePointToCapsule(const KVec3 &p, const KVec3 &capsule_p0, const KVec3 &capsule_p1, float capsule_r) {
	float dist = K_GeomDistancePointToLineSegment(p, capsule_p0, capsule_p1, nullptr);
	if (dist >= 0) {
		return dist - capsule_r;
	} else {
		return 0;
	}
}


// 直線が平行になっている場合は out_dist だけセットして false を返す
// それ以外の場合は out_dist および out_A, out_B をセットして true を返す
// 直線A上の最接近点 = A_pos + A_dir * out_A
// 直線B上の最接近点 = B_pos + B_dir * out_B
bool KGeom::K_GeomDistanceLineToLine(const KVec3 &A_pos, const KVec3 &A_dir, const KVec3 &B_pos, const KVec3 &B_dir, float *out_dist, float *out_A, float *out_B) {
	// 直線同士の最接近距離と最接近点を知る
	// http://marupeke296.com/COL_3D_No19_LinesDistAndPos.html
	KVec3 delta = B_pos - A_pos;
	float A_dot = delta.dot(A_dir);
	float B_dot = delta.dot(B_dir);
	float A_cross_B_lensq = A_dir.cross(B_dir).getLengthSq();
	if (K_GEOM_ALMOST_ZERO(A_cross_B_lensq)) {
		// 平行。距離は求まるが点は求まらない
		if (out_dist) *out_dist = delta.cross(A_dir).getLength();
		return false;
	}
	float AB = A_dir.dot(B_dir);
	float At = (A_dot - B_dot * AB) / (1.0f - AB * AB);
	float Bt = (B_dot - A_dot * AB) / (AB * AB - 1.0f);
	KVec3 A_result = A_pos + A_dir * At;
	KVec3 B_result = B_pos + B_dir * Bt;
	if (out_dist) *out_dist = (B_result - A_result).getLength();
	if (out_A) *out_A = At;
	if (out_A) *out_A = Bt;
	return true;
}

/// aabb1 と aabb2 の交差範囲
bool KGeom::K_GeomIntersectAabb(
	const KVec3 &aabb1_min, const KVec3 &aabb1_max,
	const KVec3 &aabb2_min, const KVec3 &aabb2_max,
	KVec3 *out_intersect_min, KVec3 *out_intersect_max)
{
	if (aabb1_max.x < aabb2_min.x) return false; // 交差・接触なし
	if (aabb1_max.y < aabb2_min.y) return false;
	if (aabb1_max.z < aabb2_min.z) return false;
			
	if (aabb2_max.x < aabb1_min.x) return false; // 交差・接触なし
	if (aabb2_max.y < aabb1_min.y) return false;
	if (aabb2_max.z < aabb1_min.z) return false;

	// 交差あり、交差範囲を設定する（交差ではなく接触の場合はサイズ 0 になることに注意）
	if (out_intersect_min) {
		out_intersect_min->x = KMath::max(aabb1_min.x, aabb2_min.x);
		out_intersect_min->y = KMath::max(aabb1_min.y, aabb2_min.y);
		out_intersect_min->z = KMath::max(aabb1_min.z, aabb2_min.z);
	}
	if (out_intersect_max) {
		out_intersect_max->x = KMath::min(aabb1_max.x, aabb2_max.x);
		out_intersect_max->y = KMath::min(aabb1_max.y, aabb2_max.y);
		out_intersect_max->z = KMath::min(aabb1_max.z, aabb2_max.z);
	}
	return true;
}

/// 平面同士の交線
bool KGeom::K_GeomIntersectPlane(
	const KVec3 &plane1_pos, const KVec3 &plane1_nor, 
	const KVec3 &plane2_pos, const KVec3 &plane2_nor,
	KVec3 *out_dir, KVec3 *out_pos)
{
	KVec3 nor1, nor2;
	if (!plane1_nor.getNormalizedSafe(&nor1)) return false;
	if (!plane2_nor.getNormalizedSafe(&nor2)) return false;

	float dot1 = plane1_pos.dot(nor1);
	float dot2 = plane2_pos.dot(nor2);

	KVec3 cross = nor1.cross(nor2);
	if (cross.z != 0) {
		if (out_dir) *out_dir = cross;
		if (out_pos) {
			out_pos->x = (dot1 * plane2_nor.y - dot2 * plane1_nor.y) / cross.z;
			out_pos->y = (dot1 * plane2_nor.x - dot2 * plane1_nor.x) / -cross.z;
			out_pos->z = 0;
			return true;
		}
	}
	if (cross.y != 0) {
		if (out_dir) *out_dir = cross;
		if (out_pos) {
			out_pos->x = (dot1 * plane2_nor.z - dot2 * plane1_nor.z) / -cross.y;
			out_pos->y = 0;
			out_pos->z = (dot1 * plane2_nor.x - dot2 * plane1_nor.x) / cross.y;
			return true;
		}
	}
	if (cross.x != 0) {
		if (out_dir) *out_dir = cross;
		if (out_pos) {
			out_pos->x = 0;
			out_pos->y = (dot1 * plane2_nor.z - dot2 * plane1_nor.z) / cross.x;
			out_pos->z = (dot1 * plane2_nor.y - dot2 * plane1_nor.y) / -cross.x;
			return true;
		}
	}
	return false; // cross の成分がすべてゼロ。２平面は平行で交線なし
}

#pragma endregion // KGeom






#pragma region KCollisionMath
/// 三角形の法線ベクトルを返す
///
/// 三角形法線ベクトル（正規化済み）を得る
/// 時計回りに並んでいるほうを表とする
bool KCollisionMath::getTriangleNormal(KVec3 *result, const KVec3 &o, const KVec3 &a, const KVec3 &b) {
	const KVec3 ao = a - o;
	const KVec3 bo = b - o;
	const KVec3 normal = ao.cross(bo);
	KVec3 nor;
	if (normal.getNormalizedSafe(&nor)) {
		if (result) *result = nor;
		return true;
	}
	// 三角形が線分または点に縮退している。
	// 法線は定義できない
	return false;
}

bool KCollisionMath::isAabbIntersected(const KVec3 &pos1, const KVec3 &halfsize1, const KVec3 &pos2, const KVec3 &halfsize2, KVec3 *intersect_center, KVec3 *intersect_halfsize) {
	const KVec3 span = pos1 - pos2;
	const KVec3 maxspan = halfsize1 + halfsize2;
	if (fabsf(span.x) > maxspan.x) return false;
	if (fabsf(span.y) > maxspan.y) return false;
	if (fabsf(span.z) > maxspan.z) return false;
	const KVec3 min1 = pos1 - halfsize1;
	const KVec3 max1 = pos1 + halfsize1;
	const KVec3 min2 = pos2 - halfsize2;
	const KVec3 max2 = pos2 + halfsize2;
	const KVec3 intersect_min(
		KMath::max(min1.x, min2.x),
		KMath::max(min1.y, min2.y),
		KMath::max(min1.z, min2.z));
	const KVec3 intersect_max(
		KMath::min(max1.x, max2.x),
		KMath::min(max1.y, max2.y),
		KMath::min(max1.z, max2.z));
	if (intersect_center) {
		*intersect_center = (intersect_max + intersect_min) / 2;
	}
	if (intersect_halfsize) {
		*intersect_halfsize = (intersect_max - intersect_min) / 2;
	}
	return true;
}
float KCollisionMath::getPointInLeft2D(float px, float py, float ax, float ay, float bx, float by) {
	return KGeom::K_GeomPointIsLeft2D(px, py, ax, ay, bx, by);
}
float KCollisionMath::getSignedDistanceOfLinePoint2D(float px, float py, float ax, float ay, float bx, float by) {
	float ab_x = bx - ax;
	float ab_y = by - ay;
	float ap_x = px - ax;
	float ap_y = py - ay;
	float cross = ab_x * ap_y - ab_y * ap_x;
	float delta = sqrtf(ab_x * ab_x + ab_y * ab_y);
	return cross / delta;
}
float KCollisionMath::getPerpendicularLinePoint(const KVec3 &p, const KVec3 &a, const KVec3 &b) {
	KVec3 ab = b - a;
	KVec3 ap = p - a;
	float k = ab.dot(ap) / ab.getLengthSq();
	return k;
}
float KCollisionMath::getPerpendicularLinePoint2D(float px, float py, float ax, float ay, float bx, float by) {
	KVec3 ab(bx - ax, by - ay, 0.0f);
	KVec3 ap(px - ax, py - ay, 0.0f);
	float k = ab.dot(ap) / ab.getLengthSq();
	return k;
}
bool KCollisionMath::isPointInRect2D(float px, float py, float cx, float cy, float xHalf, float yHalf) {
	if (px < cx - xHalf) return false;
	if (px > cx + xHalf) return false;
	if (py < cy - yHalf) return false;
	if (py > cy + yHalf) return false;
	return true;
}
bool KCollisionMath::isPointInXShearedRect2D(float px, float py, float cx, float cy, float xHalf, float yHalf, float xShear) {
	float ymin = cy - yHalf;
	float ymax = cy + yHalf;
	if (py < ymin) return false; // 下辺より下側にいる
	if (py > ymax) return false; // 上辺より上側にいる
	if (getPointInLeft2D(px, py, cx-xHalf-xShear, ymin, cx-xHalf+xShear, ymax) > 0) return false; // 左下から左上を見たとき、左側にいる
	if (getPointInLeft2D(px, py, cx+xHalf-xShear, ymin, cx+xHalf+xShear, ymax) < 0) return false; // 右下から右上を見た時に右側にいる
	return true;
}
bool KCollisionMath::collisionCircleWithPoint2D(float cx, float cy, float cr, float px, float py, float skin, float *xAdj, float *yAdj) {
//	K__ASSERT(cr > 0);
	K__ASSERT(skin >= 0);
	float dx = cx - px;
	float dy = cy - py;
	float dist = hypotf(dx, dy);
	if (fabsf(dist) < cr+skin) {
		if (xAdj || yAdj) {
			// めり込み量
			float penetration_depth = cr - dist;
			if (penetration_depth <= 0) {
				// 押し戻し不要（実際には接触していないが、skin による拡大判定が接触した）
				if (xAdj) *xAdj = 0;
				if (yAdj) *yAdj = 0;
			} else {
				// 押し戻し方向の単位ベクトル
				float normal_x = (dist > 0) ? dx / dist : 1.0f;
				float normal_y = (dist > 0) ? dy / dist : 0.0f;
				// 押し戻し量
				if (xAdj) *xAdj = normal_x * penetration_depth;
				if (yAdj) *yAdj = normal_y * penetration_depth;
			}
		}
		return true;
	}
	return false;
}
bool KCollisionMath::collisionCircleWithLine2D(float cx, float cy, float cr, float x0, float y0, float x1, float y1, float skin, float *xAdj, float *yAdj) {
//	K__ASSERT(cr > 0);
	K__ASSERT(skin >= 0);
	float dist = getSignedDistanceOfLinePoint2D(cx, cy, x0, y0, x1, y1);
	if (fabsf(dist) < cr+skin) {
		if (xAdj || yAdj) {
			// めり込み量
			float penetration_depth = cr - fabsf(dist);
			if (penetration_depth < 0) {
				// 実際には接触していないが、skin による拡大判定が接触した。押し戻し不要
				if (xAdj) *xAdj = 0;
				if (yAdj) *yAdj = 0;
			} else {
				// 押し戻し方向の単位ベクトル
				// (x0, y0) から (x1, y1) を見たときに、左側を法線方向とする 
				float dx = x1 - x0;
				float dy = y1 - y0;
				float dr = hypotf(dx, dy);
				float normal_x = -dy / dr;
				float normal_y =  dx / dr;
				// 押し戻し量
				if (xAdj) *xAdj = normal_x * penetration_depth;
				if (yAdj) *yAdj = normal_y * penetration_depth;
			}
		}
		return true;
	}
	return false;
}
bool KCollisionMath::collisionCircleWithLinearArea2D(float cx, float cy, float cr, float x0, float y0, float x1, float y1, float skin, float *xAdj, float *yAdj) {
//	K__ASSERT(cr > 0);
	K__ASSERT(skin >= 0);
	float dist = getSignedDistanceOfLinePoint2D(cx, cy, x0, y0, x1, y1);

	if (dist < cr+skin) {
		if (xAdj || yAdj) {
			// めり込み量
			float penetration_depth = cr - dist;
			if (penetration_depth < 0) {
				// 実際には接触していないが、skin による拡大判定が接触した。押し戻し不要
				if (xAdj) *xAdj = 0;
				if (yAdj) *yAdj = 0;
			} else {
				// 押し戻し方向の単位ベクトル
				// (x0, y0) から (x1, y1) を見たときに、左側を法線方向とする 
				float dx = x1 - x0;
				float dy = y1 - y0;
				float dr = hypotf(dx, dy);
				float normal_x = -dy / dr;
				float normal_y =  dx / dr;
				// 押し戻し量
				if (xAdj) *xAdj = normal_x * penetration_depth;
				if (yAdj) *yAdj = normal_y * penetration_depth;
			}
		}
		return true;
	}
	return false;
}
bool KCollisionMath::collisionCircleWithSegment2D(float cx, float cy, float cr, float x0, float y0, float x1, float y1, float skin, float *xAdj, float *yAdj) {
	float k = getPerpendicularLinePoint2D(cx, cy, x0, y0, x1, y1);
	if (k < 0.0f) {
		if (collisionCircleWithPoint2D(cx, cy, cr, x0, y0, skin, xAdj, yAdj))
			return true;
	} else if (k < 1.0f) {
		if (collisionCircleWithLine2D(cx, cy, cr, x0, y0, x1, y1, skin, xAdj, yAdj))
			return true; 
	} else {
		if (collisionCircleWithPoint2D(cx, cy, cr, x1, y1, skin, xAdj, yAdj))
			return true;
	}
	return false;
}
bool KCollisionMath::collisionCircleWithCircle2D(float cx, float cy, float cr, float other_x, float other_y, float other_r, float skin, float *xAdj, float *yAdj) {
//	K__ASSERT(cr > 0);
	K__ASSERT(skin >= 0);
	float dx = cx - other_x;
	float dy = cy - other_y;
	float dist = hypotf(dx, dy);
	float penetration_depth = cr + other_r - dist; // めり込み深さ
	if (penetration_depth <= -skin) {
		return false; // 非接触
	}
	if (penetration_depth < 0) {
		// 実際には接触していないが、skin による拡大判定が接触した。押し戻し不要
		if (xAdj) *xAdj = 0;
		if (yAdj) *yAdj = 0;
	} else {
		// 押し戻し方向の単位ベクトル
		float _cos = (dist > 0) ? dx / dist : 1;
		float _sin = (dist > 0) ? dy / dist : 0;
		// 押し戻し量
		if (xAdj) *xAdj = penetration_depth * _cos;
		if (yAdj) *yAdj = penetration_depth * _sin;
	}
	return true;
}
int KCollisionMath::collisionCircleWithRect2D(float cx, float cy, float cr, float xBox, float yBox, float xHalf, float yHalf, float skin, float *xAdj, float *yAdj) {
	return collisionCircleWithXShearedRect2D(cx, cy, cr, xBox, yBox, xHalf, yHalf, skin, 0, xAdj, yAdj);
}
int KCollisionMath::collisionCircleWithXShearedRect2D(float cx, float cy, float cr, float xBox, float yBox, float xHalf, float yHalf, float xShear, float skin, float *xAdj, float *yAdj) {
	float x0 = xBox - xHalf; // 左
	float y0 = yBox - yHalf; // 下
	float x1 = xBox + xHalf; // 右
	float y1 = yBox + yHalf; // 上
	// 多角形は時計回りにチェックする。角に重なった場合など複数の偏と衝突する場合があるので必ず全ての面の判定を行う
	float xOld = cx;
	float yOld = cy;
	// 左下からスタート

	// 1 左(X負）
	// 2 上(Y正）
	// 4 右(X正）
	// 8 下(Y負）
	int hit = 0;

	float adjx, adjy;
	if (KCollisionMath::collisionCircleWithSegment2D(cx, cy, cr, x0-xShear, y0, x0+xShear, y1, skin, &adjx, &adjy)) {
		cx += adjx; cy += adjy;
		hit |= 1; // 左側
	}
	if (KCollisionMath::collisionCircleWithSegment2D(cx, cy, cr, x0+xShear, y1, x1+xShear, y1, skin, &adjx, &adjy)) {
		cx += adjx; cy += adjy;
		hit |= 2; // 上側
	}
	if (KCollisionMath::collisionCircleWithSegment2D(cx, cy, cr, x1+xShear, y1, x1-xShear, y0, skin, &adjx, &adjy)) {
		cx += adjx; cy += adjy;
		hit |= 4; // 右側
	}
	if (KCollisionMath::collisionCircleWithSegment2D(cx, cy, cr, x1-xShear, y0, x0-xShear, y0, skin, &adjx, &adjy)) {
		cx += adjx; cy += adjy;
		hit |= 8; // 下側
	}
	if (xAdj) *xAdj = cx - xOld;
	if (yAdj) *yAdj = cy - yOld;
	return hit;
}
// r1, r2 -- radius
// k1, k2 -- penetratin response
bool KCollisionMath::resolveYAxisCylinderCollision(KVec3 *p1, float r1, float k1, KVec3 *p2, float r2, float k2, float skin) {
	float xAdj, zAdj;
	if (collisionCircleWithCircle2D(p1->x, p1->z, r1, p2->x, p2->z, r2, skin, &xAdj, &zAdj)) {
		if (k1 + k2 > 0) {
			p1->x += xAdj * k1;
			p1->z += zAdj * k1;
			p2->x -= xAdj * k2;
			p2->z -= zAdj * k2;
		}
		return true;
	}
	return false;
}
#pragma endregion // KCollisionMath



#pragma region KCubicBezier
#pragma region KCubicBezier functions
static KVec3 kk_BezPos(const KVec3 *p4, float t) {
	// http://geom.web.fc2.com/geometry/bezier/cubic.html
	K__ASSERT(p4);
	K__ASSERT(0 <= t && t <= 1);
	float T = 1.0f - t;
	float x = t*t*t*p4[3].x + 3*t*t*T*p4[2].x + 3*t*T*T*p4[1].x + T*T*T*p4[0].x;
	float y = t*t*t*p4[3].y + 3*t*t*T*p4[2].y + 3*t*T*T*p4[1].y + T*T*T*p4[0].y;
	float z = t*t*t*p4[3].z + 3*t*t*T*p4[2].z + 3*t*T*T*p4[1].z + T*T*T*p4[0].z;
	return KVec3(x, y, z);
}
static KVec3 kk_BezTan(const KVec3 *p4, float t) {
	// http://geom.web.fc2.com/geometry/bezier/cubic.html
	K__ASSERT(p4);
	K__ASSERT(0 <= t && t <= 1);
	float T = 1.0f - t;
	float dx = 3*(t*t*(p4[3].x-p4[2].x)+2*t*T*(p4[2].x-p4[1].x)+T*T*(p4[1].x-p4[0].x));
	float dy = 3*(t*t*(p4[3].y-p4[2].y)+2*t*T*(p4[2].y-p4[1].y)+T*T*(p4[1].y-p4[0].y));
	float dz = 3*(t*t*(p4[3].z-p4[2].z)+2*t*T*(p4[2].z-p4[1].z)+T*T*(p4[1].z-p4[0].z));
	return KVec3(dx, dy, dz);
}
// 3次ベジェ曲線を div 個の直線で分割して、その長さを返す
static float kk_BezLen1(const KVec3 *p4, int div) {
	K__ASSERT(p4);
	K__ASSERT(div >= 2);
	float result = 0;
	KVec3 pa = p4[0];
	for (int i=1; i<div; i++) {
		float t = (float)i / (div - 1);
		KVec3 pb = kk_BezPos(p4, t);
		result += (pb - pa).getLength();
		pa = pb;
	}
	return result;
}

// ベジェ曲線を2分割する
static void kk_BezDiv(const KVec3 *p4, KVec3 *out8) {
	K__ASSERT(p4);
	K__ASSERT(out8);
	// 区間1
	out8[0] = p4[0];
	out8[1] = (p4[0] + p4[1]) / 2;
	out8[2] = (p4[0] + p4[1]*2 + p4[2]) / 4;
	out8[3] = (p4[0] + p4[1]*3 + p4[2]*3 + p4[3]) / 8;
	// 区間2
	out8[4] = (p4[0] + p4[1]*3 + p4[2]*3 + p4[3]) / 8;
	out8[5] = (p4[1] + p4[2]*2 + p4[3]) / 4;
	out8[6] = (p4[2] + p4[3]) / 2;
	out8[7] = p4[3];
}

// 直線 ab で b 方向を見たとき、点(px, py)が左にあるなら正の値を、右にあるなら負の値を返す
// z 値は無視する
static float kk_IsLeft2D(const KVec3 &p, const KVec3 &a, const KVec3 &b) {
	KVec3 ab = b - a;
	KVec3 ap = p - a;
	return ab.x * ap.y - ab.y * ap.x;
}

// 直線 ab と点 p の符号付距離^2。
// 点 a から b を見た時に、点 p が左右どちらにあるかで符号が変わる。正の値なら左側にある
// z 値は無視する
static float kk_DistSq2D(const KVec3 &p, const KVec3 &a, const KVec3 &b) {
	KVec3 ab = b - a;
	KVec3 ap = p - a;
	float cross = ab.x * ap.y - ab.y * ap.x;
	float deltaSq = ab.x * ab.x + ab.y * ab.y;
	return cross * cross / deltaSq;
}

// 3次ベジェ曲線を再帰分割し、その長さを返す
// （未検証）
static float kk_BezLen2(const KVec3 *p4) {
	// 始点と終点
	KVec3 delta = p4[3] - p4[0];
	float delta_len = delta.getLength();

	// 中間点との許容距離を、始点終点の距離の 1/10 に設定する
	float tolerance = delta_len * 0.1f;

	// 曲線中間点 (t = 0.5)
	KVec3 middle = kk_BezPos(p4, 0.5f);

	// 始点＆終点を通る直線と、曲線中間点の距離
	float middle_dist_sq = kk_DistSq2D(middle, p4[0], p4[3]);

	// 始点＆終点を通る直線と、曲線中間点が一定以上離れているなら再分割する
	if (middle_dist_sq >= tolerance) {
		KVec3 pp8[8];
		kk_BezDiv(p4, pp8);
		float len0 = kk_BezLen2(pp8+0); // 前半区間の長さ
		float len1 = kk_BezLen2(pp8+4); // 後半区間の長さ
		return len0 + len1;
	}

	// 始点＆終点を通る直線と、曲線中間点がすごく近い。
	// この場合、曲線がほとんど直線になっているという場合と、
	// 制御点がアルファベットのＺ字型に配置されていて、
	// たまたま曲線中間点が始点＆終点直線に近づいているだけという場合がある
	float left1 = kk_IsLeft2D(p4[1], p4[0], p4[3]); // 始点から終点を見たときの、点1の位置
	float left2 = kk_IsLeft2D(p4[2], p4[0], p4[3]); // 始点から終点を見たときの、点2の位置

	// 直線に対する点1の距離符号と点2の距離符号が異なる場合は
	// 直線を挟んで点1と2が逆方向にある。
	// 制御点がアルファベットのＺ字型に配置されているということなので、再分割する
	if (left1 * left2 < 0) {
		KVec3 pp8[8];
		kk_BezDiv(p4, pp8);
		float len0 = kk_BezLen2(pp8+0); // 前半区間の長さ
		float len1 = kk_BezLen2(pp8+4); // 後半区間の長さ
		return len0 + len1;
	}

	// 曲線がほとんど直線と同じ。
	// 始点と終点の距離をそのまま曲線の距離として返す
	return delta.getLength();
}
#pragma endregion // KCubicBezier functions


#pragma region KCubicBezier methods
KCubicBezier::KCubicBezier() {
	clear();
}
void KCubicBezier::clear() {
	length_ = 0;
	dirty_length_ = true;
	points_.clear();
}
bool KCubicBezier::empty() const {
	return points_.empty();
}
int KCubicBezier::getSegmentCount() const {
	return (int)points_.size() / 4;
}
void KCubicBezier::setSegmentCount(int count) {
	if (count < 0) return;
	points_.resize(count * 4);
	dirty_length_ = true;
}
float KCubicBezier::getWholeLength() const {
	if (dirty_length_) {
		dirty_length_ = false;
		length_ = 0;
		for (int i=0; i<getSegmentCount(); i++) {
			length_ += getLength(i);
		}
	}
	return length_;
}
KVec3 KCubicBezier::getCoordinates(int seg, float t) const {
	KVec3 ret;
	getCoordinates(seg, t, &ret);
	return ret;
}
bool KCubicBezier::getCoordinates(int seg, float t, KVec3 *pos) const {
	if (0 <= seg && seg < getSegmentCount()) {
		if (pos) *pos = kk_BezPos(&points_[seg * 4], t);
		return true;
	}
	return false;
}
KVec3 KCubicBezier::getCoordinatesEx(float t) const {
	t = KMath::clamp01(t);
	int numseg = getSegmentCount();
	int seg = (int)(t * numseg);
	float t0 = (float)(seg+0) / numseg;
	float t1 = (float)(seg+1) / numseg;
	float tt = KMath::linearStep(t0, t1, t);
	return getCoordinates(seg, tt);
}
KVec3 KCubicBezier::getTangent(int seg, float t) const {
	KVec3 ret;
	getTangent(seg, t, &ret);
	return ret;
}
bool KCubicBezier::getTangent(int seg, float t, KVec3 *tangent) const {
	if (0 <= seg && seg < getSegmentCount()) {
		if (tangent) *tangent = kk_BezTan(&points_[seg * 4], t);
		return true;
	}
	return false;
}
float KCubicBezier::getLength(int seg) const {
	if (0 <= seg && seg < getSegmentCount()) {
		return getLength_Test1(seg, 16);
	}
	return 0.0f;
}
float KCubicBezier::getLength_Test1(int seg, int numdiv) const {
	// 直線分割による近似
	K__ASSERT(0 <= seg && seg < (int)points_.size());
	return kk_BezLen1(&points_[seg * 4], 16);
}
float KCubicBezier::getLength_Test2(int seg) const {
	// ベジェ曲線の再帰分割による近似
	K__ASSERT(0 <= seg && seg < (int)points_.size());
	return kk_BezLen2(&points_[seg * 4]);
}
void KCubicBezier::addSegment(const KVec3 &a, const KVec3 &b, const KVec3 &c, const KVec3 &d) {
	points_.push_back(a);
	points_.push_back(b);
	points_.push_back(c);
	points_.push_back(d);
	dirty_length_ = true;
}
void KCubicBezier::setPoint(int seg, int point, const KVec3 &pos) {
	int i = seg*4 + point;
	if (0 <= i && i < (int)points_.size()) {
		points_[seg*4 + point] = pos;
		dirty_length_ = true;
	}
}
void KCubicBezier::setPoint(int serial_index, const KVec3 &pos) {
	int seg = serial_index / 4;
	int idx = serial_index % 4;
	setPoint(seg, idx, pos);
}
KVec3 KCubicBezier::getPoint(int seg, int point) const {
	int i = seg*4 + point;
	if (0 <= i && i < (int)points_.size()) {
		return points_[seg*4 + point];
	}
	return KVec3();
}
KVec3 KCubicBezier::getPoint(int serial_index) const {
	int seg = serial_index / 4;
	int idx = serial_index % 4;
	return getPoint(seg, idx);
}
const KVec3 * KCubicBezier::getPointArray() const {
	return points_.data();
}
int KCubicBezier::getPointCount() const {
	return points_.size();
}
bool KCubicBezier::getFirstAnchor(int seg, KVec3 *anchor, KVec3 *handle) const {
	if (0 <= seg && seg < getSegmentCount()) {
		if (anchor) *anchor = getPoint(seg, 0);
		if (handle) *handle = getPoint(seg, 1);
		return true;
	}
	return false;
}
bool KCubicBezier::getSecondAnchor(int seg, KVec3 *handle, KVec3 *anchor) const {
	if (0 <= seg && seg < getSegmentCount()) {
		if (handle) *handle = getPoint(seg, 2);
		if (anchor) *anchor = getPoint(seg, 3);
		return true;
	}
	return false;
}
void KCubicBezier::setFirstAnchor(int seg, const KVec3 &anchor, const KVec3 &handle) {
	if (0 <= seg && seg < getSegmentCount()) {
		setPoint(seg, 0, anchor);
		setPoint(seg, 1, handle);
	}
}
void KCubicBezier::setSecondAnchor(int seg, const KVec3 &handle, const KVec3 &anchor) {
	if (0 <= seg && seg < getSegmentCount()) {
		setPoint(seg, 2, handle);
		setPoint(seg, 3, anchor);
	}
}
#pragma endregion // KCubicBezier methods
#pragma endregion // KCubicBezier




#pragma region KRect
KRect::KRect():
	xmin(0),
	ymin(0),
	xmax(0),
	ymax(0)
{
	sort();
}
KRect::KRect(const KRect &rect):
	xmin(rect.xmin),
	ymin(rect.ymin),
	xmax(rect.xmax),
	ymax(rect.ymax)
{
	sort();
}
KRect::KRect(int _xmin, int _ymin, int _xmax, int _ymax):
	xmin((float)_xmin),
	ymin((float)_ymin),
	xmax((float)_xmax),
	ymax((float)_ymax)
{
	sort();
}
KRect::KRect(float _xmin, float _ymin, float _xmax, float _ymax):
	xmin(_xmin),
	ymin(_ymin),
	xmax(_xmax),
	ymax(_ymax) {
	sort();
}
KRect::KRect(const KVec2 &pmin, const KVec2 &pmax):
	xmin(pmin.x),
	ymin(pmin.y),
	xmax(pmax.x),
	ymax(pmax.y) {
	sort();
}
KRect KRect::getOffsetRect(const KVec2 &delta) const {
	return KRect(
		delta.x + xmin,
		delta.y + ymin,
		delta.x + xmax,
		delta.y + ymax);
}
KRect KRect::getOffsetRect(float dx, float dy) const {
	return KRect(
		dx + xmin,
		dy + ymin,
		dx + xmax,
		dy + ymax);
}
void KRect::offset(float dx, float dy) {
	xmin += dx;
	ymin += dy;
	xmax += dx;
	ymax += dy;
}
void KRect::offset(int dx, int dy) {
	xmin += dx;
	ymin += dy;
	xmax += dx;
	ymax += dy;
}
KRect KRect::getInflatedRect(float dx, float dy) const {
	return KRect(xmin - dx, ymin - dy, xmax + dx, ymax + dy);
}
bool KRect::isIntersectedWith(const KRect &rc) const {
	if (xmin > rc.xmax) return false;
	if (xmax < rc.xmin) return false;
	if (ymin > rc.ymax) return false;
	if (ymax < rc.ymin) return false;
	return true;
}
KRect KRect::getIntersectRect(const KRect &rc) const {
	if (isIntersectedWith(rc)) {
		return KRect(
			KMath::max(xmin, rc.xmin),
			KMath::max(ymin, rc.ymin),
			KMath::min(xmax, rc.xmax),
			KMath::min(ymax, rc.ymax)
		);
	} else {
		return KRect();
	}
}
KRect KRect::getUnionRect(const KVec2 &p) const {
	return KRect(
		KMath::min(xmin, p.x),
		KMath::min(ymin, p.y),
		KMath::max(xmax, p.x),
		KMath::max(ymax, p.y)
	);
}
KRect KRect::getUnionRect(const KRect &rc) const {
	return KRect(
		KMath::min(xmin, rc.xmin),
		KMath::min(ymin, rc.ymin),
		KMath::max(xmax, rc.xmax),
		KMath::max(ymax, rc.ymax)
	);
}
bool KRect::isZeroSized() const {
	return KMath::equals(xmin, xmax) || KMath::equals(ymin, ymax);
}
bool KRect::isEqual(const KRect &rect) const {
	return 
		KMath::equals(xmin, rect.xmin) &&
		KMath::equals(xmax, rect.xmax) &&
		KMath::equals(ymin, rect.ymin) &&
		KMath::equals(ymax, rect.ymax);
}
bool KRect::contains(float x, float y) const {
	return (xmin <= x && x < xmax) &&
		   (ymin <= y && y < ymax);
}
bool KRect::contains(int x, int y) const {
	return (xmin <= (float)x && (float)x < xmax) &&
		   (ymin <= (float)y && (float)y < ymax);
}
bool KRect::contains(const KVec2 &p) const {
	return (xmin <= p.x && p.x < xmax) &&
		   (ymin <= p.y && p.y < ymax);
}
KVec2 KRect::getCenter() const {
	return KVec2(
		(xmin + xmax) / 2.0f,
		(ymin + ymax) / 2.0f
	);
}
KVec2 KRect::getMinPoint() const {
	return KVec2(xmin, ymin);
}
KVec2 KRect::getMaxPoint() const {
	return KVec2(xmax, ymax);
}
float KRect::getSizeX() const {
	return xmax - xmin;
}
float KRect::getSizeY() const {
	return ymax - ymin;
}
void KRect::inflate(float dx, float dy) {
	xmin -= dx;
	xmax += dx;
	ymin -= dy;
	ymax += dy;
}
void KRect::unionWith(const KRect &rect, bool unionX, bool unionY) {
	if (unionX) {
		xmin = KMath::min(xmin, rect.xmin);
		xmax = KMath::max(xmax, rect.xmax);
	}
	if (unionY) {
		ymin = KMath::min(ymin, rect.ymin);
		ymax = KMath::max(ymax, rect.ymax);
	}
}
void KRect::unionWithX(float x) {
	if (x < xmin) xmin = x;
	if (x > xmax) xmax = x;
}
void KRect::unionWithY(float y) {
	if (y < ymin) ymin = y;
	if (y > ymax) ymax = y;
}
void KRect::sort() {
	const auto x0 = KMath::min(xmin, xmax);
	const auto x1 = KMath::max(xmin, xmax);
	const auto y0 = KMath::min(ymin, ymax);
	const auto y1 = KMath::max(ymin, ymax);
	xmin = x0;
	ymin = y0;
	xmax = x1;
	ymax = y1;
}
#pragma endregion // KRect




#pragma region KRay
KRay::KRay() {
}
KRay::KRay(const KVec3 &_pos, const KVec3 &_dir) {
	pos = _pos;
	dir = _dir;
}
#pragma endregion // Ray



#pragma region KSphere
KSphere::KSphere() {
	this->r = 0;
}
KSphere::KSphere(const KVec3 &_pos, float _r) {
	this->pos = _pos;
	this->r = _r;
}
float KSphere::rayTest(const KRay &ray) const {
	KRayDesc desc;
	if (rayTest(ray, &desc, nullptr)) {
		return desc.dist;
	}
	return -1;
}
bool KSphere::rayTest(const KRay &ray, KRayDesc *out_near, KRayDesc *out_far) const {
	// レイと球の貫通点
	// http://marupeke296.com/COL_3D_No24_RayToSphere.html
	if (this->r <= 0) {
		return false;
	}
	KVec3 ray_nor;
	if (!ray.dir.getNormalizedSafe(&ray_nor)) {
		return false;
	}
	KVec3 p = this->pos - ray.pos;
	float A = ray_nor.dot(ray_nor);
	float B = ray_nor.dot(p);
	float C = p.dot(p) - this->r * this->r;
	float sq = B * B - A * C;
	if (sq < 0) {
		return false; // 衝突なし
	}
	float s = sqrtf(sq);
	float dist1 = (B - s) / A;
	float dist2 = (B + s) / A;
	if (dist1 < 0 || dist2 < 0) {
		return false; // レイ始点よりも後ろ側で衝突
	}
	if (out_near) {
		out_near->dist = dist1;
		out_near->pos = ray.pos + ray_nor * dist1;
		out_near->normal = (out_near->pos - this->pos).getNormalized();
	}
	if (out_far) {
		out_far->dist = dist2;
		out_far->pos = ray.pos + ray_nor * dist2;
		out_far->normal = (out_far->pos - this->pos).getNormalized();
	}
	return true;
}
bool KSphere::sphereTest(const KSphere &sphere, float *out_dist, KVec3 *out_normal) const {
	KVec3 delta = sphere.pos - this->pos;
	float len = delta.getLength();
	if (len < sphere.r + this->r) {
		if (out_dist) *out_dist = len - (sphere.r + this->r);
		if (out_normal) delta.getNormalizedSafe(out_normal);
		return true;
	}
	return false;
}
float KSphere::perpendicularPoint(const KVec3 &p, KVec3 *out_pos) const {
	K__ASSERT_RETURN_VAL(this->r >= 0, -1);
	KVec3 delta = p - this->pos;
	float len = delta.getLength();
	if (this->r == 0) {
		if (out_pos) *out_pos = this->pos;
		return len;
	}
	if (this->r <= len) {
		KVec3 delta_nor = delta.getNormalized();
		if (out_pos) *out_pos = this->pos + delta_nor * this->r;
		return len - this->r;
	}
	return -1;
}
float KSphere::distance(const KVec3 &p) const {
	return perpendicularPoint(p, nullptr);
}
#pragma endregion // KSphere



#pragma region KPlane
KPlane::KPlane() {
}
KPlane::KPlane(const KVec3 &_pos, const KVec3 &_normal) {
	m_pos = _pos;
	m_normal = _normal;
	normalizeThis();
}
void KPlane::normalizeThis() {
	m_normal = m_normal.getNormalized();
}
float KPlane::perpendicularPoint(const KVec3 &p, KVec3 *out_pos) const {
	// https://math.keicode.com/vector-calculus/distance-from-plane-to-point.php
	// 点 p と、原点を通る法線 n の平面との距離は
	// dist = |p dot n|
	// で求まる。これで点 p から法線交点までの距離がわかるので、法線ベクトルを利用して
	// 点 p から dist だけ離れた位置の点を得る。
	// dist = p + nor * |p dot n|
	// ただしここで使いたいのは点から面に向かうベクトルであり、面の法線ベクトルとは逆向きなので
	// 符号を反転させておく
	// dist = p - nor * |p dot n|
	if (!m_normal.isZero()) {
		KVec3 relpos = p - m_pos;
		float dist = relpos.dot(m_normal);
		if (out_pos) *out_pos = p - m_normal * dist;
		return dist;
	}
	K__MATH_ERROR;
	return 0;
}
float KPlane::distance(const KVec3 &p) const {
	KVec3 relpos = p - m_pos;
	return relpos.dot(m_normal);
}
float KPlane::rayTest(const KRay &ray) const {
	KRayDesc desc;
	if (rayTest(ray, &desc)) {
		return desc.dist;
	}
	return -1;
}
bool KPlane::rayTest(const KRay &ray, KRayDesc *out_point) const {
	// https://gdbooks.gitbooks.io/3dcollisions/content/Chapter3/raycast_plane.html
	// https://t-pot.com/program/93_RayTraceTriangle/index.html
	KVec3 plane_nor;
	KVec3 ray_nor;
	if (!m_normal.getNormalizedSafe(&plane_nor)) return false;
	if (!ray.dir.getNormalizedSafe(&ray_nor)) return false;
	KVec3 delta = m_pos - ray.pos;
	float pn = plane_nor.dot(delta);
	float nd = plane_nor.dot(ray_nor);
	if (nd == 0) {
		return false; // レイは平面に平行
	}
	float t = pn / nd;
	if (t < 0) {
		return false; // 交点はレイ始点の後方にある
	}
	if (out_point) {
		out_point->dist = t;
		out_point->pos = ray.pos + ray_nor * t;
		out_point->normal = plane_nor;
	}
	return true;
}
bool KPlane::sphereTest(const KSphere &sphere, float *out_dist, KVec3 *out_normal) const {
	// wpos からこの平面への垂線の交点
	KVec3 foot_wpos = KGeom::K_GeomPerpendicularToPlane(sphere.pos, m_pos, m_normal); // 回転を考慮しないので法線は変わらない

//	if (is_point_in_trim(foot_wpos - this->pos)) {
		KVec3 delta = sphere.pos - foot_wpos; // 平面から球への垂線ベクトル
		float dist = delta.getLength();

		if (delta.dot(m_normal) < 0) {
			// 球は平面の裏側にある。負の距離を使う
			dist = -dist;
		}

		if (dist <= sphere.r) {
			if (out_dist) *out_dist = dist;
			if (out_normal) *out_normal = m_normal;
			return true;
		}
//	}

	// TrimAABB のエッジ部分との衝突は考慮しない
	return false;
}
#pragma endregion // KPlane



#pragma region KTriangle
KTriangle::KTriangle() {
}
KTriangle::KTriangle(const KVec3 &_a, const KVec3 &_b, const KVec3 &_c) {
	this->a = _a;
	this->b = _b;
	this->c = _c;
}
KVec3 KTriangle::getNormal() const {
	KVec3 nor;
	if (KGeom::K_GeomTriangleNormal(this->a, this->b, this->c, &nor)) {
		return nor;
	}
	return KVec3();
}
bool KTriangle::isPointInside(const KVec3 &p) const {
	return KGeom::K_GeomPointInTriangle(p, a, b, c);
}
bool KTriangle::perpendicularPoint(const KVec3 &p, float *out_dist, KVec3 *out_pos) const {
	return KGeom::K_GeomPerpendicularToTriangle(p, a, b, c, out_pos);
}
float KTriangle::rayTest(const KRay &ray) const {
	KRayDesc desc;
	if (rayTest(ray, &desc)) {
		return desc.dist;
	}
	return -1;
}
bool KTriangle::rayTest(const KRay &ray, KRayDesc *out_point) const {
	KVec3 ray_nor;
	if (!ray.dir.getNormalizedSafe(&ray_nor)) {
		return false;
	}
	// 三角形の面法線を求める
	KVec3 nor;
	if (!KGeom::K_GeomTriangleNormal(this->a, this->b, this->c, &nor)) {
		return false;
	}

	// [レイ]と[三角形を含む平面]との交点距離
	KRayDesc hit;
	KPlane plane(this->a, nor);
	if (!plane.rayTest(ray, &hit)) {
		return false;
	}

	// 交点が三角形の内部にあるかどうか
	if (!KGeom::K_GeomPointInTriangle(hit.pos, this->a, this->b, this->c)) {
		return false;
	}

	if (out_point) {
		out_point->dist = hit.dist;
		out_point->pos = hit.pos;
		out_point->normal = nor;
	}
	return true;
}
bool KTriangle::sphereTest(const KSphere &sphere, float *out_dist, KVec3 *out_normal) const {
	float dist = 0;
	if (perpendicularPoint(sphere.pos, &dist, nullptr)) {
		if (dist < sphere.r) {
			if (out_dist) *out_dist = dist;
			if (out_normal) * out_normal = getNormal();
			return true;
		}
	}
	return false;
}
#pragma endregion // KTriangle



#pragma region KAabb
KAabb::KAabb() {
}
KAabb::KAabb(const KVec3 &_pos, const KVec3 &_halfsize) {
	this->pos = _pos;
	this->halfsize = _halfsize;
}
KVec3 KAabb::getMin() const {
	return pos - halfsize;
}
KVec3 KAabb::getMax() const {
	return pos + halfsize;
}
float KAabb::rayTest(const KRay &ray) const {
	KRayDesc desc;
	if (rayTest(ray, &desc, nullptr)) {
		return desc.dist;
	}
	return -1;
}
bool KAabb::rayTest(const KRay &ray, KRayDesc *out_near, KRayDesc *out_far) const {
	// 直線とAABB
	// http://marupeke296.com/COL_3D_No18_LineAndAABB.html
	// https://gdbooks.gitbooks.io/3dcollisions/content/Chapter3/raycast_aabb.html]
	// https://gamedev.stackexchange.com/questions/18436/most-efficient-aabb-vs-ray-collision-algorithms
	// 
	KVec3 ray_nor;
	if (!ray.dir.getNormalizedSafe(&ray_nor)) {
		return false;
	}
	const KVec3 minpos = this->pos - this->halfsize;
	const KVec3 maxpos = this->pos + this->halfsize;
	const float pos[] = {ray.pos.x, ray.pos.y, ray.pos.z};
	const float dir[] = {ray_nor.x, ray_nor.y, ray_nor.z };
	const float aabbmin[] = {minpos.x, minpos.y, minpos.z};
	const float aabbmax[] = {maxpos.x, maxpos.y, maxpos.z};
	float t_near = -FLT_MAX;
	float t_far = FLT_MAX;
	int hit_near = 0; // どの面と交差しているか？
	int hit_far = 0;
	for (int i=0; i<3; i++) {
		if (dir[i] == 0) {
			if (pos[i] < aabbmin[i] || pos[i] > aabbmax[i]) {
				return false; // 交差なし
			}
		}
		else {
			// スラブとの距離を算出
			float inv_dir = 1.0f / dir[i];
			float dist_min = (aabbmin[i] - pos[i]) * inv_dir; // min面との距離
			float dist_max = (aabbmax[i] - pos[i]) * inv_dir; // max面との距離
			if (dist_min > dist_max) {
				std::swap(dist_min, dist_max);
			}
			if (dist_min > t_near) {
				t_near = dist_min;
				hit_near = i;
			}
			if (dist_max < t_far) {
				t_far = dist_max;
				hit_far = i;
			}
			// スラブ交差チェック
			if (t_near > t_far) {
				return false;
			}
		}
	}
	// 交差あり
	// *out_pos_near = ray_pos + ray_nor * t_near;
	// *out_pos_far = ray_pos + ray_nor * t_far;

	// AABBの法線ベクトル
	static const KVec3 NORMALS[] = {
		KVec3(-1, 0, 0), // xmin
		KVec3( 1, 0, 0), // xmax
		KVec3( 0,-1, 0), // ymin
		KVec3( 0, 1, 0), // ymax
		KVec3( 0, 0,-1), // zmin
		KVec3( 0, 0, 1), // zmax
	};
	if (out_near) {
		out_near->dist = t_near;
		out_near->pos = ray.pos + ray_nor * t_near;

		// 近交点におけるAABB法線。
		// [レイの発射点 < aabbmin] なら min 側に当たっている。
		// [レイの発射点 > aabbmax] なら max 側に当たっている。
		// [aabbmin <= レイの発射点 <= aabbmax] のパターンは既に「交差なし」として除外されている筈なので考慮しない
		// ※hit_near には 0, 1, 2 のいずれかが入っていて、それぞれ X,Y,Z に対応する
		K__ASSERT(0 <= hit_near && hit_near < 3);
		if (pos[hit_near] < aabbmin[hit_near]) {
			out_near->normal = NORMALS[hit_near * 2 + 0]; // min 側
		} else {
			out_near->normal = NORMALS[hit_near * 2 + 1]; // max 側
		}
	}
	if (out_far) {
		out_far->dist = t_far;
		out_far->pos = ray.pos + ray_nor * t_far;

		// 遠交点におけるAABB法線。
		// [レイの発射点 < aabbmin] なら min 側に当たっている。
		// [レイの発射点 > aabbmax] なら max 側に当たっている。
		// [aabbmin <= レイの発射点 <= aabbmax] のパターンは既に「交差なし」として除外されている筈なので考慮しない
		// ※hit_far には 0, 1, 2 のいずれかが入っていて、それぞれ X,Y,Z に対応する
		K__ASSERT(0 <= hit_far && hit_far < 3);
		if (pos[hit_far] < aabbmin[hit_far]) {
			out_far->normal = NORMALS[hit_far * 2 + 0]; // min 側
		} else {
			out_far->normal = NORMALS[hit_far * 2 + 1]; // max 側
		}
	}
	return true;
}
bool KAabb::sphereTest(const KSphere &sphere, float *out_dist, KVec3 *out_normal) const {
	KVec3 delta = sphere.pos - this->pos;
	if (fabsf(delta.x) > this->halfsize.x + sphere.r) return false;
	if (fabsf(delta.y) > this->halfsize.y + sphere.r) return false;
	if (fabsf(delta.z) > this->halfsize.z + sphere.r) return false;

	if (fabsf(delta.y) <= this->halfsize.y && fabsf(delta.z) <= this->halfsize.z) {
		// a または b の位置にいる = X軸方向にめり込んでいる。左右にずらして衝突を解決する
		//
		//   z
		//   |       |
		//---+-------+---
		// a | AABB  | b
		//---0-------+---> x
		//   |       |
		//
		if (out_dist) *out_dist = fabsf(delta.x) - this->halfsize.x;
		if (out_normal) {
			out_normal->x = (delta.x >= 0) ? 1.0f : -1.0f;
			out_normal->y = 0;
			out_normal->z = 0;
		}
		return true;
	}
	if (fabsf(delta.x) <= this->halfsize.x && fabsf(delta.z) <= this->halfsize.z) {
		// a  の位置にいる（上面または下面） = Y軸方向にめり込んでいる。上下にずらして衝突を解決する
		//
		//   z
		//   |       |
		//---+-------+---
		//   |AA(a)BB|  
		//---0-------+---> x
		//   |       |
		//
		if (out_dist) *out_dist = fabsf(delta.y) - this->halfsize.y;
		if (out_normal) {
			out_normal->x = 0;
			out_normal->y = (delta.y >= 0) ? 1.0f : -1.0f;
			out_normal->z = 0;
		}
		return true;
	}
	if (fabsf(delta.x) <= this->halfsize.x && fabsf(delta.y) <= this->halfsize.y) {
		// a または b の位置にいる = Z軸方向にめり込んでいる。前後にずらして衝突を解決する
		//
		//   z
		//   |       |
		//---+-------+---
		// a | AABB  | b
		//---0-------+---> x
		//   |       |
		//
		if (out_dist) *out_dist = fabsf(delta.z) - this->halfsize.z;
		if (out_normal) {
			out_normal->x = 0;
			out_normal->y = 0;
			out_normal->z = (delta.z >= 0) ? 1.0f : -1.0f;
		}
		return true;
	}
	return false;
}
bool KAabb::intersect(const KAabb &a, const KAabb &b, KAabb *out) {
	KVec3 amin = a.getMin();
	KVec3 amax = a.getMax();
	KVec3 bmin = b.getMin();
	KVec3 bmax = b.getMax();
	KVec3 cmin, cmax;
	if (KGeom::K_GeomIntersectAabb(amin, amax, bmin, bmax, &cmin, &cmax)) {
		if (out) *out = KAabb((cmin+cmax)*0.5f, (cmax-cmin)*0.5f);
		return true;
	}
	return false;
}
float KAabb::distance(const KVec3 &p) const {
	KVec3 minpos = getMin();
	KVec3 maxpos = getMax();
	return KGeom::K_GeomDistancePointToAabb(p, minpos, maxpos);
}
#pragma endregion // KAabb



#pragma region KCylinder
KCylinder::KCylinder() {
	this->r = 0;
}
KCylinder::KCylinder(const KVec3 &_p0, const KVec3 &_p1, float _r) {
	this->p0 = _p0;
	this->p1 = _p1;
	this->r = _r;
}
float KCylinder::rayTest(const KRay &ray) const {
	KRayDesc desc;
	if (rayTest(ray, &desc, nullptr)) {
		return desc.dist;
	}
	return -1;
}
bool KCylinder::rayTest(const KRay &ray, KRayDesc *out_near, KRayDesc *out_far) const {
	// レイと無限円柱の貫通点
	// http://marupeke296.com/COL_3D_No25_RayToSilinder.html
	if (this->p0 == this->p1) {
		return false;
	}
	if (this->r <= 0) {
		return false;
	}
	KVec3 ray_nor;
	if (!ray.dir.getNormalizedSafe(&ray_nor)) {
		return false;
	}
	KVec3 c0 = this->p0 - ray.pos;
	KVec3 c1 = this->p1 - ray.pos;
	KVec3 s = c1 - c0;
	float Dvv = ray_nor.dot(ray_nor);
	float Dsv = ray_nor.dot(s);
	float Dpv = ray_nor.dot(c0);
	float Dss = s.dot(s);
	float Dps = s.dot(c0);
	float Dpp = c0.dot(c0);
	float A = Dvv - Dsv * Dsv / Dss;
	float B = Dpv - Dps * Dsv / Dss;
	float C = Dpp - Dps * Dps / Dss - this->r * this->r;
	float d = B * B - A * C;
	if (d < 0) {
		return false; // 衝突なし
	}
	d = sqrtf(d);
	if (out_near) {
		out_near->dist = (B - d) / A;
		out_near->pos = ray.pos + ray_nor * out_near->dist;
		out_near->normal = KGeom::K_GeomPerpendicularToLineVector(out_near->pos, this->p0, this->p1);
	}
	if (out_far) {
		out_far->dist = (B + d) / A;
		out_far->pos = ray.pos + ray_nor * out_far->dist;
		out_far->normal = KGeom::K_GeomPerpendicularToLineVector(out_far->pos, this->p0, this->p1);
	}
	return true;
}
bool KCylinder::sphereTest(const KSphere &sphere, float *out_dist, KVec3 *out_normal) const {
	KVec3 a = this->p0;
	KVec3 b = this->p1;
	float dist = KGeom::K_GeomDistancePointToLine(sphere.pos, a, b);
	if (dist < 0) return false;
	if (dist < this->r + sphere.r) {
		if (out_dist) *out_dist = dist - this->r;
		if (out_normal) {
			KVec3 delta = KGeom::K_GeomPerpendicularToLineVector(sphere.pos, a, b);
			delta = -delta; // 逆向きに
			if (!delta.getNormalizedSafe(out_normal)) {
				*out_normal = KVec3();
			}
		}
		return true;
	}
	return false;
}
float KCylinder::distance(const KVec3 &p) const {
	return KGeom::K_GeomDistancePointToLine(p, this->p0, this->p1);
}



#pragma endregion // KCylinder



#pragma region KCapsule
KCapsule::KCapsule() {
	this->r = 0;
}
KCapsule::KCapsule(const KVec3 &_p0, const KVec3 &_p1, float _r) {
	this->p0 = _p0;
	this->p1 = _p1;
	this->r = _r;
}
float KCapsule::rayTest(const KRay &ray) const {
	KRayDesc desc;
	if (rayTest(ray, &desc, nullptr)) {
		return desc.dist;
	}
	return -1;
}
bool KCapsule::rayTest(const KRay &ray, KRayDesc *out_near, KRayDesc *out_far) const {
	// レイとカプセルの貫通点
	// http://marupeke296.com/COL_3D_No26_RayToCapsule.html

	#define CHECK(a, b, c) ((a.x - b.x) * (c.x - b.x) + (a.y - b.y) * (c.y - b.y) + (a.z - b.z) * (c.z - b.z))

	if (ray.dir.isZero()) return false;
	if (this->p0 == this->p1) return false;
	if (this->r <= 0) return false;

	const KSphere sphere0(this->p0, this->r); // カプセル p0 側の球体
	const KSphere sphere1(this->p1, this->r); // カプセル p1 側の球体
	const KCylinder cylinder(this->p0, this->p1, this->r); // カプセル円筒形
	
	// 両端の球、円柱（無限長さ）との判定を行う
	KRayDesc ray_near, ray0_far, ray1_far, rayc_far;
	bool hit_p0 = sphere0.rayTest(ray, &ray_near, &ray0_far);
	bool hit_p1 = sphere1.rayTest(ray, &ray_near, &ray1_far);
	bool hit_cy = cylinder.rayTest(ray, &ray_near, &rayc_far);

	// 突入点について
	if (hit_p0 && CHECK(this->p1, this->p0, ray_near.pos) <= 0) {
		// this->p0 側の半球面に突入している
		if (out_near) *out_near = ray_near;

	} else if (hit_p1 && CHECK(this->p0, this->p1, ray_near.pos) <= 0) {
		// this->p1 側の半球面に突入している
		if (out_near) *out_near = ray_near;

	} else if (hit_cy && CHECK(this->p0, this->p1, ray_near.pos) > 0 && CHECK(this->p1, this->p0, ray_near.pos) > 0) {
		// 円柱面に突入している
		if (out_near) *out_near = ray_near;

	} else {
		return false; // 衝突なし
	}

	// 脱出点について調べる
	if (CHECK(this->p1, this->p0, ray0_far.pos) <= 0) {
		// this->p0 側の半球面から脱出している
		// this->p0 の球と判定したときの結果を使う
		if (out_far) *out_far = ray0_far;

	} else if (CHECK(this->p0, this->p1, ray1_far.pos) <= 0) {
		// this->p1 側の半球面から脱出している
		// this->p1 の球と判定したときの結果を使う
		if (out_far) *out_far = ray1_far;

	} else {
		// 円柱面から脱出している
		// 無限長さ円柱と判定したときの結果を使う
		if (out_far) *out_far = rayc_far;
	}
	return true;

	#undef CHECK
}
bool KCapsule::sphereTest(const KSphere &sphere, float *out_dist, KVec3 *out_normal) const {
	KVec3 a = this->p0;
	KVec3 b = this->p1;
	int zone = 0;
	float dist = KGeom::K_GeomDistancePointToLineSegment(sphere.pos, a, b, &zone);
	if (dist < 0) return false;
	if (zone < 0) {
		if (dist < this->r + sphere.r) {
			// a 半球側に衝突
			if (out_dist) *out_dist = dist - this->r;
			if (out_normal) {
				KVec3 delta = sphere.pos - a;
				if (!delta.getNormalizedSafe(out_normal)) {
					*out_normal = KVec3();
				}
			}
			return true;
		}
	}
	if (zone < 0) {
		if (dist < this->r + sphere.r) {
			// b 半球側に衝突
			if (out_dist) *out_dist = dist - this->r;
			if (out_normal) {
				KVec3 delta = sphere.pos - b;
				if (!delta.getNormalizedSafe(out_normal)) {
					*out_normal = KVec3();
				}
			}
			return true;
		}
	}
	if (zone == 0) {
		if (dist < this->r + sphere.r) {
			// 円筒部分に衝突
			if (out_dist) *out_dist = dist - this->r;
			if (out_normal) {
				KVec3 delta = KGeom::K_GeomPerpendicularToLineVector(sphere.pos, a, b);
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
float KCapsule::distance(const KVec3 &p) const {
	return KGeom::K_GeomDistancePointToCapsule(p, this->p0, this->p1, this->r);
}
#pragma endregion // KCapsule



namespace Test {
void Test_num() {
	K__VERIFY(KMath::max(0,  0) == 0);
	K__VERIFY(KMath::max(0, 10) == 10);
	K__VERIFY(KMath::max(10, 0) == 10);

	K__VERIFY(KMath::min(0,  0) == 0);
	K__VERIFY(KMath::min(0, 10) == 0);
	K__VERIFY(KMath::min(10, 0) == 0);

	K__VERIFY(KMath::clamp01(-1.0f) == 0.0f);
	K__VERIFY(KMath::clamp01( 0.0f) == 0.0f);
	K__VERIFY(KMath::clamp01( 0.5f) == 0.5f);
	K__VERIFY(KMath::clamp01( 1.0f) == 1.0f);
	K__VERIFY(KMath::clamp01( 2.0f) == 1.0f);

	K__VERIFY(KMath::lerp(10, 20,-0.5f) == 10);
	K__VERIFY(KMath::lerp(10, 20, 0.0f) == 10);
	K__VERIFY(KMath::lerp(10, 20, 0.5f) == 15);
	K__VERIFY(KMath::lerp(10, 20, 1.0f) == 20);
	K__VERIFY(KMath::lerp(10, 20, 1.5f) == 20);

	K__VERIFY(KMath::lerp(20, 10,-0.5f) == 20);
	K__VERIFY(KMath::lerp(20, 10, 0.0f) == 20);
	K__VERIFY(KMath::lerp(20, 10, 0.5f) == 15);
	K__VERIFY(KMath::lerp(20, 10, 1.0f) == 10);
	K__VERIFY(KMath::lerp(20, 10, 1.5f) == 10);

	K__VERIFY(KMath::linearStep(-10, 10,-20) == 0.0f);
	K__VERIFY(KMath::linearStep(-10, 10,-10) == 0.0f);
	K__VERIFY(KMath::linearStep(-10, 10,  0) == 0.5f);
	K__VERIFY(KMath::linearStep(-10, 10, 10) == 1.0f);
	K__VERIFY(KMath::linearStep(-10, 10, 20) == 1.0f);

	K__VERIFY(KMath::linearStepBumped(-10, 10,-20) == 0.0f);
	K__VERIFY(KMath::linearStepBumped(-10, 10,-10) == 0.0f);
	K__VERIFY(KMath::linearStepBumped(-10, 10, -5) == 0.5f);
	K__VERIFY(KMath::linearStepBumped(-10, 10,  0) == 1.0f);
	K__VERIFY(KMath::linearStepBumped(-10, 10,  5) == 0.5f);
	K__VERIFY(KMath::linearStepBumped(-10, 10, 10) == 0.0f);
	K__VERIFY(KMath::linearStepBumped(-10, 10, 20) == 0.0f);

	K__VERIFY(KMath::smoothStep(-10, 10,-20) == 0.0f);
	K__VERIFY(KMath::smoothStep(-10, 10,-10) == 0.0f);
	K__VERIFY(KMath::smoothStep(-10, 10,  0) == 0.5f);
	K__VERIFY(KMath::smoothStep(-10, 10, 10) == 1.0f);
	K__VERIFY(KMath::smoothStep(-10, 10, 20) == 1.0f);

	K__VERIFY(KMath::smoothStep(10, -10,-20) == 1.0f);
	K__VERIFY(KMath::smoothStep(10, -10,-10) == 1.0f);
	K__VERIFY(KMath::smoothStep(10, -10,  0) == 0.5f);
	K__VERIFY(KMath::smoothStep(10, -10, 10) == 0.0f);
	K__VERIFY(KMath::smoothStep(10, -10, 20) == 0.0f);

	K__VERIFY(KMath::smoothStepBumped(-10, 10,-20) == 0.0f);
	K__VERIFY(KMath::smoothStepBumped(-10, 10,-10) == 0.0f);
	K__VERIFY(KMath::smoothStepBumped(-10, 10, -5) == 0.5f);
	K__VERIFY(KMath::smoothStepBumped(-10, 10,  0) == 1.0f);
	K__VERIFY(KMath::smoothStepBumped(-10, 10,  5) == 0.5f);
	K__VERIFY(KMath::smoothStepBumped(-10, 10, 10) == 0.0f);
	K__VERIFY(KMath::smoothStepBumped(-10, 10, 20) == 0.0f);

	K__VERIFY(KMath::smoothStepBumped(10, -10,-20) == 0.0f);
	K__VERIFY(KMath::smoothStepBumped(10, -10,-10) == 0.0f);
	K__VERIFY(KMath::smoothStepBumped(10, -10, -5) == 0.5f);
	K__VERIFY(KMath::smoothStepBumped(10, -10,  0) == 1.0f);
	K__VERIFY(KMath::smoothStepBumped(10, -10,  5) == 0.5f);
	K__VERIFY(KMath::smoothStepBumped(10, -10, 10) == 0.0f);
	K__VERIFY(KMath::smoothStepBumped(10, -10, 20) == 0.0f);

	K__VERIFY(KMath::smoothStep(-10, 10,-20) == 0.0f);
	K__VERIFY(KMath::smoothStep(-10, 10,-10) == 0.0f);
	K__VERIFY(KMath::smoothStep(-10, 10,  0) == 0.5f);
	K__VERIFY(KMath::smoothStep(-10, 10, 10) == 1.0f);
	K__VERIFY(KMath::smoothStep(-10, 10, 20) == 1.0f);

	K__VERIFY(KMath::smootherStepBumped(-10, 10,-20) == 0.0f);
	K__VERIFY(KMath::smootherStepBumped(-10, 10,-10) == 0.0f);
	K__VERIFY(KMath::smootherStepBumped(-10, 10, -5) == 0.5f);
	K__VERIFY(KMath::smootherStepBumped(-10, 10,  0) == 1.0f);
	K__VERIFY(KMath::smootherStepBumped(-10, 10,  5) == 0.5f);
	K__VERIFY(KMath::smootherStepBumped(-10, 10, 10) == 0.0f);
	K__VERIFY(KMath::smootherStepBumped(-10, 10, 20) == 0.0f);
}
void Test_bezier() {
	#define RND (100 + rand() % 400)
	for (int i=0; i<1000; i++) {
		KVec3 p[] = {
			KVec3(RND, RND, 0),
			KVec3(RND, RND, 0),
			KVec3(RND, RND, 0),
			KVec3(RND, RND, 0)
		};
		KCubicBezier b;
		b.addSegment(p[0], p[1], p[2], p[3]);

		// 多角線による近似
		float len1 = b.getLength_Test1(0, 100);
		// 再帰分割での近似
		float len2 = b.getLength_Test2(0);

		// それぞれ算出した長さ値が一定以上異なっていれば SVG 形式で曲線パラメータを出力する
		// なお、このテキストをそのまま
		// http://www.useragentman.com/tests/textpath/bezier-curve-construction-set.html
		// に貼りつければブラウザで確認できる
		if (fabsf(len1 - len2) >= 1.0f) {
			std::string s = K::str_sprintf("M %g, %g C %g, %g, %g, %g, %g, %g", p[0].x, p[0].y, p[1].x, p[1].y, p[2].x, p[2].y, p[3].x, p[3].y);
			K::outputDebug(s);
		}
	}
	#undef RND
}

void Test_geom() {
	// 三角形の内部
	K__ASSERT(KGeom::K_GeomPointInTriangle(KVec3(50, 0, 100), KVec3(0, 0, 140), KVec3(100, 0, 140), KVec3(0, 0, 0)) == true); 

	// 三角形の辺に重なる場合も内部とみなす
	K__ASSERT(KGeom::K_GeomPointInTriangle(KVec3(0, 0, 100), KVec3(0, 0, 140), KVec3(100, 0, 140), KVec3(0, 0, 0)) == true); 

	// 三角形の頂点に重なる場合も内部とみなす
	K__ASSERT(KGeom::K_GeomPointInTriangle(KVec3(0, 0, 140), KVec3(0, 0, 140), KVec3(100, 0, 140), KVec3(0, 0, 0)) == true); 

	// 三角形の外部
	K__ASSERT(KGeom::K_GeomPointInTriangle(KVec3(200, 0, 200), KVec3(0, 0, 140), KVec3(100, 0, 140), KVec3(0, 0, 0)) == false); 

	// 三角形の外部（三角形の辺の延長線上にぴったり重なっている場合。外積をとるとゼロになってしまうので判定に注意が必要）
	K__ASSERT(KGeom::K_GeomPointInTriangle(KVec3(-250, 0, 140), KVec3(0, 0, 140), KVec3(100, 0, 140), KVec3(0, 0, 0)) == false); 
}

} // Test


} // namespace
