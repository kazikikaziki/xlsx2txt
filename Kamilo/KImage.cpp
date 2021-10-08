#include "KImage.h"

// Use stb_image.h
#define K_USE_STB_IMAGE

// Use stb_perlin.h
//#define K_USE_STB_PERLIN


#ifdef K_USE_STB_IMAGE
	// stb_image
	// https://github.com/nothings/stb
	// https://github.com/nothings/stb/blob/master/stb_image.h
	// license: public domain
	#include "stb_image.h"
	#include "stb_image_write.h"

	// ヘッダではなく、ソースファイルとしてとりこむ。
	// ここを削除すると実体が存在しなくなるために未定義の識別子エラーが発生するし、
	// 他にもこうして .c ファイルとして取り込んでいる部分があると二重定義でエラーになるので注意。
	#define STB_IMAGE_IMPLEMENTATION
	#include "stb_image.h"

	#define STB_IMAGE_WRITE_IMPLEMENTATION
	#include "stb_image_write.h"

#endif // K_USE_STB_IMAGE


#include <unordered_set>
#include <mutex>
#include "KMath.h"
#include "KInternal.h"

#ifdef K_USE_STB_PERLIN
#	include "stb_perlin.h"
#endif

namespace Kamilo {

static inline KColor32 _color32_mean2(const KColor32 &x, const KColor32 &y) {
	return KColor32(
		(x.r + y.r) / 2,
		(x.g + y.g) / 2,
		(x.b + y.b) / 2,
		(x.a + y.a) / 2
	);
}
static inline KColor32 _color32_mean3(const KColor32 &x, const KColor32 &y, const KColor32 &z) {
	return KColor32(
		(x.r + y.r + z.r) / 3,
		(x.g + y.g + z.g) / 3,
		(x.b + y.b + z.b) / 3,
		(x.a + y.a + z.a) / 3
	);
}
static inline KColor32 _color32_mean4(const KColor32 &x, const KColor32 &y, const KColor32 &z, const KColor32 &w) {
	return KColor32(
		(x.r + y.r + z.r + w.r) / 4,
		(x.g + y.g + z.g + w.g) / 4,
		(x.b + y.b + z.b + w.b) / 4,
		(x.a + y.a + z.a + w.a) / 4
	);
}

/// a ÷ b の端数切り上げ整数を返す
static int _ceil_div(int a, int b) {
	K__ASSERT(b > 0);
	return (int)ceilf((float)a / b);
}







#pragma region KCorePng
static void KCorePng__swap_endian32(void *data32) {
	K__ASSERT(data32);
	uint8_t *p = (uint8_t *)data32;
	uint8_t t;
	t = p[0]; p[0]=p[3]; p[3] = t; // p[0] <==> p[3]
	t = p[1]; p[1]=p[2]; p[2] = t; // p[1] <==> p[2]
}
// void stbi_write_func(void *context, void *data, int size);
void KCorePng__write_cb(void *context, void *data, int size) {
	std::string &bin = *(std::string*)context;
	bin.assign((const char *)data, size);
}

bool KCorePng::readsize(const void *png_first_24bytes, int size, int *w, int *h) {
	if (png_first_24bytes == nullptr) return false;
	if (size < 24) return false;

	// PNG ファイルフォーマット
	// https://www.setsuki.com/hsp/ext/png.htm

	// Portable Network Graphics
	// https://ja.wikipedia.org/wiki/Portable_Network_Graphics

	const char *SIGN = "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A";
	const char *IHDR = "IHDR";

	// offset [size]
	// 0x0000 [8] --- png signature (89 50 4E 47 0D 0A 1A 0A)
	// 0x0008 [4] --- IHDR chunk length
	// 0x000C [4] --- IHDR chunk type ("IHDR")
	// 0x0010 [4] --- image width (big endian!!)
	// 0x0014 [4] --- image height

	// png signature
	const uint8_t *ptr = (const uint8_t *)png_first_24bytes;
	if (memcmp(ptr+0x0000, SIGN, 8) != 0) {
		return false; // png ではない
	}

	// IHDR chunk type "IHDR"
	if (memcmp(ptr+0x000C, IHDR, 4) != 0) {
		return false; // 不正なヘッダ
	}

	uint32_t w32 = *(uint32_t*)(ptr+0x0010); // image width in BIG-ENDIGAN
	uint32_t h32 = *(uint32_t*)(ptr+0x0014); // image height in BIG-ENDIGAN
	KCorePng__swap_endian32(&w32);
	KCorePng__swap_endian32(&h32);

	if (w) *w = (int)w32;
	if (h) *h = (int)h32;
	return true;
}
bool KCorePng::decode(const void *data, int size, KBmp *bmp, int *out_w, int *out_h) {
	#ifdef K_USE_STB_IMAGE
	{
		const int COMP = 4;
		bool ok = false;
		int w=0, h=0;
		uint8_t *ptr = stbi_load_from_memory((const stbi_uc *)data, size, &w, &h, nullptr, COMP);
		if (ptr) {
			ok = true;
			if (KBmp::isvalid(bmp)) {
				if ((bmp->w >= w) && (bmp->h >= h)) {
					K__ASSERT(bmp->pitch == w*COMP);
					memcpy(bmp->data, ptr, w*h*COMP);
				} else {
					// サイズが足りない
				}
			}
			if (out_w) *out_w = w;
			if (out_h) *out_h = h;
			stbi_image_free(ptr);
		}
		return ok;
	}
	#else
	{
		K__NOT_AVAIL;
		return false;
	}
	#endif
}
bool KCorePng::encode(std::string &out, const KBmp *bmp, int level) {
	#ifdef K_USE_STB_IMAGE
	{
		static std::mutex s_mutex;
		s_mutex.lock(); // 圧縮レベルの指定にはグローバル変数を使うため、スレッドロックしておく
		int oldlevel = stbi_write_png_compression_level;
	
		out.clear();
		stbi_write_png_compression_level = level;
		stbi_write_png_to_func(KCorePng__write_cb, &out, bmp->w, bmp->h, 4, bmp->data, bmp->w*4);
		stbi_write_png_compression_level = oldlevel;
	
		s_mutex.unlock();
		return !out.empty();
	}
	#else
	{
		K__NOT_AVAIL;
		return false;
	}
	#endif
}

#pragma endregion // PNG









#pragma region Image.cpp

class CImageImpl: public KImage::Impl {
public:
	std::vector<uint8_t> m_buf;
	int m_width;
	int m_height;
	KColorFormat m_format;

