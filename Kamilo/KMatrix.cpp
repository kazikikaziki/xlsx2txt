#include "KMatrix.h"

#include <memory.h>
#include <math.h>
#include <float.h>
#include "KInternal.h"
#include "KQuat.h"


// Use SIMD
#define K_USE_SIMD


#ifdef K_USE_SIMD
#	include <xmmintrin.h> // 128bit simd
#endif


namespace Kamilo {




#pragma region Matrix
#define MATELM(mat, row, col)  ((mat)[(row)*4 + (col)]) // mat[row*4 + col]
float K_Matrix4Get(const float *mat, int row, int col) {
	K__ASSERT(mat);
	K__ASSERT(0 <= row && row < 4);
	K__ASSERT(0 <= col && col < 4);
	return MATELM(mat, row, col);
}
void K_Matrix4Set(float *mout, int row, int col, float value) {
	K__ASSERT(mout);
	K__ASSERT(0 <= row && row < 4);
	K__ASSERT(0 <= col && col < 4);
	MATELM(mout, row, col) = value;
}
void K_Matrix4SetRow(float *mout, int row, float a, float b, float c, float d) {
	K__ASSERT(mout);
	K__ASSERT(0 <= row && row < 4);
	MATELM(mout, row, 0) = a;
	MATELM(mout, row, 1) = b;
	MATELM(mout, row, 2) = c;
	MATELM(mout, row, 3) = d;
}
void K_Matrix4Copy(float *mout, const float *m) {
	K__ASSERT(mout);
	K__ASSERT(m);
	memcpy(mout, m, sizeof(float) * 16);
}
bool K_Matrix4Equals(const float *ma, const float *mb, float maxerr) {
	K__ASSERT(ma);
	K__ASSERT(mb);
	// いくら厳密な比較でも memcmp での比較はダメ。
	// -0.0f と 0.0f が区別されてしまう
	// if (tolerance == 0.0f) {
	//   return memcmp(ma, mb, sizeof(float)*16) == 0;
	// }
	for (int i=0; i<16; i++) {
		float err = fabsf(mb[i] - ma[i]);
		if (err > maxerr) { // err=0 かつ tolerace=0 のとき true になるように
			return false;
		}
	}
	return true;
}
float K_Matrix4RowDotCol(const float *ma, int ma_row, const float *mb, int mb_col) {
	K__ASSERT(ma);
	K__ASSERT(mb);
	// A と B の row 行 col 列ベクトル同士の内積
	return (
		MATELM(ma, ma_row, 0) * MATELM(mb, 0, mb_col) +
		MATELM(ma, ma_row, 1) * MATELM(mb, 1, mb_col) +
		MATELM(ma, ma_row, 2) * MATELM(mb, 2, mb_col) +
		MATELM(ma, ma_row, 3) * MATELM(mb, 3, mb_col)
	);
}
void K_Matrix4Identity(float *mout) {
	static const float tmp[] = {
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1,
	};
	K__ASSERT(mout);
	K_Matrix4Copy(mout, tmp);
}
void K_Matrix4Add(float *mout, const float *ma, const float *mb) {
	K__ASSERT(mout);
	K__ASSERT(ma);
	K__ASSERT(mb);
	for (int i=0; i<16; i++) {
		mout[i] = ma[i] + mb[i];
	}
}
void K_Matrix4Sub(float *mout, const float *ma, const float *mb) {
	K__ASSERT(mout);
	K__ASSERT(ma);
	K__ASSERT(mb);
	for (int i=0; i<16; i++) {
		mout[i] = ma[i] - mb[i];
	}
}
static void K__matrix4_mul_simd(float *mout, const float *ma, const float *mb) {
#ifdef K_USE_SIMD
	K__ASSERT(mout);
	K__ASSERT(ma);
	K__ASSERT(mb);

	// 4x4行列同士の掛算を高速化してみる
	// https://qiita.com/blue-7/items/6b607e1af48bc25ecb35

	// SIMDの組み込み関数のことはじめ
	// http://koturn.hatenablog.com/entry/2016/07/18/090000

	__m128 xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;

	xmm4 = _mm_loadu_ps(mb +  0);
	xmm5 = _mm_loadu_ps(mb +  4);
	xmm6 = _mm_loadu_ps(mb +  8);
	xmm7 = _mm_loadu_ps(mb + 12);

	// column0
	xmm0 = _mm_load1_ps(ma+0);
	xmm1 = _mm_load1_ps(ma+1);
	xmm2 = _mm_load1_ps(ma+2);
	xmm3 = _mm_load1_ps(ma+3);

	xmm0 = _mm_mul_ps(xmm0, xmm4);
	xmm1 = _mm_mul_ps(xmm1, xmm5);
	xmm2 = _mm_mul_ps(xmm2, xmm6);
	xmm3 = _mm_mul_ps(xmm3, xmm7);

	xmm0 = _mm_add_ps(xmm0, xmm1);
	xmm2 = _mm_add_ps(xmm2, xmm3);
	xmm0 = _mm_add_ps(xmm0, xmm2);

	_mm_storeu_ps(mout, xmm0);

	// column1
	xmm0 = _mm_load1_ps(ma+4);
	xmm1 = _mm_load1_ps(ma+5);
	xmm2 = _mm_load1_ps(ma+6);
	xmm3 = _mm_load1_ps(ma+7);

	xmm0 = _mm_mul_ps(xmm0, xmm4);
	xmm1 = _mm_mul_ps(xmm1, xmm5);
	xmm2 = _mm_mul_ps(xmm2, xmm6);
	xmm3 = _mm_mul_ps(xmm3, xmm7);

	xmm0 = _mm_add_ps(xmm0, xmm1);
	xmm2 = _mm_add_ps(xmm2, xmm3);
	xmm0 = _mm_add_ps(xmm0, xmm2);

	_mm_storeu_ps(mout+4, xmm0);

	// column2
	xmm0 = _mm_load1_ps(ma+ 8);
	xmm1 = _mm_load1_ps(ma+ 9);
	xmm2 = _mm_load1_ps(ma+10);
	xmm3 = _mm_load1_ps(ma+11);

	xmm0 = _mm_mul_ps(xmm0, xmm4);
	xmm1 = _mm_mul_ps(xmm1, xmm5);
	xmm2 = _mm_mul_ps(xmm2, xmm6);
	xmm3 = _mm_mul_ps(xmm3, xmm7);

	xmm0 = _mm_add_ps(xmm0, xmm1);
	xmm2 = _mm_add_ps(xmm2, xmm3);
	xmm0 = _mm_add_ps(xmm0, xmm2);

	_mm_storeu_ps(mout+8, xmm0);

	// column3
	xmm0 = _mm_load1_ps(ma+12);
	xmm1 = _mm_load1_ps(ma+13);
	xmm2 = _mm_load1_ps(ma+14);
	xmm3 = _mm_load1_ps(ma+15);

	xmm0 = _mm_mul_ps(xmm0, xmm4);
	xmm1 = _mm_mul_ps(xmm1, xmm5);
	xmm2 = _mm_mul_ps(xmm2, xmm6);
	xmm3 = _mm_mul_ps(xmm3, xmm7);

	xmm0 = _mm_add_ps(xmm0, xmm1);
	xmm2 = _mm_add_ps(xmm2, xmm3);
	xmm0 = _mm_add_ps(xmm0, xmm2);

	_mm_storeu_ps(mout+12, xmm0);
#endif
}
static void K__matrix4_mul_generic(float *mout, const float *ma, const float *mb) {
	#define DOT K_Matrix4RowDotCol
	K_Matrix4SetRow(mout, 0, DOT(ma,0, mb,0), DOT(ma,0, mb,1), DOT(ma,0, mb,2), DOT(ma,0, mb,3));
	K_Matrix4SetRow(mout, 1, DOT(ma,1, mb,0), DOT(ma,1, mb,1), DOT(ma,1, mb,2), DOT(ma,1, mb,3));
	K_Matrix4SetRow(mout, 2, DOT(ma,2, mb,0), DOT(ma,2, mb,1), DOT(ma,2, mb,2), DOT(ma,2, mb,3));
	K_Matrix4SetRow(mout, 3, DOT(ma,3, mb,0), DOT(ma,3, mb,1), DOT(ma,3, mb,2), DOT(ma,3, mb,3));
	#undef DOT
}
void K_Matrix4Mul(float *mout, const float *ma, const float *mb) {
#ifdef K_USE_SIMD
	K__matrix4_mul_simd(mout, ma, mb);
#else
	K__matrix4_mul_generic(mout, ma, mb);
#endif
}
void K_Matrix4MulVec4(float *vout4, const float *ma, const float *v4) {
	K__ASSERT(vout4);
	K__ASSERT(ma);
	K__ASSERT(v4);
	vout4[0] = MATELM(ma,0,0)*v4[0] + MATELM(ma,1,0)*v4[1] + MATELM(ma,2,0)*v4[2] + MATELM(ma,3,0)*v4[3];
	vout4[1] = MATELM(ma,0,1)*v4[0] + MATELM(ma,1,1)*v4[1] + MATELM(ma,2,1)*v4[2] + MATELM(ma,3,1)*v4[3];
	vout4[2] = MATELM(ma,0,2)*v4[0] + MATELM(ma,1,2)*v4[1] + MATELM(ma,2,2)*v4[2] + MATELM(ma,3,2)*v4[3];
	vout4[3] = MATELM(ma,0,3)*v4[0] + MATELM(ma,1,3)*v4[1] + MATELM(ma,2,3)*v4[2] + MATELM(ma,3,3)*v4[3];
}
void K_Matrix4MulVec3(float *vout3, const float *m, const float *v3) {
	K__ASSERT(vout3);
	K__ASSERT(m);
	K__ASSERT(v3);
	float A[4] = {v3[0], v3[1], v3[2], 1.0f}; // A = {x, y, z, 1}
	float B[4];
	K_Matrix4MulVec4(B, m, A); // B = m * A
	vout3[0] = B[0] / B[3]; // out.x = B.x / B.w
	vout3[1] = B[1] / B[3]; // out.y = B.y / B.w
	vout3[2] = B[2] / B[3]; // out.z = B.z / B.w
}
void K_Matrix4ToVec3(float *vout3, const float *m) {
	K__ASSERT(vout3);
	K__ASSERT(m);
	float x = MATELM(m, 3, 0);
	float y = MATELM(m, 3, 1);
	float z = MATELM(m, 3, 2);
	float w = MATELM(m, 3, 3);
	vout3[0] = x / w;
	vout3[1] = y / w;
	vout3[2] = z / w;
}

void K_matrix4_make_sub3x3(float *mout4x4, const float *a, int exclude_row, int exclude_col) {
	// 小行列を作成する
	// 4x4 から指定された行と列を削除し、4x4単位行列の左上の 3x3 の領域に縮める
	// （out4x4 自体は 4x4 行列であることに注意）
	K__ASSERT(mout4x4);
	K__ASSERT(a);
	K__ASSERT(0 <= exclude_row && exclude_row < 4);
	K__ASSERT(0 <= exclude_col && exclude_col < 4);
	int srcrow[3], srccol[3];
	for (int i=0; i<3; i++) {
		srcrow[i] = (i >= exclude_row) ? i+1 : i; // srcrow[小行列の行番号] = 元行列の行番号
		srccol[i] = (i >= exclude_col) ? i+1 : i; // srccol[小行列の列番号] = 元行列の列番号
	}

	// 4x4 単位行列として初期化
	K_Matrix4Identity(mout4x4);

	// 左上の 3x3 の領域に小行列をセットする
	for (int r=0; r<3; r++) {
		for (int c=0; c<3; c++) {
			float v = MATELM(a, srcrow[r], srccol[c]);
			K_Matrix4Set(mout4x4, r, c, v);
		}
	}
}
float K_matrix4_determinant_sub3x3(const float *m, int exclude_row, int exclude_col) {
	// exclude_row と exclude_col を除いた残りの 3x3 行列（小行列）の行列式を求める
	K__ASSERT(m);
	K__ASSERT(0 <= exclude_row && exclude_row < 4);
	K__ASSERT(0 <= exclude_col && exclude_col < 4);

	// 4x4 行列の左上 3x3 の領域に小行列を作成する
	float sub[16];
	K_matrix4_make_sub3x3(sub, m, exclude_row, exclude_col);

	// 4x4 行列の左上 3x3 の領域を 3x3 行列とみなし、その行列式を得る
	return (
		MATELM(sub,0,0) * MATELM(sub,1,1) * MATELM(sub,2,2) +
		MATELM(sub,1,0) * MATELM(sub,2,1) * MATELM(sub,0,2) +
		MATELM(sub,2,0) * MATELM(sub,0,1) * MATELM(sub,1,2) -
		MATELM(sub,0,0) * MATELM(sub,2,1) * MATELM(sub,1,2) -
		MATELM(sub,2,0) * MATELM(sub,1,1) * MATELM(sub,0,2) -
		MATELM(sub,1,0) * MATELM(sub,0,1) * MATELM(sub,2,2)
	);
}
float K_Matrix4Determinant(const float *m) {
	K__ASSERT(m);
	return (
		+ MATELM(m, 0, 0) * K_matrix4_determinant_sub3x3(m, 0, 0)
		- MATELM(m, 1, 0) * K_matrix4_determinant_sub3x3(m, 1, 0)
		+ MATELM(m, 2, 0) * K_matrix4_determinant_sub3x3(m, 2, 0)
		- MATELM(m, 3, 0) * K_matrix4_determinant_sub3x3(m, 3, 0)
	);
}
bool K_Matrix4Inverse(float *mout, const float *m) {
	float det = K_Matrix4Determinant(m);

	// 行列式が 0 でなければ逆行列が存在する。
	// 行列式は非常に小さい値になる可能性があるため、
	// 0 かどうかの判定には誤差付き判定関数を用いずに直接比較する。
	if (det != 0) {
		#define MINOR(mat, row, col) K_matrix4_determinant_sub3x3(mat, row, col)
		if (mout) {
			float s00= MINOR(m,0,0), s10=-MINOR(m,1,0), s20= MINOR(m,2,0), s30=-MINOR(m,3,0);
			float s01=-MINOR(m,0,1), s11= MINOR(m,1,1), s21=-MINOR(m,2,1), s31= MINOR(m,3,1);
			float s02= MINOR(m,0,2), s12=-MINOR(m,1,2), s22= MINOR(m,2,2), s32=-MINOR(m,3,2);
			float s03=-MINOR(m,0,3), s13= MINOR(m,1,3), s23=-MINOR(m,2,3), s33= MINOR(m,3,3);
			K_Matrix4SetRow(mout, 0, s00/det, s10/det, s20/det, s30/det);
			K_Matrix4SetRow(mout, 1, s01/det, s11/det, s21/det, s31/det);
			K_Matrix4SetRow(mout, 2, s02/det, s12/det, s22/det, s32/det);
			K_Matrix4SetRow(mout, 3, s03/det, s13/det, s23/det, s33/det);
		}
		#undef MINOR
		return true;
	}
	return false;
}
void K_Matrix4Transpose(float *mout, const float *m) {
	K__ASSERT(mout);
	K__ASSERT(m);
	K_Matrix4SetRow(mout, 0, MATELM(m,0,0), MATELM(m,1,0), MATELM(m,2,0), MATELM(m,3,0));
	K_Matrix4SetRow(mout, 1, MATELM(m,0,1), MATELM(m,1,1), MATELM(m,2,1), MATELM(m,3,1));
	K_Matrix4SetRow(mout, 2, MATELM(m,0,2), MATELM(m,1,2), MATELM(m,2,2), MATELM(m,3,2));
	K_Matrix4SetRow(mout, 3, MATELM(m,0,3), MATELM(m,1,3), MATELM(m,2,3), MATELM(m,3,3));
}
void K_Matrix4FromTranslate(float *mout, float x, float y, float z) {
	float tmp[] = {
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		x, y, z, 1,
	};
	K__ASSERT(mout);
	K_Matrix4Copy(mout, tmp);
}
void K_Matrix4FromScale(float *mout, float x, float y, float z) {
	float tmp[] = {
		x, 0, 0, 0,
		0, y, 0, 0,
		0, 0, z, 0,
		0, 0, 0, 1,
	};
	K__ASSERT(mout);
	K_Matrix4Copy(mout, tmp);
}
void K_Matrix4FromRotation(float *mout, const float *quat) {
	// http://stackoverflow.com/questions/1556260/convert-quaternion-rotation-to-rotation-matrix
	K__ASSERT(mout);
	K__ASSERT(quat);
	float x = quat[0];
	float y = quat[1];
	float z = quat[2];
	float w = quat[3];
	float tmp[] = {
		1-2*y*y-2*z*z,   2*x*y+2*w*z,   2*x*z-2*w*y, 0,
		  2*x*y-2*w*z, 1-2*x*x-2*z*z,   2*y*z+2*w*x, 0,
		  2*x*z+2*w*y,   2*y*z-2*w*x, 1-2*x*x-2*y*y, 0,
		            0,             0,             0, 1,
	};
	K_Matrix4Copy(mout, tmp);
}
void K_Matrix4FromSkewX(float *mout, float deg) {
	K__ASSERT(mout);
	K__ASSERT(-90 < deg && deg < 90);
	float t = tanf(K::degToRad(deg));
	float tmp[] = {
		1, 0, 0, 0,
		t, 1, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1,
	};
	K_Matrix4Copy(mout, tmp);
}
void K_Matrix4FromSkewY(float *mout, float deg) {
	K__ASSERT(mout);
	K__ASSERT(-90 < deg && deg < 90);
	float t = tanf(K::degToRad(deg));
	float tmp[] = {
		1, t, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1,
	};
	K_Matrix4Copy(mout, tmp);
}
void K_Matrix4FromOrtho(float *mout, float w, float h, float znear, float zfar) {
	K__ASSERT(mout);
	K__ASSERT(w > 0);
	K__ASSERT(h > 0);
	K__ASSERT(znear < zfar);
	// 平行投影行列（左手系）
	// http://www.opengl.org/sdk/docs/man2/xhtml/glOrtho.xml
	// http://msdn.microsoft.com/ja-jp/library/cc372881.aspx
	float x = 2.0f / w;
	float y = 2.0f / h;
	float a = 1.0f / (zfar - znear);
	float b = znear / (znear - zfar);
	float tmp[] = {
		x, 0, 0, 0,
		0, y, 0, 0,
		0, 0, a, 0,
		0, 0, b, 1,
	};
	K_Matrix4Copy(mout, tmp);
}
void K_Matrix4FromOrthoRect(float *mout, float left, float right, float bottom, float top, float znear, float zfar) {
	K__ASSERT(left < right);
	K__ASSERT(bottom < top);
	K__ASSERT(znear < zfar);
	// 平行投影行列（左手系）
	// http://www.opengl.org/sdk/docs/man2/xhtml/glOrtho.xml
	// http://msdn.microsoft.com/ja-jp/library/cc372882.aspx
	float x = 2.0f / (right - left);
	float y = 2.0f / (top - bottom);
	float z = 1.0f / (zfar - znear);
	float a = (right + left) / (right - left);
	float b = (bottom + top) / (bottom - top);
	float c = znear / (znear - zfar);
	float tmp[] = {
		x, 0, 0, 0,
		0, y, 0, 0,
		0, 0, z, 0,
		a, b, c, 1,
	};
	K_Matrix4Copy(mout, tmp);
}
void K_Matrix4FromOrthoCabinet(float *mout, float w, float h, float znear, float zfar, float dxdz, float dydz) {
	K__ASSERT(mout);
	K__ASSERT(w > 0);
	K__ASSERT(h > 0);
	K__ASSERT(znear < zfar);
	// Cabinet projection
	// http://en.wikipedia.org/wiki/Oblique_projection
	// 遠クリップ面の設定に関係なく、画面上で一定の傾きにする為のスケーリング定数
	float scale = (zfar - znear) / (h * 0.5f);
	// この行列は幅高さが 1.0 に正規化された空間で適用されるため、
	// アスペクト比の補正が必要になることに注意
	// https://www.opengl.org/discussion_boards/showthread.php/164200-Cavalier-oblique-projection
	float a = dxdz * scale * h / w;
	float b = dydz * scale;
	float shear[] = {
		1, 0, 0, 0, // <-- input x
		0, 1, 0, 0, // <-- input y
		a, b, 1, 0, // <-- input z
		0, 0, 0, 1  // <-- input w
	//	x  y  z  w  // <-- output
	};
	float ortho[16];
	K_Matrix4FromOrtho(ortho, w, h, znear, zfar);
	K_Matrix4Mul(mout, ortho, shear);
}
void K_Matrix4FromFrustum(float *mout, float width, float height, float znear, float zfar) {
	K__ASSERT(mout);
	K__ASSERT(width > 0);
	K__ASSERT(height > 0);
	K__ASSERT(znear < zfar);
	// 透視投影行列（左手系）
	// http://gunload.web.fc2.com/opengl/glfrustum.html
	// http://msdn.microsoft.com/ja-jp/library/cc372887.aspx
	float w = 2 * znear / width;
	float h = 2 * znear / height;
	float a = zfar / (zfar - znear);
	float b = zfar * znear / (znear - zfar);
	float tmp[] = {
		w, 0, 0, 0,
		0, h, 0, 0,
		0, 0, a, 1,
		0, 0, b, 0,
	};
	K_Matrix4Copy(mout, tmp);
}
void K_Matrix4FromFrustumFovX(float *mout, float fovx_deg, float aspect_w_div_h, float znear, float zfar) {
	K__ASSERT(mout);
	K__ASSERT(0 < fovx_deg && fovx_deg < 180);
	K__ASSERT(aspect_w_div_h > 0);
	K__ASSERT(znear < zfar);
	float fov_rad = K::degToRad(fovx_deg);
	float w = 2 * znear * tanf(fov_rad / 2); // 左右の視野角から近クリップ面の幅を得る
	float h = w / aspect_w_div_h;
	K_Matrix4FromFrustum(mout, w, h, znear, zfar);
}
void K_Matrix4FromFrustumFovY(float *mout, float fovy_deg, float aspect_h_div_w, float znear, float zfar) {
	K__ASSERT(mout);
	K__ASSERT(0 < fovy_deg && fovy_deg < 180);
	K__ASSERT(aspect_h_div_w > 0);
	K__ASSERT(znear < zfar);
	float fov_rad = K::degToRad(fovy_deg);
	float h = 2 * znear * tanf(fov_rad / 2); // 上下の視野角から近クリップ面での高さを得る
	float w = h / aspect_h_div_w;
	K_Matrix4FromFrustum(mout, w, h, znear, zfar);
}
void K_Matrix4FromFrustumRect(float *mout, float left, float right, float bottom, float top, float znear, float zfar) {
	K__ASSERT(mout);
	K__ASSERT(left < right);
	K__ASSERT(bottom < top);
	K__ASSERT(znear < zfar);
	// 透視投影行列（左手系）
	// http://gunload.web.fc2.com/opengl/glfrustum.html
	// http://msdn.microsoft.com/ja-jp/library/cc372889.aspx
	float x = (2 * znear) / (right - left);
	float y = (2 * znear) / (top - bottom);
	float a = (right + left) / (right - left);
	float b = (top + bottom) / (top - bottom);
	float c = -(zfar + znear) / (zfar - znear);
	float d = -(2 * zfar * znear) / (zfar - znear);
	float tmp[] = {
		x, 0, 0, 0,
		0, y, 0, 0,
		a, b, c, -1,
		0, 0, d, 0,
	};
	K_Matrix4Copy(mout, tmp);
}
#undef MATELM



#pragma endregion // Matrix







#pragma region KMatrix4
#pragma region KMatrix4 static methods
KMatrix4 KMatrix4::fromTranslation(float x, float y, float z) {
	KMatrix4 out;
	K_Matrix4FromTranslate(out.m, x, y, z);
	return out;
}
KMatrix4 KMatrix4::fromTranslation(const KVec3 &vec) {
	KMatrix4 out;
	K_Matrix4FromTranslate(out.m, vec.x, vec.y, vec.z);
	return out;
}
KMatrix4 KMatrix4::fromScale(float x, float y, float z) {
	KMatrix4 out;
	K_Matrix4FromScale(out.m, x, y, z);
	return out;
}
KMatrix4 KMatrix4::fromScale(const KVec3 &vec) {
	return fromScale(vec.x, vec.y, vec.z);
}
KMatrix4 KMatrix4::fromRotation(const KVec3 &axis, float degrees) {
	return fromRotation(KQuat::fromAxisDeg(axis, degrees));
}
KMatrix4 KMatrix4::fromRotation(const KQuat &q) {
	KMatrix4 out;
	K_Matrix4FromRotation(out.m, q.floats());
	return out;
}
KMatrix4 KMatrix4::fromSkewX(float deg) {
	KMatrix4 out;
	K_Matrix4FromSkewX(out.m, deg);
	return out;
}
KMatrix4 KMatrix4::fromSkewY(float deg) {
	KMatrix4 out;
	K_Matrix4FromSkewY(out.m, deg);
	return out;
}
KMatrix4 KMatrix4::fromOrtho(float width, float height, float znear, float zfar) {
	KMatrix4 out;
	K_Matrix4FromOrtho(out.m, width, height, znear, zfar);
	return out;
}
KMatrix4 KMatrix4::fromOrtho(float left, float right, float bottom, float top, float znear, float zfar) {
	KMatrix4 out;
	K_Matrix4FromOrthoRect(out.m, left, right, bottom, top, znear, zfar);
	return out;
}
KMatrix4 KMatrix4::fromCabinetOrtho(float w, float h, float zn, float zf, float dxdz, float dydz) {
	KMatrix4 out;
	K_Matrix4FromOrthoCabinet(out.m, w, h, zn, zf, dxdz, dydz);
	return out;
}
KMatrix4 KMatrix4::fromFrustumFovX(float fovx_deg, float aspect_w_div_h, float znear, float zfar) {
	KMatrix4 out;
	K_Matrix4FromFrustumFovX(out.m, fovx_deg, aspect_w_div_h, znear, zfar);
	return out;
}
KMatrix4 KMatrix4::fromFrustumFovY(float fovy_deg, float aspect_h_div_w, float znear, float zfar) {
	KMatrix4 out;
	K_Matrix4FromFrustumFovY(out.m, fovy_deg, aspect_h_div_w, znear, zfar);
	return out;
}
KMatrix4 KMatrix4::fromFrustum(float width, float height, float znear, float zfar) {
	KMatrix4 out;
	K_Matrix4FromFrustum(out.m, width, height, znear, zfar);
	return out;
}
KMatrix4 KMatrix4::fromFrustum(float left, float right, float bottom, float top, float znear, float zfar) {
	KMatrix4 out;
	K_Matrix4FromFrustumRect(out.m, left, right, bottom, top, znear, zfar);
	return out;
}
#pragma endregion // KMatrix4 static methods


#pragma region KMatrix4 methods
KMatrix4::KMatrix4() {
	_11=1; _12=0; _13=0; _14=0;
	_21=0; _22=1; _23=0; _24=0;
	_31=0; _32=0; _33=1; _34=0;
	_41=0; _42=0; _43=0; _44=1;
}
KMatrix4::KMatrix4(const KMatrix4 &a) {
	memcpy(m, a.m, sizeof(float)*16);
}
KMatrix4::KMatrix4(const float *values) {
	K__ASSERT(values);
	memcpy(m, values, sizeof(float)*16);
}
KMatrix4::KMatrix4(
	float e11, float e12, float e13, float e14,
	float e21, float e22, float e23, float e24,
	float e31, float e32, float e33, float e34,
	float e41, float e42, float e43, float e44
) {
	_11=e11; _12=e12; _13=e13; _14=e14;
	_21=e21; _22=e22; _23=e23; _24=e24;
	_31=e31; _32=e32; _33=e33; _34=e34;
	_41=e41; _42=e42; _43=e43; _44=e44;
}
bool KMatrix4::operator == (const KMatrix4 &a) const {
	return K_Matrix4Equals(m, a.m, FLT_MIN);
}
bool KMatrix4::operator != (const KMatrix4 &a) const {
	return !K_Matrix4Equals(m, a.m, FLT_MIN);
}
KMatrix4 KMatrix4::operator + (const KMatrix4 &a) const {
	KMatrix4 out;
	K_Matrix4Add(out.m, m, a.m);
	return out;
}
KMatrix4 KMatrix4::operator - (const KMatrix4 &a) const {
	KMatrix4 out;
	K_Matrix4Sub(out.m, m, a.m);
	return out;
}
KMatrix4 KMatrix4::operator * (const KMatrix4 &a) const {
	KMatrix4 out;
	K_Matrix4Mul(out.m, m, a.m);
	return out;
}
KMatrix4 & KMatrix4::operator += (const KMatrix4 &a) {
	*this = *this + a;
	return *this;
}
KMatrix4 & KMatrix4::operator -= (const KMatrix4 &a) {
	*this = *this - a;
	return *this;
}
KMatrix4 & KMatrix4::operator *= (const KMatrix4 &a) {
	*this = *this * a;
	return *this;
}

float KMatrix4::get(int row, int col) const {
	return K_Matrix4Get(m, row, col);
}
void KMatrix4::set(int row, int col, float value) {
	K_Matrix4Set(m, row, col, value);
}
bool KMatrix4::equals(const KMatrix4 &b, float tolerance) const {
	return K_Matrix4Equals(m, b.m, tolerance);
}
KMatrix4 KMatrix4::transpose() const {
	KMatrix4 out;
	K_Matrix4Transpose(out.m, m);
	return out;
}
float KMatrix4::determinant() const {
	return K_Matrix4Determinant(m);
}
KMatrix4 KMatrix4::inverse() const {
	KMatrix4 out;
	if (K_Matrix4Inverse(out.m, m)) {
		return out;
	}
	K_Matrix4Identity(out.m);
	return out;
}
bool KMatrix4::computeInverse(KMatrix4 *out) const {
	return K_Matrix4Inverse(out->m, m);
}
KVec4 KMatrix4::transform(const KVec4 &a) const {
	KVec4 out;
	K_Matrix4MulVec4(out.floats(), m, a.floats());
	return out;
}
KVec3 KMatrix4::transform(const KVec3 &a) const {
	KVec3 out;
	K_Matrix4MulVec3(out.floats(), m, a.floats());
	return out;
}
KVec3 KMatrix4::transformZero() const {
	KVec3 out;
	K_Matrix4ToVec3(out.floats(), m);
	return out;
}
#pragma endregion // KMatrix4 methods
#pragma endregion // KMatrix4


namespace Test {
bool _vec3_equals(const float *a, const float *b, float maxerr) {
	float dx = abs(a[0] - b[0]);
	float dy = abs(a[1] - b[1]);
	float dz = abs(a[2] - b[2]);
	return (dx <= maxerr) && (dy <= maxerr) && (dz <= maxerr);
}
void Test_matrix() {

	const float MAXERR = 0.0001f;

	float E[16];
	float inv_E[16];
	K_Matrix4Identity(E); // 単位行列
	K__VERIFY(K_Matrix4Inverse(inv_E, E)); // 単位行列の逆行列
	K__VERIFY(K_Matrix4Equals(inv_E, E, MAXERR)); // 単位行列の逆行列は単位行列

	// 平行移動
	float tr[16];
	K_Matrix4FromTranslate(tr, 12, -17, 79);

	// 平行移動の打ち消し
	float inv_tr[16];
	K_Matrix4FromTranslate(inv_tr, -12, 17, -79);

	// 平行移動の逆行列は、平行移動の打ち消しと同じになる
	float inv_tr2[16];
	K__VERIFY(K_Matrix4Inverse(inv_tr2, tr));
	K__VERIFY(K_Matrix4Equals(inv_tr2, inv_tr, MAXERR));

	// スケーリング
	float sc[16];
	K_Matrix4FromScale(sc, 2.0f, 1.0f, -2.0f);

	// 平行移動→スケーリング
	float mm[16];
	K_Matrix4Mul(mm, tr, sc);

	// 点 (100, 100, 0) は　((100+12)*2, (100-17)*1, (0-79)*2) に移動する
	float p0[] = {100, 100, 0};
	float p1[] = {(100+12)*2, (100-17)*1, (0-79)*2};
	float p2[3];
	K_Matrix4MulVec3(p2, mm, p0);
	K__VERIFY(_vec3_equals(p2, p1, MAXERR));
}
}

} // namespace
