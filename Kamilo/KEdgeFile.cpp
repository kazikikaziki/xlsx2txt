#include "KEdgeFile.h"

#include "KChunkedFile.h"
#include "KZlib.h"
#include "KInternal.h"

namespace Kamilo {


#define EDGE2_STRING_ENCODING "JPN"  // Edge2 は日本語環境で使う事を前提とする


static int _Min(int a, int b) {
	return (a < b) ? a : b;
}


static KImage _MakeImage(int width, int height, int bitdepth) {
	if (bitdepth == 32) {
		return KImage::createFromPixels(width, height, KColorFormat_RGBA32, NULL);
	}
	if (bitdepth == 24) {
		return KImage::createFromPixels(width, height, KColorFormat_RGB24, NULL);
	}
	if (bitdepth == 8) {
		return KImage::createFromPixels(width, height, KColorFormat_INDEX8, NULL);
	}
	K__ASSERT(0);
	return KImage();
}



#pragma region KEdgePalette
KEdgePalette::KEdgePalette() {
	m_data_b = 0;
	m_data_c = 0;
	m_data_d = 0;
	m_data_e = 0;
	m_bgr_colors.resize(256 * 3);
	m_data_f.resize(256);
	m_data_g.resize(256);
	m_data_h.resize(256*6);
	m_data_i.resize(256);
	m_data_j.resize(0);
	m_data_k = 0;
	m_data_l = 0;
	m_data_m = 0;
}
KEdgePalette::KEdgePalette(const KEdgePalette *copy) {
	*this = *copy;
}
#pragma endregion // KEdgePalette


#pragma region KEdgeLayer
KEdgeLayer::KEdgeLayer() {
	m_delay = 10;
	m_label_u8.clear();
	m_label_color_rgbx[0] = 255;
	m_label_color_rgbx[1] = 255;
	m_label_color_rgbx[2] = 255;
	m_label_color_rgbx[3] = 0;
	m_is_grouped = false;
	m_is_visibled = true;
	m_data_a = 4;
	m_data_b = 1;
	m_data_c = 0;
	m_data_d = 0;
	m_data_e.clear();
	m_data_f = 1;
	m_data_g = 2;
	m_data_h = 1;
}

/// レイヤーをコピーする
/// @param source       コピー元レイヤー
/// @param sharePixels  ピクセルバッファの参照を共有する
KEdgeLayer::KEdgeLayer(const KEdgeLayer *source, bool sharePixels) {
	m_delay = source->m_delay;
	m_label_u8 = source->m_label_u8;
	memcpy(m_label_color_rgbx, source->m_label_color_rgbx, 4);
	m_is_grouped = source->m_is_grouped;
	m_is_visibled = source->m_is_visibled;
	m_data_a = source->m_data_a;
	m_data_b = source->m_data_b;
	m_data_c = source->m_data_c;
	m_data_d = source->m_data_d;
	m_data_e.assign(source->m_data_e.begin(), source->m_data_e.end());
	m_data_f = source->m_data_f;
	m_data_g = source->m_data_g;
	m_data_h = source->m_data_h;
	if (sharePixels) {
		// 参照コピー
		m_image_rgb24 = source->m_image_rgb24;
	} else {
		// 実体コピー
		m_image_rgb24 = source->m_image_rgb24.clone();
	}
}
KEdgeLayer::~KEdgeLayer() {
}
#pragma endregion // KEdgeLayer


#pragma region KEdgePage
KEdgePage::KEdgePage() {
	m_width = 256;
	m_height = 256;
	m_delay = 10;
	m_is_grouped = false;
	m_label_u8.clear();
	m_layers.clear();
	m_label_color_rgbx[0] = 255;
	m_label_color_rgbx[1] = 255;
	m_label_color_rgbx[2] = 255;
	m_label_color_rgbx[3] = 0;
	m_data_a = 0;
	m_data_b = 1;
	m_data_c = 0;
	m_data_d = (uint32_t)(-1);
	m_data_e = (uint32_t)(-1);
	m_data_f = 0;
	m_data_g = 0;
	m_data_h = 0;
	m_data_i = 1;
	m_data_j = 2;
	m_data_k = 1;
	m_ani_xparam.dest = 0;
	m_ani_xparam.is_relative = 0;
	m_ani_yparam.dest = 0;
	m_ani_yparam.is_relative = 0;
}
KEdgePage::KEdgePage(const KEdgePage *source, bool copy_layers) {
	m_delay = source->m_delay;
	m_label_u8 = source->m_label_u8;
	m_is_grouped = source->m_is_grouped;
	memcpy(m_label_color_rgbx, source->m_label_color_rgbx, 4);
	m_width = source->m_width;
	m_height = source->m_height;
	m_data_a = source->m_data_a;
	m_data_b = source->m_data_b;
	m_data_c = source->m_data_c;
	m_data_d = source->m_data_d;
	m_data_e = source->m_data_e;
	m_data_f = source->m_data_f;
	m_data_g = source->m_data_g;
	m_data_h = source->m_data_h;
	m_data_i = source->m_data_i;
	m_data_j = source->m_data_j;
	m_data_k = source->m_data_k;
	m_ani_xparam = source->m_ani_xparam;
	m_ani_yparam = source->m_ani_yparam;
	if (copy_layers) {
		int numlayers = source->getLayerCount();
		for (int i=0; i<numlayers; i++) {
			const KEdgeLayer *src_layer = source->getLayer(i);
			addCopyLayer(src_layer, true);
		}
	}
}
KEdgePage::~KEdgePage() {
	for (auto it=m_layers.begin(); it!=m_layers.end(); it++) {
		KEdgeLayer *layer = *it;
		delete layer;
	}
}
KEdgeLayer * KEdgePage::addLayer() {
	KEdgeLayer *layer = new KEdgeLayer();
	m_layers.push_back(layer);
	return layer;
}
KEdgeLayer * KEdgePage::addCopyLayer(const KEdgeLayer *source, bool share_pixels) {
	KEdgeLayer *layer = new KEdgeLayer(source, share_pixels);
	m_layers.push_back(layer);
	return layer;
}
KEdgeLayer * KEdgePage::addLayerAt(int index) {
	KEdgeLayer *layer = new KEdgeLayer();
	if (index < 0) {
		m_layers.insert(m_layers.begin(), layer);
	} else if (index < (int)m_layers.size()) {
		m_layers.insert(m_layers.begin() + index, layer);
	} else {
		m_layers.push_back(layer);
	}
	return layer;
}
void KEdgePage::delLayer(int index) {
	if (index < 0) return;
	if (index >= (int)m_layers.size()) return;
	delete m_layers[index];
	m_layers.erase(m_layers.begin() + index);
}
KEdgeLayer * KEdgePage::getLayer(int index) {
	if (index < 0) return NULL;
	if (index >= (int)m_layers.size()) return NULL;
	return m_layers[index];
}
const KEdgeLayer * KEdgePage::getLayer(int index) const {
	if (index < 0) return NULL;
	if (index >= (int)m_layers.size()) return NULL;
	return m_layers[index];
}
int KEdgePage::getLayerCount() const {
	return (int)m_layers.size();
}
#pragma endregion // KEdgePage


#pragma region KEdgeDocument
KEdgeDocument::KEdgeDocument() {
	clear();
}
KEdgeDocument::KEdgeDocument(const KEdgeDocument *source, bool copy_pages) {
	bool copy_palettes = true;
	K__ASSERT(source);
	m_compression_flag = source->m_compression_flag;
	m_bit_depth = source->m_bit_depth;
	{
		m_fg_palette.color_index = source->m_fg_palette.color_index;
		m_fg_palette.data_a = source->m_fg_palette.data_a;
		m_fg_palette.data_c = source->m_fg_palette.data_c;
		memcpy(m_fg_palette.bgr_colors, source->m_fg_palette.bgr_colors, 3);
		memcpy(m_fg_palette.data_b, source->m_fg_palette.data_b, 6);

		m_bg_palette.color_index = source->m_bg_palette.color_index;
		m_bg_palette.data_a = source->m_bg_palette.data_a;
		m_bg_palette.data_c = source->m_bg_palette.data_c;
		memcpy(m_bg_palette.bgr_colors, source->m_bg_palette.bgr_colors, 3);
		memcpy(m_bg_palette.data_b, source->m_bg_palette.data_b, 6);
	}
	m_data_d = source->m_data_d;
	m_data_d2 = source->m_data_d2;
	m_data_d3.assign(source->m_data_d3.begin(), source->m_data_d3.end());
	m_data_e.assign(source->m_data_e.begin(), source->m_data_e.end());
	m_data_a = source->m_data_a;
	m_data_b = source->m_data_b;
	m_data_c = source->m_data_c;
	m_data_f = source->m_data_f;
	m_data_g = source->m_data_g;
	m_data_h = source->m_data_h;
	m_data_i = source->m_data_i;
	m_data_j = source->m_data_j;
	m_data_k = source->m_data_k;

	if (copy_pages) {
		int numpages = source->getPageCount();
		for (int i=0; i<numpages; i++) {
			const KEdgePage *src = source->getPage(i);
			addCopyPage(src);
		}
	}
	if (copy_palettes) {
		K__ASSERT(m_palettes.empty());
		for (size_t i=0; i<source->m_palettes.size(); i++) {
			KEdgePalette *p = source->m_palettes[i];
			m_palettes.push_back(new KEdgePalette(p));
		}
	}
}
KEdgeDocument::~KEdgeDocument() {
	clear();
}
void KEdgeDocument::clear() {
	for (auto it=m_palettes.begin(); it!=m_palettes.end(); ++it) {
		delete (*it);
	}
	for (auto it=m_pages.begin(); it!=m_pages.end(); ++it) {
		delete (*it);
	}
	m_pages.clear();
	m_palettes.clear();
	m_compression_flag = 256;
	m_bit_depth = 24;
	m_fg_palette.color_index = 0;
	m_bg_palette.color_index = 0;
	memset(&m_fg_palette, 0, sizeof(m_fg_palette));
	memset(&m_bg_palette, 0, sizeof(m_bg_palette));
	m_fg_palette.data_a = 255;
	m_fg_palette.data_c = 0;
	m_bg_palette.data_a = 255;
	m_bg_palette.data_c = 0;
	m_data_a = 0;
	m_data_b = 0;
	m_data_c = 0;
	m_data_d = 1;
	m_data_d2 = 1;
	m_data_d3.clear();
	m_data_e.clear();
	m_data_f = 0;
	m_data_g = 0;
	m_data_h = 0;
	m_data_i = 256;
	m_data_j = 1;
	m_data_k = 0;
}
bool KEdgeDocument::loadFromStream(KInputStream &file) {
	if (!file.isOpen()) return false;
	KEdgeRawReader reader;
	if (reader.read(this, file) != 0) {
		clear();
		return false;
	}
	return true;
}
bool KEdgeDocument::loadFromFileName(const std::string &filename) {
	bool success = false;
	KInputStream file = KInputStream::fromFileName(filename);
	if (file.isOpen()) {
		success = loadFromStream(file);
	}
	return success;
}
void KEdgeDocument::saveToStream(KOutputStream &file) {
	KEdgeRawWriter writer;
	writer.write(this, file);
}
void KEdgeDocument::saveToFileName(const std::string &filename) {
	KOutputStream file = KOutputStream::fromFileName(filename);
	if (file.isOpen()) {
		saveToStream(file);
	}
}

// Make document for Edge2 Application ( http://takabosoft.com/edge2 )
// All pages need to have least 1 layer.
void KEdgeDocument::autoComplete() {
	if (m_bit_depth == 0) {
		m_bit_depth = 0;
	}

	// Edge needs least one palette
	if (m_palettes.empty()) {
		m_palettes.push_back(new KEdgePalette());
	}

	// Edge needs least one page
	if (m_pages.empty()) {
		m_pages.push_back(new KEdgePage());
	}

	for (size_t p=0; p<m_pages.size(); p++) {
		KEdgePage *page = m_pages[p];

		if (page->m_width==0 || page->m_height==0) {
			int DEFAULT_SIZE = 8;
			page->m_width  = (uint16_t)DEFAULT_SIZE;
			page->m_height = (uint16_t)DEFAULT_SIZE;
		}

		// Page needs least one layer
		if (page->m_layers.empty()) {
			KEdgeLayer *layer = new KEdgeLayer();
			page->m_layers.push_back(layer);
		}

		// Make empty image if not exists
		for (size_t l=0; l<page->m_layers.size(); l++) {
			KEdgeLayer *layer = page->m_layers[l];
			if (layer->m_image_rgb24.empty()) {
				layer->m_image_rgb24 = _MakeImage(page->m_width, page->m_height, m_bit_depth);
			}
		}
	}
}

KEdgePage * KEdgeDocument::addPage() {
	KEdgePage *page = new KEdgePage();
	m_pages.push_back(page);
	return page;
}
KEdgePage * KEdgeDocument::addCopyPage(const KEdgePage *source) {
	KEdgePage *page = new KEdgePage(source, true);
	m_pages.push_back(page);
	return page;
}
KEdgePage * KEdgeDocument::addPageAt(int index) {
	KEdgePage *page = new KEdgePage();
	if (index < 0) {
		m_pages.insert(m_pages.begin(), page);
	} else if (index < (int)m_pages.size()) {
		m_pages.insert(m_pages.begin() + index, page);
	} else {
		m_pages.push_back(page);
	}
	return page;
}
void KEdgeDocument::delPage(int index) {
	if (index < 0) return;
	if (index >= (int)m_pages.size()) return;
	delete m_pages[index];
	m_pages.erase(m_pages.begin() + index);
}
int KEdgeDocument::getPageCount() const {
	return (int)m_pages.size();
}
KEdgePage * KEdgeDocument::getPage(int index) {
	if (index < 0) return NULL;
	if (index >= (int)m_pages.size()) return NULL;
	return m_pages[index];
}
const KEdgePage * KEdgeDocument::getPage(int index) const {
	if (index < 0) return NULL;
	if (index >= (int)m_pages.size()) return NULL;
	return m_pages[index];
}
uint8_t * KEdgeDocument::getPalettePointer(int index) {
	if (0 <= index && index < (int)m_palettes.size()) {
		return (uint8_t *)&m_palettes[index]->m_bgr_colors[0];
	}
	return NULL;
}
int KEdgeDocument::getPaletteCount() const {
	return (int)m_palettes.size();
}
KEdgePalette * KEdgeDocument::getPalette(int index) {
	return m_palettes[index];
}
const KEdgePalette * KEdgeDocument::getPalette(int index) const {
	return m_palettes[index];
}
std::string KEdgeDocument::expotyXml() const {
	std::string xml_u8;
	char tmp[1024] = {0};

	int numPages = getPageCount();
	sprintf_s(tmp, sizeof(tmp), "<Edge pages='%d'>\n", numPages);
	xml_u8 += tmp;
	for (int p=0; p<numPages; p++) {
		const KEdgePage *page = getPage(p);
		int numLayers = page->getLayerCount();
		sprintf_s(tmp, sizeof(tmp), "  <Page label='%s' labelColor='#%02x%02x%02x' delay='%d' w='%d' h='%d' layers='%d'>\n", 
			page->m_label_u8.c_str(), 
			page->m_label_color_rgbx[0], page->m_label_color_rgbx[1], page->m_label_color_rgbx[2],
			page->m_delay, page->m_width, page->m_height, numLayers);
		xml_u8 += tmp;
		for (int l=0; l<numLayers; l++) {
			const KEdgeLayer *layer = page->getLayer(l);
			sprintf_s(tmp, sizeof(tmp), "    <Layer label='%s' labelColor='#%02x%02x%02x' delay='%d'/>\n",
				layer->m_label_u8.c_str(), 
				layer->m_label_color_rgbx[0], layer->m_label_color_rgbx[1], layer->m_label_color_rgbx[2],
				layer->m_delay);
			xml_u8 += tmp;
		}
		xml_u8 += "  </Page>\n";
	}

	int numPal = getPaletteCount();
	sprintf_s(tmp, sizeof(tmp), "  <Palettes count='%d'>\n", numPal);
	xml_u8 += tmp;
	for (int i=0; i<numPal; i++) {
		const KEdgePalette *pal = getPalette(i);
		sprintf_s(tmp, sizeof(tmp), "    <Pal label='%s'/>\n", pal->m_name_u8.c_str());
		xml_u8 += tmp;
	}
	xml_u8 += "  </Palettes>\n";

	xml_u8 += "</Edge>\n";
	return xml_u8;
}

/// パレットを画像化したものを作成する
/// 横幅は 16 ドット、高さは 16 * パレット数になる
/// EDGE 背景色に該当するピクセルのみアルファ値が 0 になるが、それ以外のピクセルのアルファは必ず 255 になる
KImage KEdgeDocument::exportPaletteImage() const {
	if (m_palettes.empty()) return KImage();
	KImage pal = KImage::createFromSize(K_PALETTE_IMAGE_SIZE, K_PALETTE_IMAGE_SIZE * m_palettes.size());
	uint8_t *dest = pal.lockData();
	for (size_t p=0; p<m_palettes.size(); p++) {
		const auto &bgr = m_palettes[p]->m_bgr_colors;
		for (int i=0; i<256; i++) {
			uint8_t b = bgr[i*3+0];
			uint8_t g = bgr[i*3+1];
			uint8_t r = bgr[i*3+2];
			uint8_t a = (m_bg_palette.color_index == i) ? 0 : 255;
			dest[(256 * p + i) * 4 + 0] = r;
			dest[(256 * p + i) * 4 + 1] = g;
			dest[(256 * p + i) * 4 + 2] = b;
			dest[(256 * p + i) * 4 + 3] = a;
		}
	}
	pal.unlockData();
	return pal;
}

/// Edge レイヤーの画像を、パレット番号が取得可能な形式で書き出す。
/// RGBそれぞれにパレット番号を、A には常に255をセットするため、パレット変換しないで表示した場合は奇妙なグレースケール画像に見える
/// 書き出し時のフォーマットは RGBA32 ビットになる
/// 元画像がパレットでない場合は NULL を返す
KImage KEdgeDocument::exportSurfaceRaw(int pageindex, int layerindex, int transparent_index) const {
	KImage output;
	const KEdgePage *page = getPage(pageindex);
	if (page) {
		const KEdgeLayer *layer = page->getLayer(layerindex);
		if (layer && m_bit_depth == 8) {
			int w = page->m_width;
			int h = page->m_height;
			const uint8_t *p_src = layer->m_image_rgb24.getData();
			output = KImage::createFromSize(w, h);
			uint8_t *p_dst = output.lockData();
			for (int y=0; y<h; y++) {
				for (int x=0; x<w; x++) {
					uint8_t i = p_src[y * w + x];
					p_dst[(y * w + x) * 4 + 0] = i; // パレットインデックスをそのままRGB値として書き込む
					p_dst[(y * w + x) * 4 + 1] = i;
					p_dst[(y * w + x) * 4 + 2] = i;
					p_dst[(y * w + x) * 4 + 3] = (transparent_index == i) ? 0 : 255;
				}
			}
			output.unlockData();
		}
	}
	return output;
}
static KImage kk_ImageFromRGB24(int w, int h, const uint8_t *pixels, const uint8_t *transparent_color_BGR) {
	if (w <= 0 || h <= 0 || pixels == NULL) { K__ERROR("invalid argument"); return KImage(); }
	KEdgeBin buf(w * h * 4, '\0');
	uint8_t *buf_p = (uint8_t*)&buf[0];
	const uint8_t *t = transparent_color_BGR;
	const int srcPitch = w * 3;
	const int dstPitch = w * 4;
	for (int y=0; y<h; y++) {
		for (int x=0; x<w; x++) {
			const uint8_t r = pixels[srcPitch*y + x*3 + 2]; // r
			const uint8_t g = pixels[srcPitch*y + x*3 + 1]; // g
			const uint8_t b = pixels[srcPitch*y + x*3 + 0]; // b
			const uint8_t a = (t && b==t[0] && g==t[1] && r==t[2]) ? 0 : 255;
			buf_p[dstPitch*y + x*4 + 0] = r;
			buf_p[dstPitch*y + x*4 + 1] = g;
			buf_p[dstPitch*y + x*4 + 2] = b;
			buf_p[dstPitch*y + x*4 + 3] = a;
		}
	}
	return KImage::createFromPixels(w, h, KColorFormat_RGBA32, buf_p);
}

static KImage kk_ImageFromIndexed(int w, int h, const uint8_t *indexed_format_pixels, const uint8_t *palette_colors_BGR, int transparent_color_index) {
	if (w <= 0 || h <= 0) { K__ERROR("invalid argument"); return KImage(); }
	K__ASSERT(indexed_format_pixels);
	K__ASSERT(palette_colors_BGR);
	const int dstPitch = w * 4;
	KEdgeBin buf(w * h * 4, '\0');
	uint8_t *buf_p = (uint8_t*)&buf[0];
	for (int y=0; y<h; y++) {
		for (int x=0; x<w; x++) {
			const uint8_t idx = indexed_format_pixels[w * y + x];
			if (idx == transparent_color_index) {
				buf_p[dstPitch*y + x*4 + 0] = 0; // r
				buf_p[dstPitch*y + x*4 + 1] = 0; // g
				buf_p[dstPitch*y + x*4 + 2] = 0; // b
				buf_p[dstPitch*y + x*4 + 3] = 0; // a
			} else {
				buf_p[dstPitch*y + x*4 + 0] = palette_colors_BGR[idx * 3 + 2]; // r
				buf_p[dstPitch*y + x*4 + 1] = palette_colors_BGR[idx * 3 + 1]; // g
				buf_p[dstPitch*y + x*4 + 2] = palette_colors_BGR[idx * 3 + 0]; // b
				buf_p[dstPitch*y + x*4 + 3] = 255; // a
			}
		}
	}
	return KImage::createFromPixels(w, h, KColorFormat_RGBA32, buf_p);
}

/// Edge レイヤーの画像を KImage に書き出す
/// 書き出し時のフォーマットは常に RGBA32 ビットになる（Edgeの背景色を透過色にする）
/// 失敗した場合は NULL を返す
KImage KEdgeDocument::exportSurface(int pageindex, int layerindex, int paletteindex) const {
	KImage output;
	const KEdgePage *page = getPage(pageindex);
	if (page) {
		const KEdgeLayer *layer = page->getLayer(layerindex);
		if (layer) {
			// Make surface
			const uint8_t *p_src = layer->m_image_rgb24.getData();
			int w = page->m_width;
			int h = page->m_height;
			if (m_bit_depth == 24) {
				output = kk_ImageFromRGB24(w, h, p_src, m_bg_palette.bgr_colors);
			} else if (m_bit_depth == 8) {
				const uint8_t *colors = (uint8_t *)&m_palettes[paletteindex]->m_bgr_colors[0];
				output = kk_ImageFromIndexed(w, h, p_src, colors, m_bg_palette.color_index);
			}
		}
	}
	return output;
}

/// 4枚の元画像とチャンネルを指定し、抽出した４つのチャンネル画像を合成して一枚の画像にする。
/// @arg src_surface_r 画像の src_channel_r チャンネル画像を、Rチャンネルとして使う。
/// @arg src_surface_g 画像の src_channel_g チャンネル画像を、Gチャンネルとして使う。
/// @arg src_surface_b 画像の src_channel_b チャンネル画像を、Bチャンネルとして使う。
/// @arg src_surface_a 画像の src_channel_a チャンネル画像を、Aチャンネルとして使う。
///
/// @param src_r  Rチャンネルとして使う元画像
/// @param src_g  Gチャンネルとして使う元画像
/// @param src_b  Bチャンネルとして使う元画像
/// @param src_a  Aチャンネルとして使う元画像
/// @param off_r  src_r から色を取り出す時の各ピクセル先頭からのオフセットバイト数。1ピクセルが4バイトで構成されているとき、0 から 3 までの値を指定できる
/// @param off_g  （略）
/// @param off_b  （略）
/// @param off_a  （略）
static KImage kk_ImageFromChannels(const KImage *src_r, const KImage *src_g, const KImage *src_b, const KImage *src_a, int off_r, int off_g, int off_b, int off_a) {
	K__ASSERT(src_r);
	K__ASSERT(src_g);
	K__ASSERT(src_b);
	K__ASSERT(src_a);
	if(off_r < 0 || 4 <= off_r) { K__ERROR("Invalid offset bytes for R channel"); return KImage(); }
	if(off_g < 0 || 4 <= off_g) { K__ERROR("Invalid offset bytes for G channel"); return KImage(); }
	if(off_b < 0 || 4 <= off_b) { K__ERROR("Invalid offset bytes for B channel"); return KImage(); }
	if(off_a < 0 || 4 <= off_a) { K__ERROR("Invalid offset bytes for A channel"); return KImage(); }
	// すべてのサーフェスでサイズが一致することを確認
	const int w = src_r->getWidth();
	const int h = src_r->getHeight();
	if (w <= 0 || h <= 0) { K__ERROR("Invalid image size"); return KImage(); }
	const bool size_r_ok = (src_r->getWidth()==w && src_r->getHeight()==h);
	const bool size_g_ok = (src_g->getWidth()==w && src_g->getHeight()==h);
	const bool size_b_ok = (src_b->getWidth()==w && src_b->getHeight()==h);
	const bool size_a_ok = (src_a->getWidth()==w && src_a->getHeight()==h);
	if (!size_r_ok || !size_g_ok || !size_b_ok || !size_a_ok) { K__ERROR("Source images have different sizes"); return KImage(); }
	// 準備
	KImage out = KImage::createFromPixels(w, h, KColorFormat_RGBA32, NULL);
	// RGBAそれぞれのチャンネルを合成する
	const uint8_t *src_p_r = src_r->getData();
	const uint8_t *src_p_g = src_g->getData();
	const uint8_t *src_p_b = src_b->getData();
	const uint8_t *src_p_a = src_a->getData();
	uint8_t *dstP = out.lockData();
	K__ASSERT(dstP);
	for (int y=0; y<h; y++) {
		for (int x=0; x<w; x++) {
			const int i = (w * y + x) * 4;
			dstP[i + 0] = src_p_r[i + off_r];
			dstP[i + 1] = src_p_g[i + off_g];
			dstP[i + 2] = src_p_b[i + off_b];
			dstP[i + 3] = src_p_a[i + off_a];
		}
	}
	out.unlockData();
	return out;
}





/// Edge レイヤーの画像をアルファチャンネルつきで KImage に書き出す
/// 書き出し時のフォーマットは常に RGBA32 ビットで、アルファ値の入力にはアルファマスクレイヤーを指定する
/// マスクレイヤーはアルファ値をグレースケールで記録したもので、変換元のレイヤーと同じページ内にある
/// マスクレイヤーのインデックスに -1 が指定されている場合は、Edgeの背景色を透過させる
/// 失敗した場合は NULL を返す
KImage KEdgeDocument::exportSurfaceWithAlpha(int pageindex, int layerindex, int maskindex, int paletteindex) const {
	if (maskindex < 0) {
		// アルファマスクが省略されている
		return exportSurface(pageindex, layerindex, paletteindex);

	} else {
		KImage rgb = exportSurface(pageindex, layerindex, paletteindex);
		KImage mask = exportSurface(pageindex, maskindex, 0);
		KImage output = kk_ImageFromChannels(&rgb, &rgb, &rgb, &mask, 0, 1, 2, 0);
		return output;
	}
}

/// 指定された色に最も近いカラーインデックスを返す
/// 検索できない場合は defaultindex を返す
int KEdgeDocument::findColorIndex(int paletteindex, uint8_t r, uint8_t g, uint8_t b, int defaultindex) const {
	if (paletteindex < 0 || (int)m_palettes.size() <= paletteindex) return defaultindex;

	int result = defaultindex;
	int min_diff = 255;
	const KEdgePalette *pal = m_palettes[paletteindex];
	for (int i=0; i<256; i++) {
		int pal_b = (int)pal->m_bgr_colors[i*3 + 0];
		int pal_g = (int)pal->m_bgr_colors[i*3 + 1];
		int pal_r = (int)pal->m_bgr_colors[i*3 + 2];

		int diff = abs(pal_r - r) + abs(pal_g - g) + abs(pal_b - b);
		if (diff < min_diff) {
			min_diff = diff;
			result = i;
		}
	}
	return result;
}

/// KImage で指定された画像を Edge レイヤーに書き出す
/// 書き込み先の Edge レイヤーにパレットが設定されている場合は、適当に減色してパレットに対応させる
/// 書き込みに成功すれば true を返す
bool KEdgeDocument::writeImageToLayer(int pageindex, int layerindex, int paletteindex, const KImage *image) {
	if (image == NULL) {
		K__ERROR("E_INVALID_ARGUMENT");
		return false;
	}
	if (image->getBytesPerPixel() != 3) {
		K__ERROR("E_EDGE_INVALID_BYTES_PER_PIXEL: %d", image->getBytesPerPixel());
		return false;
	}

	KEdgePage *page = getPage(pageindex);
	if (page == NULL) {
		K__ERROR("E_EDGE_INVALID_PAGE_INDEX: PAGE=%d/%d", pageindex, getPageCount());
		return false;
	}

	KEdgeLayer *layer = page->getLayer(layerindex);
	if (layer == NULL) {
		K__ERROR("E_EDGE_INVALID_LAYER_INDEX: PAGE=%d, LAYER=%d/%d", pageindex, layerindex, page->getLayerCount());
		return false;
	}

	// コピー範囲を決定
	const int w = _Min((int)page->m_width, image->getWidth());
	const int h = _Min((int)page->m_height, image->getHeight());

	if (m_bit_depth == 24) {
		// RGB24ビットカラー

		// コピー先のバッファ
		uint8_t *dst_p = layer->m_image_rgb24.lockData();
		// RGB値を直接コピーする。アルファチャンネルは無視する
		const uint8_t *src_p = image->getData();
		int src_data_size = image->getDataSize();
		const int dst_pitch = page->m_width * 3;
		const int src_pitch = image->getPitch();
		memset(dst_p, 0, src_data_size); // アラインメントによる余白も含めてクリア
		for (int y=0; y<h; y++) {
			for (int x=0; x<w; x++) {
				const uint8_t r = src_p[src_pitch*y + x*3 + 0];
				const uint8_t g = src_p[src_pitch*y + x*3 + 1];
				const uint8_t b = src_p[src_pitch*y + x*3 + 2];
				// EdgeにはBGRの順番で保存する
				dst_p[dst_pitch*y + x*3 + 0] = b;
				dst_p[dst_pitch*y + x*3 + 1] = g;
				dst_p[dst_pitch*y + x*3 + 2] = r;
			}
		}
		layer->m_image_rgb24.unlockData();
		return true;
	}

	if (m_bit_depth == 8) {
		// インデックスカラー

		// コピー先のバッファ
		uint8_t *dst_p = layer->m_image_rgb24.lockData();
		// パレットを考慮してコピーする
		const uint8_t *src_p = image->getData();
		int src_data_size = image->getDataSize();
		const int dst_pitch = page->m_width;
		const int src_pitch = image->getPitch();
		memset(dst_p, 0, src_data_size); // アラインメントによる余白も含めてクリア
		for (int y=0; y<h; y++) {
			for (int x=0; x<w; x++) {
				const uint8_t r = src_p[src_pitch*y + 3*x + 0];
				const uint8_t g = src_p[src_pitch*y + 3*x + 1];
				const uint8_t b = src_p[src_pitch*y + 3*x + 2];
				dst_p[dst_pitch*y + x] = (uint8_t)findColorIndex(paletteindex, r, g, b, 0);
			}
		}
		layer->m_image_rgb24.unlockData();
		return true;
	}

	K__ERROR("E_EDGE_INVALID_BIT_DEPTH: DEPTH=%d, PAGE=%d, LAYER=%d", m_bit_depth, pageindex, layerindex);
	return false;
}
#pragma endregion // KEdgeDocument


#pragma region KEdgePalFile
KEdgePalFile::KEdgePalFile() {
	memset(m_bgr, 0, sizeof(m_bgr));
	memset(m_data, 0, sizeof(m_data));
}
KImage KEdgePalFile::exportImage(bool index0_for_transparent) const {
	KImage img = KImage::createFromSize(16, 16);
	uint8_t *dst = img.lockData();
	for (int i=0; i<256; i++) {
		dst[i*4 + 0] = m_bgr[i*3+2]; // r
		dst[i*4 + 1] = m_bgr[i*3+1]; // g
		dst[i*4 + 2] = m_bgr[i*3+0]; // b
		dst[i*4 + 3] = 255; // a
	}
	if (index0_for_transparent) {
		dst[0*4 + 3] = 0; // a
	}
	img.unlockData();
	return img;
}
#pragma endregion // KEdgePalFile


#pragma region KEdgePalReader
bool KEdgePalReader::loadFromFileName(const std::string &filename) {
	bool ok = false;
	KInputStream file = KInputStream::fromFileName(filename);
	if (file.isOpen()) {
		ok = loadFromStream(file);
	}
	return ok;
}
bool KEdgePalReader::loadFromMemory(const void *data, size_t size) {
	bool ok = false;
	KInputStream file = KInputStream::fromMemory(data, size);
	if (file.isOpen()) {
		ok = loadFromStream(file);
	}
	return ok;
}
bool KEdgePalReader::loadFromStream(KInputStream &file) {
	if (!file.isOpen()) {
		return false;
	}
	// 識別子
	{
		char sign[12];
		file.read(sign, sizeof(sign));
		if (strcmp(sign, "EDGE2 PAL") != 0) {
			return false; // sign not matched
		}
	}
	// 展開後のデータサイズ
	uint32_t uzsize = file.readUint32();
	K__ASSERT(uzsize > 0);

	// 圧縮データ
	int zsize = file.size() - file.tell();
	KEdgeBin zbin = file.readBin(zsize);
	K__ASSERT(zsize > 0);
	K__ASSERT(! zbin.empty());

	// 展開
	KEdgeBin bin = KZlib::uncompress_zlib(zbin.data(), zsize, uzsize);
	K__ASSERT(! bin.empty());
	K__ASSERT(bin.size() == uzsize);

	KChunkedFileReader::init(bin.data(), bin.size());

	uint16_t a;
	readChunk(0x03e8, 2, &a);

	// パレットをすべて読みだす
	while (!eof()) {
		KEdgePalFile pal;
		std::string name_mb;
		readChunk(0x03e9, &name_mb);
		pal.m_name_u8 = K::strAnsiToUtf8(name_mb, EDGE2_STRING_ENCODING);
		readChunk(0x03ea, 256*3, pal.m_bgr);
		readChunk(0x03eb, 256, pal.m_data);
		m_items.push_back(pal);
	}
	return true;
}
std::string KEdgePalReader::exportXml() { // UTF8
	char tmp[1024] = {0};
	std::string xml_u8 = "<PalFile>\n";
	for (size_t i=0; i<m_items.size(); i++) {
		sprintf_s(tmp, sizeof(tmp), "  <Pal label='%s'/><!-- %d -->\n", m_items[i].m_name_u8.c_str(), i);
		xml_u8 += tmp;
	}
	xml_u8 += "</PalFile>\n";
	return xml_u8;
}

#pragma endregion // KEdgePalReader


#pragma region KEdgeRawReader
KEdgeRawReader::KEdgeRawReader() {
}
// Edge2 バイナリファイルからデータを読み取る
int KEdgeRawReader::read(KEdgeDocument *e, KInputStream &file) {
	if (e == NULL) return 0;
	if (!file.isOpen()) return 0;

	uint16_t zflag;
	KEdgeBin uzbin = unzip(file, &zflag);

	if (uzbin.empty()) return -1;

	// EDGE を得る
	readFromUncompressedBuffer(e, uzbin.data(), uzbin.size());
	e->m_compression_flag = zflag;
	return 0;
}

KEdgeBin KEdgeRawReader::unzip(KInputStream &file, uint16_t *zflag) {
	K__VERIFY(zflag);

	// 識別子
	{
		char sign[6];
		file.read(sign, sizeof(sign));
		if (strcmp(sign, "EDGE2") != 0) {
			return KEdgeBin(); // signature doest not matched
		}
	}
	// 圧縮フラグ
	file.read(zflag, 2);

	// 展開後のデータサイズ
	uint32_t uzsize;
	file.read(&uzsize, 4);
	K__ASSERT(uzsize > 0);

	// 圧縮データ
	int zsize = file.size() - file.tell();
	KEdgeBin zbin = file.readBin(zsize);
	K__ASSERT(zsize > 0);
	K__ASSERT(! zbin.empty());

	// 展開
	KEdgeBin uzbin = KZlib::uncompress_zlib(zbin, uzsize);
	K__ASSERT(! uzbin.empty());
	K__ASSERT(uzbin.size() == uzsize);

	return uzbin;
}

void KEdgeRawReader::scanLayer(KEdgeDocument *e, KEdgePage *page, KEdgeLayer *layer) {
	std::string label_mb;
	readChunk(0x03e8, &label_mb);
	layer->m_label_u8 = K::strAnsiToUtf8(label_mb, EDGE2_STRING_ENCODING);

	readChunk(0x03e9, 4, &layer->m_data_a);
	readChunk(0x03ea, 1, &layer->m_is_grouped);
	readChunk(0x03eb, 1, &layer->m_data_b);
	readChunk(0x03ec, 1, &layer->m_data_c);
	readChunk(0x03ed, 1, &layer->m_is_visibled);
	{
		layer->m_image_rgb24 = _MakeImage(page->m_width, page->m_height, e->m_bit_depth);
		uint8_t *p = layer->m_image_rgb24.lockData();
		K__ASSERT(p);
		uint32_t len = page->m_width * page->m_height * e->m_bit_depth / 8;
		readChunk(0x03ee, len, p); // 8bit palette or 24bit BGR
		layer->m_image_rgb24.unlockData();
	}
	readChunk(0x03ef, 1, &layer->m_data_d);
	readChunk(0x03f0, 0, &layer->m_data_e); // 子チャンクを含む
	readChunk(0x03f1, 4, layer->m_label_color_rgbx);
	{
		uint32_t raw_delay = 0;
		readChunk(0x07d0, 4, &raw_delay);
		layer->m_delay = raw_delay / 100;
	}
	readChunk(0x07d1, 1, &layer->m_data_f);
	readChunk(0x07d2, 1, &layer->m_data_g);
	readChunk(0x07d3, 1, &layer->m_data_h);
}

void KEdgeRawReader::scanPage(KEdgeDocument *e, KEdgePage *page) {
	uint16_t numlayers;

	std::string label_mb;
	readChunk(0x03e8, &label_mb);
	page->m_label_u8 = K::strAnsiToUtf8(label_mb, EDGE2_STRING_ENCODING);

	readChunk(0x03e9, 4, &page->m_data_a);
	readChunk(0x03ea, 1, &page->m_is_grouped);
	readChunk(0x03eb, 1, &page->m_data_b);
	readChunk(0x03ec, 1, &page->m_data_c);
	readChunk(0x03ed, 2, &page->m_width);
	readChunk(0x03ee, 2, &page->m_height);
	readChunk(0x03ef, 4, &page->m_data_d);
	readChunk(0x03f0, 4, &page->m_data_e);
	readChunk(0x03f1, 4, page->m_label_color_rgbx);
	readChunk(0x07d0, 1, &page->m_data_f);
	readChunk(0x07d1, 4, &page->m_data_g);
	readChunk(0x07d2, 2, &numlayers);
	// レイヤ
	for (uint16_t i=0; i<numlayers; i++) {
		openChunk(0x07d3);
		KEdgeLayer *layer = new KEdgeLayer();
		scanLayer(e, page, layer);
		page->m_layers.push_back(layer);
		closeChunk();
	}
	readChunk(0x07d4, 4, &page->m_data_h);
	{
		uint32_t raw_delay = 0;
		readChunk(0x0bb8, 4, &raw_delay);
		page->m_delay = raw_delay / 100;
	}
	readChunk(0x0bb9, 1, &page->m_data_i);
	readChunk(0x0bba, 1, &page->m_data_j);
	readChunk(0x0bbb, 5, &page->m_ani_xparam);
	readChunk(0x0bbc, 5, &page->m_ani_yparam);
	readChunk(0x0bbd, 1, &page->m_data_k);
}

void KEdgeRawReader::scanEdge(KEdgeDocument *e) {
	e->m_pages.clear();
	e->m_palettes.clear();

	uint16_t numpages;
	readChunk(0x03e8, 1, &e->m_bit_depth);
	readChunk(0x03e9, 1, &e->m_data_a);
	readChunk(0x03ea, 1, &e->m_data_b);
	readChunk(0x03eb, 1, &e->m_data_c);

	openChunk(0x03ed);
	{
		readChunk(0x3e8, 1, &e->m_data_d);
		readChunk(0x3e9, 1, &e->m_data_d2);
	}
	closeChunk();

	// Optional chunk
	if (checkChunkId(0x03ee)) {
		readChunk(0x03ee, 0, &e->m_data_d3);
	}
	if (checkChunkId(0x03ef)) {
		readChunk(0x03ef, 0, &e->m_data_d4);
	}

	readChunk(0x0fa1, 0, &e->m_data_e);

	readChunk(0x07d0, 2, &numpages);
	readChunk(0x07d1, 4, &e->m_data_f);
	readChunk(0x07d2, 1, &e->m_data_g);

	// Pages
	for (uint16_t i=0; i<numpages; ++i) {
		openChunk(0x07d3);
		KEdgePage *page = new KEdgePage();
		scanPage(e, page);
		e->m_pages.push_back(page);
		closeChunk();
	}

	uint16_t numpals;
	readChunk(0x07d4, 4, &e->m_data_h);
	readChunk(0x0bb8, 2, &numpals);
	readChunk(0x0bb9, 2, &e->m_data_i);
	readChunk(0x0bba, 4, &e->m_data_j);

	// パレット（3 * 256バイト * 任意の個数）
	for (uint16_t i=0; i<numpals; ++i) {
		openChunk(0x0bbb);

		KEdgePalette *pal = new KEdgePalette();
		
		std::string name_mb;
		readChunk(0x03e8, 0, &name_mb);
		pal->m_name_u8 = K::strAnsiToUtf8(name_mb, EDGE2_STRING_ENCODING);
		
		readChunk(0x03e9, 4,     &pal->m_data_b);
		readChunk(0x03ea, 1,     &pal->m_data_c);
		readChunk(0x03eb, 1,     &pal->m_data_d);
		readChunk(0x03ec, 1,     &pal->m_data_e);
		readChunk(0x03ed, 256*3, &pal->m_bgr_colors);
		readChunk(0x03ef, 256,   &pal->m_data_f);
		readChunk(0x03f0, 256,   &pal->m_data_g);
		readChunk(0x03f1, 256*6, &pal->m_data_h);
		readChunk(0x03f2, 256,   &pal->m_data_i);
		readChunk(0x03f3, 0,     &pal->m_data_j); // 子チャンクを含む
		readChunk(0x07d0, 4,     &pal->m_data_k); // Delay?
		readChunk(0x07d1, 1,     &pal->m_data_l);
		readChunk(0x07d2, 1,     &pal->m_data_m);
		e->m_palettes.push_back(pal);

		closeChunk();
	}
	readChunk(0x0bbc, 4, &e->m_data_k);

	// 前景色
	openChunk(0x0bbf);
	{
		readChunk(0x03e8, 1, &e->m_fg_palette.color_index);
		readChunk(0x03e9, 3,  e->m_fg_palette.bgr_colors);
		readChunk(0x03ea, 1, &e->m_fg_palette.data_a);
		readChunk(0x03eb, 6,  e->m_fg_palette.data_b);
		readChunk(0x03ec, 1, &e->m_fg_palette.data_c);
	}
	closeChunk();

	// 背景色
	openChunk(0x0bc0);
	{
		readChunk(0x03e8, 1, &e->m_bg_palette.color_index);
		readChunk(0x03e9, 3,  e->m_bg_palette.bgr_colors);
		readChunk(0x03ea, 1, &e->m_bg_palette.data_a);
		readChunk(0x03eb, 6,  e->m_bg_palette.data_b);
		readChunk(0x03ec, 1, &e->m_bg_palette.data_c);
	}
	closeChunk();
}

// 非圧縮の Edge2 バイナリデータからデータを読み取る
// @param buf バイナリ列(ZLibで展開後のデータであること)
// @param len バイナリ列のバイト数
void KEdgeRawReader::readFromUncompressedBuffer(KEdgeDocument *e, const void *buf, size_t len) {
	init(buf, len);
	scanEdge(e);
}
#pragma endregion // KEdgeRawReader


#pragma region KEdgeRawWriter
KEdgeRawWriter::KEdgeRawWriter() {
	clear();
}
void KEdgeRawWriter::write(KEdgeDocument *e, KOutputStream &file) {
	// 無圧縮 Edge バイナリを作成する
	writeUncompressedEdge(e);
	
	// データを圧縮し、Edge2 ツールで扱える形式にしたものを得る
	writeCompressedEdge(file);
}
void KEdgeRawWriter::writeLayer(KEdgePage *page, int layerindex, int bitdepth) {
	K__ASSERT(page);
	KEdgeLayer *layer = page->getLayer(layerindex);
	K__ASSERT(layer);

	std::string label_mb = K::strUtf8ToAnsi(layer->m_label_u8, EDGE2_STRING_ENCODING);
	writeChunkN(0x03e8, label_mb.c_str());
	writeChunk4(0x03e9, layer->m_data_a);
	writeChunk1(0x03ea, layer->m_is_grouped);
	writeChunk1(0x03eb, layer->m_data_b);
	writeChunk1(0x03ec, layer->m_data_c);
	writeChunk1(0x03ed, layer->m_is_visibled?1:0);
	{
		// write RGB pixels
		uint32_t pixels_size = page->m_width * page->m_height * bitdepth/8;
		uint8_t * pixels_ptr = layer->m_image_rgb24.lockData();
		if (pixels_ptr) {
			K__ASSERT(pixels_size == (uint32_t)layer->m_image_rgb24.getDataSize());
			writeChunkN(0x03ee, pixels_size, pixels_ptr);
			layer->m_image_rgb24.unlockData();
		} else {
			// 画像が Empty 状態になっている
			writeChunkN(0x03ee, pixels_size, NULL);
		}
	}
	writeChunk1(0x03ef, layer->m_data_d);
	writeChunkN(0x03f0, layer->m_data_e);
	writeChunkN(0x03f1, 4, layer->m_label_color_rgbx);
	writeChunk4(0x07d0, layer->m_delay * 100); // delay must be written x100
	writeChunk1(0x07d1, layer->m_data_f);
	writeChunk1(0x07d2, layer->m_data_g);
	writeChunk1(0x07d3, layer->m_data_h);
}

void KEdgeRawWriter::writePage(KEdgeDocument *e, int pageindex) {
	K__ASSERT(e);
	KEdgePage *page = e->getPage(pageindex);
	K__ASSERT(page);
	K__ASSERT(page->m_width > 0);
	K__ASSERT(page->m_height > 0);

	std::string label_mb = K::strUtf8ToAnsi(page->m_label_u8, EDGE2_STRING_ENCODING);
	writeChunkN(0x03e8, label_mb.c_str());
	writeChunk4(0x03e9, page->m_data_a);
	writeChunk1(0x03ea, page->m_is_grouped);
	writeChunk1(0x03eb, page->m_data_b);
	writeChunk1(0x03ec, page->m_data_c);
	writeChunk2(0x03ed, page->m_width);
	writeChunk2(0x03ee, page->m_height);
	writeChunk4(0x03ef, page->m_data_d);
	writeChunk4(0x03f0, page->m_data_e);
	writeChunkN(0x03f1, 4, page->m_label_color_rgbx);
	writeChunk1(0x07d0, page->m_data_f);
	writeChunk4(0x07d1, page->m_data_g);

	uint16_t numLayers = (uint16_t)page->getLayerCount();
	writeChunk2(0x07d2, numLayers);

	// Layers
	K__ASSERT(numLayers > 0);
	for (uint16_t i=0; i<numLayers; i++) {
		openChunk(0x07d3);
		writeLayer(page, i, e->m_bit_depth);
		closeChunk();
	}

	writeChunk4(0x07d4, page->m_data_h);
	writeChunk4(0x0bb8, page->m_delay * 100); // delay must be written x100
	writeChunk1(0x0bb9, page->m_data_i);
	writeChunk1(0x0bba, page->m_data_j);
	writeChunkN(0x0bbb, 5, &page->m_ani_xparam);
	writeChunkN(0x0bbc, 5, &page->m_ani_yparam);
	writeChunk1(0x0bbd, page->m_data_k);
}

void KEdgeRawWriter::writeEdge(KEdgeDocument *e) {
	K__ASSERT(e);
	K__ASSERT(e->m_bit_depth==8 || e->m_bit_depth==24);

	writeChunk1(0x03e8, e->m_bit_depth);
	writeChunk1(0x03e9, e->m_data_a);
	writeChunk1(0x03ea, e->m_data_b);
	writeChunk1(0x03eb, e->m_data_c);

	openChunk(0x03ed);
	{
		writeChunk1(0x03e8, e->m_data_d);
		writeChunk1(0x03e9, e->m_data_d2);
	}
	closeChunk();

	if (e->m_data_d3.size() > 0) {
		writeChunkN(0x03ee, e->m_data_d3);
	} else {
		// データサイズゼロとして書くのではなく
		// チャンク自体を書き込まない
	}

	writeChunkN(0x0fa1, e->m_data_e);

	uint16_t numPages = (uint16_t)e->getPageCount();
	writeChunk2(0x07d0, numPages);
	writeChunk4(0x07d1, e->m_data_f);
	writeChunk1(0x07d2, e->m_data_g);

	// Pages
	K__ASSERT(numPages > 0);
	for (uint16_t i=0; i<numPages; i++) {
		openChunk(0x07d3);
		writePage(e, i);
		closeChunk();
	}

	writeChunk4(0x07d4, e->m_data_h);
	
	uint16_t numPalettes = (uint16_t)e->getPaletteCount();
	writeChunk2(0x0bb8, numPalettes);
	writeChunk2(0x0bb9, e->m_data_i);
	writeChunk4(0x0bba, e->m_data_j);

	// Palettes
	K__ASSERT(numPalettes > 0);
	for (uint16_t i=0; i<numPalettes; i++) {
		openChunk(0x0bbb);
		const KEdgePalette *pal = e->m_palettes[i];

		std::string name_mb = K::strUtf8ToAnsi(pal->m_name_u8, EDGE2_STRING_ENCODING);
		writeChunkN(0x03e8, name_mb.c_str());
		writeChunk4(0x03e9, pal->m_data_b);
		writeChunk1(0x03ea, pal->m_data_c);
		writeChunk1(0x03eb, pal->m_data_d);
		writeChunk1(0x03ec, pal->m_data_e);
		writeChunkN(0x03ed, pal->m_bgr_colors);
		writeChunkN(0x03ef, pal->m_data_f);
		writeChunkN(0x03f0, pal->m_data_g);
		writeChunkN(0x03f1, pal->m_data_h);
		writeChunkN(0x03f2, pal->m_data_i);
		writeChunkN(0x03f3, pal->m_data_j);
		writeChunk4(0x07d0, pal->m_data_k);
		writeChunk1(0x07d1, pal->m_data_l);
		writeChunk1(0x07d2, pal->m_data_m);
		closeChunk();
	}

	writeChunk4(0x0bbc, e->m_data_k);

	// 前景色
	openChunk(0x0bbf);
	{
		writeChunk1(0x03e8,    e->m_fg_palette.color_index);
		writeChunkN(0x03e9, 3, e->m_fg_palette.bgr_colors);
		writeChunk1(0x03ea,    e->m_fg_palette.data_a);
		writeChunkN(0x03eb, 6, e->m_fg_palette.data_b);
		writeChunk1(0x03ec,    e->m_fg_palette.data_c);
	}
	closeChunk();
		
	// 背景色
	openChunk(0x0bc0);
	{
		writeChunk1(0x03e8,    e->m_bg_palette.color_index);
		writeChunkN(0x03e9, 3, e->m_bg_palette.bgr_colors);
		writeChunk1(0x03ea,    e->m_bg_palette.data_a);
		writeChunkN(0x03eb, 6, e->m_bg_palette.data_b);
		writeChunk1(0x03ec,    e->m_bg_palette.data_c);
	}
	closeChunk();
}

void KEdgeRawWriter::writeCompressedEdge(KOutputStream &file) const {
	if (!file.isOpen()) {
		K__ERROR("");
		return;
	}
	// 圧縮前のサイズ
	uint32_t uncompsize = size();

	KEdgeBin zbuf = KZlib::compress_zlib(data(), size(), 1);

	// ヘッダーを出力
	uint16_t compflag = 256;
	file.write("EDGE2\0", 6);
	file.write((char *)&compflag, 2);
	file.write((char *)&uncompsize, 4);

	// 圧縮データを出力
	file.write(zbuf.data(), zbuf.size());
}

void KEdgeRawWriter::writeUncompressedEdge(KEdgeDocument *e) {
	if (e == NULL) {
		K__ERROR("");
		return;
	}
	// 初期化
	clear();
		
	// 無圧縮 Edge データを作成
	writeEdge(e);
}
#pragma endregion // KEdgeRawWriter



#pragma region KEdgeWriter
KEdgeWriter::KEdgeWriter() {
	m_doc = new KEdgeDocument();
	m_doc->m_palettes.push_back(new KEdgePalette());
	m_doc->m_bit_depth = 24;
}
KEdgeWriter::~KEdgeWriter() {
	delete (m_doc);
}
void KEdgeWriter::clear() {
	delete(m_doc);
	//
	m_doc = new KEdgeDocument();
	m_doc->m_palettes.push_back(new KEdgePalette());
	m_doc->m_bit_depth = 24;
}
void KEdgeWriter::addPage(const KImage *img, int delay) {
	if (img==NULL || img->empty()) return;
	int img_w = img->getWidth();
	int img_h = img->getHeight();
	{
		KEdgePage *page = m_doc->addPage();
		page->m_width  = (uint16_t)img_w;
		page->m_height = (uint16_t)img_h;
		page->m_delay = delay;
		{
			KEdgeLayer *layer = page->addLayer();
			if (img->getBytesPerPixel() == 4) { // 32ビット画像から24ビット画像へ
				KImage image_rgb24 = KImage::createFromPixels(img_w, img_h, KColorFormat_RGB24, NULL);
				int src_p = img->getPitch();
				const uint8_t *sbuf = img->getData();
				int dst_p = image_rgb24.getPitch();
				uint8_t *dbuf = image_rgb24.lockData();
				for (int y=0; y<img_h; y++) {
					for (int x=0; x<img_w; x++) {
						uint8_t r = sbuf[src_p * y + 4 * x + 0];
						uint8_t g = sbuf[src_p * y + 4 * x + 1];
						uint8_t b = sbuf[src_p * y + 4 * x + 2];
					//	uint8_t a = sbuf[src_p * y + 4 * x + 3];

						// edge へは BGR 順で保存することに注意
						dbuf[dst_p * y + 3 * x + 0] = b;
						dbuf[dst_p * y + 3 * x + 1] = g;
						dbuf[dst_p * y + 3 * x + 2] = r;
					}
				}
				layer->m_image_rgb24 = image_rgb24;
			}
		}
	}
}
void KEdgeWriter::saveToStream(KOutputStream &file) {
	K__ASSERT(m_doc);
	m_doc->saveToStream(file);
}
#pragma endregion // KEdgeWriter


} // namespace
