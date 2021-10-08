#pragma once
#include <inttypes.h>
#include <string>
#include <vector>
#include "KChunkedFile.h"
#include "KImage.h"

namespace Kamilo {

// ドット絵ツール Edge2 の標準ファイル形式 .edg ファイルを読み書きするためのクラス群
// @see http://takabosoft.com/edge2


/// パレットを画像として保存する時の画像サイズ。
/// ひとつのパレットは、16x16ドットの RGBA32 で画像化される
const int K_PALETTE_IMAGE_SIZE = 16;


class KEdgePalette;
class KEdgeMemory;
class KEdgeLayer;
class KEdgePage;
class KEdgeDocument;

typedef std::string KEdgeBin;

/// EDGE2 のパレット
class KEdgePalette {
public:
	KEdgePalette();
	KEdgePalette(const KEdgePalette *copy);
public:
	std::string m_name_u8;
	uint32_t m_data_b;
	uint8_t  m_data_c;
	uint8_t  m_data_d;
	uint8_t  m_data_e;
	uint32_t m_data_k;
	uint8_t  m_data_l;
	uint8_t  m_data_m;
	KEdgeBin m_bgr_colors;
	KEdgeBin m_data_f;
	KEdgeBin m_data_g;
	KEdgeBin m_data_h;
	KEdgeBin m_data_i;
	KEdgeBin m_data_j;
};

/// EDGE2 のレイヤー
class KEdgeLayer {
public:
	KEdgeLayer();
	KEdgeLayer(const KEdgeLayer *source, bool sharePixels);
	~KEdgeLayer();
public:
	uint32_t m_delay;
	uint8_t  m_is_grouped;
	uint8_t  m_is_visibled;
	uint8_t  m_label_color_rgbx[4];
	uint32_t m_data_a;
	uint8_t  m_data_b;
	uint8_t  m_data_c;
	uint8_t  m_data_d;
	uint8_t  m_data_f;
	uint8_t  m_data_g;
	uint8_t  m_data_h;
	KEdgeBin m_data_e;
	std::string m_label_u8;
	KImage m_image_rgb24;
};

/// EDGE2 のページ
class KEdgePage {
public:
#pragma pack(push, 1)
	struct AniParam {
		uint32_t dest;
		uint8_t is_relative;
	};
#pragma pack(pop)
	KEdgePage();
	KEdgePage(const KEdgePage *source, bool copy_layers);
	~KEdgePage();
	KEdgeLayer * addLayer();
	KEdgeLayer * addCopyLayer(const KEdgeLayer *source, bool share_pixels);
	KEdgeLayer * addLayerAt(int index);
	void delLayer(int index);
	KEdgeLayer * getLayer(int index);
	const KEdgeLayer * getLayer(int index) const;
	int getLayerCount() const;
public:
	uint32_t m_delay;
	uint8_t  m_is_grouped;
	uint8_t  m_label_color_rgbx[4];
	uint16_t m_width;
	uint16_t m_height;
	uint32_t m_data_a;
	uint8_t  m_data_b;
	uint8_t  m_data_c;
	uint32_t m_data_d;
	uint32_t m_data_e;
	uint8_t  m_data_f;
	uint32_t m_data_g;
	uint32_t m_data_h;
	uint8_t  m_data_i;
	uint8_t  m_data_j;
	uint8_t  m_data_k;
	std::string m_label_u8;
	std::vector<KEdgeLayer *> m_layers;
	AniParam m_ani_xparam;
	AniParam m_ani_yparam;
};

/// EDGE2 形式のファイル .edg に対応するオブジェクト
class KEdgeDocument {
public:
	struct Pal {
		uint8_t color_index;
		uint8_t bgr_colors[3];
		uint8_t data_a;
		uint8_t data_b[6];
		uint8_t data_c;
	};
	KEdgeDocument();
	KEdgeDocument(const KEdgeDocument *source, bool copy_pages);
	~KEdgeDocument();
	void clear();

	bool loadFromStream(KInputStream &file);
	bool loadFromFileName(const std::string &filename);
	void saveToStream(KOutputStream &file);
	void saveToFileName(const std::string &filename);

