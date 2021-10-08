/// @file
/// Copyright (c) 2020 Heliodor
/// This software is released under the MIT License.
/// http://opensource.org/licenses/mit-license.php

#pragma once
#include <memory> // std::shared_ptr
#include <string>
#include <vector>
#include <unordered_map>
#include <inttypes.h>
#include "KColor.h"
#include "KStream.h"

namespace Kamilo {

#pragma region KBmp
/// RGBA32 の生の配列。
/// メモリ管理などの機能はない
/// フォーマットは R8G8B8A8 固定
struct KBmp {
	static const int K_BMP_BYTE_DEPTH = 4;  // KBmp のフォーマットは 1ピクセル4バイト固定
	static bool isvalid(const KBmp *bmp) {
		return bmp && bmp->data && (bmp->w > 0) && (bmp->h > 0) && (bmp->pitch >= bmp->w);
	}

	KBmp() {
		w = h = pitch = 0;
		data = nullptr;
	}
	int get_offset(int x, int y) const {
		return pitch * y + x * K_BMP_BYTE_DEPTH;
	}

	int w;         ///< 横ピクセル数
	int h;         ///< 縦ピクセル数
	int pitch;     ///< 1行あたりのバイト数
	uint8_t *data; ///< RGBA 配列

};
#pragma endregion // KBmp




class KCorePng {
public:
	/// png のヘッダだけ読み取り、画像サイズを得る
	/// @param png_first_24bytes   png ファイルの最初の24バイト部分を指定する
	/// @param size                png_first_24bytes に渡したデータのバイト数（もちろん24バイト以上必要）
	/// @param[out] w 　           画像の幅
	/// @param[out] h 　           画像の高さ
	static bool readsize(const void *png_first_24bytes, int size, int *w, int *h);

	/// PNG データから画像を復元して RGBA32 配列を得る
	/// bmp が必要十分なサイズを持っているなら、画像データを bmp にセットする(この場合 w, h は無視される）
	/// bmp に nullptr を指定すると out_w, out_h に必要な画像サイズをセットする。
	static bool decode(const void *png, int size, KBmp *bmp, int *out_w, int *out_h);

	/// RGBA32 配列から PNG データを作成する
	static bool encode(std::string &out, const KBmp *bmp, int level);
};



#pragma region Image.h
enum KColorFormat {
	KColorFormat_NOFMT,  ///< フォーマット情報なし
	KColorFormat_RGBA32, ///< R8 G8 B8 A8（4バイト/ピクセル）
	KColorFormat_RGB24,  ///< R8 G8 B8（3バイト/ピクセル）
	KColorFormat_BGR24,  ///< B8 G8 R8（3バイト/ピクセル）
	KColorFormat_INDEX8, ///< インデックス画像（1バイト/ピクセル）
};


/// 画像データを保持・管理するためのクラスで、画像ファイルのセーブとロード、基本的な画像処理に対応する。
/// ブレンド関数を指定しての合成など、もう少し高度なことが行いたい場合は KImageUtils を使う
/// @see KImageUtils
/// @note
/// 代入演算子 = で代入した場合は、単なる参照コピー（ポインタのコピーと同義）になる。
/// 本当の意味でのコピーを作りたい場合は KImage::clone() を使う
class KImage {
public:
	static KImage createFromSize(int w, int h, const KColor32 &color=KColor32(0,0,0,0));
	static KImage createFromPixels(int w, int h, KColorFormat fmt, const void *data);
	static KImage createFromFileInMemory(const void *bin, size_t size);
	static KImage createFromFileInMemory(const std::string &bin);
	static KImage createFromFileName(const std::string &filename);
	static KImage createFromStream(KInputStream &input);
	static KImage createFromImageRect(const KImage &source, int x, int y, int w, int h);

