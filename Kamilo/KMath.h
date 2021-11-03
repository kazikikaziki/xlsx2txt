/// @file
/// Copyright (c) 2020 Heliodor
/// This software is released under the MIT License.
/// http://opensource.org/licenses/mit-license.php

#pragma once
#include <inttypes.h> // uint32_t
#include <math.h> // fabsf
#include <vector>
#include "KVec.h"
#include "KQuat.h"

#define K__MATH_ERROR  K__math_error(__FUNCTION__)


#pragma region Numeric
namespace Kamilo {

class KMatrix4;

void K__math_error(const char *func);

class KMath {
public:
	static constexpr float PI = 3.14159265358979323846f; ///< 円周率

	/// @var EPS
	/// 実数比較の時の最大許容誤差
	///
	/// KENG_STRICT_EPSILON が定義されている場合は標準ヘッダ float.h の FLT_EPSILON をそのまま使う。
	/// この場合 KVec3::isNormalized() などの判定がかなりシビアになるので注意すること。
	///
	/// KENG_STRICT_EPSILON が 0 の場合は適当な精度の機械イプシロンを使う。
	/// FLT_EPSILON の精度が高すぎて比較に失敗する場合は KENG_STRICT_EPSILON を 0 にしておく。
	#ifdef KENG_STRICT_EPSILON
	static constexpr float EPS = FLT_EPSILON;
	#else
	static constexpr float EPS = 1.0f / 8192; // 0.00012207031
	#endif

	static int min(int a, int b);
	static int min3(int a, int b, int c) { return min(min(a, b), c); }
	static int min4(int a, int b, int c, int d) { return min(min(a, b), min(c, d)); }
	
	static float min(float a, float b);
	static float min3(float a, float b, float c) { return min(min(a, b), c); }
	static float min4(float a, float b, float c, float d) { return min(min(a, b), min(c, d)); }

	static int max(int a, int b);
	static int max3(int a, int b, int c) { return max(max(a, b), c); }
	static int max4(int a, int b, int c, int d) { return max(max(a, b), max(c, d)); }
	
	static float max(float a, float b);
	static float max3(float a, float b, float c) { return max(max(a, b), c); }
	static float max4(float a, float b, float c, float d) { return max(max(a, b), max(c, d)); }


	/// x の符号を -1, 0, 1 で返す
	static float signf(float x);
	static int signi(int x);

	/// 度からラジアンに変換
	static float degToRad(float deg);

	/// ラジアンから度に変換
	static float radToDeg(float rad);

	/// x が 2 の整数乗になっているか
	static bool isPow2(int x);

	/// パーリンノイズを -1.0 以上 1.0 以下の範囲で生成する
	static float perlin(float x, float y, float z, int x_wrap, int y_wrap, int z_wrap);

	/// パーリンノイズを 0.0 以上 1.0 以下の範囲で生成する
	static float perlin01(float x, float y, float z, int x_wrap, int y_wrap, int z_wrap);

	/// 開始値 edge0 と終了値 edge1 を係数 x で線形補間する。x が edge0 と edge1 の範囲外にある場合でも戻り値を丸めない
	static float linearStepUnclamped(float edge0, float edge1, float x);
	
	/// 開始値 edge0 と終了値 edge1 を係数 x で線形補間する。いわゆる smoothstep の線形バージョン
	static float linearStep(float edge0, float edge1, float x);
	
	/// linearStep と同じだが、山型の補完になる (x=0.5 で終了値 edge1 に達し、x=0.0およびx=1.0で開始値 edge0 になる)
	static float linearStepBumped(float edge0, float edge1, float x);

	/// 開始値 edge0 と終了値 edge1 を係数 x でエルミート補完する。@see https://ja.wikipedia.org/wiki/Smoothstep
	static float smoothStep(float edge0, float edge1, float x);

	/// smoothStep と同じだが、山型の補完になる (x=0.5 で終了値 edge1 に達し、x=0.0およびx=1.0で開始値 edge0 になる)
	static float smoothStepBumped(float edge0, float edge1, float x);

