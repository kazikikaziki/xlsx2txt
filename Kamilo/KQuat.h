#pragma once
#include "KVec.h"

namespace Kamilo {

class KQuat {
public:
	static KQuat fromXDeg(float deg);
	static KQuat fromYDeg(float deg);
	static KQuat fromZDeg(float deg);
	static KQuat fromAxisDeg(const KVec3 &axis, float deg); ///< 軸ベクトルと回転角度（度）からクォターニオンを作成
	static KQuat fromAxisRad(const KVec3 &axis, float rad); ///< 軸ベクトルと回転角度（ラジアン）からクォターニオンを作成

	KQuat();
	KQuat(const KQuat &q);
	KQuat(float qx, float qy, float qz, float qw);

	const float * floats() const;
	float * floats();

	bool operator == (const KQuat &b) const;
	bool operator != (const KQuat &b) const;
	KQuat operator - () const;
	KQuat operator * (const KQuat &b) const; ///< クォターニオン同士の積
	KQuat operator * (const KVec3 &v) const; ///< クォターニオンとベクトルの積
	KQuat & operator = (const KQuat &b);
	KQuat & operator *= (const KQuat &b);
	KQuat & operator *= (const KVec3 &v);

	static bool equals(const KQuat &a, const KQuat &b, float maxerr);
	static float dot(const KQuat &a, const KQuat &b);
	static KQuat mul(const KQuat &a, const KQuat &b);
	static KQuat mul(const KQuat &q, const KVec3 &v);
	static KQuat lerp(const KQuat &a, const KQuat &b, float t);
	static KQuat slerp(const KQuat &a, const KQuat &b, float t);
	static KQuat shorter(const KQuat &a, const KQuat &b);

	bool equals(const KQuat &b, float maxerr) const;
	float dot(const KQuat &b) const; ///< 内積
	KQuat lerp(const KQuat &b, float t) const; ///< クォターニオンの線形補間
	KQuat slerp(const KQuat &b, float t) const; ///< クォターニオンの球面補間
	KQuat shorter(const KQuat &b) const;

	float lengthSq() const; ///< ノルム2乗
	float length() const; ///< ノルム
	KQuat normalized() const; ///< 正規化したものを返す
	bool normalizedSafe(KQuat *out) const;
	bool isZero() const; ///< すべての要素が0に等しいか？
	bool isNormalized() const; ///< 正規化済みか？
	KQuat inverse() const; ///< クォターニオンの逆元（qを打ち消す回転）
	KVec3 axis() const; ///< クォターニオンの軸ベクトル
	float radians() const; /// クォターニオンの回転角度（ラジアン）
	float degrees() const; /// クォターニオンの回転角度（度）
	KQuat rotate(const KQuat &q) const; ///< クォターニオンを回す
	KVec3 rotate(const KVec3 &v) const; ///< ベクトルを回す
	//
	float x, y, z, w;
};


namespace Test {
void Test_quat();
}

} // namespace