	/// コンストラクタ
	/// 何のデータも保持しない。画像サイズは 0 x 0 になり、 empty() は true を返す
	KImage();
#if 0
	/// コンストラクタ
	/// 指定されたサイズと色で初期化する。
	/// @see create(int, int, const KColor32 &)
	KImage(int w, int h, const KColor32 &color);

	/// コンストラクタ。
	/// 指定されたサイズで初期化する。
	/// @see create(int, int, KColorFormat, const void *) を参照
	KImage(int w, int h, KColorFormat fmt=KColorFormat_RGBA32, const void *data=nullptr);
#endif
	/// 画像を廃棄する。普通はこの関数を呼ぶ必要はないが、
	/// 明示的に empty() の状態にしたい場合に用いる
	void destroy();

	/// 画像情報を保持していなければ true を返す
	/// この場合 getWidth() と getHeight() は 0 になり、 getData() は nullptr になる
	bool empty() const;

	/// 横ピクセル数
	int getWidth() const;

	/// 縦ピクセル数
	int getHeight() const;
	
	/// 1行分のピクセルを表現するのに何バイト使うかを返す。
	/// getBytesPerPixel() * getWidth() と一致するとは限らない
	int getPitch() const;

	/// ピクセルフォーマットを返す
	KColorFormat getFormat() const;

	/// １ピクセルを表現するのに何バイト使うかを返す。
	/// RGBA32 フォーマットなら 4 になる
	int getBytesPerPixel() const;

	/// データのアドレスを返す（読み取り専用）
	/// @see lockData()
	/// @see getOffsetBytes()
	const uint8_t * getData() const;

	bool lock(KBmp *bmp) const;
	void unlock() const;

	/// データをスレッドロックし、アドレスを返す
	/// @see unlockData()
	/// @see getOffsetBytes()
	uint8_t * lockData();

	/// データのスレッドロックを解除する
	void unlockData();

	/// データのバイト数を返す
	int getDataSize() const;

	/// 画像のコピーを作成する
	/// @note 複製には必ず clone() を用いる。代入記号 = は参照コピーになる
	KImage clone() const;
	KImage cloneRect(int x, int y, int w, int h) const;

	/// 画像の png フォーマット表現を返す。
	/// @param png_compress_level PNGの圧縮レベルを指定する。0で無圧縮、9で最大圧縮
	std::string saveToMemory(int png_compress_level=1) const;
	bool saveToFileName(const std::string &filename) const;

	/// source の部分画像を、書き込み座標 (x, y) を左上とする矩形にコピーする
	/// @note RGBA32 フォーマットでのみ利用可能
	void copyRect(int x, int y, const KImage &source, int sx, int sy, int sw, int sh);
	
	/// source 全体を、書き込み座標 (x, y) を左上とする矩形にコピーする
	/// @note RGBA32 フォーマットでのみ利用可能
	void copyImage(int x, int y, const KImage &source);

	/// ピクセル色を得る。
	/// 範囲外座標を指定した場合は KColor32::ZERO を返す
	/// @note RGBA32 フォーマットでのみ利用可能
	KColor32 getPixel(int x, int y) const;
	KColor32 getPixelBox(int x, int y, int w, int h) const;

	/// ピクセル色を設定する。
	/// @note RGBA32 フォーマットでのみ利用可能
	void setPixel(int x, int y, const KColor32 &color);

#if 1 // 廃止予定
	KColor32 getPixelFast(int x, int y) const { return getPixel(x, y); }
	void setPixelFast(int x, int y, const KColor32 &color) { setPixel(x, y, color); }
#endif
	/// 画像を color で塗りつぶす
	/// fill(KColor32::ZERO) にすると 0 で塗りつぶすことになる
	/// @note RGBA32 フォーマットでのみ利用可能
	void fill(const KColor32 &color);

	class Impl {
	public:
		virtual ~Impl() {}
		virtual uint8_t * buffer() = 0;
		virtual uint8_t * lock() = 0;
		virtual void unlock() = 0;
		virtual void info(int *w, int *h, KColorFormat *fmt) = 0;
	};

private:

