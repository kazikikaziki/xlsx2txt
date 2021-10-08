/// @file
/// Copyright (c) 2020 Heliodor
/// This software is released under the MIT License.
/// http://opensource.org/licenses/mit-license.php

#pragma once
#include <memory> // std::shared_ptr
#include <vector>
#include "KVideo.h"
#include "KInternal.h"


namespace Kamilo {

class KInputStream;
class KTextureBank;
struct K_Metrics;

static constexpr char *K_FONT_TEXTURE_TAG = "_FONT";

/// フォント１文字の情報。
/// 単位はすべてピクセル
/// <pre>
/// GLYPH METRICS
///
///            |
///            |   left     right
///            |    +---------+ top
///            |    |         |
///            |    |         |
///            |    |         |
///            |    |         |
///  baseline -O----+---------+----A---->
///            |    |         | 
///            |    +---------+ bottom
///            |
///            V
/// O: origin
/// A: next origin
/// advance: A - O
/// </pre>
/// @see KFont
struct KGlyph {
	KGlyph() {
		advance = 0;
		left = top = right = bottom = 0;
		texture = NULL;
		u0 = v0 = u1 = v1 = 0.0f;
	}

	/// 文字AABBの幅（ピクセル単位）
	int w() const { return right - left; }

	/// 文字AABBの高さ（ピクセル単位）
	int h() const { return bottom - top; }

	/// AABB 左端（オリジン基準）
	/// ※オリジン左端を超える可能性 (left < 0) に注意。(Aの左下や V の左上など）
	int left;

	/// AABB 右端（オリジン基準）
	int right;

	/// AABB 上端（オリジン基準、Y軸下向き）
	/// ※ベースライン下側 (top > 0) になる可能性に注意（アンダースコアなど）
	int top;

	/// AABB 下端（オリジン基準、Y軸下向き）
	/// ベースラインよりも上側 (bottom < 0) になる可能性に注意（ハットマーク ^ など）
	int bottom;

	/// 標準の文字送り量
	/// 現在のオリジンから次の文字のオリジンまでの長さ
	float advance;

	/// このグリフに関連付けられたユーザー定義のテクスチャ。
	/// グリフを KImage ではなくテクスチャとして取得することができる。
	KTEXID texture;

	/// テクスチャ上でのグリフ範囲
	float u0, v0, u1, v1;
};


/// TrueType や OpenType フォントをロードするためのクラス
///
/// @see KFontFile フォント自体ではなくフォントファイルを扱いたいときに使う
/// @see KPlatformFonts システムにインストール済みのフォントについての情報を得たい場合に使う
///
/// @snippet KFont.cpp test
class KFont {
public:
	/// 内部で生成したフォントテクスチャを削除する
	/// フォントを再読み込みしたい場合などにはこれを実行しないとテクスチャが古いままになってしまう
	static void globalSetup(KTextureBank *texbank);
	static void globalShutdown();
	static void globalClearGlyphs();

	/// ファイルに含まれているフォント数
	static int getFontCollectionCount(const void *data, int size);

