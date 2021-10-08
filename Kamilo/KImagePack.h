#pragma once
#include <vector>
#include "KImage.h"

namespace Kamilo {

class KImage;
class KVec3;
class KVec2;

/// 画像以外の情報
struct KImgPackExtraData {
	KImgPackExtraData() {
		page = 0;
		layer = 0;
		blend = -1;
		data0 = 0;
	}

	int page;  /// ページ番号
	int layer; /// レイヤー番号
	int blend; /// KBlend
	int data0;  /// User data
};

/// 画像情報
struct KImgPackItem {
	KImgPackItem() {
		xcells = 0;
		ycells = 0;
		pack_offset = 0;
	}

	std::vector<int> cells;
	KImage img;
	int xcells;
	int ycells;
	int pack_offset;
	KImgPackExtraData extra;
};

class CImgPackWImpl; // internal

/// パック画像を作成する
class KImgPackW {
public:
	KImgPackW(int cellsize=16, bool exclude_dup_cells=false);

	bool empty() const;

	void setCellSize(int cellsize, int cellspace);

	/// パックに画像を追加する
	/// img: 追加する画像
	/// extra: 画像に付随する情報。不要なら NULL を指定する
	void addImage(const KImage &img, const KImgPackExtraData *extra);

	/// パックを保存する
	/// imagefile: 画像ファイル名（getPackedImageで得られた画像を書き込む）
	/// metafile:  テキストファイル名（getMetaStringで得られたテキストを書き込む）
	bool saveToFileNames(const std::string &imagefile, const std::string &metafile) const;

	/// ユーザー定義の拡張データ文字列を追加する
	void setExtraString(const std::string &data);

	/// パッキングされた画像を得る
	KImage getPackedImage() const;

	/// パッキング用画像のために必要な画像サイズを得る
	bool getBestSize(int *w, int *h) const;

	/// メタ情報テキストを得る
	void getMetaString(std::string *p_meta) const;

private:
	std::shared_ptr<CImgPackWImpl> m_Impl;
};


class CImgPackRImpl; // internal



/// パック画像をロードする
class KImgPackR {
public:
	KImgPackR();

	bool empty() const;

	/// KImgPackW で出力したメタファイルの内容テキストを指定して KImgPackR を作成する
	bool loadFromMetaString(const std::string &xml_u8);

	/// KImgPackW で出力したメタファイル名を指定して KImgPackR を作成する
	bool loadFromMetaFileName(const std::string &metafilename);

	/// 画像数を得る
	int getImageCount() const;

	/// インデックスを指定して、その画像の大きさを得る
	void getImageSize(int index, int *w, int *h) const;

	/// インデックスを指定して、その画像の拡張パラメータを得る
	void getImageExtra(int index, KImgPackExtraData *extra) const;

	/// このパック画像に関連付けられたユーザー定義の文字列を得る。
	/// これは CImgPack::setExtraString() で指定されたものと同じ
	const std::string & getExtraString() const;

	/// インデックスを指定して、その画像を得る
	/// pack_img  パックされた画像。（KImgPackW::saveToFileNames で保存した画像）
	/// index     取得したい画像のインデックス番号
	KImage getImage(const KImage &pack_img, int index) const;

	/// index 番目の画像に必要な頂点数を得る (TRIANGLE_LIST)
	int getVertexCount(int index) const;

	/// 頂点座標配列を作成する（ワールド座標系は左下原点のY軸上向き, テクスチャ座標系は左上原点のY軸下向き）
	/// pack_w, pack_h パックされた画像（＝テクスチャ）のサイズ。（KImgPackW::saveToFileNames で保存した画像）
	/// index  番目の画像の座標配列を得る (TRIANGLE_LIST)
	const KVec3 * getPositionArray(int pack_w, int pack_h, int index) const;
	const KVec2 * getTexCoordArray(int pack_w, int pack_h, int index) const;

private:
	std::shared_ptr<CImgPackRImpl> m_Impl;
};

} // namespace