	/// 画像サイズと色を指定して画像を初期化する
	/// @param w, h  画像サイズ
	/// @param color 色
	void _create(int w, int h, const KColor32 &color=KColor32(0,0,0,0));

	/// 画像サイズとフォーマット、ピクセルデータを指定して画像を初期化する
	/// @param w, h 画像サイズ
	/// @param fmt  フォーマット
	/// @param data ピクセルデータ。w, h, fmt で指定したサイズとフォーマットに従っていないといけない。
	///             nullptr を指定した場合は 0 で初期化される
	void _create(int w, int h, KColorFormat fmt, const void *data);

	/// 画像をロードする。
	/// このライブラリでは画像のロードについて外部ライブラリ stb_image.h を利用しているため、
	/// 受け入れ可能フォーマットは stb_image.h に依存する。
	/// ライブラリが通常オプションでビルドされているなら、JPEG, PNG, BMP, GIF, PSD, PIC, PNM　に対応する。
	/// @see https://github.com/nothings/stb
	bool _loadFromMemory(const void *bin, size_t size);
	bool _loadFromMemory(const std::string &bin);
	bool _loadFromFileName(const std::string &filename);

	void get_info(int *w, int *h, KColorFormat *f) const;

	std::shared_ptr<Impl> m_impl;
};


/// 画像 KImage に対して様々な加工をするためのヘルパークラス
///
/// @snippet KImage.cpp test
class KImageUtils {
public:
	struct CopyRange {
		int dst_x, dst_y;
		int src_x, src_y;
		int cpy_w, cpy_h;
	};

	static KColor32 raw_get_pixel(const KBmp &bmp, int x, int y);
	static void raw_set_pixel(KBmp &bmp, int x, int y, const KColor32 &color);
	static void raw_copy(KBmp &dst, const KBmp &src);
	static void raw_copy_rect(KBmp &dst, int x, int y, const KBmp &src, int sx, int sy, int sw, int sh);
	static void raw_fill_rect(KBmp &bmp, int left, int top, int width, int height, const KColor32 &color);
	static void raw_get_channel(KBmp &dst, const KBmp &src, int channel);
	static void raw_join_channels(KBmp &dst, const KBmp *srcR, int valR, const KBmp *srcG, int valG, const KBmp *srcB, int valB, const KBmp *srcA, int valA);
	static void raw_half_scale(KBmp &dst, const KBmp &src);
	static void raw_double_scale(KBmp &dst, const KBmp &src);
	static void raw_gray_to_alpha(KBmp &bmp, const KColor32 &fill);
	static void raw_alpha_to_gray(KBmp &bmp);
	static void raw_add(KBmp &bmp, const KColor32 &color32);
	static void raw_mul(KBmp &bmp, const KColor32 &color32);
	static void raw_mul(KBmp &bmp, float factor);
	static void raw_inv(KBmp &bmp);
	static void raw_blur_x(KBmp &dst, const KBmp &src);
	static void raw_blur_y(KBmp &dst, const KBmp &src);
	static void raw_outline(KBmp &dst, const KBmp &src, const KColor32 &color);
	static void raw_expand(KBmp &dst, const KBmp &src, const KColor32 &color);
	static void raw_silhouette(KBmp &bmp, const KColor32 &color);
	static void raw_perlin(KBmp &bmp, int xwrap, int ywrap, float mul);
	static bool raw_has_non_black_pixel(const KBmp &bmp, int x, int y, int w, int h);
	static bool raw_has_opaque_pixel(const KBmp &bmp, int x, int y, int w, int h);
	static void raw_scan_non_black_cells(const KBmp &bmp, int cellsize, std::vector<int> *cells, int *xcells, int *ycells);
	static void raw_scan_opaque_cells(const KBmp &bmp, int cellsize, std::vector<int> *cells, int *xcells, int *ycells);
	static bool raw_adjust_rect(const KBmp &bmp, int *_x, int *_y, int *_w, int *_h);
	static bool raw_adjust_rect(const KBmp &dst, const KBmp &src, CopyRange *range);
	static void raw_offset(KBmp &dst, const KBmp &src, int dx, int dy);

