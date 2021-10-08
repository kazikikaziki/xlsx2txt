#pragma once

namespace Kamilo {

class KVec2 {
public:
	KVec2(): x(0), y(0) {}
	KVec2(const KVec2 &a): x(a.x), y(a.y) {}
	KVec2(float ax, float ay): x(ax), y(ay) {}
	KVec2(int ax, int ay): x((float)ax), y((float)ay) {}

	const float * floats() const;
	float * floats();
	bool operator == (const KVec2 &b) const;
	bool operator != (const KVec2 &b) const;
	KVec2 operator + (const KVec2 &b) const;
	KVec2 operator - (const KVec2 &b) const;
	KVec2 operator - () const;
	KVec2 operator * (float b) const;
	KVec2 operator / (float b) const;
	KVec2 operator / (const KVec2 &b) const;
	KVec2 & operator /= (float b);
	KVec2 & operator /= (const KVec2 &b);
	KVec2 & operator = (const KVec2 &b);
	KVec2 & operator += (const KVec2 &b);
	KVec2 & operator -= (const KVec2 &b);
	KVec2 & operator *= (float b);
	float dot(const KVec2 &b) const; ///< 内積
	float cross(const KVec2 &b) const; ///< 外積
	float getLength() const; ///< 長さ
	float getLengthSq() const; ///< 長さ2乗
	KVec2 getNormalized() const; ///< 正規化する
	bool getNormalizedSafe(KVec2 *n) const;
	KVec2 floor() const; ///< 各要素を整数に切り下げる
	KVec2 ceil() const; ///< 各要素を整数に切り上げる
	bool isZero() const; ///< すべての要素が0に等しいか？
	bool isNormalized() const; ///< 正規化済みか？
	KVec2 lerp(const KVec2 &a, float t) const; ///< 線形補間した値を返す
	KVec2 getmin(const KVec2 &b) const;
	KVec2 getmax(const KVec2 &b) const;
	bool equals(const KVec2 &b, float maxerr) const;
	//
	float x, y;
};


class KVec3 {
public:
	KVec3(): x(0), y(0), z(0) {}
	KVec3(const KVec3 &a): x(a.x), y(a.y), z(a.z) {}
	KVec3(float ax, float ay, float az): x(ax), y(ay), z(az) {}
	KVec3(int ax, int ay, int az): x((float)ax), y((float)ay), z((float)az) {}
	
	const float * floats() const;
	float * floats();
	bool operator == (const KVec3 &b) const;
	bool operator != (const KVec3 &b) const;
	KVec3 operator + (const KVec3 &b) const;
	KVec3 operator - (const KVec3 &b) const;
	KVec3 operator - () const;
	KVec3 operator * (float b) const;
	KVec3 operator * (const KVec3 &b) const;
	KVec3 operator / (float b) const;
	KVec3 operator / (const KVec3 &b) const;
	KVec3 & operator /= (float b);
	KVec3 & operator /= (const KVec3 &b);
	float operator[](int index) const;
	float & operator[](int index);
	KVec3 & operator = (const KVec3 &b);
	KVec3 & operator += (const KVec3 &b);
	KVec3 & operator -= (const KVec3 &b);
	KVec3 & operator *= (float b);
	KVec3 & operator *= (const KVec3 &b);
	float dot(const KVec3 &b) const; ///< 内積
	KVec3 cross(const KVec3 &b) const; ///< 外積
	float getLength() const; ///< 長さ
	float getLengthSq() const; ///< 長さ2乗
	KVec3 getNormalized() const; ///< 正規化する
	bool getNormalizedSafe(KVec3 *n) const;
	KVec3 floor() const; ///< 各要素を整数に切り下げる
	KVec3 ceil() const; ///< 各要素を整数に切り上げる
	bool isZero() const; ///< すべての要素が0に等しいか
	bool isNormalized() const; ///< 正規化済みか？
	KVec3 lerp(const KVec3 &a, float t) const; ///< 線形補間した値を返す
	KVec3 getmin(const KVec3 &b) const;
	KVec3 getmax(const KVec3 &b) const;
	bool equals(const KVec3 &b, float maxerr) const;
	//
	float x, y, z;
};


class KVec4 {
public:
	KVec4(): x(0), y(0), z(0), w(0) {}
	KVec4(const KVec4 &a): x(a.x), y(a.y), z(a.z), w(a.w) {}
	KVec4(float ax, float ay, float az, float aw): x(ax), y(ay), z(az), w(aw) {}
	KVec4(int ax, int ay, int az, int aw): x((float)ax), y((float)ay), z((float)az), w((float)aw) {}

	const float * floats() const;
	float * floats();
	bool operator == (const KVec4 &b) const;
	bool operator != (const KVec4 &b) const;
	KVec4 operator + (const KVec4 &b) const;
	KVec4 operator - (const KVec4 &b) const;
	KVec4 operator - () const;
	KVec4 operator * (float b) const;
	KVec4 operator / (float b) const;
	KVec4 operator / (const KVec4 &b) const;
	KVec4 & operator /= (float b);
	KVec4 & operator /= (const KVec4 &b);
	KVec4 & operator = (const KVec4 &b);
	KVec4 & operator += (const KVec4 &b);
	KVec4 & operator -= (const KVec4 &b);
	KVec4 & operator *= (float b);
	float dot(const KVec4 &b) const; ///< 内積
	float getLengthSq() const; ///< 長さ2乗
	float getLength() const; ///< 長さ
	KVec4 getNormalized() const; ///< 正規化したものを返す
	bool isZero() const; ///< すべての要素が0に等しいか？
	bool isNormalized() const; ///< 正規化済みか？
	KVec4 lerp(const KVec4 &a, float t) const; ///< 線形補間した値を返す
	KVec4 getmin(const KVec4 &b) const;
	KVec4 getmax(const KVec4 &b) const;
	bool equals(const KVec4 &b, float maxerr) const;
	//
	float x, y, z, w;
};


} // namespace