	CImageImpl(int w, int h, KColorFormat fmt) {
		m_width = w;
		m_height = h;
		m_format = fmt;
		switch (m_format) {
		case KColorFormat_RGBA32: m_buf.resize(m_width * m_height * 4); break;
		case KColorFormat_RGB24:  m_buf.resize(m_width * m_height * 3); break;
		case KColorFormat_BGR24:  m_buf.resize(m_width * m_height * 3); break;
		case KColorFormat_INDEX8: m_buf.resize(m_width * m_height * 1); break;
		default: break;
		}
	}
	virtual void info(int *w, int *h, KColorFormat *f) override {
		if (w) *w = m_width;
		if (h) *h = m_height;
		if (f) *f = m_format;
	}
	virtual uint8_t * buffer() override {
		return m_buf.data();
	}
	virtual uint8_t * lock() override {
		return m_buf.data();
	}
	virtual void unlock() override {
	}
};




static void kk_blit(KImage &dstImg, int dstRectX, int dstRectY, const KImage &srcImg, int srcRectX, int srcRectY, int copyRectW, int copyRectH) {
	int dstW = dstImg.getWidth();
	int dstH = dstImg.getHeight();
	int dstPitch = dstImg.getPitch();
	KColorFormat dstFmt = dstImg.getFormat();

	int srcW = srcImg.getWidth();
	int srcH = srcImg.getHeight();
	int srcPitch = srcImg.getPitch();
	KColorFormat srcFmt = srcImg.getFormat();

	// コピー範囲を調整
	KImageUtils::CopyRange range;
	range.dst_x = dstRectX;
	range.dst_y = dstRectY;
	range.src_x = srcRectX;
	range.src_y = srcRectY;
	range.cpy_w = copyRectW;
	range.cpy_h = copyRectH;
	KImageUtils::adjustRect(dstImg, srcImg, &range);

	// サイズが 0 だったらコピー不要
	if (dstW == 0 || dstH == 0) return;
	if (srcW == 0 || srcH == 0) return;
	if (range.cpy_w == 0 || range.cpy_h == 0) return;

	if (dstFmt == srcFmt) {
		// 両画像のフォーマットが等しい。
		if (range.dst_x==0 && range.dst_y==0 && // コピー先範囲の左上が原点？
			range.src_x==0 && range.src_y==0 && // コピー元範囲の左上が原点？
			range.cpy_w == srcW && range.cpy_h == srcH && // コピー範囲が全体サイズに等しい？
			dstPitch == srcPitch
		) {
			// 全範囲コピーかつピッチが等しい
			uint8_t *dstPtr = dstImg.lockData();
			const uint8_t *srcPtr = srcImg.getData();
			K__ASSERT(dstPtr);
			K__ASSERT(srcPtr);
			memcpy(dstPtr, srcPtr, srcImg.getDataSize());
			dstImg.unlockData();

		} else {
			// コピー範囲が異なる。
			// ライン単位でコピーする
			const int pixelBytes = srcImg.getBytesPerPixel();
			const int copyBytes = range.cpy_w * pixelBytes;
			uint8_t *dstPtr = dstImg.lockData();
			const uint8_t *srcPtr = srcImg.getData();
			K__ASSERT(dstPtr);
			K__ASSERT(srcPtr);
			for (int yy=0; yy<range.cpy_h; yy++) {
				const int dstOffset = ((range.dst_y + yy) * dstW + range.dst_x) * pixelBytes;
				const int srcOffset = ((range.src_y + yy) * srcW + range.src_x) * pixelBytes;
				memcpy(dstPtr+dstOffset, srcPtr+srcOffset, copyBytes);
			}
			dstImg.unlockData();
		}

	} else {
		// 両画像のフォーマットが異なる。
		// ピクセル単位でコピーする
		for (int yy=0; yy<range.cpy_h; yy++) {
			for (int xx=0; xx<range.cpy_w; xx++) {
				const KColor32 color = srcImg.getPixel(range.src_x + xx, range.src_y + yy);
				dstImg.setPixel(range.dst_x + xx, range.dst_y + yy, color);
			}
		}
	}
}



#pragma region KImage
KImage::KImage() {
	_create(0, 0, KColorFormat_NOFMT, nullptr);
}
#if 0
KImage::KImage(int w, int h, const KColor32 &color) {
	_create(w, h, color);
}
KImage::KImage(int w, int h, KColorFormat fmt, const void *data) {
	_create(w, h, fmt, data);
}
#endif
void KImage::_create(int w, int h, const KColor32 &color) {
	_create(w, h, KColorFormat_RGBA32, nullptr);
	fill(color);
}
void KImage::_create(int w, int h, KColorFormat fmt, const void *data) {
	m_impl = nullptr;
	if (w > 0 && h > 0) {
		m_impl = std::make_shared<CImageImpl>(w, h, fmt);
		if (data) {
			void *buf = m_impl->lock();
			memcpy(buf, data, getDataSize());
			m_impl->unlock();
		}
	}
}
void KImage::destroy() {
	_create(0, 0, KColorFormat_NOFMT, nullptr);
}
void KImage::get_info(int *w, int *h, KColorFormat *f) const {
	if (m_impl) {
		m_impl->info(w, h, f);
	}
}
bool KImage::lock(KBmp *bmp) const {
	int w=0, h=0;
	KColorFormat f = KColorFormat_NOFMT;
	if (m_impl) {
		m_impl->info(&w, &h, &f);
		if (f == KColorFormat_RGBA32) {
			bmp->data = const_cast<uint8_t*>(m_impl->buffer());
			bmp->w = w;
			bmp->h = h;
			bmp->pitch = w * KBmp::K_BMP_BYTE_DEPTH;
			return true;
		}
	}
	return false;
}
void KImage::unlock() const {
}

const uint8_t * KImage::getData() const {
	if (m_impl) {
		return m_impl->buffer();
	}
	return nullptr;
}
int KImage::getDataSize() const {
	int w=0, h=0;
	get_info(&w, &h, nullptr);
	return getBytesPerPixel() * w * h;
}
uint8_t * KImage::lockData() {
	if (m_impl) {
		return m_impl->lock();
	}
	return nullptr;
}
void KImage::unlockData() {
	if (m_impl) {
		m_impl->unlock();
	}
}
bool KImage::empty() const {
	int w=0, h=0;
	get_info(&w, &h, nullptr);
	return w==0 || h==0;
}
int KImage::getWidth() const {
	int w = 0;
	get_info(&w, nullptr, nullptr);
	return w;
}
int KImage::getHeight() const {
	int h = 0;
	get_info(nullptr, &h, nullptr);
	return h;
}
KColorFormat KImage::getFormat() const {
	KColorFormat fmt = KColorFormat_NOFMT;
	get_info(nullptr, nullptr, &fmt);
	return fmt;
}
int KImage::getPitch() const {
	int w=0, h=0;
	KColorFormat fmt = KColorFormat_NOFMT;
	get_info(&w, &h, &fmt);
	if (w > 0 && h > 0) {
		switch (fmt) {
		case KColorFormat_NOFMT:  return getDataSize() / h;
		case KColorFormat_RGBA32: return w * 4;
		case KColorFormat_RGB24:  return w * 3;
		case KColorFormat_BGR24:  return w * 3;
		case KColorFormat_INDEX8: return w;
		}
	}
	return 0;
}
int KImage::getBytesPerPixel() const {
	switch (getFormat()) {
	case KColorFormat_NOFMT:  return 0;
	case KColorFormat_RGBA32: return 4;
	case KColorFormat_RGB24:  return 3;
	case KColorFormat_BGR24:  return 3;
	case KColorFormat_INDEX8: return 1;
	default: return 0;
	}
}
KImage KImage::clone() const {
	int w=0, h=0;
	KColorFormat fmt = KColorFormat_NOFMT;
	get_info(&w, &h, &fmt);
	return KImage::createFromPixels(w, h, fmt, getData());
}
KImage KImage::cloneRect(int x, int y, int w, int h) const {
	return KImage::createFromImageRect(*this, x, y, w, h);
}
std::string KImage::saveToMemory(int png_compress_level) const {
	std::string bin;
	KBmp bmp;
	if (lock(&bmp)) {
		KCorePng::encode(bin, &bmp, png_compress_level);
		unlock();
	}
	if (bin.empty()) {
		K__ERROR("E_PNG: FAILED TO MAKE PNG IMAGE");
	}
	return bin;
}
bool KImage::saveToFileName(const std::string &filename_u8) const {
	std::string bin = saveToMemory(1);
	if (bin.empty()) {
		return false;
	}
	FILE *fp = K::fileOpen(filename_u8, "wb");
	if (fp == nullptr) {
		return false;
	}
	fwrite(bin.data(), bin.size(), 1, fp);
	fclose(fp);
	return true;
}
bool KImage::_loadFromFileName(const std::string &filename_u8) {
	FILE *fp = K::fileOpen(filename_u8, "rb");
	if (fp == nullptr) {
		return false;
	}
	std::string bin;
	{
		uint8_t buf[1024 * 10];
		size_t n = 0;
		while ((n=fread(buf, 1, sizeof(buf), fp)) > 0) {
			size_t i = bin.size();
			bin.resize(i + n);
			memcpy(&bin[i], buf, n);
		}
	}
	fclose(fp);
	if (bin.empty()) {
		return false;
	}
	return _loadFromMemory(bin);
}
bool KImage::_loadFromMemory(const std::string &bin) {
	return _loadFromMemory(bin.data(), bin.size());
}
bool KImage::_loadFromMemory(const void *bin, size_t size) {
	bool ok = false;

	int w=0, h=0;
	if (KCorePng::readsize(bin, size, &w, &h)) { // 画像サイズ取得
		_create(w, h); // 画像作成
		KBmp bmp;
		if (lock(&bmp)) {
			ok = KCorePng::decode(bin, size, &bmp, nullptr, nullptr); // 画像をロード
			unlock();
		}
	} else {
		// pngではない。一度すべて読みこんで画像サイズを得る
		if (KCorePng::decode(bin, size, nullptr, &w, &h)) {
			_create(w, h); // 画像作成
			KBmp bmp;
			if (lock(&bmp)) {
				ok = KCorePng::decode(bin, size, &bmp, nullptr, nullptr); // 画像をロード
				unlock();
			}
		}
	}

	if (ok) {
		return true;
	} else {
		destroy();
		K__ERROR("Failed to load image");
		return false;
	}
}
void KImage::copyRect(int dst_x, int dst_y, const KImage &src, int src_x, int src_y, int cpy_w, int cpy_h) {
	kk_blit(*this, dst_x, dst_y, src, src_x, src_y, cpy_w, cpy_h);
}
void KImage::copyImage(int x, int y, const KImage &source) {
	int w = source.getWidth();
	int h = source.getHeight();
	copyRect(x, y, source, 0, 0, w, h);
}
KColor32 KImage::getPixel(int x, int y) const {
	int w=0, h=0; KColorFormat fmt=KColorFormat_NOFMT;
	get_info(&w, &h, &fmt);
	if (x < 0 || w <= x) return KColor32::ZERO;
	if (y < 0 || h <= y) return KColor32::ZERO;
	const uint8_t *data = getData();
	const uint8_t *ptr;
	switch (fmt) {
	case KColorFormat_RGBA32:
		ptr = data + (w * y + x) * 4;
		return KColor32(ptr[0], ptr[1], ptr[2], ptr[3]);

	case KColorFormat_RGB24: 
		ptr = data + (w * y + x) * 3;
		return KColor32(ptr[0], ptr[1], ptr[2], 255);

	case KColorFormat_BGR24: 
		ptr = data + (w * y + x) * 3;
		return KColor32(ptr[2], ptr[1], ptr[0], 255);

	case KColorFormat_INDEX8:
		ptr = data + (w * y + x) * 1;
		return KColor32(ptr[0], ptr[0], ptr[0], 255); // インデックス情報しかないので、インデックス値をグレースケール化した色を返す

	default:
		return KColor32::ZERO;
	}
}
KColor32 KImage::getPixelBox(int x, int y, int w, int h) const {
	int imgW=0, imgH=0;
	get_info(&imgW, &imgH, nullptr);
	
	int r=0, g=0, b=0, a=0;
	for (int yy=y; yy<y+h; yy++) {
		for (int xx=x; xx<x+w; xx++) {
			const int px = KMath::min(xx, imgW-1); // 範囲外座標の場合は端の色を見る
			const int py = KMath::min(yy, imgH-1);
			const KColor32 color = getPixel(px, py);
			r += color.r;
			g += color.g;
			b += color.b;
			a += color.a;
		}
	}

	const int div = w * h;
	r = KMath::clampi(r / div, 0, 255);
	g = KMath::clampi(g / div, 0, 255);
	b = KMath::clampi(b / div, 0, 255);
	a = KMath::clampi(a / div, 0, 255);
	return KColor32(r, g, b, a);
}
void KImage::setPixel(int x, int y, const KColor32 &color) {
	int w=0, h=0; KColorFormat fmt=KColorFormat_NOFMT;
	get_info(&w, &h, &fmt);
	if (x < 0 || w <= x) return;
	if (y < 0 || h <= y) return;
	uint8_t *data = lockData();
	uint8_t *ptr;
	switch (fmt) {
	case KColorFormat_RGBA32:
		ptr = data + (w * y + x) * 4;
		ptr[0] = color.r;
		ptr[1] = color.g;
		ptr[2] = color.b;
		ptr[3] = color.a;
		break;

	case KColorFormat_RGB24:
		ptr = data + (w * y + x) * 3;
		ptr[0] = color.r;
		ptr[1] = color.g;
		ptr[2] = color.b;
		break;

	case KColorFormat_BGR24:
		ptr = data + (w * y + x) * 3;
		ptr[0] = color.b;
		ptr[1] = color.g;
		ptr[2] = color.r;
		break;

	case KColorFormat_INDEX8:
		ptr = data + (w * y + x) * 1;
		ptr[0] = color.r; // r をインデックス値として使う
		break;
	}
	unlockData();
}
void KImage::fill(const KColor32 &color) {
	int w=0, h=0; KColorFormat fmt=KColorFormat_NOFMT;
	get_info(&w, &h, &fmt);
	if (w==0 || h==0) return;

	uint8_t *ptr = lockData();
	K__ASSERT(ptr);
	switch (fmt) {
	case KColorFormat_RGBA32:
		if (color==KColor32::ZERO) {
			memset(ptr, 0, w*h*4);
		} else {
			// 1行目を真面目に処理
			for (int x=0; x<w; x++) {
				ptr[x*4+0] = color.r;
				ptr[x*4+1] = color.g;
				ptr[x*4+2] = color.b;
				ptr[x*4+3] = color.a;
			}
			// 2行目以降には1行目をコピーする
			for (int y=1; y<h; y++) {
				memcpy(ptr + w*4*y, ptr, w*4);
			}
		}
		break;

	case KColorFormat_RGB24:
		if (color==KColor32::ZERO) {
			memset(ptr, 0, w*h*3);
		} else {
			// 1行目を真面目に処理
			for (int x=0; x<w; x++) {
				ptr[x*3+0] = color.r;
				ptr[x*3+1] = color.g;
				ptr[x*3+2] = color.b;
			}
			// 2行目以降には1行目をコピーする
			for (int y=1; y<h; y++) {
				memcpy(ptr + w*3*y, ptr, w*3);
			}
		}
		break;

	case KColorFormat_BGR24:
		if (color==KColor32::ZERO) {
			memset(ptr, 0, w*h*3);
		} else {
			// 1行目を真面目に処理
			for (int x=0; x<w; x++) {
				ptr[x*3+0] = color.b;
				ptr[x*3+1] = color.g;
				ptr[x*3+2] = color.r;
			}
			// 2行目以降には1行目をコピーする
			for (int y=1; y<h; y++) {
				memcpy(ptr + w*3*y, ptr, w*3);
			}
		}
		break;

	case KColorFormat_INDEX8:
		memset(ptr, color.r, w*h); // r をインデックス値として使う
		break;
	}
	unlockData();
}
KImage KImage::createFromSize(int w, int h, const KColor32 &color) {
	KImage img;
	img._create(w, h, color);
	return img;
}
KImage KImage::createFromPixels(int w, int h, KColorFormat fmt, const void *data) {
	KImage img;
	img._create(w, h, fmt, data);
	return img;
}
KImage KImage::createFromFileInMemory(const void *bin, size_t size) {
	KImage img;
	img._loadFromMemory(bin, size);
	return img;
}
KImage KImage::createFromFileInMemory(const std::string &bin) {
	KImage img;
	img._loadFromMemory(bin);
	return img;
}
KImage KImage::createFromStream(KInputStream &input) {
	if (input.isOpen()) {
		std::string bin = input.readBin();
		return createFromFileInMemory(bin);
	} else {
		K__ERROR("Empty input stream");
		return KImage();
	}
}
KImage KImage::createFromFileName(const std::string &filename) {
	KImage img;
	img._loadFromFileName(filename);
	return img;
}
KImage KImage::createFromImageRect(const KImage &source, int x, int y, int w, int h) {
	KImage img = KImage::createFromSize(w, h);
	img.copyRect(0, 0, source, x, y, w, h);
	return img;
}
#pragma endregion // KImage


#pragma region KImageUtils
KColor32 KImageUtils::raw_get_pixel(const KBmp &bmp, int x, int y) {
	const uint8_t *p = bmp.data + (bmp.pitch * y + x * 4);
	return KColor32(
		p[0], // r
		p[1], // g
		p[2], // b
		p[3]  // a
	);
}
void KImageUtils::raw_set_pixel(KBmp &bmp, int x, int y, const KColor32 &color) {
	uint8_t *p = bmp.data + (bmp.pitch * y + x * 4);
	p[0] = color.r;
	p[1] = color.g;
	p[2] = color.b;
	p[3] = color.a;
}
bool KImageUtils::raw_adjust_rect(const KBmp &bmp, int *_x, int *_y, int *_w, int *_h) {
	K__ASSERT(bmp.w >= 0);
	K__ASSERT(bmp.h >= 0);
	K__ASSERT(_x && _y && _w && _h);
	bool changed = false;
	int x0 = *_x;
	int y0 = *_y;
	int x1 = x0 + *_w; // x1は x範囲+1 であることに注意！
	int y1 = y0 + *_h; // y1は y範囲+1 であることに注意！
	if (x0 < 0) { x0 = 0; changed = true; }
	if (y0 < 0) { y0 = 0; changed = true; }
	if (x1 > bmp.w) { x1 = bmp.w; changed = true; }
	if (y1 > bmp.h) { y1 = bmp.h; changed = true; }
	if (changed) {
		*_x = x0;
		*_y = y0;
		*_w = KMath::max(0, x1 - x0);
		*_h = KMath::max(0, y1 - y0);
		return true;
	}
	return false;
}
bool KImageUtils::raw_adjust_rect(const KBmp &dst, const KBmp &src, CopyRange *range) {
	return adjustRect(dst.w, dst.h, src.w, src.h, range);
}

void KImageUtils::raw_copy(KBmp &dst, const KBmp &src) {
	K__ASSERT(dst.w == src.w);
	K__ASSERT(dst.h == src.h);

	if (dst.pitch == src.pitch) {
		// ピッチサイズがおなじ。一括コピーできる
		int size = src.pitch * src.h;
		memcpy(dst.data, src.data, size);

	} else {
		// ピッチが異なる。1行ずつコピーする
		int linesize = KMath::min(src.pitch, dst.pitch);
		for (int y=0; y<src.h; y++) {
			const uint8_t *s = src.data + src.get_offset(0, y);
			/***/ uint8_t *d = dst.data + dst.get_offset(0, y);
			memcpy(d, s, linesize);
		}
	}
}
void KImageUtils::raw_copy_rect(KBmp &dst, int x, int y, const KBmp &src, int sx, int sy, int sw, int sh) {
	// コピー範囲を調整
	KImageUtils::CopyRange range;
	range.dst_x = x;
	range.dst_y = y;
	range.src_x = sx;
	range.src_y = sy;
	range.cpy_w = sw;
	range.cpy_h = sh;
	raw_adjust_rect(dst, src, &range);

	int cpylen = range.cpy_w * KBmp::K_BMP_BYTE_DEPTH;
	for (int yy=0; yy<range.cpy_h; yy++) {
		int dst_offset = dst.get_offset(range.dst_x, range.dst_y + yy);
		int src_offset = src.get_offset(range.src_x, range.src_y + yy);
		memcpy(dst.data+dst_offset, src.data+src_offset, cpylen);
	}
}
void KImageUtils::raw_offset(KBmp &dst, const KBmp &src, int dx, int dy) {
	raw_fill_rect(dst, 0, 0, dst.w, dst.h, KColor::ZERO);
	raw_copy_rect(dst, dx, dy, src, 0, 0, src.w, src.h);
}
void KImageUtils::raw_fill_rect(KBmp &bmp, int left, int top, int width, int height, const KColor32 &color) {
	// 塗りつぶし範囲を調整
	int x = left;
	int y = top;
	int w = width;
	int h = height;
	raw_adjust_rect(bmp, &x, &y, &w, &h);

	// 1行目だけまじめに処理する
	if (w > 0 && h > 0) {
		for (int i=x; i<x+w; i++) {
			uint8_t *p = bmp.data + bmp.get_offset(i, y);
			p[0] = color.r;
			p[1] = color.g;
			p[2] = color.b;
			p[3] = color.a;
		}
	}
	// 2行目以降は、1行目をコピーする
	if (w > 0 && h > 0) {
		for (int i=y+1; i<y+h; i++) {
			uint8_t *src = bmp.data + bmp.get_offset(x, y); // 1行目
			uint8_t *dst = bmp.data + bmp.get_offset(x, i); // 1行目以降
			memcpy(dst, src, bmp.pitch);
		}
	}
}
void KImageUtils::raw_get_channel(KBmp &dst, const KBmp &src, int channel) {
	K__ASSERT(dst.w == src.w);
	K__ASSERT(dst.h == src.h);
	K__ASSERT(0 <= channel && channel < KBmp::K_BMP_BYTE_DEPTH);
	for (int y=0; y<src.h; y++) {
		for (int x=0; x<src.w; x++) {
			int offset = src.get_offset(x, y);
			uint8_t *s = src.data + offset;
			uint8_t *d = dst.data + offset;
			d[0] = s[channel]; // R
			d[1] = s[channel]; // G
			d[2] = s[channel]; // B
			d[3] = 255;        // A
		}
	}
}

// srcR: R チャンネルとして使う画像。グレースケールだと仮定して、この画像の R 値を使う。nullptrだった場合は valR の値を使う
// rscG: G チャンネルとして使う画像。
// rscB: B チャンネルとして使う画像。
// rscA: A チャンネルとして使う画像。
void KImageUtils::raw_join_channels(KBmp &dst,
	const KBmp *srcR, int valR,
	const KBmp *srcG, int valG,
	const KBmp *srcB, int valB,
	const KBmp *srcA, int valA
) {
	if (srcR) { K__ASSERT(dst.w==srcR->w && dst.h==srcR->h); }
	if (srcG) { K__ASSERT(dst.w==srcG->w && dst.h==srcG->h); }
	if (srcB) { K__ASSERT(dst.w==srcB->w && dst.h==srcB->h); }
	if (srcA) { K__ASSERT(dst.w==srcA->w && dst.h==srcA->h); }
	for (int y=0; y<dst.h; y++) {
		for (int x=0; x<dst.w; x++) {
			int offset = dst.get_offset(x, y);
			const uint8_t R = srcR ? (srcR->data + offset)[0] : valR; // srcR の R 成分または valR を取って、新しいピクセルの R とする
			const uint8_t G = srcG ? (srcG->data + offset)[0] : valG; // srcG の R 成分または valG を取って、新しいピクセルの G とする
			const uint8_t B = srcB ? (srcB->data + offset)[0] : valB; // srcB の R 成分または valB を取って、新しいピクセルの B とする
			const uint8_t A = srcA ? (srcA->data + offset)[0] : valA; // srcA の R 成分または valA を取って、新しいピクセルの A とする
			KColor32 out(R, G, B, A);
			raw_set_pixel(dst, x, y, out);
		}
	}
}
void KImageUtils::raw_half_scale(KBmp &dst, const KBmp &src) {
	// src を、その半分のサイズ dst に書き出す
	K__ASSERT(dst.w*2 <= src.w); // src のサイズが奇数だった時に 1 ピクセルの誤差が発生するので == で判定したらダメ
	K__ASSERT(dst.h*2 <= src.h);
	for (int y=0; y<dst.h; y++) {
		for (int x=0; x<dst.w; x++) {
			// (x, y) を左上とする2x2の4ピクセルの平均色を計算する
			KColor32 A = raw_get_pixel(src, x*2,   y*2  );
			KColor32 B = raw_get_pixel(src, x*2+1, y*2  );
			KColor32 C = raw_get_pixel(src, x*2,   y*2+1);
			KColor32 D = raw_get_pixel(src, x*2+1, y*2+1);
			KColor32 out = _color32_mean4(A, B, C, D);
			raw_set_pixel(dst, x, y, out);
		}
	}
}
void KImageUtils::raw_double_scale(KBmp &dst, const KBmp &src) {
	// src を、その倍のサイズ dst に書き出す
	K__ASSERT(src.w*2 <= dst.w); // dst のサイズが奇数だった時に 1 ピクセルの誤差が発生するので == で判定したらダメ
	K__ASSERT(src.h*2 <= dst.h);
	for (int y=0; y<src.h; y++) {
		for (int x=0; x<src.w; x++) {
			KColor32 c = raw_get_pixel(src, x, y);
			raw_set_pixel(dst, x*2,   y*2,   c);
			raw_set_pixel(dst, x*2+1, y*2,   c);
			raw_set_pixel(dst, x*2,   y*2+1, c);
			raw_set_pixel(dst, x*2+1, y*2+1, c);
		}
	}
}
void KImageUtils::raw_gray_to_alpha(KBmp &bmp, const KColor32 &fill) {
	// RGB グレースケール値を Alpha に変換し、RGBを fill で塗りつぶす
	for (int y=0; y<bmp.h; y++) {
		for (int x=0; x<bmp.w; x++) {
			KColor32 color = raw_get_pixel(bmp, x, y);
			KColor32 out;
			out.r = fill.r;
			out.g = fill.g;
			out.b = fill.b;
			out.a = color.grayscale();
			raw_set_pixel(bmp, x, y, out);
		}
	}
}
void KImageUtils::raw_alpha_to_gray(KBmp &bmp) {
	for (int y=0; y<bmp.h; y++) {
		for (int x=0; x<bmp.w; x++) {
			KColor32 color = raw_get_pixel(bmp, x, y);
			KColor32 out;
			out.r = color.a;
			out.g = color.a;
			out.b = color.a;
			out.a = 255;
			raw_set_pixel(bmp, x, y, out);
		}
	}
}
void KImageUtils::raw_add(KBmp &bmp, const KColor32 &color32) {
	for (int y=0; y<bmp.h; y++) {
		for (int x=0; x<bmp.w; x++) {
			KColor32 color = raw_get_pixel(bmp, x, y);
			KColor32 out = KColor32::add(color, color32);
			raw_set_pixel(bmp, x, y, out);
		}
	}
}
void KImageUtils::raw_mul(KBmp &bmp, const KColor32 &color32) {
	for (int y=0; y<bmp.h; y++) {
		for (int x=0; x<bmp.w; x++) {
			KColor32 color = raw_get_pixel(bmp, x, y);
			KColor32 out = KColor32::mul(color, color32);
			raw_set_pixel(bmp, x, y, out);
		}
	}
}
void KImageUtils::raw_mul(KBmp &bmp, float factor) {
	for (int y=0; y<bmp.h; y++) {
		for (int x=0; x<bmp.w; x++) {
			KColor32 color = raw_get_pixel(bmp, x, y);
			KColor32 out = KColor32::mul(color, factor);
			raw_set_pixel(bmp, x, y, out);
		}
	}
}
void KImageUtils::raw_inv(KBmp &bmp) {
	// RGB を反転する。Alphaは無変更
	for (int y=0; y<bmp.h; y++) {
		for (int x=0; x<bmp.w; x++) {
			KColor32 color = raw_get_pixel(bmp, x, y);
			KColor32 out;
			out.r = 255 - color.r; 
			out.g = 255 - color.g;
			out.b = 255 - color.b;
			out.a = color.a;
			raw_set_pixel(bmp, x, y, out);
		}
	}
}
void KImageUtils::raw_blur_x(KBmp &dst, const KBmp &src) {
	K__ASSERT(dst.w >= src.w);
	K__ASSERT(dst.h >= src.h);
	for (int y=0; y<src.h; y++) {
		for (int x=1; x+1<src.w; x++) {
			KColor32 color0 = raw_get_pixel(src, x-1, y);
			KColor32 color1 = raw_get_pixel(src, x  , y);
			KColor32 color2 = raw_get_pixel(src, x+1, y);
			KColor32 out = _color32_mean3(color0, color1, color2);
			raw_set_pixel(dst, x, y, out);
		}
	}
}
void KImageUtils::raw_blur_y(KBmp &dst, const KBmp &src) {
	K__ASSERT(dst.w >= src.w);
	K__ASSERT(dst.h >= src.h);
	for (int x=0; x<src.w; x++) {
		for (int y=1; y+1<src.h; y++) {
			KColor32 color0 = raw_get_pixel(src, x, y-1);
			KColor32 color1 = raw_get_pixel(src, x, y  );
			KColor32 color2 = raw_get_pixel(src, x, y+1);
			KColor32 out = _color32_mean3(color0, color1, color2);
			raw_set_pixel(dst, x, y, out);
		}
	}
}
void KImageUtils::raw_outline(KBmp &dst, const KBmp &src, const KColor32 &color) {
	K__ASSERT(dst.w >= src.w);
	K__ASSERT(dst.h >= src.h);
	for (int y=0; y<src.h; y++) {
		for (int x=0; x<src.w; x++) {
			KColor32 dot = raw_get_pixel(src, x, y);
			if (dot.a == 0) {
				// 完全透明ピクセルである。
				// 周りに不透明ピクセルがあれば輪郭点を打つ
				if ((x+1 <  src.w && raw_get_pixel(src, x+1, y).a > 0) ||
				    (x-1 >= 0     && raw_get_pixel(src, x-1, y).a > 0) ||
				    (y+1 <  src.h && raw_get_pixel(src, x, y+1).a > 0) ||
				    (y-1 >= 0     && raw_get_pixel(src, x, y-1).a > 0)
				) {
					raw_set_pixel(dst, x, y, color);
				}
			}
		}
	}
}
void KImageUtils::raw_expand(KBmp &dst, const KBmp &src, const KColor32 &color) {
	for (int y=0; y<src.h; y++) {
		for (int x=0; x<src.w; x++) {
			KColor32 dot = raw_get_pixel(src, x, y);
			// 周囲ピクセルを MAX 合成する
			for (int i=-1; i<=1; i++) for (int j=-1; j<=1; j++) {
				int xx = KMath::clampi(x+i, 0, src.w-1);
				int yy = KMath::clampi(y+i, 0, src.h-1);
				KColor col = raw_get_pixel(src, xx, yy);
				dot = KColor::getmax(dot, col);
			}
			raw_set_pixel(dst, x, y, dot);
		}
	}
}
void KImageUtils::raw_silhouette(KBmp &bmp, const KColor32 &color) {
	for (int y=0; y<bmp.h; y++) {
		for (int x=0; x<bmp.w; x++) {
			KColor32 dot = raw_get_pixel(bmp, x, y);
			dot.r = color.r;
			dot.g = color.g;
			dot.b = color.b;
			dot.a = (uint8_t)((int)dot.a * (int)color.a / 255);
			raw_set_pixel(bmp, x, y, dot);
		}
	}
}
void KImageUtils::raw_perlin(KBmp &bmp, int x_wrap, int y_wrap, float mul) {
	for (int y=0; y<bmp.h; y++) {
		for (int x=0; x<bmp.w; x++) {
			float px = (float)x / bmp.w;
			float py = (float)y / bmp.h;
			float pz = 0.0f;
			float noise = KMath::perlin01(px, py, pz, x_wrap, y_wrap, 0);
			uint8_t val = (uint8_t)KMath::lerp(0, 255, noise * mul);
			// グレースケール画像として書き込む
			KColor32 color;
			color.r = val;
			color.g = val;
			color.b = val;
			color.a = 255;
			raw_set_pixel(bmp, x, y, color);
		}
	}
}
bool KImageUtils::raw_has_non_black_pixel(const KBmp &bmp, int x, int y, int w, int h) {
	K__ASSERT(bmp.data);
	K__ASSERT(0 <= x && x+w <= bmp.w);
	K__ASSERT(0 <= y && y+h <= bmp.h);
	for (int yi=y; yi<y+h; yi++) {
		for (int xi=x; xi<x+w; xi++) {
			KColor32 color = raw_get_pixel(bmp, xi, yi);
			if (color.r || color.g || color.b) return true;
		}
	}
	return false;
}

/// 指定範囲の中に不透明なピクセルが存在するなら true を返す
bool KImageUtils::raw_has_opaque_pixel(const KBmp &bmp, int x, int y, int w, int h) {
	K__ASSERT(bmp.data);
	K__ASSERT(0 <= x && x+w <= bmp.w);
	K__ASSERT(0 <= y && y+h <= bmp.h);
	for (int yi=y; yi<y+h; yi++) {
		for (int xi=x; xi<x+w; xi++) {
			KColor32 color = raw_get_pixel(bmp, xi, yi);
			if (color.a) return true;
		}
	}
	return false;
}

/// 画像をセル分割したとき、それぞれのセルがRGB>0なピクセルを含むかどうかを調べる
/// img 画像
/// cellsize セルの大きさ（ピクセル単位、正方形）
/// cells 有効なピクセルを含むセルのインデックス配列
void KImageUtils::raw_scan_non_black_cells(const KBmp &bmp, int cellsize, std::vector<int> *cells, int *xcells, int *ycells) {
	K__ASSERT(bmp.data);
	K__ASSERT(bmp.w > 0);
	K__ASSERT(bmp.h > 0);
	int xcount = _ceil_div(bmp.w, cellsize);
	int ycount = _ceil_div(bmp.h, cellsize);
	cells->clear();
	for (int yi=0; yi<ycount; yi++) {
		for (int xi=0; xi<xcount; xi++) {
			int x = cellsize * xi;
			int y = cellsize * yi;
			int w = cellsize;
			int h = cellsize;
			raw_adjust_rect(bmp, &x, &y, &w, &h);
			if (raw_has_non_black_pixel(bmp, x, y, w, h)) {
				cells->push_back(xcount * yi + xi);
			}
		}
	}
	if (xcells) *xcells = xcount;
	if (ycells) *ycells = ycount;
}

/// 画像をセル分割したとき、それぞれのセルが不透明ピクセルを含むかどうかを調べる
/// img 画像
/// cellsize セルの大きさ（ピクセル単位、正方形）
/// cells 有効なピクセルを含むセルのインデックス配列
void KImageUtils::raw_scan_opaque_cells(const KBmp &bmp, int cellsize, std::vector<int> *cells, int *xcells, int *ycells) {
	K__ASSERT(bmp.data);
	K__ASSERT(bmp.w > 0);
	K__ASSERT(bmp.h > 0);
	int xcount = _ceil_div(bmp.w, cellsize);
	int ycount = _ceil_div(bmp.h, cellsize);
	cells->clear();
	for (int yi=0; yi<ycount; yi++) {
		for (int xi=0; xi<xcount; xi++) {
			int x = cellsize * xi;
			int y = cellsize * yi;
			int w = cellsize;
			int h = cellsize;
			raw_adjust_rect(bmp, &x, &y, &w, &h);
			if (raw_has_opaque_pixel(bmp, x, y, w, h)) {
				cells->push_back(xcount * yi + xi);
			}
		}
	}
	if (xcells) *xcells = xcount;
	if (ycells) *ycells = ycount;
}

KImage KImageUtils::makeImageFromAscii(int tex_w, int tex_h, int bmp_w, int bmp_h, const std::unordered_map<char, KColor32> &colormap, const char *bmp_chars) {
	KImage img = KImage::createFromSize(tex_w, tex_h, KColor32());
	for (int y=0; y<bmp_h; y++) {
		for (int x=0; x<bmp_w; x++) {
			char ascii = bmp_chars[bmp_w * y + x];
			auto it = colormap.find(ascii);
			if (it != colormap.end()) {
				img.setPixel(x, y, it->second);
			}
		}
	}
	return img;
}

/// 指定範囲の中にRGB>0なピクセルが存在するなら true を返す
bool KImageUtils::hasNonBlackPixel(const KImage &img, int x, int y, int w, int h) {
	bool result = false;
	KBmp raw;
	img.lock(&raw);
	result = raw_has_non_black_pixel(raw, x, y, w, h);
	img.unlock();
	return result;
}

/// 指定範囲の中に不透明なピクセルが存在するなら true を返す
bool KImageUtils::hasOpaquePixel(const KImage &img, int x, int y, int w, int h) {
	bool result = false;
	KBmp raw;
	img.lock(&raw);
	result = raw_has_opaque_pixel(raw, x, y, w, h);
	img.unlock();
	return result;
}

/// 画像をセル分割したとき、それぞれのセルがRGB>0なピクセルを含むかどうかを調べる
/// img 画像
/// cellsize セルの大きさ（ピクセル単位、正方形）
/// cells 有効なピクセルを含むセルのインデックス配列
void KImageUtils::scanNonBlackCells(const KImage &img, int cellsize, std::vector<int> *cells, int *xcells, int *ycells) {
	KBmp raw;
	img.lock(&raw);
	raw_scan_non_black_cells(raw, cellsize, cells, xcells, ycells);
	img.unlock();
}

/// 画像をセル分割したとき、それぞれのセルが不透明ピクセルを含むかどうかを調べる
/// img 画像
/// cellsize セルの大きさ（ピクセル単位、正方形）
/// cells 有効なピクセルを含むセルのインデックス配列
void KImageUtils::scanOpaqueCells(const KImage &img, int cellsize, std::vector<int> *cells, int *xcells, int *ycells) {
	KBmp raw;
	img.lock(&raw);
	raw_scan_opaque_cells(raw, cellsize, cells, xcells, ycells);
	img.unlock();
}
void KImageUtils::halfScale(KImage &image) {
	int w = image.getWidth();
	int h = image.getHeight();
	KImage result = KImage::createFromSize(w/2, h/2);

	KBmp srcbmp; image.lock(&srcbmp);
	KBmp dstbmp; result.lock(&dstbmp);

	raw_half_scale(dstbmp, srcbmp);

	result.unlock();
	image.unlock();

	image = result.clone();
}
void KImageUtils::doubleScale(KImage &image) {
	int w = image.getWidth();
	int h = image.getHeight();
	KImage result = KImage::createFromSize(w*2, h*2);

	KBmp srcbmp; image.lock(&srcbmp);
	KBmp dstbmp; result.lock(&dstbmp);
	
	raw_double_scale(dstbmp, srcbmp);

	result.unlock();
	image.unlock();

	image = result.clone();
}
void KImageUtils::offset(KImage &image, int dx, int dy) {
	KImage tmp = image.clone();
	image.fill(KColor32());
	image.copyImage(dx, dy, tmp);
}
void KImageUtils::add(KImage &image, const KColor32 &color32) {
	KBmp bmp;
	image.lock(&bmp);
	raw_add(bmp, color32);
	image.unlock();
}
void KImageUtils::mul(KImage &image, const KColor32 &color32) {
	KBmp bmp;
	image.lock(&bmp);
	raw_mul(bmp, color32);
	image.unlock();
}
void KImageUtils::mul(KImage &image, float factor) {
	KBmp bmp;
	image.lock(&bmp);
	raw_mul(bmp, factor);
	image.unlock();
}
void KImageUtils::inv(KImage &image) {
	KBmp bmp;
	image.lock(&bmp);
	raw_inv(bmp);
	image.unlock();
}
void KImageUtils::grayToAlpha(KImage &image, const KColor32 &fill) {
	KBmp bmp;
	image.lock(&bmp);
	raw_gray_to_alpha(bmp, fill);
	image.unlock();
}
void KImageUtils::alphaToGray(KImage &image) {
	KBmp bmp;
	image.lock(&bmp);
	raw_alpha_to_gray(bmp);
	image.unlock();
}
void KImageUtils::blurX(KImage &dst, const KImage &src) {
	KBmp dstbmp; dst.lock(&dstbmp);
	KBmp srcbmp; src.lock(&srcbmp);

	KImageUtils::raw_blur_x(dstbmp, srcbmp);

	src.unlock();
	dst.unlock();
}
void KImageUtils::blurY(KImage &dst, const KImage &src) {
	KBmp dstbmp; dst.lock(&dstbmp);
	KBmp srcbmp; src.lock(&srcbmp);

	KImageUtils::raw_blur_y(dstbmp, srcbmp);

	src.unlock();
	dst.unlock();
}
void KImageUtils::blurX(KImage &image) {
	KImage tmp = image.clone();
	blurX(image, tmp);
}
void KImageUtils::blurY(KImage &image) {
	KImage tmp = image.clone();
	blurY(image, tmp);
}
void KImageUtils::outline(KImage &image, const KColor32 &color) {
	int w = image.getWidth();
	int h = image.getHeight();
	KImage result = KImage::createFromSize(w*2, h*2);

	KBmp srcbmp; image.lock(&srcbmp);
	KBmp dstbmp; result.lock(&dstbmp);
	
	raw_outline(dstbmp, srcbmp, color);

	result.unlock();
	image.unlock();

	image = result.clone();
}
void KImageUtils::silhouette(KImage &image, const KColor32 &color) {
	KBmp bmp;
	image.lock(&bmp);
	raw_silhouette(bmp, color);
	image.unlock();
}
void KImageUtils::expand(KImage &image, const KColor32 &color) {
	KImage tmp = image.clone();
	KBmp bmpDst, bmpSrc;
	tmp.lock(&bmpSrc);
	image.lock(&bmpDst);
	raw_expand(bmpDst, bmpSrc, color);
	image.unlock();
	tmp.unlock();
}

/// 指定範囲が画像範囲を超えないように調整する
bool KImageUtils::adjustRect(int img_w, int img_h, int *_x, int *_y, int *_w, int *_h) {
	K__ASSERT(img_w >= 0);
	K__ASSERT(img_h >= 0);
	K__ASSERT(_x && _y && _w && _h);
	bool changed = false;
	int x0 = *_x;
	int y0 = *_y;
	int x1 = x0 + *_w; // x1は x範囲+1 であることに注意！
	int y1 = y0 + *_h; // y1は y範囲+1 であることに注意！
	if (x0 < 0) { x0 = 0; changed = true; }
	if (y0 < 0) { y0 = 0; changed = true; }
	if (x1 > img_w) { x1 = img_w; changed = true; }
	if (y1 > img_h) { y1 = img_h; changed = true; }
	if (changed) {
		*_x = x0;
		*_y = y0;
		*_w = KMath::max(0, x1 - x0);
		*_h = KMath::max(0, y1 - y0);
		return true;
	}
	return false;
}
bool KImageUtils::adjustRect(const KImage &dst, const KImage &src, CopyRange *range) {
	return adjustRect(dst.getWidth(), dst.getHeight(), src.getWidth(), src.getHeight(), range);
}
bool KImageUtils::adjustRect(int dst_w, int dst_h, int src_w, int src_h, CopyRange *range) {
	K__ASSERT(dst_w >= 0);
	K__ASSERT(dst_h >= 0);
	K__ASSERT(src_w >= 0);
	K__ASSERT(src_h >= 0);
	K__ASSERT(range);
	bool changed = false;

	// 転送元が範囲内に収まるように調整
	if (range->dst_x < 0) {
		int adj = -range->dst_x;
		range->dst_x += adj;
		range->src_x += adj;
		range->cpy_w -= adj;
		changed = true;
	}
	if (range->dst_y < 0) {
		int adj = -range->dst_y;
		range->dst_y += adj;
		range->src_y += adj;
		range->cpy_h -= adj;
		changed = true;
	}
	if (dst_w < range->dst_x + range->cpy_w) {
		int adj = range->dst_x + range->cpy_w - dst_w;
		range->cpy_w -= adj;
		changed = true;
	}
	if (dst_h < range->dst_y + range->cpy_h) {
		int adj = range->dst_y + range->cpy_h - dst_h;
		range->cpy_h -= adj;
		changed = true;
	}

	// 転送先が範囲内に収まるように調整
	if (range->src_x < 0) {
		int adj = -range->src_x;
		range->dst_x += adj;
		range->src_x += adj;
		range->cpy_w -= adj;
		changed = true;
	}
	if (range->src_y < 0) {
		int adj = -range->src_y;
		range->dst_y += adj;
		range->src_y += adj;
		range->cpy_h -= adj;
		changed = true;
	}
	if (src_w < range->src_x + range->cpy_w) {
		int adj = range->src_x + range->cpy_w - src_w;
		range->cpy_w -= adj;
		changed = true;
	}
	if (src_h < range->src_y + range->cpy_h) {
		int adj = range->src_y + range->cpy_h - src_h;
		range->cpy_h -= adj;
		changed = true;
	}

	// 矩形サイズが負の値にならないよう調整
	if (range->cpy_w < 0) {
		range->cpy_w = 0;
		changed = true;
	}
	if (range->cpy_h < 0) {
		range->cpy_h = 0;
		changed = true;
	}
	return changed;
}
void KImageUtils::blendAlpha(KImage &image, const KImage &over) {
	blend(image, 0, 0, over, KBlendFuncAlpha());
}
void KImageUtils::blendAlpha(KImage &image, int x, int y, const KImage &over, const KColor32 &color) {
	blend(image, x, y, over, KBlendFuncAlpha(color));
}
void KImageUtils::blendAdd(KImage &image, const KImage&over) {
	blend(image, 0, 0, over, KBlendFuncAdd());
}
void KImageUtils::blendAdd(KImage &image, int x, int y, const KImage &over, const KColor32 &color) {
	blend(image, x, y, over, KBlendFuncAdd(color));
}
void KImageUtils::horzLine(KImage &image, int x0, int x1, int y, const KColor32 &color) {
	if (y < 0 || image.getHeight() <= y) return;
	x0 = KMath::max(x0, 0);
	x1 = KMath::min(x1, image.getWidth()-1);
	for (int i=x0; i<=x1; i++) {
		image.setPixel(i, y, color);
	}
}
void KImageUtils::vertLine(KImage &image, int x, int y0, int y1, const KColor32 &color) {
	if (x < 0 || image.getWidth() <= x) return;
	y0 = KMath::max(y0, 0);
	y1 = KMath::min(y1, image.getHeight()-1);
	for (int i=y0; i<=y1; i++) {
		image.setPixel(x, i, color);
	}
}
KImage KImageUtils::perlin(int size, int xwrap, int ywrap, float mul) {
	KImage img = KImage::createFromSize(size, size);
	KBmp bmp;
	img.lock(&bmp);
	raw_perlin(bmp, xwrap, ywrap, mul);
	img.unlock();
	return img;
}

void KImageUtils::modifyColor(KImage &image, const float *matrix4x3) {
	// RGBを変更する
	// rgba_matrix4x4 には RGBA の係数（重み）を記述した行列を指定する
	//        inR inG inB inConst
	// outR	= ### ### ### ###
	// outG	= ### ### ### ###
	// outB	= ### ### ### ###
	KBmp bmp;
	image.lock(&bmp);
	for (int y=0; y<bmp.h; y++) {
		for (int x=0; x<bmp.w; x++) {
			int i = bmp.get_offset(x, y);

			// 入力
			float inR = bmp.data[i + 0] / 255.0f;
			float inG = bmp.data[i + 1] / 255.0f;
			float inB = bmp.data[i + 2] / 255.0f;
			float inA = bmp.data[i + 3] / 255.0f;

			// 出力
			const float *m = matrix4x3;
			float outR = inR * m[0] + inG * m[1] + inB * m[2] + m[3];
			float outG = inR * m[4] + inG * m[5] + inB * m[6] + m[7];
			float outB = inR * m[8] + inG * m[9] + inB * m[10]+ m[11];
			float outA = inA;

			bmp.data[i + 0] = KMath::clampi((int)outR, 0, 255);
			bmp.data[i + 1] = KMath::clampi((int)outG, 0, 255);
			bmp.data[i + 2] = KMath::clampi((int)outB, 0, 255);
			bmp.data[i + 3] = KMath::clampi((int)outA, 0, 255);
		}
	}
	image.unlock();
}

#pragma endregion // KImageUtils


namespace Test {

void Test_image() {
	{
		const uint32_t data4x4[] = {
			0x00000000, 0x00000000, 0xFFFFFFFF, 0x00000000,
			0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000,
			0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
			0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0x00000000,
		};
		KImage img = KImage::createFromSize(4, 4);
		for (int i=0; i<16; i++) {
			img.setPixel(i%4, i/4, KColor32(data4x4[i]));
		}
		KImageUtils::halfScale(img); // 2x2ごとに混合（単純平均）しサイズを1/2に
		K__ASSERT(img.getWidth() == 2);
		KColor32 a = img.getPixel(0, 0);
		KColor32 b = img.getPixel(1, 0);
		KColor32 c = img.getPixel(0, 1);
		KColor32 d = img.getPixel(1, 1);
		K__ASSERT(a == KColor32(0x3F3F3F3F)); // 0xFFFFFFFF の 1/4
		K__ASSERT(b == KColor32(0x7F7F7F7F)); // 0xFFFFFFFF の 1/2
		K__ASSERT(c == KColor32(0xFFFFFFFF)); // 0xFFFFFFFF の 1/1
		K__ASSERT(d == KColor32(0x7F7F7F7F)); // 0xFFFFFFFF の 1/2
	}

	{
		KImage img = KImage::createFromSize(3, 1);
		img.setPixel(0, 0, KColor32(0x00000000));
		img.setPixel(1, 0, KColor32(0x33333333));
		img.setPixel(2, 0, KColor32(0xFFFFFFFF));
		KImageUtils::blurX(img); // 3x1の範囲を単純平均
		KColor32 a = img.getPixel(1, 0); // 中心ピクセルを取る
		K__ASSERT(a == KColor32(0x66666666)); // (0x00 + 0x33 + 0xFF)/3
	}

	{
		KImage img = KImage::createFromSize(1, 1, KColor32(30, 30, 30, 255));
		KImageUtils::mul(img, 2.0f); // RGBA各要素を 2.0 倍し、0..255の範囲に丸める
		KColor32 a = img.getPixel(0, 0);
		K__ASSERT(a == KColor32(60, 60, 60, 255));
	}

	{
		KImage img = KImage::createFromSize(1, 1, KColor32(30, 30, 30, 255));
		KImageUtils::mul(img, KColor32(127, 127, 255, 255/10)); // RGBA各要素を 0.5, 0.5, 1.0, 0.1 倍
		KColor32 a = img.getPixel(0, 0);
		K__ASSERT(a == KColor32(14, 14, 30, 25));
	}

	{
		KImage img = KImage::createFromSize(1, 1, KColor32(55, 55, 55, 230));
		KImageUtils::inv(img);// RGBを反転。Aは不変
		KColor32 a = img.getPixel(0, 0);
		K__ASSERT(a == KColor32(200, 200, 200, 230));
	}
}

void Test_imageutils() {
	Test_image();
}

} // Test

#pragma endregion // Image.cpp


} // namespace