	/// アスキーアート文字列からビットマップ画像を作成する
	/// @param tex_w, tex_h 作成する画像のサイズ
	/// @param bmp_w, bmp_h 入力画像のサイズ
	/// @param colormap 文字から色への変換テーブル。このテーブルに無い文字はすべて KColor::ZERO として扱う
	/// @param bmp_chars １ピクセル１文字であらわしたビットマップ
	/// @snippet CreateInspector.cpp aa_test
	static KImage makeImageFromAscii(int tex_w, int tex_h, int bmp_w, int bmp_h, const std::unordered_map<char, KColor32> &colormap, const char *bmp_chars);

	/// 指定範囲の中にRGB>0なピクセルが存在するなら true を返す
	static bool hasNonBlackPixel(const KImage &img, int x, int y, int w, int h);

	/// 指定範囲の中に不透明なピクセルが存在するなら true を返す
	static bool hasOpaquePixel(const KImage &img, int x, int y, int w, int h);

	/// 画像をセル分割したとき、それぞれのセルがRGB>0なピクセルを含むかどうかを調べる
	/// @param      image          画像
	/// @param      cellsize       セルの大きさ（ピクセル単位、正方形）
	/// @param[out] cells          セル内のピクセル状態。全ピクセルのRGBが0なら false, 一つでも非ゼロピクセルが存在するなら true
	/// @param[out] xcells, ycells 取得した縦横のセルの数。nullptrでもよい。画像が中途半端な大きさだった場合は余白が拡張されることに注意
	static void scanNonBlackCells(const KImage &image, int cellsize, std::vector<int> *cells, int *xcells, int *ycells);

	/// 画像をセル分割したとき、それぞれのセルが不透明ピクセルを含むかどうかを調べる
	/// @param      image          画像
	/// @param      cellsize       セルの大きさ（ピクセル単位、正方形）
	/// @param[out] cells          セル内のピクセル状態。全ピクセルの Alpha が 0 なら false, 一つでも Alpha > 0 なピクセルが存在するなら true
	/// @param[out] xcells, ycells 取得した縦横のセルの数。nullptrでもよい。画像が中途半端な大きさだった場合は余白が拡張されることに注意
	static void scanOpaqueCells(const KImage &image, int cellsize, std::vector<int> *cells, int *xcells, int *ycells);

	/// 指定範囲が画像範囲を超えないように調整する
	static bool adjustRect(int img_w, int img_h, int *_x, int *_y, int *_w, int *_h);
	static bool adjustRect(int dst_w, int dst_h, int src_w, int src_h, CopyRange *range);
	static bool adjustRect(const KImage &dst, const KImage &src, CopyRange *range);

	/// ユーザー定義の方法で画像を合成する
	///
	/// ピクセル毎にどのような計算を行うかを func に指定して使う。
	///
	/// @param image 基底画像
	/// @param x     重ねる位置 X
	/// @param y     重ねる位置 Y
	/// @param over  重ねる画像
	/// @param func  ピクセルごとの合成計算を定義したオブジェクト
	///
	/// func には以下の形式に従った関数オブジェクトを指定する
	/// @code
	/// class myfunc {
	/// public:
	///     KColor32 operator()(const KColor32 &dst, const KColor32 &src) const {
	///        ...
	///     }
	/// };
	/// @endcode
	///
	/// Alpha や Add などよく使う合成に関しては、以下の定義済みの関数オブジェクトがある
	///   - KBlendFuncAlpha
	///   - KBlendFuncAdd
	///   - KBlendFuncSub
	///   - KBlendFuncMin
	///   - KBlendFuncMax
	template <typename T> static void blend(KImage &image, int x, int y, const KImage &over, const T &func);
	template <typename T> static void raw_blend(KBmp &image, int x, int y, const KBmp &over, const T &func);

