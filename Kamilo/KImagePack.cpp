#include "KImagePack.h"
//
#include "KCrc32.h"
#include "KVec.h"
#include "KStream.h"
#include "KXml.h"
#include "KInternal.h"

namespace Kamilo {

/// a ÷ b の端数切り上げ整数を返す
static int K__CeilDiv(int a, int b) {
	return (int)ceilf((float)a / b);
}

/// x <= 2^n を満たす最小の 2^n を返す
static int K__CeilToPow2(int x) {
	int s = 1;
	while (s < x) {
		s <<= 1;
	}
	return s;
}


#pragma region CImgPackW
class CImgPackWImpl {
public:
	std::string m_Extra;
	std::vector<KImgPackItem> m_Items;
	std::unordered_map<uint32_t, int> m_Hash; // <hash, cellindex>
	int m_CellSize;
	int m_CellSpace;
	int m_CellCount;
	bool m_ExcludeDupCells;

	CImgPackWImpl(int cellsize, bool exclude_dup_cells) {
		m_Items.clear();
		m_CellSize = cellsize;
		m_CellSpace = 0;
		m_CellCount = 0;
		m_ExcludeDupCells = exclude_dup_cells;
	}
	void setCellSize(int cellsize, int cellspace) {
		m_Items.clear();
		m_CellSize = cellsize;
		m_CellSpace = cellspace;
		m_CellCount = 0;
	}
	bool empty() {
		return m_Items.empty();
	}
	void addImage(const KImage &img, const KImgPackExtraData *extra) {
		int reduce_cells = 0;

		KImgPackItem item;
		item.img = img;
		item.xcells = 0;
		item.ycells = 0;
		item.pack_offset = m_CellCount;
		if (extra) item.extra = *extra;
		KImageUtils::scanOpaqueCells(img, m_CellSize, &item.cells, &item.xcells, &item.ycells);
		if (m_ExcludeDupCells) {
			// 重複確認。テスト用。重複除外をONにした場合、まだ復元できない！！！！！！！！！！！！！！！！！！！！！！
			for (int i=item.cells.size()-1; i>=0; i--) {
				int idx = item.cells[i];
				int x = m_CellSize * (idx % item.xcells);
				int y = m_CellSize * (idx / item.ycells);
				KImage sub = img.cloneRect(x, y, m_CellSize, m_CellSize);
				uint32_t hash = KCrc32::fromData(sub.getData(), sub.getDataSize());
				if (m_Hash.find(hash) != m_Hash.end()) {
					// 同一画像のセルが存在する
					item.cells.erase(item.cells.begin() + i);
					reduce_cells++;
				} else {
					m_Hash[hash] = i;
				}
			}
		}
		m_Items.push_back(item);
		m_CellCount += item.cells.size();
	}
	bool getBestSize(int *w, int *h) const {
		K__ASSERT(w && h);
		const int BIGNUM = 1000000;
		int max_width = 1024 * 8;
		int best_w = 0;
		int best_h = 0;
		int best_loss = BIGNUM;
		int cellarea = m_CellSize + m_CellSpace * 2; // 1セルのために必要なサイズ。余白が設定されている場合はそれも含む
		int min_w_pow2 = K__CeilToPow2(cellarea); // とりあえず１セルが収まるような最低の 2^n から調べていく
		for (int w_pow2=min_w_pow2; w_pow2<=max_width; w_pow2*=2) {
			int xcells = w_pow2 / cellarea; // 横セル数
			int req_ycells = K__CeilDiv(m_CellCount, xcells); // 必要な縦セル数（端数切り上げ）
			int req_height = req_ycells * cellarea; // 必要な縦ピクセル数
			int actual_height = K__CeilToPow2(req_height); // 必要な縦ピクセル数を満たし、かつ 2^n になるような縦ピクセル数
			int actual_ycells = actual_height / cellarea; // 実際に縦に配置できるセル数
			int actual_total = xcells * actual_ycells; // 実際に配置できるセル総数
			if (actual_total < m_CellCount) {
				continue; // 足りない
			}
			int loss = actual_total - m_CellCount; // 無駄になったセル数
			if (loss < best_loss) {
				best_w = w_pow2;
				best_h = actual_height;
				best_loss = loss;

			} else if (loss == best_loss) {
				// ロス数が同じ場合、縦横の和が小さい方を選ぶ
				if (w_pow2 + actual_height < best_w + best_h) {
					best_w = w_pow2;
					best_h = actual_height;
				//	best_loss = loss;
				}
			}
		}
		if (best_w && best_h) {
			*w = best_w;
			*h = best_h;
			return true;
		}
		return false;
	}
	int getNumberOfCellsRequired() const {
		return m_CellCount;
	}
	bool isCapacityOK(int dest_width, int dest_height) const {
		int cellarea = m_CellSize + m_CellSpace * 2; // 1セルのために必要なサイズ。余白が設定されている場合はそれも含む
		int xcells = dest_width  / cellarea; // 端数切捨て
		int ycells = dest_height / cellarea;
		return m_CellCount <= xcells * ycells;
	}
	KImage getPackedImage() const {
		// 画像を格納するのに最適なテクスチャサイズを得る
		int w = 0;
		int h = 0;
		getBestSize(&w, &h);

		KImage dest = KImage::createFromSize(w, h);
		int cellarea = m_CellSize + m_CellSpace * 2; // 1セルのために必要なサイズ。余白が設定されている場合はそれも含む
		int dest_xcells = w / cellarea; // 端数切捨て
		for (auto it=m_Items.begin(); it!=m_Items.end(); ++it) {
			const KImgPackItem &item = *it;
			for (size_t c=0; c<item.cells.size(); c++) {
				int orig_idx = item.cells[c];
				int orig_col = orig_idx % item.xcells;
				int orig_row = orig_idx / item.xcells;
				int orig_x = m_CellSize * orig_col;
				int orig_y = m_CellSize * orig_row;

				int pack_idx = item.pack_offset + c;
				int pack_col = pack_idx % dest_xcells;
				int pack_row = pack_idx / dest_xcells;
				int pack_x = cellarea * pack_col;
				int pack_y = cellarea * pack_row;

				dest.copyRect(pack_x+m_CellSpace, pack_y+m_CellSpace, item.img, orig_x, orig_y, m_CellSize, m_CellSize);
			}
		}
		return dest;
	}
	void getMetaString(std::string *p_meta) const {
		K__ASSERT(p_meta);
		std::string &meta = *p_meta;
		char s[256];
		sprintf_s(s, sizeof(s), "<pack cellsize='%d' cellspace='%d' numimages='%d'>\n", m_CellSize, m_CellSpace, m_Items.size());
		meta = s;

		meta += "<extra>" + m_Extra + "</extra>\n";

		for (auto it=m_Items.begin(); it!=m_Items.end(); ++it) {
			const KImgPackItem &item = *it;
			sprintf_s(s, sizeof(s), "<img w='%d' h='%d' offset='%d' numcells='%d' page='%d' layer='%d' blend='%d' data0='%d'>",
				item.img.getWidth(),
				item.img.getHeight(),
				item.pack_offset, 
				item.cells.size(),
				item.extra.page,
				item.extra.layer,
				item.extra.blend,
				item.extra.data0
			);
			meta += s;
			for (size_t c=0; c<item.cells.size(); c++) {
				int src_idx = item.cells[c];
				sprintf_s(s, sizeof(s), "%d ", src_idx);
				meta += s;
			}
			meta += "</img>\n";
		}
		meta += "</pack>\n";
	}
	void setExtraString(const std::string &data) {
		m_Extra = data;
	}
	bool saveToFileNames(const std::string &imagefile, const std::string &metafile) const {
		// パック画像を保存
		if (!imagefile.empty()) {
			KImage teximg = getPackedImage();
			std::string png = teximg.saveToMemory(1/*FASTEST*/);

			KOutputStream output = KOutputStream::fromFileName(imagefile);
			if (output.isOpen()) {
				output.write(png.data(), png.size());
			} else {
				K__ERROR("Failed to write imagefile: %s", imagefile.c_str());
				return false;
			}
		}

		// パック情報を保存
		if (!metafile.empty()) {
			std::string meta;
			getMetaString(&meta);

			KOutputStream output = KOutputStream::fromFileName(metafile);
			if (output.isOpen()) {
				output.write(meta.data(), meta.size());
			} else {
				K__ERROR("Failed to write metafile: %s", metafile.c_str());
				return false;
			}
		}

		// デバッグ情報（使用セル数）
		if (0) {
			KOutputStream info = KOutputStream::fromFileName("___cells.txt", "ab");
			info.writeString(K::str_sprintf("%s, %d\n", imagefile.c_str(), m_CellCount));
		}
		return true;
	}
};
#pragma endregion // CImgPackW


#pragma region KImgPackW
KImgPackW::KImgPackW(int cellsize, bool exclude_dup_cells) {
	m_Impl = std::shared_ptr<CImgPackWImpl>(new CImgPackWImpl(cellsize, exclude_dup_cells));
}
bool KImgPackW::empty() const {
	return m_Impl->empty();
}
void KImgPackW::setCellSize(int size, int space) {
	m_Impl->setCellSize(size, space);
}
void KImgPackW::addImage(const KImage &img, const KImgPackExtraData *extra) {
	m_Impl->addImage(img, extra);
}
bool KImgPackW::saveToFileNames(const std::string &imagefile, const std::string &metafile) const {
	return m_Impl->saveToFileNames(imagefile, metafile);
}
void KImgPackW::setExtraString(const std::string &data) {
	m_Impl->setExtraString(data);
}
KImage KImgPackW::getPackedImage() const {
	return m_Impl->getPackedImage();
}
bool KImgPackW::getBestSize(int *w, int *h) const {
	return m_Impl->getBestSize(w, h);
}
void KImgPackW::getMetaString(std::string *p_meta) const {
	m_Impl->getMetaString(p_meta);
}
#pragma endregion // CImgPackW


#pragma region CImgPackR
class CImgPackRImpl {
public:
	mutable std::vector<KVec3> m_pos;
	mutable std::vector<KVec2> m_tex;
	std::vector<KImgPackItem> m_itemlist;
	std::string m_extra;
	int m_cellsize;
	int m_cellspace;
	float m_adj;