	/// smoothStep よりもさらに滑らかな補間を行う @see https://en.wikipedia.org/wiki/Smoothstep
	static float smootherStep(float edge0, float edge1, float x); 
	static float smootherStepBumped(float edge0, float edge1, float x);

	/// 整数の範囲を lower と upper の範囲内に収めた値を返す
	static int clampi(int a, int lower, int upper);

	/// 実数の範囲を lower と upper の範囲内に収めた値を返す
	static float clampf(float a, float lower, float upper);
	static float clamp01(float a);

	/// 四捨五入した値を返す
	static float round(float a);

	/// a を b で割ったあまりを 0 以上 b 未満の値で求める
	/// a >= 0 の場合は a % b と全く同じ結果になる。
	/// a < 0 での挙動が a % b とは異なり、a がどんな値でも戻り値は必ず正の値になる
	static int repeati(int a, int b);

	/// a と b の剰余を 0 以上 b 未満の範囲にして返す
	/// a >= 0 の場合は fmodf(a, b) と全く同じ結果になる。
	/// a < 0 での挙動が fmodf(a, b) とは異なり、a がどんな値でも戻り値は必ず正の値になる
	static float repeatf(float a, float b);

	/// 値が 0..n-1 の範囲で繰り返されるように定義されている場合に、
	/// 値 a を基準としたときの b までの符号付距離を返す。
	/// 値 a, b は範囲 [0..n-1] 内に収まるように転写され、絶対値の小さいほうの符号付距離を返す。
	/// たとえば n=10, a=4, b=6 としたとき、a から b までの距離は 2 と -8 の二種類ある。この場合 2 を返す
	/// a=6, b=4 だった場合、距離は -2 と 8 の二種類で、この場合は -2 を返す。
	static int delta_repeati(int a, int b, int n);

	/// 範囲 [a..b] である値 t を範囲 [a..b] でスケール変換した値を返す。
	/// t が範囲外だった場合、戻り値も範囲外になる
	/// @param a 開始値
	/// @param b 終了値
	/// @param t 補間係数
	/// @return  補間結果
	static float lerpUnclamped(float a, float b, float t);
	static float lerp(float a, float b, float t);

	/// 角度（度）を -180 度以上 +180 未満に正規化する
	static float normalize_deg(float deg);

	/// 角度（ラジアン）を -PI 以上 +PI 未満に正規化する
	static float normalize_rad(float rad);

	/// a と b の差が maxerr 以下（未満ではない）であれば true を返す
	/// maxerr を 0 に設定した場合は a == b と同じことになる
	/// @param a   比較値1
	/// @param b   比較値2
	/// @param eps 同値判定の精度
	static bool equals(float a, float b, float eps=KMath::EPS);

	/// 実数が範囲 lower .. upper と等しいかその内側にあるなら true を返す
	static bool inRange(float a, float lower=0.0f, float upper=1.0f);
	static bool inRange(int a, int lower, int upper);

	/// 実数がゼロとみなせるならば true を返す
	/// @param a   比較値
	/// @param eps 同値判定の精度
	static bool isZero(float a, float eps=KMath::EPS);

