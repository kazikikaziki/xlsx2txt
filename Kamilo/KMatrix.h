#pragma once
#include "KVec.h"

namespace Kamilo {

class KQuat;

class KMatrix4 {
public:
	/// 平行移動行列
	static KMatrix4 fromTranslation(float x, float y, float z);
	static KMatrix4 fromTranslation(const KVec3 &vec);
	
	/// スケーリング行列
	static KMatrix4 fromScale(float x, float y, float z);
	static KMatrix4 fromScale(const KVec3 &vec);
	
	/// 回転行列（左手系）
	static KMatrix4 fromRotation(const KVec3 &axis, float degrees);
	static KMatrix4 fromRotation(const KQuat &q);
	
	/// せん断行列
	/// X軸に平行にdeg度傾斜させる（Y軸が時計回りにdeg度傾く）
	static KMatrix4 fromSkewX(float deg);

	/// せん断行列
	/// Y軸に平行にdeg度傾斜させる（X軸が時計回りにdeg度傾く）
	static KMatrix4 fromSkewY(float deg);

	/// 平行投影行列（左手系＝画面奥側がZの正方向）
	/// @param width  近クリップ面での視錐台の幅
	/// @param height 近クリップ面での視錐台の高さ
	/// @param znear  近クリップ面
	/// @param zfar   遠クリップ面
	static KMatrix4 fromOrtho(float width, float height, float znear, float zfar);

	/// 平行投影行列（左手系＝画面奥側がZの正方向）
	/// @param left   左クリップ面
	/// @param right  右クリップ面
	/// @param bottom 下クリップ面
	/// @param top    上クリップ面
	/// @param znear  近クリップ面
	/// @param zfar   遠クリップ面
	static KMatrix4 fromOrtho(float left, float right, float bottom, float top, float znear, float zfar);

	/// 斜投影行列作成（左手系:画面奥側がZの正方向）
	/// @param width  近クリップ面での視錐台の幅
	/// @param height 近クリップ面での視錐台の高さ
	/// @param znear  近クリップ面
	/// @param zfar   遠クリップ面
	/// @param dxdz 剪断係数(dx/dz)。Z増加に対するXの増加割合
	/// @param dydz 剪断係数(dy/dz)。Z増加に対するYの増加割合
	static KMatrix4 fromCabinetOrtho(float width, float height, float znear, float zfar, float dxdz, float dydz);

	/// 透視投影行列（左手系:画面奥側がZの正方向）
	/// @param fovx_deg       左右方向の視野角（度）
	/// @param aspect_w_div_h アスペクト比（W/H）
	/// @param znear          近クリップ面
	/// @param zfar           遠クリップ面
	static KMatrix4 fromFrustumFovX(float fovx_deg, float aspect_w_div_h, float znear, float zfar);

	/// 透視投影行列（左手系:画面奥側がZの正方向）
	/// @param fovy_deg       上下方向の視野角（度）
	/// @param aspect_h_div_w アスペクト比（H/W）
	/// @param znear          近クリップ面
	/// @param zfar           遠クリップ面
	static KMatrix4 fromFrustumFovY(float fovy_deg, float aspect_h_div_w, float znear, float zfar);

	/// 透視投影行列（左手系:画面奥側がZの正方向）
	/// @param width  近クリップ面での視錐台の幅
	/// @param height 近クリップ面での視錐台の高さ
	/// @param znear  近クリップ面
	/// @param zfar   遠クリップ面
	static KMatrix4 fromFrustum(float width, float height, float znear, float zfar);

	/// 透視投影行列（左手系:画面奥側がZの正方向）
	/// @param left   左クリップ面
	/// @param right  右クリップ面
	/// @param bottom 下クリップ面
	/// @param top    上クリップ面
	/// @param znear  近クリップ面
	/// @param zfar   遠クリップ面
	static KMatrix4 fromFrustum(float left, float right, float bottom, float top, float znear, float zfar);

public:
	/// コンストラクタ（単位行列で初期化）
	KMatrix4();
	
	/// コンストラクタ（コピー）
	KMatrix4(const KMatrix4 &a);

	/// コンストラクタ（成分を与えて初期化）
	KMatrix4(float e11, float e12, float e13, float e14,
	        float e21, float e22, float e23, float e24,
	        float e31, float e32, float e33, float e34,
	        float e41, float e42, float e43, float e44);

	KMatrix4(const float *values);

	bool operator == (const KMatrix4 &a) const;
	bool operator != (const KMatrix4 &a) const;
	KMatrix4 operator + (const KMatrix4 &a) const;
	KMatrix4 operator - (const KMatrix4 &a) const;
	KMatrix4 operator * (const KMatrix4 &a) const;
	KMatrix4 & operator += (const KMatrix4 &a);
	KMatrix4 & operator -= (const KMatrix4 &a);
	KMatrix4 & operator *= (const KMatrix4 &a);

	float get(int row, int col) const;
	void set(int row, int col, float value);
	const float * floats() const { return m; }

	bool equals(const KMatrix4 &b, float tolerance) const;
	KMatrix4 transpose() const;
	float determinant() const;
	KMatrix4 inverse() const;

	KVec4 transform(const KVec4 &v) const;
	KVec3 transform(const KVec3 &v) const;
	KVec3 transformZero() const;

	bool computeInverse(KMatrix4 *out) const;

	union {
		struct {
			float _11, _12, _13, _14;
			float _21, _22, _23, _24;
			float _31, _32, _33, _34;
			float _41, _42, _43, _44;
		};
		float m[16];
	};
};

namespace Test {
void Test_matrix();
}

} // namespace
