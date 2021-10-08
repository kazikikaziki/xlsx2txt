#pragma once
#include <inttypes.h>

namespace Kamilo {

const float K_MAX_COLOR_ERROR = 0.5f / 255.0f;

class KColor32;

class KColor {
public:
	static const KColor WHITE; ///< KColor(1.0f, 1.0f, 1.0f, 1.0f) を定数化したもの
	static const KColor BLACK; ///< KColor(0.0f, 0.0f, 0.0f, 1.0f) を定数化したもの
	static const KColor ZERO;  ///< KColor(0.0f, 0.0f, 0.0f, 0.0f) を定数化したもの。コンストラクタによる初期状態 KColor() と同じ

public:
	/// c1 と c2 の間でパラメータ t (0.0 <= t <= 1.0) による線形補間を行う
	static KColor lerp(const KColor &c1, const KColor &c2, float t);

	/// 各要素ごとに大きい方の値を選択した結果を返す
	static KColor getmax(const KColor &dst, const KColor &src);

	/// 各要素ごとに小さい方の値を選択した結果を返す
	static KColor getmin(const KColor &dst, const KColor &src);

	/// 単純加算する。
	/// @param dst 元の色
	/// @param src 新しく重ねる色
	/// 各要素の値が uint8 の範囲を超えた場合は 0 または 255 に丸められる
	static KColor add(const KColor &dst, const KColor &src);

	/// 各要素の加重値 factor 及びアルファ値を考慮して加算する。
	/// @param dst 元の色
	/// @param src 新しく重ねる色
	/// @param factor 各成分の合成重み
	/// 各要素の値が uint8 の範囲を超えた場合は 0 または 255 に丸められる
	static KColor addAlpha(const KColor &dst, const KColor &src, const KColor &factor);
	static KColor addAlpha(const KColor &dst, const KColor &src);
	
	/// 単純減算する。
	/// @param dst 元の色
	/// @param src 新しく重ねる色
	/// 各要素の値が uint8 の範囲を超えた場合は 0 または 255 に丸められる
	static KColor sub(const KColor &dst, const KColor &src);

	/// 各要素の加重値 factor 及びアルファ値を考慮して減算する。
	/// @param dst 元の色
	/// @param src 新しく重ねる色
	/// @param factor 各成分の合成重み
	/// 各要素の値が uint8 の範囲を超えた場合は 0 または 255 に丸められる
	static KColor subAlpha(const KColor &dst, const KColor &src, const KColor &factor);
	static KColor subAlpha(const KColor &dst, const KColor &src);

	/// ARGB の成分ごとの乗算
	static KColor mul(const KColor &dst, const KColor &src);

	/// アルファブレンドを計算する。
	/// @param dst 元の色
	/// @param src 新しく重ねる色
	/// @param factor 各成分の合成重み
	/// 各要素の値が uint8 の範囲を超えた場合は 0 または 255 に丸められる
	static KColor blendAlpha(const KColor &dst, const KColor &src, const KColor &factor);
	static KColor blendAlpha(const KColor &dst, const KColor &src);

public:
	KColor();
	KColor(float _r, float _g, float _b, float _a);
	KColor(const KColor &rgb, float _a);
	KColor(const KColor32 &argb32);

	KColor32 toColor32() const;

	float * floats() const;

	/// RGBA 値がゼロならば true を返す
	float grayscale() const;
	/// 各成分の値を 0 以上 1 以下にクリップした結果を返す
	KColor clamp() const;
	KColor operator + (const KColor &op) const;
	KColor operator + (float k) const;
	KColor operator - (const KColor &op) const;
	KColor operator - (float k) const;
	KColor operator - () const;
	KColor operator * (const KColor &op) const;
	KColor operator * (float k) const;
	KColor operator / (float k) const;
	KColor operator / (const KColor &op) const;
	bool operator == (const KColor &op) const;
	bool operator != (const KColor &op) const;
	KColor & operator += (const KColor &op);
	KColor & operator += (float k);
	KColor & operator -= (const KColor &op);
	KColor & operator -= (float k);
	KColor & operator *= (const KColor &op);
	KColor & operator *= (float k);
	KColor & operator /= (const KColor &op);
	KColor & operator /= (float k);

	bool equals(const KColor &other, float tolerance=1.0f/255) const;