	/// 三角波を生成する
	/// cycle で与えられた周期で 0 .. 1 の間を線形に往復する。
	/// t=0       ---> 0.0
	/// t=cycle/2 ---> 1.0
	/// t=cycle   ---> 0.0
	static float triangle_wave(float t, float cycle);
	static float triangle_wave(int t, int cycle);
};

#pragma endregion // Numeric









#pragma region KCubicBezier
/// 3次のベジェ曲線を扱う
///
/// 3次ベジェ曲線は、4個の制御点から成る。
/// 制御点0 : 始点 (= アンカー)
/// 制御点1 : 始点側ハンドル
/// 制御点2 : 終点側ハンドル
/// 制御点3 : 終点
/// <pre>
/// handle            handle
///  (1)                (2)
///   |    segment       |
///   |  +------------+  |
///   | /              \ |
///   |/                \|
///  [0]                [3]
/// anchor             anchor
/// </pre>
/// セグメントを連続してつなげていき、1本の連続する曲線を表す。
/// そのため、セグメント接続部分での座標と傾きは一致している必要がある
/// 次の図で言うと、セグメントAの終端点[3]と、セグメントBの始点[0]は同一座標に無いといけない。
/// また、滑らかにつながるには、セグメントAの終端傾き (2)→[3] とセグメントBの始点傾き [0]→(1)
/// が一致していないといけない
/// <pre>
///  (1)                (2)
///   |    segment A     |
///   |  +------------+  |
///   | /              \ |
///   |/                \|
///  [0]                [3]
///                     [0]                [3]
///                      |\                /|
///                      | \              / |
///                      |  +------------+  |
///                     (1)    segment B   (2)
/// </pre>
/// @see https://ja.javascript.info/bezier-curve
/// @see https://postd.cc/bezier-curves/
class KCubicBezier {
public:
	KCubicBezier();

	void clear();
	bool empty() const;

	/// 区間インデックス seg における３次ベジェ曲線の、係数 t における座標を得る。
	/// 成功した場合は pos に座標をセットして true を返す。
	/// 範囲外のインデックスを指定した場合は何もせずに false を返す
	/// @param      seg 区間インデックス
	/// @param      t   区間内の位置を表す値。区間を 0.0 以上 1.0 以下で正規化したもの
	/// @param[out] pos 曲線上の座標
	bool getCoordinates(int seg, float t, KVec3 *pos) const;
	KVec3 getCoordinates(int seg, float t) const;
	KVec3 getCoordinatesEx(float t) const;

	/// 区間インデックス seg における３次ベジェ曲線の、係数 t における傾きを得る。
	/// 成功した場合は tangent に傾きをセットして true を返す。
	/// 範囲外のインデックスを指定した場合は何もせずに false を返す
	/// @param      seg     区間インデックス
	/// @param      t       区間内の位置を表す値。区間を 0.0 以上 1.0 以下で正規化したもの
	/// @param[out] tangent 傾きの値
	bool getTangent(int seg, float t, KVec3 *tangent) const;

	/// getTangent(int, float, KVec3*) と同じだが、傾きを直接返す
	KVec3 getTangent(int seg, float t) const;

	/// 区間インデックス seg における３次ベジェ曲線の長さを得る
	/// @param seg 区間インデックス
	float getLength(int seg) const;
	
	/// テスト用。曲線を numdiv 個の直線に分割して近似長さを求める
	float getLength_Test1(int seg, int numdiv) const;
	
	/// テスト用。曲線を等価なベジェ曲線に再帰分割して近似長さを求める
	float getLength_Test2(int seg) const;

	/// 全体の長さを返す
	float getWholeLength() const;

	const KVec3 * getPointArray() const;

	int getPointCount() const;

	/// 区間数を返す
	int getSegmentCount() const;
	
	/// 制御点の数を変更する
	void setSegmentCount(int count);

	/// 区間を追加する。
	/// １区間には４個の制御点が必要になる。
	/// @param a 始点アンカー
	/// @param b 始点ハンドル。始点での傾きを決定する
	/// @param c 終点ハンドル。終点での傾きを決定する
	/// @param d 終点アンカー
	void addSegment(const KVec3 &a, const KVec3 &b, const KVec3 &c, const KVec3 &d);

	/// 区間インデックス seg の index 番目にある制御点の座標を設定する
	/// @param seg    区間番号。0 以上 getSegmentCount() 未満
	/// @param point  制御点番号。制御点は4個で、始点から順番に 0, 1, 2, 3 となる
	/// @param pos    制御点座標
	void setPoint(int seg, int point, const KVec3 &pos);

	/// コントロールポイントの通し番号を指定して座標を指定する。
	/// 例えば 0 は区間[0]の始点を、4は区間[1]の始点を、15は区間[3]の終点を表す
	void setPoint(int serial_index, const KVec3 &pos);