	static KFont createFromStream(KInputStream &input, int ttc_index=0);
	static KFont createFromFileName(const std::string &filename, int ttc_index=0);
	static KFont createFromMemory(const void *data, int size, int ttc_index=0);

public:
	/// TrueType または OpenType フォントに格納されている名前情報の識別番号。
	///
	/// Naming Table
	/// http://www.microsoft.com/typography/otspec/name.htm
	///
	/// True Type Font のフォーマット
	/// https://www48.atwiki.jp/sylx/pages/19.html
	///
	/// @code
	/// // 使用例（ＭＳ ゴシック）
	/// KFont *font = KFont::createFromFileName("c:\\windows\\fonts\\msgothic.ttc", 0);
	/// font->getNameInJapanese(KFont::NID_FAMILY, buf, bufsize);
	///
	/// // 結果例（ＭＳ ゴシック）
	/// NID_COPYRIGHT : "(c) 2017 data:株式会社リコー typeface:リョービイマジクス(株)" 
	/// NID_FAMILY    : "ＭＳ ゴシック"
	/// NID_SUBFAMILY : "標準"
	/// NID_FONTID    : "Microsoft:ＭＳ ゴシック"
	/// NID_FULLNAME  : "ＭＳ ゴシック"
	/// NID_VERSION   : "Version 5.11"
	/// NID_POSTSCRIPT: "MS-Gothic"
	/// NID_TRADEMARK : "ＭＳ ゴシックは米国 Microsoft Corporation の米国及びその他の国の登録商標または商標です。"
	/// @endcode
	enum NameId {
		NID_COPYRIGHT  = 0, ///< "(c) 2017 data:株式会社リコー typeface:リョービイマジクス(株)"
		NID_FAMILY     = 1, ///< "ＭＳ ゴシック"
		NID_SUBFAMILY  = 2, ///< "標準"
		NID_FONTID     = 3, ///< "Microsoft:ＭＳ ゴシック"
		NID_FULLNAME   = 4, ///< "ＭＳ ゴシック"
		NID_VERSION    = 5, ///< "Version 5.11"
		NID_POSTSCRIPT = 6, ///< "MS-Gothic"
		NID_TRADEMARK  = 7, ///< "ＭＳ ゴシックは米国 Microsoft Corporation の米国及びその他の国の登録商標または商標です。"
	};
	enum Style {
		STYLE_NONE       = 0,
		STYLE_BOLD       = 1,
		STYLE_ITALIC     = 2,
		STYLE_UNDERSCORE = 4,
	};
	typedef int TrueTypeFlags; ///< #KTrueTypeFlag の組み合わせ

	class Impl {
	public:
		virtual ~Impl() {}

		/// chr で指定した文字の、サイズ fontsize における字形を img に書き込む。
		/// img のフォーマットは KColorFormat_INDEX8（１ピクセル＝１バイト）のみ対応する。
		/// ビットマップフォントの場合 fontsize で指定したサイズのグリフが得られるとは限らない。
		/// 指定された文字を含んでいない場合は何もせずに false を返す
		/// glyph, image は NULL でもよい
		virtual bool get_glyph(wchar_t chr, float fontsize, K_Metrics *met, std::string *out_alpha8) const = 0;

		/// 文字 chr1 と chr2 を並べるときの文字送り調整量をピクセル単位で返す。
		/// 調整量とは、chr1 の文字送り慮う advance から、さらにプラスする値のこと。
		/// この値は 0 なら標準文字送り量のままで、正の値なら広くなり、負の値なら狭くなる。
		/// 文字送り調整量はフォントサイズ fontsize に比例する
		virtual float get_kerning(wchar_t chr1, wchar_t chr2, float fontsize) const { return 0.0f; }

		/// nid で指定された情報文字列を得る。文字列が存在しないか未対応の場合は false を返す
		/// lang_id = Locale ID = LCID (K_LCID_ENGLISH, K_LCID_JAPANESE)
		virtual bool get_name_string(KFont::NameId nid, K_LCID lang_id, char *out_u8, int out_size) const { return false; }

		/// フォントスタイル
		virtual bool get_style(bool *b, bool *i, bool *u) const { return false; }
	};

	KFont();
	explicit KFont(Impl *impl);
	bool isOpen() const;
	bool operator == (const KFont &other) const;

	/// フォントファイルを指定してフォントをロードする
	/// @param file      ファイル
	/// @param ttc_index ロードするフォントのインデックス
	bool loadFromStream(KInputStream &input, int ttc_index=0);

	/// フォントファイル名を指定してフォントをロードする
	/// @param filename  フォントファイル名
	/// @param ttc_index ロードするフォントのインデックス
	bool loadFromFileName(const std::string &filename, int ttc_index=0);

	/// フォントファイルのバイナリを指定してフォントをロードする
	/// @param data      フォントファイルのバイナリデータ
	/// @param size      バイナリデータのバイト数
	/// @param ttc_index ロードするフォントのインデックス
	bool loadFromMemory(const void *data, int size, int ttc_index=0);

	bool empty() const;
	int id() const;

	/// フォントフラグを取得する
	TrueTypeFlags getFlags() const;