	void autoComplete();
	KEdgePage * addPage();
	KEdgePage * addCopyPage(const KEdgePage *source);
	KEdgePage * addPageAt(int index);
	void delPage(int index);
	int getPageCount() const;
	KEdgePage * getPage(int index);
	const KEdgePage * getPage(int index) const;
	uint8_t * getPalettePointer(int index);
	int getPaletteCount() const;
	KEdgePalette * getPalette(int index);
	const KEdgePalette * getPalette(int index) const;
	KImage exportSurfaceRaw(int pageindex, int layerindex, int transparent_index) const;
	KImage exportSurface(int pageindex, int layerindex, int paletteindex) const;
	KImage exportSurfaceWithAlpha(int pageindex, int layerindex, int maskindex, int paletteindex) const;
	KImage exportPaletteImage() const;
	std::string expotyXml() const; // XML(utf8)
	int findColorIndex(int paletteindex, uint8_t r, uint8_t g, uint8_t b, int defaultindex) const;
	bool writeImageToLayer(int pageindex, int layerindex, int paletteindex, const KImage *surface);
public:
	uint16_t m_compression_flag;
	uint8_t  m_bit_depth;
	Pal      m_fg_palette;
	Pal      m_bg_palette;
	uint8_t  m_data_a;
	uint8_t  m_data_b;
	uint8_t  m_data_c;
	uint8_t  m_data_d;
	uint8_t  m_data_d2;
	KEdgeBin m_data_d3;
	KEdgeBin m_data_d4;
	KEdgeBin m_data_e;
	uint32_t m_data_f;
	uint8_t  m_data_g;
	uint32_t m_data_h;
	uint16_t m_data_i;
	uint32_t m_data_j;
	uint32_t m_data_k;
	std::vector<KEdgePalette *> m_palettes;
	std::vector<KEdgePage *> m_pages;
};

/// EDGE2 でエクスポートできるパレットファイル .PAL に対応
class KEdgePalFile {
public:
	KEdgePalFile();

	/// パレットをRGBA32ビット画像としてエクスポートする
	/// img: 書き出し用画像。16x16の大きさを持っていないといけない
	/// index0_for_transparent: インデックス[0]を完全透過として扱う
	KImage exportImage(bool index0_for_transparent) const;

	/// BGR 配列 256 色ぶん
	uint8_t m_bgr[256*3];

	/// 不明な 256 バイトのデータ。透過色？
	uint8_t m_data[256];

	/// パレットのラベル名
	std::string m_name_u8;
};

class KEdgePalReader: public KChunkedFileReader {
public:
	bool loadFromStream(KInputStream &file);
	bool loadFromFileName(const std::string &filename);
	bool loadFromMemory(const void *data, size_t size);

	std::string exportXml(); // UTF8
	std::vector<KEdgePalFile> m_items;
};

class KEdgeRawReader: public KChunkedFileReader {
public:
	static KEdgeBin unzip(KInputStream &file, uint16_t *zflag=NULL);

	KEdgeRawReader();

	// Edge2 バイナリファイルからデータを読み取る
	int read(KEdgeDocument *e, KInputStream &file);

private:
	void scanLayer(KEdgeDocument *e, KEdgePage *page, KEdgeLayer *layer);
	void scanPage(KEdgeDocument *e, KEdgePage *page);
	void scanEdge(KEdgeDocument *e);
	void readFromUncompressedBuffer(KEdgeDocument *e, const void *buf, size_t len);
};

class KEdgeRawWriter: public KChunkedFileWriter {
public:
	KEdgeRawWriter();
	void write(KEdgeDocument *e, KOutputStream &file);
private:
	void writeLayer(KEdgePage *page, int layerindex, int bitdepth);
	void writePage(KEdgeDocument *e, int pageindex);
	void writeEdge(KEdgeDocument *e);
	void writeCompressedEdge(KOutputStream &file) const;
	void writeUncompressedEdge(KEdgeDocument *e);
};

class KEdgeWriter {
	KEdgeDocument *m_doc;
public:
	KEdgeWriter();
	~KEdgeWriter();
	void clear();
	void addPage(const KImage *img, int delay);
	void saveToStream(KOutputStream &file);
};

} // namespace