	/// 区間インデックス seg の index 番目にある制御点の座標を返す
	/// @param seg    区間番号。0 以上 getSegmentCount() 未満
	/// @param point  制御点番号。制御点は4個で、始点から順番に 0, 1, 2, 3
	KVec3 getPoint(int seg, int point) const;

	/// コントロールポイントの通し番号を指定して座標を取得する
	KVec3 getPoint(int serial_index) const;

	/// 区間インデックス seg における３次ベジェ曲線の始点側アンカーとハンドルポイントを得る
	/// @param      seg    区間番号。0 以上 getSegmentCount() 未満
	/// @param[out] anchor 始点側アンカー
	/// @param[out] handle 始点側アンカーのハンドルポイント
	bool getFirstAnchor(int seg, KVec3 *anchor, KVec3 *handle) const;

	/// 区間インデックス seg における３次ベジェ曲線の終点側アンカーとハンドルポイントを得る
	/// @param seg    区間番号。0 以上 getSegmentCount() 未満
	/// @param[out] anchor 終点側アンカー
	/// @param[out] handle 終点側アンカーのハンドルポイント
	bool getSecondAnchor(int seg, KVec3 *handle, KVec3 *anchor) const;

	/// 区間インデックス seg における３次ベジェ曲線の始点側アンカーとハンドルポイントを指定する
	/// @param seg    区間番号。0 以上 getSegmentCount() 未満
	/// @param anchor 始点側アンカー
	/// @param handle 始点側アンカーのハンドルポイント
	void setFirstAnchor(int seg, const KVec3 &anchor, const KVec3 &handle);

	/// 区間インデックス seg における３次ベジェ曲線の終点側アンカーとハンドルポイントを指定する
	/// @param seg    区間番号。0 以上 getSegmentCount() 未満
	/// @param anchor 終点側アンカー
	/// @param handle 終点側アンカーのハンドルポイント
	void setSecondAnchor(int seg, const KVec3 &handle, const KVec3 &anchor);

private:
	std::vector<KVec3> points_; /// 制御点の配列
	mutable float length_; /// 曲線の始点から終点までの経路長さ（ループしている場合は一周の長さ）
	mutable bool dirty_length_;
};
#pragma endregion // KCubicBezier


class KRect {
public:
	KRect();
	KRect(const KRect &rect);
	KRect(int _xmin, int _ymin, int _xmax, int _ymax);
	KRect(float _xmin, float _ymin, float _xmax, float _ymax);
	KRect(const KVec2 &pmin, const KVec2 &pmax);
	KRect getOffsetRect(const KVec2 &p) const;
	KRect getOffsetRect(float dx, float dy) const;

	/// 外側に dx, dy だけ広げた矩形を返す（負の値を指定した場合は内側に狭める）
	KRect getInflatedRect(float dx, float dy) const;

	/// rc と交差しているか判定する
	bool isIntersectedWith(const KRect &rc) const;

	/// rc と交差しているならその範囲を返す
	KRect getIntersectRect(const KRect &rc) const;
	KRect getUnionRect(const KVec2 &p) const;
	KRect getUnionRect(const KRect &rc) const;
	bool isZeroSized() const;
	bool isEqual(const KRect &rect) const;
	bool contains(const KVec2 &p) const;
	bool contains(float x, float y) const;
	bool contains(int x, int y) const;
	KVec2 getCenter() const;
	KVec2 getMinPoint() const;
	KVec2 getMaxPoint() const;
	float getSizeX() const;
	float getSizeY() const;

	/// 外側に向けて dx, dy だけ膨らませる
	void inflate(float dx, float dy);

	/// rect 全体を dx, dy だけ移動する
	void offset(float dx, float dy);
	void offset(int dx, int dy);

	/// rect を含むように矩形を拡張する
	/// unionX X方向について拡張する
	/// unionY Y方向について拡張する
	void unionWith(const KRect &rect, bool unionX=true, bool unionY=true);

	/// 数直線上で座標 x を含むように xmin, xmax を拡張する
	void unionWithX(float x);

	/// 数直線上で座標 y を含むように ymin, ymax を拡張する
	void unionWithY(float y);

