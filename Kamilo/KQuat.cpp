#include "KQuat.h"
#include "KInternal.h"

#define QUAT_ERROR() K__ERROR("Quaternion operation failed")

namespace Kamilo {



#pragma region KQuat
KQuat KQuat::fromXDeg(float deg) {
	return KQuat::fromAxisDeg(KVec3(1, 0, 0), deg);
}
KQuat KQuat::fromYDeg(float deg) {
	return KQuat::fromAxisDeg(KVec3(0, 1, 0), deg);
}
KQuat KQuat::fromZDeg(float deg) {
	return KQuat::fromAxisDeg(KVec3(0, 0, 1), deg);
}
KQuat KQuat::fromAxisDeg(const KVec3 &axis, float deg) {
	float rad = K::degToRad(deg);
	return fromAxisRad(axis, rad);
}
KQuat KQuat::fromAxisRad(const KVec3 &axis, float rad) {
	// http://marupeke296.com/DXG_No10_Quaternion.html
	// https://gist.github.com/mattatz/40a91588d5fb38240403f198a938a593
	float len = axis.getLength();
	if (len > 0) {
		float s = sinf(rad / 2.0f) / len;
		return KQuat(
			axis.x * s,
			axis.y * s,
			axis.z * s,
			cosf(rad / 2.0f)
		);
	}
	QUAT_ERROR();
	return KQuat();
}


KQuat::KQuat() {
	x = 0;
	y = 0;
	z = 0;
	w = 1;
}
KQuat::KQuat(const KQuat &q) {
	x = q.x;
	y = q.y;
	z = q.z;
	w = q.w;
}
KQuat::KQuat(float qx, float qy, float qz, float qw) {
	x = qx;
	y = qy;
	z = qz;
	w = qw;
}
const float * KQuat::floats() const {
	return (float*)this;
}
float * KQuat::floats() {
	return (float*)this;
}
bool KQuat::operator == (const KQuat &b) const {
	return x==b.x && y==b.y && z==b.z && w==b.w;
}
bool KQuat::operator != (const KQuat &b) const {
	return x!=b.x || y!=b.y || z!=b.z || w!=b.w;
}
KQuat KQuat::operator - () const {
	return KQuat(-x, -y, -z, -w);
}

KQuat KQuat::operator * (const KQuat &b) const {
	return mul(*this, b);
}
KQuat KQuat::operator * (const KVec3 &v) const {
	return mul(*this, v);
}

KQuat & KQuat::operator = (const KQuat &b) {
	x = b.x;
	y = b.y;
	z = b.z;
	w = b.w;
	return *this;
}
KQuat & KQuat::operator *= (const KQuat &b) {
	*this = (*this) * b;
	return *this;
}

KQuat & KQuat::operator *= (const KVec3 &v) {
	*this = (*this) * v;
	return *this;
}

/// クォターニオン同士の積
KQuat KQuat::mul(const KQuat &a, const KQuat &b) {
	return KQuat(
		a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y,
		a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x,
		a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w,
		a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z
	);
}
/// クォターニオンとベクトルの積
KQuat KQuat::mul(const KQuat &q, const KVec3 &v) {
	return KQuat(
		 q.w*v.x + q.y*v.z - q.z*v.y,
		 q.w*v.y + q.z*v.x - q.x*v.z,
		 q.w*v.z + q.x*v.y - q.y*v.x,
		-q.x*v.x - q.y*v.y - q.z*v.z
	);
}
bool KQuat::equals(const KQuat &a, const KQuat &b, float maxerr) {
	float dx = fabsf(a.x - b.x);
	float dy = fabsf(a.y - b.y);
	float dz = fabsf(a.z - b.z);
	float dw = fabsf(a.w - b.w);
	return (dx <= maxerr) && (dy <= maxerr) && (dz <= maxerr) && (dw <= maxerr);
}
float KQuat::dot(const KQuat &a, const KQuat &b) {
	return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
}
KQuat KQuat::lerp(const KQuat &a, const KQuat &b, float t) {
	// https://www.f-sp.com/entry/2017/06/30/221124
	// http://marupeke296.com/DXG_No58_RotQuaternionTrans.html
	float d = dot(a, b);
	float rad;
	if (-1.0f <= d && d <= 1.0f) {
		rad = acosf(d);
	} else {
		QUAT_ERROR();
		return KQuat();
	}
	float sin_rad = sinf(rad);
	if (sin_rad == 0) {
		QUAT_ERROR();
		return KQuat();
	}
	float k0 = sinf((1.0f - t) * rad) / sin_rad;
	float k1 = sinf((       t) * rad) / sin_rad;
	KQuat tmp(
		a.x * k0 + b.x * k1,
		a.y * k0 + b.y * k1,
		a.z * k0 + b.z * k1,
		a.w * k0 + b.w * k1
	);
	return tmp.normalized();
}
KQuat KQuat::slerp(const KQuat &a, const KQuat &b, float t) {
	float magnitude = sqrtf(a.lengthSq() * b.lengthSq());
	if (magnitude == 0) {
		QUAT_ERROR();
		return KQuat();
	}
	float product = dot(a, b) / magnitude;
	if (fabsf(product) < 1.0f) {
		float sig = (product >= 0) ? 1.0f : -1.0f;
		float rad = acosf(sig * product);
		float k0  = sinf(rad * (1.0f - t));
		float k1  = sinf(rad * sig * t);
		float div = 1.0f / sinf(rad);
		return KQuat(
			(a.x * k0 + b.x * k1) * div,
			(a.y * k0 + b.y * k1) * div,
			(a.z * k0 + b.z * k1) * div,
			(a.w * k0 + b.w * k1) * div
		);
	} else {
		return a;
	}
}
KQuat KQuat::shorter(const KQuat &a, const KQuat &b) {
	// クォターニオン a から b に補完したい時、実際に補間に使う b を返す。
	// 符号を反転させたクォターニオンは反転前と全く同じ回転を表すが、
	// 4次元空間内では原点を挟んで対称な位置にある（符号が逆なので）。
	// なのでクォターニオン b と -b は回転としては同じだが、
	// a から b に補完するのと a から -b に補完するのでは途中経過が異なる。
	// b と -b を比べて、a に近い方の b を返す
	// https://www.f-sp.com/entry/2017/06/30/221124
	float d = dot(a, b);
	if (d >= 0) {
		// a と b のなす角度 <= 180度なので a --> b で補間する
		return b;
	} else {
		// a と b のなす角度 > 180度なので a --> -b で補間する
		return -b; // 全要素の符号を反転
	}
}

bool KQuat::equals(const KQuat &b, float maxerr) const {
	return equals(*this, b, maxerr);
}
float KQuat::dot(const KQuat &b) const {
	return dot(*this, b);
}
float KQuat::length() const {
	return sqrtf(lengthSq());
}
float KQuat::lengthSq() const {
	return dot(*this, *this);
}
KQuat KQuat::inverse() const {
	return KQuat(-x, -y, -z, w); // 複素共益を取る
}
KVec3 KQuat::axis() const {
	KVec3 out;
	if (KVec3(x, y, z).getNormalizedSafe(&out)) {
		return out;
	}
	QUAT_ERROR();
	return KVec3();
}
float KQuat::radians() const {
	if (-1.0f <= w && w <= 1.0f) {
		return 2 * acosf(w);
	}
	QUAT_ERROR();
	return 0.0f;
}
float KQuat::degrees() const {
	return K::radToDeg(radians());
}
KQuat KQuat::normalized() const {
	KQuat out;
	if (normalizedSafe(&out)) {
		return out;
	}
	QUAT_ERROR();
	return KQuat();
}
bool KQuat::normalizedSafe(KQuat *out) const {
	float len = length();
	if (len > 0) {
		out->x = x / len;
		out->y = y / len;
		out->z = z / len;
		out->w = w / len;
		return true;
	}
	return false;
}
KQuat KQuat::lerp(const KQuat &b, float t) const {
	return lerp(*this, b, t);
}
KQuat KQuat::slerp(const KQuat &b, float t) const {
	return slerp(*this, b, t);
}

/// クォターニオン a から b に補完したい時、実際に補間に使う b を返す。
/// 符号を反転させたクォターニオンは反転前と全く同じ回転を表すが、
/// 4次元空間内では原点を挟んで対称な位置にある（符号が逆なので）。
/// なのでクォターニオン b と -b は回転としては同じだが、
/// a から b に補完するのと a から -b に補完するのでは途中経過が異なる。
/// a と -b を比べて、a に近い方の b を返す
/// https://www.f-sp.com/entry/2017/06/30/221124
KQuat KQuat::shorter(const KQuat &b) const {
	return shorter(*this, b);
}

KQuat KQuat::rotate(const KQuat &b) const {
	return mul(*this, b);
}

/// クオータニオン q で ベクトル v を回す
KVec3 KQuat::rotate(const KVec3 &v) const {
	// https://qiita.com/drken/items/0639cf34cce14e8d58a5
	// 回転済みベクトル = q * v * q^-1
#if 0
	const KQuat &q = *this;
	KQuat rot = q * v * q.inverse();
	return KVec3(rot.x, rot.y, rot.z);
#else
	// https://qiita.com/drken/items/0639cf34cce14e8d58a5
	// v' = q * v * q^-1
	const KQuat &q = *this;
	KQuat qa = mul(q, v);   // qa = q * v
	KQuat qi = q.inverse(); // qi = q^-1
	KQuat qb = mul(qa, qi); // qb = qa * qi = (q * v) * (q^-1)
	return KVec3(qb.x, qb.y, qb.z);
#endif
}



/// すべての要素が0に等しいか？
/// ※normalized() の事前チェックには使えない。計算中にアンダーフローでゼロになる場合がある
bool KQuat::isZero() const {
	return lengthSq() == 0;
}
bool KQuat::isNormalized() const {
	return lengthSq() == 1;
}
#pragma endregion // KQuat




namespace Test {
void Test_quat() {
	const float MAXERR = 0.0001f;

	const KVec3 point0(100, 0, 200); // 基本座標
	const KVec3 point45_ref(100/sqrtf(2.0f), 100/sqrtf(2.0f), 200.0f); // point0をZ軸45度回転
	const KVec3 point90_ref(0, 100, 200); // point0 をZ軸90度回転

	KVec3 axis_z(0, 0, 1); // Z軸

	// 45度回転用のクォータニオン
	KQuat rot_z45 = KQuat::fromAxisDeg(axis_z, 45); // Z軸45度

	// 30, 15 回転を合成すると 45 クォータニオンになるはず
	{
		KQuat rot_z30 = KQuat::fromAxisDeg(axis_z, 30); // Z軸30度
		KQuat rot_z15 = KQuat::fromAxisDeg(axis_z, 15); // Z軸15度
		KQuat rot_z45_test = rot_z30.rotate(rot_z15); // 30+15
		K__VERIFY(rot_z45_test.equals(rot_z45, MAXERR));
	}
	KVec3 point45;
	{
		point45 = rot_z45.rotate(point0); // point0 をZ軸で45度回転
		K__VERIFY(point45.equals(point45_ref, MAXERR));
	}
	KVec3 point90;
	{
		point90 = rot_z45.rotate(point45); // point45 をZ軸で45度回転
		K__VERIFY(point90.equals(point90_ref, MAXERR));
	}
}

}


} // namespace