	/// #blend の func に KBlendFuncAlpha を与えたもの
	static void blendAlpha(KImage &image, const KImage &over);

	/// #blend の func に KBlendFuncAlpha を与えたもの
	static void blendAlpha(KImage &image, int x, int y, const KImage &over, const KColor32 &color=KColor32::WHITE);

	/// #blend の func に KBlendFuncAdd を与えたもの
	static void blendAdd(KImage &image, const KImage &over);

	/// #blend の func に KBlendFuncAdd を与えたもの
	static void blendAdd(KImage &image, int x, int y, const KImage &over, const KColor32 &color=KColor32::WHITE);
	
	/// X方向に1段階のブラーをかける。エフェクトを強くしたい場合は重ね掛けする
	static void blurX(KImage &image);
	
	/// X方向に1段階のブラーをかける。エフェクトを強くしたい場合は重ね掛けする
	static void blurX(KImage &dst, const KImage &src);

	/// Y方向に1段階のブラーをかける。エフェクトを強くしたい場合は重ね掛けする
	static void blurY(KImage &image);

	/// Y方向に1段階のブラーをかける。エフェクトを強くしたい場合は重ね掛けする
	static void blurY(KImage &dst, const KImage &src);

	/// 画像サイズを 1/2 にする
	static void halfScale(KImage &image);

	/// 画像サイズを 2 倍にする
	static void doubleScale(KImage &image);

	/// ピクセルごとにカラー定数 color を加算合成する
	static void add(KImage &image, const KColor32 &color);

	/// ピクセルごとに定数 mul を乗算する
	static void mul(KImage &image, float mul);

	/// ピクセルごとにカラー定数 color を乗算合成する
	/// @param image 画像
	/// @param color image の各ピクセルに対して乗算合成する色。
	///              単純な乗算ではないことに注意。
	///              乗算数 color の各値は 0 ～ 255 になっているが、それを 0.0 ～ 1.0 と解釈して乗算する。
	///              例えば color に KColor32(127,127,127,127) を指定するのは、ピクセルのRGBA各値を 0.5 倍するのと同じ
	static void mul(KImage &image, const KColor32 &color);

	/// RGB を反転する。Alphaは変更しない
	static void inv(KImage &image);

	/// 画像を dx, dy だけずらす
	static void offset(KImage &image, int dx, int dy);

	/// 水平線を描画する
	static void horzLine(KImage &image, int x0, int x1, int y, const KColor32 &color);

	/// 垂直線を描画する
	static void vertLine(KImage &image, int x, int y0, int y1, const KColor32 &color);

	/// 透明部分だけを残し、不透明部分をすべて color に置き換える
	static void silhouette(KImage &image, const KColor32 &color);

	/// 不透明部分を拡張する
	static void expand(KImage &image, const KColor32 &color);

	/// 不透明部分と完全透明部分の境目に輪郭線を描画する
	static void outline(KImage &image, const KColor32 &color);

	/// RGB グレースケール化した値を Alpha に適用し、RGBを fill で塗りつぶすことで、
	/// グレースケール画像をアルファチャンネルに変換する
	static void grayToAlpha(KImage &image, const KColor32 &fill=KColor32::WHITE);
	static void alphaToGray(KImage &image);
	
	/// パーリンノイズ画像（正方形）を作成する
	static KImage perlin(int size=256, int xwrap=4.0f, int ywrap=4.0f, float mul=1.0f);