	/// xmin <= xmax かつ ymin <= ymax であることを保証する
	void sort();
	//
	float xmin;
	float ymin;
	float xmax;
	float ymax;
};

class KRecti {
public:
	KRecti() {
		x = 0;
		y = 0;
		w = 0;
		h = 0;
	}
	KRecti(int _x, int _y, int _w, int _h) {
		x = _x;
		y = _y;
		w = _w;
		h = _h;
	}
	int x, y;
	int w, h;
};


class KGeom {
public:
	static constexpr float K_GEOM_EPS = 0.0000001f;
	static bool K_GEOM_ALMOST_ZERO(float x) { return fabsf(x) < K_GEOM_EPS; }
	static bool K_GEOM_ALMOST_SAME(float x, float y) { return K_GEOM_ALMOST_ZERO(x - y); }

	// AABBと点の最短距離
	static float K_GeomDistancePointToAabb(const KVec3 &p, const KVec3 &aabb_min, const KVec3 &aabb_max);

	/// XY平面上で (ax, ay) から (bx, by) を見ている時に点(px, py) が視線の左右どちらにあるか
	/// 左なら正の値、右なら負の値、視線上にあるなら 0 を返す
	static float K_GeomPointIsLeft2D(float px, float py, float ax, float ay, float bx, float by);

	/// 三角形 abc の法線ベクトルを得る（時計回りの並びを表とする）
	/// 点または線分に縮退している場合は法線を定義できず false を返す。成功すれば out_cw_normalized_normal に正規化された法線をセットする
	static bool K_GeomTriangleNormal(const KVec3 &a, const KVec3 &b, const KVec3 &c, KVec3 *out_cw_normalized_normal);

	/// 点 p が三角形 abc 内にあるか判定する
	static bool K_GeomPointInTriangle(const KVec3 &p, const KVec3 &a, const KVec3 &b, const KVec3 &c);

	/// 点 p から直線 ab におろした垂線の交点
	/// @param out_dist_a[out] a から交点までの距離
	/// @param out_pos[out]    交点座標. これは a + (b-a).getNormalized() * out_dist_from_a に等しい
	static bool K_GeomPerpendicularToLine(const KVec3 &p, const KVec3 &a, const KVec3 &b, float *out_dist_from_a, KVec3 *out_pos);

	/// 点 p から直線 ab におろした垂線のベクトル(点 p から交点座標までのベクトル)
	static KVec3 K_GeomPerpendicularToLineVector(const KVec3 &p, const KVec3 &a, const KVec3 &b);

	/// 点 p から平面（点 plane_pos を通り法線 plane_normal を持つ）におろした垂線の交点
	static KVec3 K_GeomPerpendicularToPlane(const KVec3 &p, const KVec3 &plane_pos, const KVec3 &plane_normal);

	/// 点 p から三角形 abc におろした垂線の交点
	static bool K_GeomPerpendicularToTriangle(const KVec3 &p, const KVec3 &a, const KVec3 &b, const KVec3 &c, KVec3 *out_pos);

	/// 点 p と直線 ab の最短距離
	static float K_GeomDistancePointToLine(const KVec3 &p, const KVec3 &a, const KVec3 &b);

	/// 点 p と線分 ab との距離を得る
	/// p から直線 ab におろした垂線が a  の外側にあるなら zone に -1 をセットして距離 pa を返す
	/// p から直線 ab におろした垂線が ab の内側にあるなら zone に  0 をセットして p と直線 ab の距離を返す
	/// p から直線 ab におろした垂線が b  の外側にあるなら zone に  1 をセットして距離 pb を返す
	static float K_GeomDistancePointToLineSegment(const KVec3 &p, const KVec3 &a, const KVec3 &b, int *zone=nullptr);