	/// TrueType または OpenType フォントに格納されている英語の名前情報を name にセットして true を返す。
	/// 名前が取得できなかった場合は何もせずに false を返す。
	/// 比較的長い文字列が格納されている場合もあるので注意する事。
	/// 例えば Arial (arial.ttc) の CopyRight 表記は複数行から成り、改行文字を含めて542バイト271文字ある
	bool getNameInEnglish(NameId nid, char *out_u8, int out_size) const;

	/// TrueType または OpenType フォントに格納されている日本語の名前情報を返す
	bool getNameInJapanese(NameId nid, char *out_u8, int out_size) const;

	/// chr で指定した文字のグリフを得る
	///
	/// @param chr 文字
	/// @param fontsize    フォントサイズ（ピクセル単位でのフォント高さ）
	/// @param style       得られる文字画像のスタイル
	/// @param with_alpha  文字の形をアルファチャンネルで表現するかどうか。<br>
	///                    true を指定するとアルファ値だけを使って文字を表現する。その場合 RGB は全て primary の色になる。<br>
	///                    false を指定するとアルファを使わず、単色で文字を表現する。その時の色は primary で、アルファ値は全ての点で255になる。
	///
	/// @note
	/// このメソッドの戻り値 KGlyph の textur フィールドは通常 NULL のままである。
	/// KGlyph::texture を使いたい場合は KUserFontTextureCallback を実装して KFont::setUserTextureCallback で登録しておく必要がある
	KGlyph getGlyph(wchar_t chr, float fontsize, KFont::Style style=STYLE_NONE, bool with_alpha=true) const;

	/// chr で指定した文字のグリフを得る
	///
	/// 各引数の意味は KFont::getGlyph を参照すること。<br>
	/// img のビット深度は 8 固定つまり１ピクセル＝１バイトとして書き込む。
	/// ビットマップフォントの場合 fontsize で指定したサイズのグリフが得られるとは限らない。
	/// 指定された文字を含んでいない場合は空の KImage を返す
	///
	/// @note
	/// KImage ではなくテクスチャ画像としてグリフを取得したい場合は KFont::getGlyph で得られた KGlyph の texture フィールドを使う。
	/// ただし、事前に KUserFontTextureCallback を実装し KFont::setUserTextureCallback で登録しておく必要がある
	KImage getGlyphImage32(wchar_t chr, float fontsize, KFont::Style style=STYLE_NONE, bool with_alpha=true) const;

	/// getGlyphImage32 と同じだが、単色 (256階調グレースケール）で画像を得る
	KImage getGlyphImage8(wchar_t chr, float fontsize) const;

	/// 文字 chr1 と chr2 を並べるときの文字送り調整量をピクセル単位で返す。
	/// chr1 の FontGlyph::advance で指定された文字送り量から、さらに追加するピクセル数を返す。
	/// カーニングしない場合は 0 を返し、文字間を詰める必要があるなら負の値を返す。
	/// 文字送り調整量はフォントサイズ height_in_pixel に比例する
	float getKerningAdvanve(wchar_t chr1, wchar_t chr2, float fontsize) const;

private:
	std::shared_ptr<Impl> m_Impl;
};




/// システムにインストール済みのフォント情報を得る
/// @snippet KPlatformFonts.cpp test
class KPlatformFonts {
public:
	struct INFO {
		std::string family;          ///< フォントファミリー名（"ＭＳ ゴシック" "Arial"）
		std::string filename;        ///< フォントファイル名（"arial.ttf")
		KFont::TrueTypeFlags tt_flags; ///< スタイルフラグ
		int ttc_index;        ///< フォントファイルが複数のフォントを含んでいる場合、そのインデックス

		INFO() {
			tt_flags = KFont::STYLE_NONE;
			ttc_index = 0;
		}
	};
	static std::string getFontDirectory();
	void scan();
	int size() const;
	const INFO & operator[](int index) const;

private:
	std::vector<INFO> m_list;
};

namespace Test {
void Test_font(const std::string &dir);
void Test_font_ex(const std::string &font_filename, float fontsize, const std::string &output_image_filename);
void Test_font_printInfo(const std::string &output_dir, const std::string &fontpath);
void Test_fontscan(const std::string &output_dir);
}

} // namespace