	float r, g, b, a;
};


/// KColor と同じくRGBA色を表すが、それぞれの色要素を float ではなく符号なし8ビット整数で表す。
/// 各メンバーは B, G, R, A の順番で並んでおり、Direct3D9 で使われている 32 ビット整数の ARGB 表記
/// 0xAARRGGBB のリトルエンディアン形式と同じになっている。
/// そのため、KColor32 をそのまま頂点配列の DWORD として使う事ができる
/// @see KColor
class KColor32 {
public:
	static const KColor32 WHITE; ///< KColor32(255, 255, 255, 255) を定数化したもの
	static const KColor32 BLACK; ///< KColor32(0, 0, 0, 255) を定数化したもの
	static const KColor32 ZERO;  ///< KColor32(0, 0, 0, 0) を定数化したもの。コンストラクタによる初期状態 KColor32() と同じ

	/// 各要素ごとに大きい方の値を選択した結果を返す
	static KColor32 getmax(const KColor32 &dst, const KColor32 &src);

	/// 各要素ごとに小さい方の値を選択した結果を返す
	static KColor32 getmin(const KColor32 &dst, const KColor32 &src);

	/// 単純加算する。
	/// @param dst 元の色
	/// @param src 新しく重ねる色
	/// 各要素の値が uint8 の範囲を超えた場合は 0 または 255 に丸められる
	static KColor32 add(const KColor32 &dst, const KColor32 &src);

	/// 各要素の加重値 factor 及びアルファ値を考慮して加算する。
	/// @param dst 元の色
	/// @param src 新しく重ねる色
	/// @param factor 各成分の合成重み
	/// 各要素の値が uint8 の範囲を超えた場合は 0 または 255 に丸められる
	static KColor32 addAlpha(const KColor32 &dst, const KColor32 &src, const KColor32 &factor);
	static KColor32 addAlpha(const KColor32 &dst, const KColor32 &src);
	
	/// 単純減算する。
	/// @param dst 元の色
	/// @param src 新しく重ねる色
	/// 各要素の値が uint8 の範囲を超えた場合は 0 または 255 に丸められる
	static KColor32 sub(const KColor32 &dst, const KColor32 &src);

	/// 各要素の加重値 factor 及びアルファ値を考慮して減算する。
	/// @param dst 元の色
	/// @param src 新しく重ねる色
	/// @param factor 各成分の合成重み
	/// 各要素の値が uint8 の範囲を超えた場合は 0 または 255 に丸められる
	static KColor32 subAlpha(const KColor32 &dst, const KColor32 &src, const KColor32 &factor);
	static KColor32 subAlpha(const KColor32 &dst, const KColor32 &src);

	/// ARGB の成分ごとの乗算
	static KColor32 mul(const KColor32 &dst, const KColor32 &src);
	static KColor32 mul(const KColor32 &dst, float factor);

	/// アルファブレンドを計算する。
	/// @param dst 元の色
	/// @param src 新しく重ねる色
	/// @param factor 各成分の合成重み
	/// 各要素の値が uint8 の範囲を超えた場合は 0 または 255 に丸められる
	static KColor32 blendAlpha(const KColor32 &dst, const KColor32 &src, const KColor32 &factor);
	static KColor32 blendAlpha(const KColor32 &dst, const KColor32 &src);

	/// 線形補完した色を返す。
	/// @param c1 色1
	/// @param c2 色2
	/// @param t ブレンド係数。0.0 なら c1 100% になり、1.0 なら c2 100% になる
	static KColor32 lerp(const KColor32 &c1, const KColor32 &c2, float t);

	static KColor32 fromARGB32(uint32_t argb32);
	static KColor32 fromXRGB32(uint32_t argb32);

	KColor32();
	KColor32(const KColor32 &other);

	/// コンストラクタ。
	/// 0 以上 255 以下の値で R, G, B, A を指定する。
	/// 範囲外の値を指定した場合でも丸められないので注意 (0xFF との and を取る）
	///
	/// @note
	/// 符号なし32ビットでの順番 A, R, G, B とは引数の順番が異なるので注意すること。
	/// これは KColor::KColor(float, float, float, float) の順番と合わせてあるため。
	KColor32(int _r, int _g, int _b, int _a);

	/// 0xAARRGGBB 形式の符号なし32ビット整数での色を指定する。
	/// KColor32(0x11223344) は KColor32(0x44, 0x11, 0x22, 0x33) と同じ
	explicit KColor32(uint32_t argb);

	KColor32(const KColor &color);
	bool operator == (const KColor32 &other) const;
	bool operator != (const KColor32 &other) const;

	KColor toColor() const;

	/// uint32_t での色表現（AARRGGBB）を返す
	uint32_t toUInt32() const;

	/// NTSC 加重平均法によるグレースケール値（256段階）を返す
	uint8_t grayscale() const;

	/// リトルエンディアンで uint32_t にキャストしたときに ARGB の順番になるようにする
	/// 0xAARRGGBB <==> *(uint32_t*)(&color32_value)
	uint8_t b, g, r, a;
};

namespace Test {
	void Test_color();
}

} // namespace