	static float K_GeomDistancePointToCapsule(const KVec3 &p, const KVec3 &capsule_p0, const KVec3 &capsule_p1, float capsule_r);
	static float K_GeomDistancePointToObb(const KVec3 &p, const KVec3 &obb_pos, const KVec3 &obb_halfsize, const KMatrix4 &obb_matrix);
	static bool  K_GeomDistanceLineToLine(const KVec3 &A_pos, const KVec3 &A_dir, const KVec3 &B_pos, const KVec3 &B_dir, float *out_dist, float *out_A, float *out_B);

	/// aabb1 と aabb2 の交差範囲を求める。
	/// 交差ありの場合は交差区間をセットして true を返す。なお。交差ではなく接触状態の場合は交差区間のサイズが 0 になることに注意
	/// 交差なしの場合は false を返す
	static bool K_GeomIntersectAabb(
		const KVec3 &aabb1_min, const KVec3 &aabb1_max,
		const KVec3 &aabb2_min, const KVec3 &aabb2_max,
		KVec3 *out_intersect_min, KVec3 *out_intersect_max
	);

	/// 平面同士の交線
	static bool K_GeomIntersectPlane(
		const KVec3 &plane1_pos, const KVec3 &plane1_nor,
		const KVec3 &plane2_pos, const KVec3 &plane2_nor,
		KVec3 *out_dir, KVec3 *out_pos
	);
};



class KCollisionMath {
public:
	/// 三角形の法線ベクトルを返す
	///
	/// 三角形法線ベクトル（正規化済み）を得る
	/// 時計回りに並んでいるほうを表とする
	static bool getTriangleNormal(KVec3 *result, const KVec3 &o, const KVec3 &a, const KVec3 &b);

	static bool isAabbIntersected(const KVec3 &pos1, const KVec3 &halfsize1, const KVec3 &pos2, const KVec3 &halfsize2, KVec3 *intersect_center=nullptr, KVec3 *intersect_halfsize=nullptr);

	/// 直線 ab で b 方向を見たとき、点(px, py)が左にあるなら正の値を、右にあるなら負の値を返す
	static float getPointInLeft2D(float px, float py, float ax, float ay, float bx, float by);

	/// 直線と点の有向距離（点AからBに向かって左側が負、右側が正）
	static float getSignedDistanceOfLinePoint2D(float px, float py, float ax, float ay, float bx, float by);

	/// 点 p から直線 ab へ垂線を引いたときの、垂線と直線の交点 h を求める。
	/// 戻り値は座標ではなく、点 a b に対する点 h の内分比を返す（点 h は必ず直線 ab 上にある）。
	/// 戻り値を k としたとき、交点座標は b * k + a * (1 - k) で求まる。
	/// 戻り値が 0 未満ならば交点は a の外側にあり、0 以上 1 未満ならば交点は ab 間にあり、1 以上ならば交点は b の外側にある。
	static float getPerpendicularLinePoint(const KVec3 &p, const KVec3 &a, const KVec3 &b);
	static float getPerpendicularLinePoint2D(float px, float py, float ax, float ay, float bx, float by);

	/// 点が矩形の内部であれば true を返す
	static bool isPointInRect2D(float px, float py, float cx, float cy, float xHalf, float yHalf);

	/// 点が剪断矩形（X方向）の内部であれば true を返す
	static bool isPointInXShearedRect2D(float px, float py, float cx, float cy, float xHalf, float yHalf, float xShear);

	/// 円が点に衝突しているか確認し、衝突している場合は円の補正移動量を (xAdj, yAdj) にセットして true を返す。
	/// 衝突していない場合は false を返す。
	/// 衝突判定には、長さ skin だけの余裕を持たせることができる（厳密に触れていなくても、skin 以内の距離ならば接触しているとみなす）
	/// 衝突を解消するためには、円を (xAdj, yAdj) だけ移動させるか、点を (-xAdj, -yAdj) だけ移動させる。
	static bool collisionCircleWithPoint2D(float cx, float cy, float cr, float px, float py, float skin=1.0f, float *xAdj=nullptr, float *yAdj=nullptr);