	CImgPackRImpl() {
		m_cellsize = 0;
		m_cellspace = 0;

		// ピクセルとの境界線をぴったりとってしまうと、計算誤差によって隣のピクセルを取得してしまう可能性がある。
		// 少しだけピクセル内側を指すようにする
		// セルの余白を設定しているなら、この値は 0 でも大丈夫。
		// セル余白をゼロにして調整値を設定するか、
		// セル余白を設定して調整値を 0 にするか、どちらかで対処する
		m_adj = 0.01f;
	}
	void clear() {
		m_itemlist.clear();
		m_cellsize = 0;
		m_cellspace = 0;
		m_extra.clear();
	}
	bool loadFromMetaString(const std::string &xml_u8) {
		clear();

		KXmlElement *xml = KXmlElement::createFromString(xml_u8, __FUNCTION__);
		if (xml == NULL) {
			K__ERROR(u8"E_XML at CImgPackR");
			return false;
		}

		const KXmlElement *xPack = xml->findNode("pack");
		if (xPack) {
			m_cellsize = xPack->getAttrInt("cellsize");
			K__ASSERT(m_cellsize > 0);

			m_cellspace = xPack->getAttrInt("cellspace");
			K__ASSERT(m_cellspace >= 0);

			const KXmlElement *xExtra = xPack->findNode("extra");
			if (xExtra) {
				m_extra = xExtra->getText("");
			}

			// <img w='250' h='305' offset='0' numcells='217' page='0' layer='0' blend='-1' data0='0'>6 7 8 21 22 23 24 25 37 38 39 40 41 53 54 55 56 57 69 70 71 72</img>
			int cellarea = m_cellsize + m_cellspace * 2; // 1セルのために必要なサイズ。余白が設定されている場合はそれも含む
			int numimages = xPack->getAttrInt("numimages");
			for (int i=0; i<xPack->getChildCount(); i++) {
				const KXmlElement *xImg = xPack->getChild(i);
				if (!xImg->hasTag("img")) continue;

				int w = xImg->getAttrInt("w");
				int h = xImg->getAttrInt("h");

				KImgPackItem item;
				item.pack_offset = xImg->getAttrInt("offset");
				item.img = KImage::createFromSize(w, h);
				item.xcells  = w / cellarea;
				item.ycells  = h / cellarea;
				item.extra.page  = xImg->getAttrInt("page");
				item.extra.layer = xImg->getAttrInt("layer");
				item.extra.blend = xImg->getAttrInt("blend", -1); // KBlendのデフォルト値は -1 (KBlend_INVALID) であることに注意
				item.extra.data0 = xImg->getAttrInt("data0");
				int numcells = xImg->getAttrInt("numcells");

				// 空白区切りでセル番号が列挙してある
				const char *text = xImg->getText(""); // ascii 文字だけだとわかりきっているので、文字列コード考慮しない
				auto tok = K::strSplit(text, " ");
				K__ASSERT((int)tok.size() == numcells); // セル番号はセルと同じ個数だけあるはず
				item.cells.resize(numcells);

				// セル番号を得る
				for (int i=0; i<numcells; i++) {
					int idx = 0;
					K::strToInt(tok[i], &idx);
					item.cells[i] = idx;
				}

				m_itemlist.push_back(item);
			}
			K__ASSERT((int)m_itemlist.size() == numimages);
		}
		xml->drop();
		return true;
	}
	bool loadFromMetaFileName(const std::string &metafilename) {
		KInputStream file = KInputStream::fromFileName(metafilename);
		std::string text = file.readBin();
		if (text.empty()) {
			return false;
		}
		if (!loadFromMetaString(text.c_str())) {
			return false;
		}
		return true;
	}
	int getImageCount() const {
		return (int)m_itemlist.size();
	}
	void getImageSize(int index, int *w, int *h) const {
		K__ASSERT(w && h);
		const KImgPackItem &item = m_itemlist[index];
		if (w) *w = item.img.getWidth();
		if (h) *h = item.img.getHeight();
	}
	void getImageExtra(int index, KImgPackExtraData *extra) const {
		K__ASSERT(0 <= index && index < (int)m_itemlist.size());
		const KImgPackItem &item = m_itemlist[index];
		if (extra) *extra = item.extra;
	}
	KImage getImage(const KImage &pack_img, int index) const {
		K__ASSERT(m_cellsize > 0);
		K__ASSERT(!pack_img.empty());
		K__ASSERT(pack_img.getFormat() == KColorFormat_RGBA32);

		int cellarea = m_cellsize + m_cellspace * 2; // 1セルのために必要なサイズ。余白が設定されている場合はそれも含む
		const KImgPackItem &packitem = m_itemlist[index];
		KImage img = KImage::createFromSize(packitem.img.getWidth(), packitem.img.getHeight());
		int pack_xcells = pack_img.getWidth() / cellarea;
		int orig_xcells = packitem.img.getWidth() / m_cellsize;
		for (size_t i=0; i<packitem.cells.size(); i++) {
			int pack_idx = packitem.pack_offset + i;
			int pack_col = pack_idx % pack_xcells;
			int pack_row = pack_idx / pack_xcells;
			int pack_x = cellarea * pack_col;
			int pack_y = cellarea * pack_row;
			int orig_idx = packitem.cells[i];
			int orig_col = orig_idx % orig_xcells;
			int orig_row = orig_idx / orig_xcells;
			int orig_x = m_cellsize * orig_col;
			int orig_y = m_cellsize * orig_row;
			img.copyRect(orig_x, orig_y, pack_img, pack_x+m_cellspace, pack_y+m_cellspace, m_cellsize, m_cellsize);
		}

		return img;
	}
	int getVertexCount(int index) const {
		if (index < 0 || (int)m_itemlist.size() <= index) {
			return 0;
		}
		const KImgPackItem &item = m_itemlist[index];
		return item.cells.size() * 6; // 1セルにつき2個の三角形(=6頂点)が必要
	}
	const KVec3 * getPositionArray(int pack_w, int pack_h, int index) const {
		K__ASSERT(m_cellsize > 0);
		K__ASSERT(pack_w > 0);
		K__ASSERT(pack_h > 0);
		K__ASSERT(0 <= index && index < (int)m_itemlist.size());
		const KImgPackItem &item = m_itemlist[index];
		m_pos.resize(item.cells.size() * 6);

		// 頂点座標を設定
		const int orig_xcells = K__CeilDiv(item.img.getWidth(), m_cellsize); // 元画像の横セル数（端数切り上げ）
		const int orig_height = item.img.getHeight();
		for (size_t i=0; i<item.cells.size(); i++) {
			int orig_idx = item.cells[i];
			int orig_col = orig_idx % orig_xcells;
			int orig_row = orig_idx / orig_xcells;
			int orig_x = m_cellsize * orig_col;
			int orig_y = m_cellsize * orig_row;

			// セルの各頂点の座標（ビットマップ座標系 左上基準 Y軸下向き）
			int bmp_x0 = orig_x;
			int bmp_y0 = orig_y;
			int bmp_x1 = orig_x + m_cellsize;
			int bmp_y1 = orig_y + m_cellsize;

			// セルの各頂点の座標（ワールド座標系）
			int x0 = bmp_x0;
			int x1 = bmp_x1;
		#if 1
			// 左下基準、Y軸上向き
			int yTop = orig_height - bmp_y0;
			int yBtm = orig_height - bmp_y1;
		#else
			// 左上基準 Y軸下向き
			int yTop = bmp_y0;
			int yBtm = bmp_y1;
		#endif
			// 利便性のため、6頂点のうちの最初の4頂点は、左上を基準に時計回りに並ぶようにする
			// この処理は画像復元の処理に影響することに注意 (KVideoBank::exportSpriteImage)
			//
			// 0 ---- 1 左上から時計回りに 0, 1, 2, 3 になる
			// |      |
			// |      |
			// 3------2
			m_pos[i*6+0] = KVec3(x0, yTop, 0); // 0:左上
			m_pos[i*6+1] = KVec3(x1, yTop, 0); // 1:右上
			m_pos[i*6+2] = KVec3(x1, yBtm, 0); // 2:右下
			m_pos[i*6+3] = KVec3(x0, yBtm, 0); // 3:左下
			m_pos[i*6+4] = KVec3(x0, yTop, 0); // 0:左上
			m_pos[i*6+5] = KVec3(x1, yBtm, 0); // 2:右下
		}
		return m_pos.data();
	}
	const KVec2 * getTexCoordArray(int pack_w, int pack_h, int index) const {
		K__ASSERT(m_cellsize > 0);
		K__ASSERT(pack_w > 0);
		K__ASSERT(pack_h > 0);
		K__ASSERT(0 <= index && index < (int)m_itemlist.size());
		const KImgPackItem &item = m_itemlist[index];
		m_tex.resize(item.cells.size() * 6);

		// テクスチャ座標を設定
		const int cellarea = m_cellsize + m_cellspace * 2; // 1セルのために必要なサイズ。余白が設定されている場合はそれも含む
		const int pack_xcells = pack_w / cellarea; // パック画像に入っている横セル数
		for (size_t i=0; i<item.cells.size(); i++) {
			int pack_idx = item.pack_offset + i;
			int pack_col = pack_idx % pack_xcells;
			int pack_row = pack_idx / pack_xcells;
			int pack_x = cellarea * pack_col + m_cellspace;
			int pack_y = cellarea * pack_row + m_cellspace;

			// セルの各頂点の座標（ビットマップ座標系 Y軸下向き）
			int bmp_x0 = pack_x;
			int bmp_y0 = pack_y;
			int bmp_x1 = pack_x + m_cellsize;
			int bmp_y1 = pack_y + m_cellsize;

			// セルの各頂点のUV座標
			// m_adj ピクセルだけ内側を取るようにする（m_adj は 1未満の微小数）
			float u0 = (float)(bmp_x0 + m_adj) / pack_w;
			float v0 = (float)(bmp_y0 + m_adj) / pack_h;
			float u1 = (float)(bmp_x1 - m_adj) / pack_w;
			float v1 = (float)(bmp_y1 - m_adj) / pack_h;

			// 頂点の順番については getMeshPositionArray を参照
			m_tex[i*6+0] = KVec2(u0, v0); // 0:左上
			m_tex[i*6+1] = KVec2(u1, v0); // 1:右上
			m_tex[i*6+2] = KVec2(u1, v1); // 2:右下
			m_tex[i*6+3] = KVec2(u0, v1); // 3:左下
			m_tex[i*6+4] = KVec2(u0, v0); // 0:左上
			m_tex[i*6+5] = KVec2(u1, v1); // 2:右下
		}
		return m_tex.data();
	}
	const std::string & getExtraString() const {
		return m_extra;
	}
};
#pragma endregion // CImgPackR


#pragma region KImgPackR
KImgPackR::KImgPackR() {
	m_Impl = std::shared_ptr<CImgPackRImpl>(new CImgPackRImpl());
}
bool KImgPackR::empty() const {
	return m_Impl->getImageCount() == 0;
}
bool KImgPackR::loadFromMetaString(const std::string &xml_u8) {
	return m_Impl->loadFromMetaString(xml_u8);
}
bool KImgPackR::loadFromMetaFileName(const std::string &metafilename) {
	return m_Impl->loadFromMetaFileName(metafilename);
}
int KImgPackR::getImageCount() const {
	return m_Impl->getImageCount();
}
void KImgPackR::getImageSize(int index, int *w, int *h) const {
	m_Impl->getImageSize(index, w, h);
}
void KImgPackR::getImageExtra(int index, KImgPackExtraData *extra) const {
	m_Impl->getImageExtra(index, extra);
}
const std::string & KImgPackR::getExtraString() const {
	return m_Impl->getExtraString();
}
KImage KImgPackR::getImage(const KImage &pack_img, int index) const {
	return m_Impl->getImage(pack_img, index);
}
int KImgPackR::getVertexCount(int index) const {
	return m_Impl->getVertexCount(index);
}
const KVec3 * KImgPackR::getPositionArray(int pack_w, int pack_h, int index) const {
	return m_Impl->getPositionArray(pack_w, pack_h, index);
}
const KVec2 * KImgPackR::getTexCoordArray(int pack_w, int pack_h, int index) const {
	return m_Impl->getTexCoordArray(pack_w, pack_h, index);
}




} // namespace
#pragma endregion // KImgPackR
