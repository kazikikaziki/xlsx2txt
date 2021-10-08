#include "KVec.h"
#include "KMath.h"
#include "KInternal.h"

#define VEC_ERROR() K__ERROR("Vector operation failed")

namespace Kamilo {

#pragma region vec
template <typename T> void K_vec2_make(T *out, T x, T y) {
	out[0] = x;
	out[1] = y;
}
template <typename T> void K_vec3_make(T *out, T x, T y, T z) {
	out[0] = x;
	out[1] = y;
	out[2] = z;
}
template <typename T> void K_vec4_make(T *out, T x, T y, T z, T w) {
	out[0] = x;
	out[1] = y;
	out[2] = z;
	out[3] = w;
}

template <typename T> void K_vec2_clear(T *out) {
	K_vec2_make(out, (T)0, (T)0);
}
template <typename T> void K_vec3_clear(T *out) {
	K_vec3_make(out, (T)0, (T)0, (T)0);
}
template <typename T> void K_vec4_clear(T *out) {
	K_vec4_make(out, (T)0, (T)0, (T)0, (T)0);
}

template <typename T> void K_vec2_copy(T *out, const T *a) {
	K_vec2_make(out, a[0], a[1]);
}
template <typename T> void K_vec3_copy(T *out, const T *a) {
	K_vec3_make(out, a[0], a[1], a[2]);
}
template <typename T> void K_vec4_copy(T *out, const T *a) {
	K_vec4_make(out, a[0], a[1], a[2], a[3]);
}

/// ベクトル成分のどれかひとつが 0 になっているか調べる
template <typename T> bool K_vec2_anyzero(const T *a) {
	return a[0]==0 || a[1]==0;
}
template <typename T> bool K_vec3_anyzero(const T *a) {
	return a[0]==0 || a[1]==0 || a[2]==0;
}
template <typename T> bool K_vec4_anyzero(const T *a) {
	return a[0]==0 || a[1]==0 || a[2]==0 || a[3]==0;
}

/// すべてのベクトル成分が 0 になっているか調べる
template <typename T> bool K_vec2_allzero(const T *a) {
	return a[0]==0 && a[1]==0;
}
template <typename T> bool K_vec3_allzero(const T *a) {
	return a[0]==0 && a[1]==0 && a[2]==0;
}
template <typename T> bool K_vec4_allzero(const T *a) {
	return a[0]==0 && a[1]==0 && a[2]==0 && a[3]==0;
}

/// ベクトルの要素ごとの和 (a + b) を得る
template <typename T> void K_vec2_add(T *out, const T *a, const T *b) {
	out[0] = a[0] + b[0];
	out[1] = a[1] + b[1];
}
template <typename T> void K_vec3_add(T *out, const T *a, const T *b) {
	out[0] = a[0] + b[0];
	out[1] = a[1] + b[1];
	out[2] = a[2] + b[2];
}
template <typename T> void K_vec4_add(T *out, const T *a, const T *b) {
	out[0] = a[0] + b[0];
	out[1] = a[1] + b[1];
	out[2] = a[2] + b[2];
	out[3] = a[3] + b[3];
}

/// ベクトルの要素ごとの差 (a - b) を得る
template <typename T> void K_vec2_sub(T *out, const T *a, const T *b) {
	out[0] = a[0] - b[0];
	out[1] = a[1] - b[1];
}
template <typename T> void K_vec3_sub(T *out, const T *a, const T *b) {
	out[0] = a[0] - b[0];
	out[1] = a[1] - b[1];
	out[2] = a[2] - b[2];
}
template <typename T> void K_vec4_sub(T *out, const T *a, const T *b) {
	out[0] = a[0] - b[0];
	out[1] = a[1] - b[1];
	out[2] = a[2] - b[2];
	out[3] = a[3] - b[3];
}

/// ベクトルの要素ごとの積 (a * b) を得る
template <typename T> void K_vec2_mul(T *out, const T *a, const T *b) {
	out[0] = a[0] * b[0];
	out[1] = a[1] * b[1];
}
template <typename T> void K_vec3_mul(T *out, const T *a, const T *b) {
	out[0] = a[0] * b[0];
	out[1] = a[1] * b[1];
	out[2] = a[2] * b[2];
}
template <typename T> void K_vec4_mul(T *out, const T *a, const T *b) {
	out[0] = a[0] * b[0];
	out[1] = a[1] * b[1];
	out[2] = a[2] * b[2];
	out[3] = a[3] * b[3];
}

/// ベクトル a と定数 n の乗算
template <typename T> void K_vec2_mul_n(T *out, const T *a, T n) {
	out[0] = a[0] * n;
	out[1] = a[1] * n;
}
template <typename T> void K_vec3_mul_n(T *out, const T *a, T n) {
	out[0] = a[0] * n;
	out[1] = a[1] * n;
	out[2] = a[2] * n;
}
template <typename T> void K_vec4_mul_n(T *out, const T *a, T n) {
	out[0] = a[0] * n;
	out[1] = a[1] * n;
	out[2] = a[2] * n;
	out[3] = a[3] * n;
}

/// ベクトルの要素ごとの商 (a / b) を得る
/// 値が 0 の要素がある場合はゼロ除算ができずに失敗し、なにもせずに false を返す
template <typename T> bool K_vec2_div(T *out, const T *a, const T *b) {
	if (K_vec2_anyzero(b)) {
		return false;
	} else {
		out[0] = a[0] / b[0];
		out[1] = a[1] / b[1];
		return true;
	}
}
template <typename T> bool K_vec3_div(T *out, const T *a, const T *b) {
	if (K_vec3_anyzero(b)) {
		return false;
	} else {
		out[0] = a[0] / b[0];
		out[1] = a[1] / b[1];
		out[2] = a[2] / b[2];
		return true;
	}
}
template <typename T> bool K_vec4_div(T *out, const T *a, const T *b) {
	if (K_vec4_anyzero(b)) {
		return false;
	} else {
		out[0] = a[0] / b[0];
		out[1] = a[1] / b[1];
		out[2] = a[2] / b[2];
		out[3] = a[3] / b[3];
		return true;
	}
}

/// ベクトル a と定数 n の除算
template <typename T> bool K_vec2_div_n(T *out, const T *a, T n) {
	if (n == 0) {
		return false;
	} else {
		out[0] = a[0] / n;
		out[1] = a[1] / n;
		return true;
	}
}
template <typename T> bool K_vec3_div_n(T *out, const T *a, T n) {
	if (n == 0) {
		return false;
	} else {
		out[0] = a[0] / n;
		out[1] = a[1] / n;
		out[2] = a[2] / n;
		return true;
	}
}
template <typename T> bool K_vec4_div_n(T *out, const T *a, T n) {
	if (n == 0) {
		return false;
	} else {
		out[0] = a[0] / n;
		out[1] = a[1] / n;
		out[2] = a[2] / n;
		out[3] = a[3] / n;
		return true;
	}
}

/// ベクトルの内積 (|a||b|cosθ を求める）
template <typename T> T K_vec2_dot(const T *a, const T *b) {
	return a[0]*b[0] + a[1]*b[1];
}
template <typename T> T K_vec3_dot(const T *a, const T *b) {
	return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}
template <typename T> T K_vec4_dot(const T *a, const T *b) {
	return a[0]*b[0] + a[1]*b[1] + a[2]*b[2] + a[3]*b[3];
}

/// ベクトルの外積（2次元ベクトル a b に対して |a||b|sinθ を求める）
template <typename T> T K_vec2_cross_scalar(const T *a, const T *b) {
	return a[0]*b[1] - a[1]*b[0]; // a.x*b.y-a.y*b.x = |a||b|sin = ベクトル a と b の成す平行四辺形の符号付面積
}

/// ベクトルの外積（ベクトル a と b に垂直なベクトルを求める）
template <typename T> void K_vec3_cross(T *out, const T *a, const T *b) {
	out[0] = a[1]*b[2] - a[2]*b[1]; // cross.x = a.y*b.z - a.z*b.y
	out[1] = a[2]*b[0] - a[0]*b[2]; // cross.y = a.z*b.x - a.x*b.z = ベクトル a と b に垂直なベクトル
	out[2] = a[0]*b[1] - a[1]*b[0]; // cross.z = a.x*b.y - a.y*b.x
}

/// ベクトル成分の二乗和（自分自身との内積）
template <typename T> T K_vec2_lensq(const T *a) {
	return K_vec2_dot(a, a);
}
template <typename T> T K_vec3_lensq(const T *a) {
	return K_vec3_dot(a, a);
}
template <typename T> T K_vec4_lensq(const T *a) {
	return K_vec4_dot(a, a);
}

/// ベクトルの長さ（型に注意。整数型のベクトルの長さは整数に丸められてしまう）
template <typename T> T K_vec2_len(const T *a) {
	return (T)sqrt(K_vec2_lensq(a));
}
template <typename T> T K_vec3_len(const T *a) {
	return (T)sqrt(K_vec3_lensq(a));
}
template <typename T> T K_vec4_len(const T *a) {
	return (T)sqrt(K_vec4_lensq(a));
}

/// ベクトルの長さを正規化する（型に注意。整数型のベクトルは正しく正規化できない）
/// なお、a のメンバーが全てゼロであるかどうかで事前判断してはいけない。
/// sqrt(a^2) の計算中にアンダーフローが発生して 0 になる場合がある
template <typename T> bool K_vec2_normalize(T *out, const T *a) {
	return K_vec2_div_n(out, a, K_vec2_len(a));
}
template <typename T> bool K_vec3_normalize(T *out, const T *a) {
	return K_vec3_div_n(out, a, K_vec3_len(a));
}
template <typename T> bool K_vec4_normalize(T *out, const T *a) {
	return K_vec4_div_n(out, a, K_vec4_len(a));
}

/// ベクトル同士の lerp を計算する
template <typename T> void K_vec2_lerp(T *out, const T *a, const T *b, T t) {
	out[0] = KMath::lerp(a[0], b[0], t);
	out[1] = KMath::lerp(a[1], b[2], t);
}
template <typename T> void K_vec3_lerp(T *out, const T *a, const T *b, T t) {
	out[0] = KMath::lerp(a[0], b[0], t);
	out[1] = KMath::lerp(a[1], b[1], t);
	out[2] = KMath::lerp(a[2], b[2], t);
}
template <typename T> void K_vec4_lerp(T *out, const T *a, const T *b, T t) {
	out[0] = KMath::lerp(a[0], b[0], t);
	out[1] = KMath::lerp(a[1], b[1], t);
	out[2] = KMath::lerp(a[2], b[2], t);
	out[3] = KMath::lerp(a[3], b[3], t);
}

/// ベクトル同士が完全に等しいか調べる
template <typename T> bool K_vec2_equals(const T *a, const T *b) {
	return a[0]==b[0] && a[1]==b[1];
}
template <typename T> bool K_vec3_equals(const T *a, const T *b) {
	return a[0]==b[0] && a[1]==b[1] && a[2]==b[2];
}
template <typename T> bool K_vec4_equals(const T *a, const T *b) {
	return a[0]==b[0] && a[1]==b[1] && a[2]==b[2] && a[3]==b[3];
}


/// ベクトル同士が指定誤差以内に収まっているか調べる
template <typename T> bool K_vec2_equals(const T *a, const T *b, T maxerr) {
	T dx = abs(a[0] - b[0]);
	T dy = abs(a[1] - b[1]);
	return (dx <= maxerr) && (dy <= maxerr);
}
template <typename T> bool K_vec3_equals(const T *a, const T *b, T maxerr) {
	T dx = abs(a[0] - b[0]);
	T dy = abs(a[1] - b[1]);
	T dz = abs(a[2] - b[2]);
	return (dx <= maxerr) && (dy <= maxerr) && (dz <= maxerr);
}
template <typename T> bool K_vec4_equals(const T *a, const T *b, T maxerr) {
	T dx = abs(a[0] - b[0]);
	T dy = abs(a[1] - b[1]);
	T dz = abs(a[2] - b[2]);
	T dw = abs(a[3] - b[3]);
	return (dx <= maxerr) && (dy <= maxerr) && (dz <= maxerr) && (dw <= maxerr);
}
#pragma endregion // vec

#pragma region KVec2
const float * KVec2::floats() const {
	return (float*)this;
}
float * KVec2::floats() {
	return (float*)this;
}
bool KVec2::operator == (const KVec2 &b) const {
	return K_vec2_equals(&x, &b.x);
}
bool KVec2::operator != (const KVec2 &b) const {
	return !K_vec2_equals(&x, &b.x);
}
KVec2 KVec2::operator + (const KVec2 &b) const {
	KVec2 out;
	K_vec2_add(&out.x, &x, &b.x);
	return out;
}
KVec2 KVec2::operator - (const KVec2 &b) const {
	KVec2 out;
	K_vec2_sub(&out.x, &x, &b.x);
	return out;
}
KVec2 KVec2::operator - () const {
	KVec2 out;
	K_vec2_mul_n(&out.x, &x, -1.0f);
	return out;
}
KVec2 KVec2::operator * (float b) const {
	KVec2 out;
	K_vec2_mul_n(&out.x, &x, b);
	return out;
}
KVec2 KVec2::operator / (float b) const {
	KVec2 out;
	if (!K_vec2_div_n(out.floats(), floats(), b)) {
		VEC_ERROR();
		return KVec2();
	}
	return out;
}
KVec2 KVec2::operator / (const KVec2 &b) const {
	KVec2 out;
	if (!K_vec2_div(out.floats(), floats(), b.floats())) {
		VEC_ERROR();
		return KVec2();
	}
	return out;
}
KVec2 & KVec2::operator /= (float b) {
	K__ASSERT(b != 0);
	*this = *this / b;
	return *this;
}
KVec2 & KVec2::operator /= (const KVec2 &b) {
	K__ASSERT(!K_vec2_anyzero(b.floats()));
	*this = *this / b;
	return *this;
}
KVec2 & KVec2::operator = (const KVec2 &b) {
	x = b.x;
	y = b.y;
	return *this;
}
KVec2 & KVec2::operator += (const KVec2 &b) {
	x += b.x;
	y += b.y;
	return *this;
}
KVec2 & KVec2::operator -= (const KVec2 &b) {
	x -= b.x;
	y -= b.y;
	return *this;
}
KVec2 & KVec2::operator *= (float b) {
	x *= b;
	y *= b;
	return *this;
}

float KVec2::dot(const KVec2 &b) const {
	return K_vec2_dot(floats(), b.floats());
}
float KVec2::cross(const KVec2 &b) const {
	return K_vec2_cross_scalar(floats(), b.floats());
}
KVec2 KVec2::floor() const {
	return KVec2(
		floorf(x),
		floorf(y)
	);
}
KVec2 KVec2::ceil() const {
	return KVec2(
		ceilf(x),
		ceilf(y)
	);
}
float KVec2::getLength() const {
	return K_vec2_len(floats());
}
float KVec2::getLengthSq() const {
	return K_vec2_lensq(floats());
}
KVec2 KVec2::getNormalized() const {
	KVec2 out;
	if (!K_vec2_normalize(out.floats(), floats())) {
		VEC_ERROR();
		return KVec2();
	}
	return out;
}
bool KVec2::getNormalizedSafe(KVec2 *n) const {
	return K_vec2_normalize(n->floats(), floats());
}
/// すべての要素が0に等しいか？
/// ※getNormalized() の事前チェックには使えない。計算中にアンダーフローでゼロになる場合がある
bool KVec2::isZero() const {
	return K_vec2_allzero(floats());
}
bool KVec2::isNormalized() const {
	return getLengthSq() == 1;
}
KVec2 KVec2::lerp(const KVec2 &a, float t) const {
	return KVec2(
		K::lerp(x, a.x, t),
		K::lerp(y, a.y, t)
	);
}
KVec2 KVec2::getmin(const KVec2 &b) const {
	return KVec2(K::min(x, b.x), K::min(y, b.y));
}
KVec2 KVec2::getmax(const KVec2 &b) const {
	return KVec2(K::max(x, b.x), K::max(y, b.y));
}
bool KVec2::equals(const KVec2 &b, float maxerr) const {
	float dx = fabsf(x - b.x);
	float dy = fabsf(y - b.y);
	return (dx <= maxerr) && (dy <= maxerr);
}
#pragma endregion // KVec2




#pragma region KVec3
const float * KVec3::floats() const {
	return (float*)this;
}
float * KVec3::floats() {
	return (float*)this;
}
bool KVec3::operator == (const KVec3 &b) const {
	return x==b.x && y==b.y && z==b.z;
}
bool KVec3::operator != (const KVec3 &b) const {
	return x!=b.x || y!=b.y || z!=b.z;
}
KVec3 KVec3::operator + (const KVec3 &b) const {
	return KVec3(x+b.x, y+b.y, z+b.z);
}
KVec3 KVec3::operator - (const KVec3 &b) const {
	return KVec3(x-b.x, y-b.y, z-b.z);
}
KVec3 KVec3::operator - () const {
	return KVec3(-x, -y, -z);
}
KVec3 KVec3::operator * (float b) const {
	return KVec3(x*b, y*b, z*b);
}
KVec3 KVec3::operator * (const KVec3 &b) const {
	return KVec3(x*b.x, y*b.y, z*b.z);
}
KVec3 KVec3::operator / (float b) const { 
	KVec3 out;
	if (!K_vec3_div_n(out.floats(), floats(), b)) {
		VEC_ERROR();
		return KVec3();
	}
	return out;
}
KVec3 KVec3::operator / (const KVec3 &b) const {
	KVec3 out;
	if (!K_vec3_div(out.floats(), floats(), b.floats())) {
		VEC_ERROR();
		return KVec3();
	}
	return out;
}
KVec3 & KVec3::operator /= (float b) {
	K__ASSERT(b != 0);
	*this = *this / b;
	return *this;
}
KVec3 & KVec3::operator /= (const KVec3 &b) {
	K__ASSERT(!K_vec3_anyzero(b.floats()));
	*this = *this / b;
	return *this;
}
float KVec3::operator[](int index) const {
	return floats()[index];
}
float & KVec3::operator[](int index) {
	return floats()[index];
}
KVec3 & KVec3::operator = (const KVec3 &b) {
	x = b.x;
	y = b.y;
	z = b.z;
	return *this;
}
KVec3 & KVec3::operator += (const KVec3 &b) {
	x += b.x;
	y += b.y;
	z += b.z;
	return *this;
}
KVec3 & KVec3::operator -= (const KVec3 &b) {
	x -= b.x;
	y -= b.y;
	z -= b.z;
	return *this;
}
KVec3 & KVec3::operator *= (float b) {
	x *= b;
	y *= b;
	z *= b;
	return *this;
}
KVec3 & KVec3::operator *= (const KVec3 &b) {
	x *= b.x;
	y *= b.y;
	z *= b.z;
	return *this;
}
float KVec3::dot(const KVec3 &b) const {
	return K_vec3_dot(floats(), b.floats());
}
KVec3 KVec3::cross(const KVec3 &b) const {
	KVec3 out;
	K_vec3_cross(out.floats(), floats(), b.floats());
	return out;
}
KVec3 KVec3::floor() const {
	return KVec3(
		floorf(x),
		floorf(y),
		floorf(z)
	);
}
KVec3 KVec3::ceil() const {
	return KVec3(
		ceilf(x),
		ceilf(y),
		ceilf(z)
	);
}
float KVec3::getLength() const {
	return K_vec3_len(floats());
}
float KVec3::getLengthSq() const {
	return K_vec3_lensq(floats());
}
KVec3 KVec3::getNormalized() const {
	KVec3 out;
	if (!K_vec3_normalize(out.floats(), floats())) {
	//	VEC_ERROR();
		return KVec3();
	}
	return out;
}
bool KVec3::getNormalizedSafe(KVec3 *n) const {
	return K_vec3_normalize(n->floats(), floats());
}
/// すべての要素が0に等しいか？
/// ※getNormalized() の事前チェックには使えない。計算中にアンダーフローでゼロになる場合がある
bool KVec3::isZero() const {
	return K_vec3_allzero(floats());
}
bool KVec3::isNormalized() const {
	return getLengthSq() == 1;
}
KVec3 KVec3::lerp(const KVec3 &a, float t) const {
	return KVec3(
		K::lerp(x, a.x, t),
		K::lerp(y, a.y, t),
		K::lerp(z, a.z, t)
	);
}
KVec3 KVec3::getmin(const KVec3 &b) const {
	return KVec3(K::min(x, b.x), K::min(y, b.y), K::min(z, b.z));
}
KVec3 KVec3::getmax(const KVec3 &b) const {
	return KVec3(K::max(x, b.x), K::max(y, b.y), K::max(z, b.z));
}
bool KVec3::equals(const KVec3 &b, float maxerr) const {
	float dx = fabsf(x - b.x);
	float dy = fabsf(y - b.y);
	float dz = fabsf(z - b.z);
	return (dx <= maxerr) && (dy <= maxerr) && (dz <= maxerr);
}
#pragma endregion // KVec3




#pragma region KVec4
const float * KVec4::floats() const {
	return (float*)this;
}
float * KVec4::floats() {
	return (float*)this;
}
bool KVec4::operator == (const KVec4 &b) const {
	return x==b.x && y==b.y && z==b.z && w==b.w;
}
bool KVec4::operator != (const KVec4 &b) const {
	return x!=b.x || y!=b.y || z!=b.z || w!=b.w;
}
KVec4 KVec4::operator + (const KVec4 &b) const {
	return KVec4(x+b.x, y+b.y, z+b.z, w+b.w);
}
KVec4 KVec4::operator - (const KVec4 &b) const {
	return KVec4(x-b.x, y-b.y, z-b.z, w-b.w);
}
KVec4 KVec4::operator - () const {
	return KVec4(-x, -y, -z, -w);
}
KVec4 KVec4::operator * (float b) const {
	return KVec4(x*b, y*b, z*b, w*b);
}
KVec4 KVec4::operator / (float b) const {
	KVec4 out;
	if (!K_vec4_div_n(out.floats(), floats(), b)) {
		VEC_ERROR();
		return KVec4();
	}
	return out;
}
KVec4 KVec4::operator / (const KVec4 &b) const {
	KVec4 out;
	if (!K_vec4_div(out.floats(), floats(), b.floats())) {
		VEC_ERROR();
		return KVec4();
	}
	return out;
}
KVec4 & KVec4::operator /= (float b) {
	K__ASSERT(b != 0);
	*this = *this / b;
	return *this;
}
KVec4 & KVec4::operator /= (const KVec4 &b) {
	K__ASSERT(!K_vec4_anyzero(b.floats()));
	*this = *this / b;
	return *this;
}
KVec4 & KVec4::operator = (const KVec4 &b) {
	x = b.x;
	y = b.y;
	z = b.z;
	w = b.w;
	return *this;
}
KVec4 & KVec4::operator += (const KVec4 &b) {
	x += b.x;
	y += b.y;
	z += b.z;
	w += b.w;
	return *this;
}
KVec4 & KVec4::operator -= (const KVec4 &b) {
	x -= b.x;
	y -= b.y;
	z -= b.z;
	w -= b.w;
	return *this;
}
KVec4 & KVec4::operator *= (float b) {
	x *= b;
	y *= b;
	z *= b;
	w *= b;
	return *this;
}
float KVec4::dot(const KVec4 &b) const {
	return K_vec4_dot(floats(), b.floats());
}
float KVec4::getLength() const {
	return K_vec4_len(floats());
}
float KVec4::getLengthSq() const {
	return K_vec4_lensq(floats());
}
KVec4 KVec4::getNormalized() const {
	KVec4 out;
	if (!K_vec4_normalize(out.floats(), floats())) {
		VEC_ERROR();
		return KVec4();
	}
	return out;
}
/// すべての要素が0に等しいか？
/// ※getNormalized() の事前チェックには使えない。計算中にアンダーフローでゼロになる場合がある
bool KVec4::isZero() const {
	return K_vec4_allzero(floats());
}
bool KVec4::isNormalized() const {
	return getLengthSq() == 1;
}
KVec4 KVec4::lerp(const KVec4 &a, float t) const {
	return KVec4(
		K::lerp(x, a.x, t),
		K::lerp(y, a.y, t),
		K::lerp(z, a.z, t),
		K::lerp(w, a.w, t)
	);
}
KVec4 KVec4::getmin(const KVec4 &b) const {
	return KVec4(
		K::min(x, b.x),
		K::min(y, b.y),
		K::min(z, b.z),
		K::min(w, b.w)
	);
}
KVec4 KVec4::getmax(const KVec4 &b) const {
	return KVec4(
		K::max(x, b.x),
		K::max(y, b.y),
		K::max(z, b.z),
		K::max(w, b.w)
	);
}
bool KVec4::equals(const KVec4 &b, float maxerr) const {
	float dx = fabsf(x - b.x);
	float dy = fabsf(y - b.y);
	float dz = fabsf(z - b.z);
	float dw = fabsf(w - b.w);
	return (dx <= maxerr) && (dy <= maxerr) && (dz <= maxerr) && (dw <= maxerr);
}
#pragma endregion // KVec4





} // namespace