	/// 円が直線に衝突しているか確認し、衝突している場合は、円を直線の左側に押し出すための移動量を (xAdj, yAdj) にセットして true を返す。
	/// 衝突していない場合は false を返す。
	/// 衝突判定には、長さ skin だけの余裕を持たせることができる（厳密に触れていなくても、skin 以内の距離ならば接触しているとみなす）
	/// 衝突を解消するためには、円を (xAdj, yAdj) だけ移動させるか、直線を (-xAdj, -yAdj) だけ移動させる。
	static bool collisionCircleWithLine2D(float cx, float cy, float cr, float x0, float y0, float x1, float y1, float skin=1.0f, float *xAdj=nullptr, float *yAdj=nullptr);
	
	/// collisionCircleWithLine2D と似ているが、円が直線よりも右側にある場合は常に接触しているとみなし、直線の左側に押し出す
	static bool collisionCircleWithLinearArea2D(float cx, float cy, float cr, float x0, float y0, float x1, float y1, float skin=1.0f, float *xAdj=nullptr, float *yAdj=nullptr);

	/// 円が線分に衝突しているか確認し、衝突している場合は円の補正移動量を (xAdj, yAdj) にセットして true を返す。
	/// 衝突していない場合は false を返す。
	/// 衝突判定には、長さ skin だけの余裕を持たせることができる（厳密に触れていなくても、skin 以内の距離ならば接触しているとみなす）
	/// 衝突を解消するためには、円を (xAdj, yAdj) だけ移動させるか、直線を (-xAdj, -yAdj) だけ移動させる。
	static bool collisionCircleWithSegment2D(float cx, float cy, float cr, float x0, float y0, float x1, float y1, float skin=1.0f, float *xAdj=nullptr, float *yAdj=nullptr);

	/// 円同士が衝突しているか確認し、衝突している場合は円の補正移動量を (xAdj, yAdj) にセットして true を返す。
	/// 衝突していない場合は false を返す。
	/// 衝突判定には、長さ skin だけの余裕を持たせることができる（厳密に触れていなくても、skin 以内の距離ならば接触しているとみなす）
	/// 衝突を解消するためには、円1を (xAdj, yAdj) だけ移動させるか、円2を (-xAdj, -yAdj) だけ移動させる。
	/// 双方の円を平均的に動かして解消するなら、円1を (xAdj/2, yAdj/2) 動かし、円2を(-xAdj/2, -yAdj/2) だけ動かす
	static bool collisionCircleWithCircle2D(float x1, float y1, float r1, float x2, float y2, float r2, float skin=1.0f, float *xAdj=nullptr, float *yAdj=nullptr);

	/// 円と矩形が衝突しているなら、矩形のどの辺と衝突したかの値をフラグ組汗で返す
	/// 1 左(X負）
	/// 2 上(Y正）
	/// 4 右(X正）
	/// 8 下(Y負）
	static int collisionCircleWithRect2D(float cx, float cy, float cr, float xBox, float yBox, float xHalf, float yHalf, float skin=1.0f, float *xAdj=nullptr, float *yAdj=nullptr);
	static int collisionCircleWithXShearedRect2D(float cx, float cy, float cr, float xBox, float yBox, float xHalf, float yHalf, float xShear, float skin=1.0f, float *xAdj=nullptr, float *yAdj=nullptr);

	/// p1 を通り Y 軸に平行な半径 r1 の円柱と、
	/// p2 を通り Y 軸に平行な半径 r2 の円柱の衝突判定と移動を行う。
	/// 衝突しているなら p1 と p2 の双方を X-Z 平面上で移動させ、移動後の座標を p1 と p2 にセットして true を返す
	/// 衝突していなければ何もせずに false を返す
	/// k1, k2 には衝突した場合のめり込み解決感度を指定する。
	///     0.0 を指定すると全く移動しない。1.0 だと絶対にめり込まず、硬い感触になる
	///     0.2 だとめり込み量を 20% だけ解決する。めり込みの解決に時間がかかり、やわらかい印象になる。
	///     0.8 だとめり込み量を 80% だけ解決する。めり込みの解決が早く、硬めの印象になる
	/// skin 衝突判定用の許容距離を指定する。0.0 だと振動しやすくなる
	static bool resolveYAxisCylinderCollision(KVec3 *p1, float r1, float k1, KVec3 *p2, float r2, float k2, float skin);
};


class KRay {
public:
	KRay();
	KRay(const KVec3 &_pos, const KVec3 &_dir);