	// RGBを変更する
	// rgba_matrix4x4 には RGBA の係数（重み）を記述した行列を指定する
	//        inR inG inB inConst
	// outR	= ### ### ### ###
	// outG	= ### ### ### ###
	// outB	= ### ### ### ###
	/*
		float matrix[] = {
		//  inR   inG   inB   inConst
			0.0f, 0.5f, 0.5f, 0.0f, // outR
			0.0f, 0.4f, 0.4f, 0.0f, // outG
			1.0f, 0.0f, 0.0f, 0.0f, // outB
		};
	*/
	static void modifyColor(KImage &image, const float *matrix4x3);
};

#pragma region BlendFunc
/// min 合成用のクラスで、 KImageUtils::blend() の pred に指定して使う
/// @see KImageUtils::blend
struct KBlendFuncMin {
	KColor32 operator()(const KColor32 &dst, const KColor32 &src) const {
		return KColor32::getmin(dst, src);
	}
};

/// max 合成用のクラスで、 KImageUtils::blend() の pred に指定して使う
/// @see KImageUtils::blend
struct KBlendFuncMax {
	KColor32 operator()(const KColor32 &dst, const KColor32 &src) const {
		return KColor32::getmax(dst, src);
	}
};

/// add 合成用のクラスで、 KImageUtils::blend() の pred に指定して使う
/// @see KImageUtils::blend
struct KBlendFuncAdd {
	KColor32 m_color; ///< src に乗算する色
	KBlendFuncAdd(KColor32 color=KColor32::WHITE) {
		m_color = color;
	}
	KColor32 operator()(const KColor32 &dst, const KColor32 &src) const {
		return KColor32::addAlpha(dst, src, m_color);
	}
};

/// sub 合成用のクラスで、 KImageUtils::blend() の pred に指定して使う
/// @see KImageUtils::blend
struct KBlendFuncSub {
	KColor32 m_color; ///< src に乗算する色
	KBlendFuncSub(KColor32 color=KColor32::WHITE) {
		m_color = color;
	}
	KColor32 operator()(const KColor32 &dst, const KColor32 &src) const {
		return KColor32::subAlpha(dst, src, m_color);
	}
};

/// alpha 合成用のクラスで、 KImageUtils::blend() の pred に指定して使う
/// @see KImageUtils::blend
struct KBlendFuncAlpha {
	KColor32 m_color; ///< src に乗算する色
	KBlendFuncAlpha(KColor32 color=KColor32::WHITE) {
		m_color = color;
	}
	KColor32 operator()(const KColor32 &dst, const KColor32 &src) const {
		return KColor32::blendAlpha(dst, src, m_color);
	}
};
#pragma endregion // BlendFunc


/// KImageUtils::blend 実装部
template <typename T> void KImageUtils::blend(KImage &base, int x, int y, const KImage &over, const T &func) {
	KBmp bsbmp; base.lock(&bsbmp);
	KBmp ovbmp; over.lock(&ovbmp);
	raw_blend(bsbmp, x, y, ovbmp, func);
	over.unlock();
	base.unlock();
}

template <typename T> void KImageUtils::raw_blend(KBmp &base, int x, int y, const KBmp &over, const T &func) {
	CopyRange range;
	range.dst_x = x;
	range.dst_y = y;
	range.src_x = 0;
	range.src_y = 0;
	range.cpy_w = over.w;
	range.cpy_h = over.h;
	raw_adjust_rect(base, over, &range);
	for (int yy=0; yy<range.cpy_h; yy++) {
		for (int xx=0; xx<range.cpy_w; xx++) {
			KColor32 src = raw_get_pixel(over, range.src_x + xx, range.src_y + yy);
			KColor32 dst = raw_get_pixel(base, range.dst_x + xx, range.dst_y + yy);
			KColor32 out = func(dst, src);
			raw_set_pixel(base, range.dst_x + xx, range.dst_y + yy, out);
		}
	}
}

namespace Test {
void Test_imageutils();
void Test_output_parlin_graph_images();
void Test_output_parlin_texture_images();
}
#pragma endregion // Image.h



} // namespace