	KVec3 pos;
	KVec3 dir;
};

struct KRayDesc {
	KRayDesc() {
		dist = 0;
	}
	float dist;   // レイ始点からの距離
	KVec3 pos;    // 交点座標
	KVec3 normal; // 交点座標における対象面の法線ベクトル（正規化済み）
};

struct KSphere {
	KSphere();
	KSphere(const KVec3 &_pos, float _r);
	float rayTest(const KRay &ray) const;
	bool rayTest(const KRay &ray, KRayDesc *out_near, KRayDesc *out_far) const;
	bool sphereTest(const KSphere &sphere, float *out_dist, KVec3 *out_normal) const;
	float perpendicularPoint(const KVec3 &p, KVec3 *out_pos) const;
	float distance(const KVec3 &p) const;

	KVec3 pos;
	float r;
};

struct KPlane {
	KPlane();
	KPlane(const KVec3 &_pos, const KVec3 &_normal);
	float rayTest(const KRay &ray) const;
	bool rayTest(const KRay &ray, KRayDesc *out_point) const;
	bool sphereTest(const KSphere &sphere, float *out_dist, KVec3 *out_normal) const;
	float perpendicularPoint(const KVec3 &p, KVec3 *out_pos) const; // 点pから平面におろした垂線の交点
	float distance(const KVec3 &p) const;
	void normalizeThis();

	KVec3 m_pos;
	KVec3 m_normal;
};

struct KTriangle {
	KTriangle();
	KTriangle(const KVec3 &_a, const KVec3 &_b, const KVec3 &_c);
	float rayTest(const KRay &ray) const;
	bool rayTest(const KRay &ray, KRayDesc *out_point) const;
	bool sphereTest(const KSphere &sphere, float *out_dist, KVec3 *out_normal) const;
	bool perpendicularPoint(const KVec3 &p, float *out_dist, KVec3 *out_pos) const; // 点pから三角形におろした垂線の交点
	bool isPointInside(const KVec3 &p) const;
	KVec3 getNormal() const;

	KVec3 a, b, c;
};

struct KAabb {
	static bool intersect(const KAabb &a, const KAabb &b, KAabb *out);

	KAabb();
	KAabb(const KVec3 &_pos, const KVec3 &_halfsize);
	float rayTest(const KRay &ray) const;
	bool rayTest(const KRay &ray, KRayDesc *out_near, KRayDesc *out_far) const;
	bool sphereTest(const KSphere &sphere, float *out_dist, KVec3 *out_normal) const;
	float distance(const KVec3 &p) const; // 点pとの最短距離
	KVec3 getMin() const;
	KVec3 getMax() const;

	KVec3 pos;
	KVec3 halfsize;
};

struct KCylinder {
	KCylinder();
	KCylinder(const KVec3 &_p0, const KVec3 &_p1, float _r);
	float rayTest(const KRay &ray) const;
	bool rayTest(const KRay &ray, KRayDesc *out_near, KRayDesc *out_far) const;
	bool sphereTest(const KSphere &sphere, float *out_dist, KVec3 *out_normal) const;
	float distance(const KVec3 &p) const;

	KVec3 p0;
	KVec3 p1;
	float r;
};

struct KCapsule {
	KCapsule();
	KCapsule(const KVec3 &_p0, const KVec3 &_p1, float _r);
	float rayTest(const KRay &ray) const;
	bool rayTest(const KRay &ray, KRayDesc *out_near, KRayDesc *out_far) const;
	bool sphereTest(const KSphere &sphere, float *out_dist, KVec3 *out_normal) const;
	float distance(const KVec3 &p) const;

	KVec3 p0;
	KVec3 p1;
	float r;
};



namespace Test {
void Test_num();
void Test_bezier();
void Test_geom();
}


} // namespace
