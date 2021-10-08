#include "KFont.h"

// Use stb_truetype
#define K_USE_STB_TRUETYPE

#include <algorithm>
#include <Shlobj.h> // SHGetFolderPath
#include "KRes.h"
#include "KInternal.h"


#ifdef K_USE_STB_TRUETYPE
	// stb_truetype
	// https://github.com/nothings/stb
	// license: public domain
	#define STB_TRUETYPE_IMPLEMENTATION
	#include "stb_truetype.h"
#endif // K_USE_STB_TRUETYPE



namespace Kamilo {


struct K_Metrics {
	float advance;
	float left;
	float top; // ベースラインを基準にしたときの文字の上端
	float width;
	float height; // 文字全体の高さ。Y軸下向きなので top + height が下端になる
};

class CNanoFontImpl: public KFont::Impl {
public:
	static const int COL_COUNT = 16;
	static const int ROW_COUNT = 6;
	static const int CHAR_W    = 6;
	static const int CHAR_H    = 9;
	static const int BASELINE  = 6;
	static const int BMP_W     = CHAR_W * COL_COUNT;
	static const int BMP_H     = CHAR_H * 6;
	
	CNanoFontImpl() {
	}
	static const char * get_table_static() {
		static const char *data =
			" !\"#$%&'()*+,-./"
			"0123456789:;<=>?"
			"@ABCDEFGHIJKLMNO"
			"PQRSTUVWXYZ[\\]^_"
			" abcdefghijklmno"
			"pqrstuvwxyz{|}~ ";
		return data;
	}
	static const char * get_bitmap_static() {
		static const char * data =
			//     6     12    18    24    30    36    42    48    54    60    66    72    78    84    90    96
			//-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+
			"        #    # #   # #   ####     #   ##    #      #   #                                        "
			"        #    # #   # #  # #   ## #   #      #     #     #   # # #   #                         # "
			"        #    # #  ##### # #   ## #   #     #      #     #    ###    #                        #  "
			"        #          # #   ###    #    #  #         #     #     #   #####       #####         #   "
			"        #          # #    # #   #   # # #         #     #    ###    #                      #    "
			"                  #####   # #  # ## #  #          #     #   # # #   #    ##          ##   #     "
			"        #          # #  ####  #  ##  ## #          #   #                 ##          ##         " // <== baseline
			"                                                                          #                     "
			"                                                                         #                      "
			//-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+
			" ###   ##    ###   ###  #   # ####   ##   #####  ###   ###                 #         #     ###  "
			"#   #   #   #   # #   # #   # #     #     #   # #   # #   #  ##   ##      #           #   #   # "
			"#  ##   #       #     # #   # #     #         # #   # #   #  ##   ##     #    #####    #      # "
			"# # #   #     ##   ###  ##### ####  ####     #   ###   ####             #               #   ##  "
			"##  #   #    #        #     #     # #   #    #  #   #     #              #    #####    #    #   "
			"#   #   #   #     #   #     #     # #   #   #   #   #     #  ##   ##      #           #         "
			" ###   ###  #####  ###      #  ###   ###    #    ###    ##   ##   ##       #         #      #   " // <== baseline
			"                                                                   #                            "
			"                                                                  #                             "
			//-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+
			" ###   ###  ####   ###  ####  ##### #####  ###  #   # #####  #### #   # #     #   # #   #  ###  "
			"#   # #   # #   # #   # #   # #     #     #     #   #   #      #  #  #  #     ## ## #   # #   # "
			"# ### #   # #   # #     #   # #     #     #     #   #   #      #  #  #  #     ## ## ##  # #   # "
			"# # # #   # ####  #     #   # ####  ####  # ### #####   #      #  ###   #     # # # # # # #   # "
			"# ### ##### #   # #     #   # #     #     #   # #   #   #      #  #  #  #     # # # #  ## #   # "
			"#     #   # #   # #   # #   # #     #     #   # #   #   #      #  #  #  #     #   # #   # #   # "
			" #### #   # ####   ###  ####  ##### #      #### #   # ##### ###   #   # ##### #   # #   #  ###  " // <== baseline
			"                                                                                                "
			"                                                                                                "
			//-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+
			"####   ###  ####   ###  ##### #   # #   # #   # #   # #   # #####   ### #   # ###     #         "
			"#   # #   # #   # #   #   #   #   # #   # #   #  # #  #   #     #   #    # #    #    # #        "
			"#   # #   # #   # #       #   #   # #   # # # #  # #  #   #    #    #     #     #   #   #       "
			"####  # # # ####   ###    #   #   # #   # # # #   #    ###    #     #   #####   #               "
			"#     # # # #  #      #   #   #   #  # #  # # #  # #    #    #      #     #     #               "
			"#     #  #  #  #  #   #   #   #   #  # #  # # #  # #    #   #       #   #####   #               "
			"#      ## # #   #  ###    #    ###    #    # #  #   #   #   #####   ###   #   ###         ##### " // <== baseline
			"                                                                                                "
			"                                                                                                "
			//-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+
			"            #               #          ##       #       #      #  #     ###                     "
			"            #               #         #         #                 #       #                     "
			"       ###  ####   ####  ####  ###  #####  #### # ##  ###     ##  #   #   #   ####  ####   ###  "
			"          # #   # #     #   # #   #   #   #   # ##  #   #      #  # ##    #   # # # #   # #   # "
			"       #### #   # #     #   # #####   #   #   # #   #   #      #  ##      #   # # # #   # #   # "
			"      #   # #   # #     #   # #       #   #   # #   #   #      #  # ##    #   # # # #   # #   # "
			"       #### ####   ####  ####  ###    #    #### #   # #####    #  #   ##  ### # # # #   #  ###  " // <== baseline
			"                                              #             #  #                                "
			"                                           ###               ##                                 "
			//-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+
			"                          #                                          ##   #   ##     ##  #      "
			"                          #                                         #     #     #   #  ##       "
			"####   #### # ###  ###  ##### #   # #   # # # # #   # #   # #####   #     #     #               "
			"#   # #   # ##    #       #   #   # #   # # # #  # #  #   #    #  ##      #      ##             "
			"#   # #   # #      ###    #   #   #  # #  # # #   #    ####   #     #     #     #               "
			"#   # #   # #         #   #   #   #  # #  # # #  # #      #  #      #     #     #               "
			"####   #### #     ####     ##  ####   #    # #  #   #  ###  #####    ##   #   ##                " // <== baseline
			"#         #                                                                                     "
			"#         #                                                                                     "
			//-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+
		; // <== end of data
		K__ASSERT(sizeof(data) == BMP_W * BMP_H);
		return data;
	}

	virtual bool get_glyph(wchar_t chr, float fontsize, K_Metrics *met, std::string *out_alpha8) const override {

		// 文字テーブルから目的の文字を見つける。
		// 見つからなければ失敗
		const char *p = strchr(get_table_static(), (int)chr);
		if (p == nullptr) {
			return false;
		}
		
		if (met) {
			met->advance = CHAR_W;
			met->left    = 0; // ベースライン左端を起点とする文字範囲
			met->top     = BASELINE; // 文字範囲の上端。ベースライン基準のＹ軸上向きにするため、ビットマップのＹ座標を反転させる
			met->width   = CHAR_W;
			met->height  = CHAR_H;
		}

		if (out_alpha8) {
			// ビットマップ情報
			const char *glyph_bitmap = get_bitmap_static();

			// 画像データを作成する
			out_alpha8->resize(CHAR_W * CHAR_H);
			for (int y=0; y<CHAR_H; y++) {
				for (int x=0; x<CHAR_W; x++) {
					char c = glyph_bitmap[BMP_W * y + x];
					int idx = CHAR_W * y + x;
					uint8_t val = (c == '#') ? 0xFF : 0x00;
					(*out_alpha8)[idx] = val; // A
				}
			}
		}
		return true;
	}
};

#ifdef K_USE_STB_TRUETYPE
class CStbFontImpl: public KFont::Impl {
public:
	std::string bin_;
	stbtt_fontinfo info_;

	CStbFontImpl(const void *data, int size, int ttc_index, int *err) {
		K__ASSERT(data && size > 0);
		K__ASSERT(err);
		memset(&info_, 0, sizeof(info_));
		bin_.resize(size);
		memcpy(&bin_[0], data, size);
		int offset = stbtt_GetFontOffsetForIndex((unsigned char *)bin_.data(), ttc_index);
		if (offset < 0) {
			K__ERROR("CStbFont: no TTC data indexed '%d'", ttc_index);
			*err = 1;
			return;
		}
		if (!stbtt_InitFont(&info_, (unsigned char *)bin_.data(), offset)) {
			// フォントを解析できない。
			// ちなみに stbtt は wingding 等の非文字フォントには対応していない（文字コード不明なので）
			K__ERROR("CStbFont: failed to stbtt_InitFont");
			*err = 1;
			return;
		}
		*err = 0;
	}

	/// TrueType に埋め込まれている文字列を得る
	/// nid 名前番号
	/// langID 言語番号
	///     1033 (0x0409) = 米国英語
	///     1041 (0x0411) = 日本語
	///     ロケール ID (LCID) の一覧
	///     "https://docs.microsoft.com/ja-jp/previous-versions/windows/scripting/cc392381(v=msdn.10)"
	///
	bool get_name_string(KFont::NameId nid, K_LCID lang_id, char *out_u8, int out_size) const {
		K__ASSERT(out_u8);
		int name_bytes = 0;

		const int WSIZE = 1024; // 著作権表示など、結構長いテキストが入っている場合があるので多めにとっておく
		wchar_t ws[WSIZE] = {0};

		// フォントに埋め込まれている文字列を得る。
		// この文字列は2バイトのビッグエンディアンでエンコードされている
		const char *name_u16be = stbtt_GetFontNameString(
			&info_,
			&name_bytes,
			STBTT_PLATFORM_ID_MICROSOFT,
			STBTT_MS_EID_UNICODE_BMP,
			lang_id,
			nid
		);
		const int name_len = name_bytes / sizeof(uint16_t);
		if (name_len+1 >= WSIZE) {
			K__ERROR("Font name string buffer has not enough size");
			return false;
		}
		// ビッグエンディアンで得られた文字列をリトルエンディアンに変換する
		if (name_u16be && name_bytes > 0) {
			for (int i=0; i<name_len; i++) {
				uint8_t hi = name_u16be[i*sizeof(uint16_t)  ];
				uint8_t lo = name_u16be[i*sizeof(uint16_t)+1];
				ws[i] = (hi << 8) | lo;
			}
			ws[name_len] = '\0';
		}
		K::strWideToUtf8(out_u8, out_size, ws);
		return name_bytes > 0;
	}

	virtual bool get_glyph(wchar_t chr, float fontsize, K_Metrics *met, std::string *out_alpha8) const override {
		if (!K::str_iswprint(chr)) {
			// 印字可能ではない（改行文字、制御文字など）
			// ※空白文字は「印字可能」であることに注意
			return false;
		}

		// フォントデザイン単位からピクセルへの変換係数
		const float scale = stbtt_ScaleForPixelHeight(&info_, (float)fontsize);
		if (scale <= 0) return false;

		// フォントデザイン単位(Design Units)と座標系でのメトリクス (Ｙ軸上向き、原点はデザイナーに依存）
		int du_ascent;  // 標準文字の上端
		int du_descent; // 標準文字の下端
		int du_linegap; // 文字下端から行の下端（＝次行の上端）までのスペース
		stbtt_GetFontVMetrics(&info_, &du_ascent, &du_descent, &du_linegap);

		int du_advance;
		int du_bearing_left;
		stbtt_GetCodepointHMetrics(&info_, chr, &du_advance, &du_bearing_left);

		// 文字ビットマップ座標系でのメトリクス（ベースライン左端を原点、Y軸下向き）
		int bmp_left;   // 文字AABBの左端。du_bering_left * unit_scale と同じ
		int bmp_right;  // 文字AABBの右端
		int bmp_top;    // 文字AABBの上端。Y軸下向きなので、ベースラインよりも高ければ負の値になる
		int bmp_bottom; // 文字AABBの下端。Y軸下向きなので、ベースラインよりも低ければ正の値になる
		stbtt_GetCodepointBitmapBoxSubpixel(&info_, chr, scale, scale, 0, 0, &bmp_left, &bmp_top, &bmp_right, &bmp_bottom);
		K__ASSERT(bmp_left <= bmp_right);
		K__ASSERT(bmp_top <= bmp_bottom);

		if (met) {
			met->advance = (float)du_advance * scale;
			met->left   = (float)bmp_left;
			met->top    = (float)bmp_top;
			met->width  = (float)(bmp_right - bmp_left);
			met->height = (float)(bmp_bottom - bmp_top);
		}
		if (out_alpha8) {
			if (stbtt_IsGlyphEmpty(&info_, stbtt_FindGlyphIndex(&info_, chr))) {
				// 字形が存在しない(空白文字や制御文字)
				// ちなみに字形が存在しなくても、空白文字の場合はちゃんと文字送り量 advance が指定されているので、無視してはいけない
			} else {
				int w = bmp_right - bmp_left;
				int h = bmp_bottom - bmp_top;
				K__ASSERT(w > 0 && h > 0);
				out_alpha8->resize(w * h);
				stbtt_MakeCodepointBitmapSubpixel(&info_, (unsigned char*)out_alpha8->data(), w, h, w, scale, scale, 0, 0, chr);
			}
		}
		return true;
	}
	virtual float get_kerning(wchar_t chr1, wchar_t chr2, float fontsize) const override {
		float scale = stbtt_ScaleForPixelHeight(&info_, fontsize);
		float kern = (float)stbtt_GetCodepointKernAdvance(&info_, chr1, chr2);
		return kern * scale;
	}
	virtual bool get_style(bool *b, bool *i, bool *u) const override { 
		// 注意
		// K_stbtt_GetFlags は独自に追加した関数であり、stb_truetype には元々含まれていない。
		// フォントスタイルが Bold か Italic か、などはフラグに保持されているが、
		// stb_truetype にはフラグを単独で取得する関数が用意されていなかった。
		// 比較する関数 (stbtt_FindMatchingFont) ならあるが、これはフラグのほかに名前も比較してしまうため、
		// それを元にフラグだけを返す関数を追加してある。
		// 定義は以下の通り: フラグは STBTT_MACSTYLE_*** で調べる
		//	STBTT_DEF int K_stbtt_GetFlags(stbtt_fontinfo *info) {
		//		return ttSHORT(info->data + info->head + 44);
		//	}
		int flags = K_stbtt_GetFlags(&info_);
		if (b) *b = (flags & STBTT_MACSTYLE_BOLD) != 0;
		if (i) *i = (flags & STBTT_MACSTYLE_ITALIC) != 0;
		if (u) *u = (flags & STBTT_MACSTYLE_UNDERSCORE) != 0;
		return flags;
	}
};
#endif // K_USE_STB_TRUETYPE



#pragma region Font.cpp

class CGlyphPack {
public:
	static const int MARGIN = 1;
	static const int TEXSIZE = 1024 * 2;
	
	struct _PAGE {
		KTEXID tex;
		KImage img;
		int ymax;
		int xcur, ycur;

		_PAGE() {
			tex = nullptr;
			ymax = xcur = ycur = 0;
		}
	};

	std::vector<_PAGE> pages_;
	std::unordered_map<std::string, KGlyph> table_;
	KTextureBank *mTexBank;
	
	CGlyphPack() {
		m_numtex = 0;
		mTexBank = nullptr;
	}
	void setup(KTextureBank *texbank) {
		clearGlyphs();
		mTexBank = texbank;
	}

	void clearGlyphs() {
		pages_.clear();
		table_.clear();
	}

	// グリフを得る。未登録の場合は fales を返す
	// font フォント
	// chr 文字
	// fontsize ピクセル単位のフォントサイズ
	// style フォントの種類
	// with_alpha アルファチャンネル付きなら true グレースケール表現なら false
	bool getGlyph(const KFont *font, wchar_t chr, float fontsize, KFont::Style style, bool with_alpha, KGlyph *glyph) const {
		std::string key = make_key(font, chr, fontsize, style, with_alpha);
		return get_item(key, glyph);
	}

	// グリフを追加する
	bool addGlyph(const KFont *font, wchar_t chr, float fontsize, KFont::Style style, bool with_alpha, const KImage &img32, KGlyph &glyph) {
		std::string key = make_key(font, chr, fontsize, style, with_alpha);
		return add_item(key, img32, glyph);
	}

private:
	static std::string make_key(const KFont *font, wchar_t chr, float fontsize, KFont::Style style, bool with_alpha) {
		K__ASSERT(font);
		int sizekey = (int)(fontsize * 10); // 0.1刻みで識別できるようにしておく

		char s[256];
		sprintf_s(s, sizeof(s), "%x|%d|%d|%d|%d", font->id(), chr, sizekey, style, with_alpha);
		return s;
	}

	bool get_item(const std::string &key, KGlyph *glyph) const {
		auto it = table_.find(key);
		if (it != table_.end()) {
			*glyph = it->second;
			return true;
		}
		return false;
	}

	/// 画像 image の内容をテクスチャー tex にコピーする。
	/// image のサイズは tex と同じであると仮定してよい
	void on_update_texture(KTEXID texid, const KImage *image) {
		KTexture *tex = KVideo::findTexture(texid);
		if (tex) {
			void *ptr = tex->lockData();
			if (ptr) {
				memcpy(ptr, image->getData(), image->getDataSize());
				tex->unlockData();
			}
		}
	}

	/// サイズ (w, h) の新しいテクスチャーを作成する
	KTEXID on_new_texture(int w, int h) {
		KTEXID ret = nullptr;
		if (mTexBank) {
			std::string texname = K::str_sprintf("__font%04d", m_numtex);
			m_numtex++;
			ret = mTexBank->addTextureFromSize(texname, w, h);

			// フォント用のテクスチャであることが分かるように、タグをつけておく
			KBank::getTextureBank()->setTag(ret, KName(K_FONT_TEXTURE_TAG));

			// 保護しておく
			KBank::getTextureBank()->setProtect(ret, true);
		}
		return ret;
	}

	/// glyph にはテクスチャフィールド以外が埋まったものを指定する。
	/// テクスチャへの追加が成功すれば、テクスチャとUV座標フィールドに値をセットして true を返す
	bool add_item(const std::string &key, const KImage &img32, KGlyph &glyph) {
		// img32 は余白なしのグリフ画像
		int w = img32.getWidth();
		int h = img32.getHeight();
		_PAGE *page = get_page(w, h);
		if (page) {
			page->img.copyImage(page->xcur, page->ycur, img32); // ページに文字画像を書き込む
			page->ymax = KMath::max(page->ymax, page->ycur + h + MARGIN); // AABB更新
			on_update_texture(page->tex, &page->img);
			glyph.texture = page->tex;
			glyph.u0 = (float)(page->xcur + 0) / TEXSIZE;
			glyph.v0 = (float)(page->ycur + 0) / TEXSIZE;
			glyph.u1 = (float)(page->xcur + w) / TEXSIZE;
			glyph.v1 = (float)(page->ycur + h) / TEXSIZE;
			page->xcur += w + MARGIN;
			table_[key] = glyph;
			return true;
		}
		return false;
	}
	// サイズ w,　h の画像の追加先を得る
	_PAGE * get_page(int w, int h) {
		if (pages_.size() > 0) {
			_PAGE *page = &pages_.back();
			// 現ページの書き込み位置にそのまま収まる？
			{
				int wspace = page->img.getWidth()  - page->xcur;
				int hspace = page->img.getHeight() - page->ycur;
				if (wspace >= w && hspace >= h) {
					return page;
				}
			}
			// 改行
			page->xcur = 0;
			page->ycur = page->ymax;
			// この状態でもう一度確認
			{
				int wspace = page->img.getWidth()  - page->xcur;
				int hspace = page->img.getHeight() - page->ycur;
				if (wspace >= w && hspace >= h) {
					return page;
				}
			}
		}

		// 新規ページを追加
		{
			_PAGE page;
			page.img = KImage::createFromSize(TEXSIZE, TEXSIZE);
			page.tex = on_new_texture(TEXSIZE, TEXSIZE);
			page.xcur = 0;
			page.ycur = 0;
			page.ymax = 0;
			pages_.push_back(page);
			return &pages_.back();
		}
		return nullptr;
	}

	int m_numtex;
};


static CGlyphPack g_FontAtlas;


static std::string kk_GetGlyphKey(const KFont *font, wchar_t chr, float fontsize, KFont::Style style, bool with_alpha) {
	K__ASSERT(font);
	int sizekey = (int)(fontsize * 10); // 0.1刻みで識別できるようにしておく

	char s[256];
	sprintf_s(s, sizeof(s), "%d|%d|%d|%d|%d", font->id(), chr, sizekey, style, with_alpha);
	return s;
}

#pragma region KFont
KFont::KFont() {
	m_Impl = nullptr;
}
KFont::KFont(Impl *impl) {
	if (impl) {
		m_Impl = std::shared_ptr<Impl>(impl);
	} else {
		m_Impl = nullptr;
	}
}
bool KFont::isOpen() const {
	return m_Impl != nullptr;
}
bool KFont::operator == (const KFont &other) const {
	return m_Impl == other.m_Impl;
}
bool KFont::empty() const {
	return m_Impl == nullptr;
}
int KFont::id() const {
	return (int)m_Impl.get();
}
bool KFont::loadFromStream(KInputStream &input, int ttc_index) {
	m_Impl = nullptr;
	std::string bin = input.readBin();
	if (bin.empty()) {
		K__ERROR("");
		return false;
	}
#ifdef K_USE_STB_TRUETYPE
	int err = 0;
	Impl *impl = new CStbFontImpl(bin.data(), bin.size(), ttc_index, &err);
	if (err) {
		K__ERROR("");
		delete impl;
		return false;
	}
#else
	Impl *impl = new CNanoFontImpl();
#endif
	m_Impl = std::shared_ptr<Impl>(impl);
	return true;
}
bool KFont::loadFromFileName(const std::string &filename, int ttc_index) {
	KInputStream input = KInputStream::fromFileName(filename);
	return loadFromStream(input, ttc_index);
}
bool KFont::loadFromMemory(const void *data, int size, int ttc_index) {
	KInputStream input = KInputStream::fromMemory(data, size);
	return loadFromStream(input, ttc_index);
}












#define OUTINE_WIDTH 1
#define BOLD_INFLATE 2

/// 文字画像 (Alpha8) をRGB32に転写したグレースケール不透明画像を作成する
static KImage Glyph_toColor(const KImage *raw) {
	if (raw==nullptr || raw->empty()) return KImage();
	int w = raw->getWidth();
	int h = raw->getHeight();
	KImage img = KImage::createFromSize(w, h);
	const uint8_t *src = raw->getData();
	uint8_t *dst = img.lockData();
	for (int y=0; y<h; y++) {
		for (int x=0; x<w; x++) {
			int i = w * y + x;
			dst[i*4+0] = src[i]; // R
			dst[i*4+1] = src[i]; // G
			dst[i*4+2] = src[i]; // B
			dst[i*4+3] = 255; // A
		}
	}
	img.unlockData();
	return img;
}

/// 文字画像 (Alpha8) をRGB32に転写したアルファチャンネル画像を作成する
static KImage Glyph_toAlpha(const KImage *raw) {
	if (raw==nullptr || raw->empty()) return KImage();
	int w = raw->getWidth();
	int h = raw->getHeight();
	KImage img = KImage::createFromSize(w, h);
	const uint8_t *src = raw->getData();
	uint8_t *dst = img.lockData();
	for (int y=0; y<h; y++) {
		for (int x=0; x<w; x++) {
			int i = w * y + x;
			dst[i*4+0] = 255; // R
			dst[i*4+1] = 255; // G
			dst[i*4+2] = 255; // B
			dst[i*4+3] = src[i]; // A
		}
	}
	img.unlockData();
	return img;
}

inline uint8_t kk_uint8_add(uint8_t a, uint8_t b) {
#if 0
	// ADD
	int val = (int)a + (int)b;
#else
	// SCREEN
	int val = (int)a + (int)(255 - a) * b / 255;
#endif
	if (val < 255) {
		return (uint8_t)val;
	} else {
		return 255;
	}
}

/// 文字画像 (Alpha8) を右に1ドット太らせる
static KImage Glyph_bold(const KImage *raw) {
	if (raw==nullptr || raw->empty()) return KImage();
	K__ASSERT(raw->getFormat() == KColorFormat_INDEX8);
	int sw = raw->getWidth();
	int dw = sw + BOLD_INFLATE;
	int h = raw->getHeight();
	KImage img = KImage::createFromPixels(dw, h, KColorFormat_INDEX8, nullptr);
	const uint8_t *src = raw->getData();
	uint8_t *dst = img.lockData();
	switch (BOLD_INFLATE) {
	case 1:
		for (int y=0; y<h; y++) {
			for (int x=0; x<sw; x++) {
				int si = sw * y + x;
				int di = dw * y + x;
				dst[di  ] = kk_uint8_add(dst[di],   src[si]);
				dst[di+1] = kk_uint8_add(dst[di+1], src[si]);
			}
		}
		break;
	case 2:
		for (int y=0; y<h; y++) {
			for (int x=0; x<sw; x++) {
				int si = sw * y + x;
				int di = dw * y + x;
				dst[di  ] = kk_uint8_add(dst[di],   src[si]);
				dst[di+1] = kk_uint8_add(dst[di+1], src[si]);
				dst[di+2] = kk_uint8_add(dst[di+2], src[si]);
			}
		}
		break;
	}
	img.unlockData();
	return img;
}

/// 文字画像 (Alpha8) を上下左右に1ドット太らせる
static KImage Glyph_inflate(const KImage *raw) {
	if (raw==nullptr || raw->empty()) return KImage();
	K__ASSERT(raw->getFormat() == KColorFormat_INDEX8);
	int w = raw->getWidth();
	int h = raw->getHeight();
	KImage img = KImage::createFromPixels(w+2, h+2, KColorFormat_INDEX8, nullptr);
	const uint8_t *src = raw->getData();
	uint8_t *dst = img.lockData();
	int sw = w;
	int dw = w + 2;
	for (int y=0; y<h; y++) {
		for (int x=0; x<w; x++) {
			int si = sw * y + x;
			int dx = 1 + x;
			int dy = 1 + y;
			int di = dw * dy + dx;
			uint8_t src_v = src[si];
			di = dw * (dy-1) + dx;
			dst[di-1] = KMath::max(dst[di-1], src_v);
			dst[di  ] = KMath::max(dst[di  ], src_v);
			dst[di+1] = KMath::max(dst[di+1], src_v);
			di = dw * (dy  ) + dx;
			dst[di-1] = KMath::max(dst[di-1], src_v);
			dst[di  ] = KMath::max(dst[di  ], src_v);
			dst[di+1] = KMath::max(dst[di+1], src_v);
			di = dw * (dy+1) + dx;
			dst[di-1] = KMath::max(dst[di-1], src_v);
			dst[di  ] = KMath::max(dst[di  ], src_v);
			dst[di+1] = KMath::max(dst[di+1], src_v);
		}
	}
	img.unlockData();
	return img;
}

/// 文字画像 (Alpha8) から val を引く
static KImage Glyph_sub(const KImage *raw, int px, int py, const KImage *raw2) {
	if (raw==nullptr || raw->empty())  return KImage();
	if (raw2==nullptr || raw2->empty()) return *raw;

	K__ASSERT(raw->getFormat() == KColorFormat_INDEX8);
	K__ASSERT(raw2->getFormat()== KColorFormat_INDEX8);

	// rawをコピー
	KImage img = raw->clone();

	// raw2 と重なる部分だけ演算する
	const uint8_t *rawbuf = raw2->getData();
	uint8_t *imgbuf = img.lockData();
	int dw = raw->getWidth();
	int sw = raw2->getWidth();
	int sh = raw2->getHeight();
	for (int y=0; y<sh; y++) {
		for (int x=0; x<sw; x++) {
			int dx = px + x;
			int dy = py + y;
			int di = dw * dy + dx;
			int si = sw *  y +  x;
			if (imgbuf[di] > rawbuf[si]) {
				imgbuf[di] -= rawbuf[si];
			} else {
				imgbuf[di] = 0;
			}
		}
	}
	img.unlockData();
	return img;
}

class BlendOutlineFilled {
public:
	KColor32 operator()(const KColor32 &dst, const KColor32 &src) const {
		KColor32 out;
		int inv_a = 255 - src.a;
		out.r = (uint8_t)((int)(dst.r * inv_a + src.r * src.a) / 255);
		out.g = (uint8_t)((int)(dst.g * inv_a + src.g * src.a) / 255);
		out.b = (uint8_t)((int)(dst.b * inv_a + src.b * src.a) / 255);
		out.a = dst.a;
		return out;
	}
};

KImage KFont::getGlyphImage8(wchar_t chr, float fontsize) const {
	KImage img8;
	if (m_Impl) {
		K_Metrics met;
		std::string alphamap8;
		if (m_Impl->get_glyph(chr, fontsize, &met, &alphamap8)) {
			img8 = KImage::createFromPixels((int)met.width, (int)met.height, KColorFormat_INDEX8, alphamap8.data());
		}
	}
	return img8;
}
KImage KFont::getGlyphImage32(wchar_t chr, float fontsize, KFont::Style style, bool with_alpha) const {
	if (m_Impl == nullptr) return KImage();

	switch (style) {
	case KFont::STYLE_NONE:
		{
			KImage glyph_u8 = getGlyphImage8(chr, fontsize); // 基本グリフ
			if (with_alpha) {
				return Glyph_toAlpha(&glyph_u8);
			} else {
				return Glyph_toColor(&glyph_u8);
			}
		}
	case KFont::STYLE_BOLD:
		{
			KImage glyph_u8 = getGlyphImage8(chr, fontsize); // 基本グリフ
			KImage bold_u8 = Glyph_bold(&glyph_u8);
			if (with_alpha) {
				return Glyph_toAlpha(&bold_u8);
			} else {
				return Glyph_toColor(&bold_u8);
			}
		}
	default:
		{
			return KImage();
		}
	}
}
float KFont::getKerningAdvanve(wchar_t chr1, wchar_t chr2, float fontsize) const {
	return m_Impl ? m_Impl->get_kerning(chr1, chr2, fontsize) : 0;
}
KGlyph KFont::getGlyph(wchar_t chr, float fontsize, KFont::Style style, bool with_alpha) const {
	if (m_Impl == nullptr) return KGlyph();
	K__ASSERT(chr);
	K__ASSERT(fontsize > 0);

	KGlyph glyph;
	
	// ロード済みならばそれを返す
	if (g_FontAtlas.getGlyph(this, chr, fontsize, style, with_alpha, &glyph)) {
		return glyph;
	}

	// グリフ情報を得る。この時点ではテクスチャに関するフィールドが埋まっていない
	K_Metrics m;
	memset(&m, 0, sizeof(m));
	m_Impl->get_glyph(chr, fontsize, &m, nullptr);

	glyph.advance = m.advance;
	glyph.left   = (int)m.left;
	glyph.top    = (int)m.top;
	glyph.right  = (int)(m.left + m.width);
	glyph.bottom = (int)(m.top + m.height);
	glyph.texture = 0;
	glyph.u0 = 0;
	glyph.v0 = 0;
	glyph.u1 = 0;
	glyph.v1 = 0;

	if (glyph.w() > 0 && style==KFont::STYLE_BOLD) {
		glyph.right += BOLD_INFLATE; // BOLD_INFLATEピクセル太る
		glyph.advance += BOLD_INFLATE; // 文字送りもBOLD_INFLATEピクセル伸びる
	}

	// グリフ画像を得る
	KImage img32 = getGlyphImage32(chr, fontsize, style, with_alpha);

	// 登録する
	g_FontAtlas.addGlyph(this, chr, fontsize, style, with_alpha, img32, glyph);

	return glyph;
}
bool KFont::getNameInJapanese(NameId nid, char *out_u8, int out_size) const {
	if (m_Impl) {
		return m_Impl->get_name_string(nid, K_LCID_JAPANESE, out_u8, out_size);
	}
	return false;
}
bool KFont::getNameInEnglish(NameId nid, char *out_u8, int out_size) const {
	if (m_Impl) {
		return m_Impl->get_name_string(nid, K_LCID_ENGLISH, out_u8, out_size);
	}
	return false;
}
KFont::TrueTypeFlags KFont::getFlags() const {
	if (m_Impl) {
		bool b=false;
		bool i=false;
		bool u=false;
		if (m_Impl->get_style(&b, &i, &u)) {
			return (
				(b ? KFont::STYLE_BOLD : 0) |
				(i ? KFont::STYLE_ITALIC : 0) |
				(u ? KFont::STYLE_UNDERSCORE : 0)
			);
		}
	}
	return 0;
}

void KFont::globalSetup(KTextureBank *texbank) {
	g_FontAtlas.setup(texbank);
}
void KFont::globalShutdown() {
	g_FontAtlas.setup(nullptr);
}
void KFont::globalClearGlyphs() {
	g_FontAtlas.clearGlyphs();
}
int KFont::getFontCollectionCount(const void *data, int size) {
#ifdef K_USE_STB_TRUETYPE
	if (data == nullptr) return 0;
	if (size < 12) return 0; // 判定には12バイト必要
	return stbtt_GetNumberOfFonts((uint8_t*)data);
#else
	return 0;
#endif
}
KFont KFont::createFromStream(KInputStream &input, int ttc_index) {
	KFont font;
	font.loadFromStream(input, ttc_index);
	return font;
}
KFont KFont::createFromFileName(const std::string &filename, int ttc_index) {
	KFont font;
	font.loadFromFileName(filename, ttc_index);
	return font;
}
KFont KFont::createFromMemory(const void *data, int size, int ttc_index) {
	KFont font;
	font.loadFromMemory(data, size, ttc_index);
	return font;
}
#pragma endregion // KFont



namespace Test {
static std::string _Test_getname(const KFont &font, KFont::NameId nid) {
	// まず日本語での取得を試みる。
	// ダメだったら英語で取得する
	char s[1024] = {0}; // 著作権表示など、結構長いテキストが入っている場合があるので多めにとっておく
	if (font.getNameInJapanese(nid, s, sizeof(s))) {
		return s;
	}
	if (font.getNameInEnglish(nid, s, sizeof(s))) {
		return s;
	}
	return "";
}
void Test_font_printInfo(const std::string &output_dir, const std::string &filename) {
	std::string msg_u8;
	std::string bin;
	{
		KInputStream file = KInputStream::fromFileName(filename);
		if (file.isOpen()) {
			bin = file.readBin();
		}
	}

	int numfonts = KFont::getFontCollectionCount(bin.data(), bin.size());
	if (numfonts > 0) {
		for (int i=0; i<numfonts; i++) {
			KFont font = KFont::createFromMemory(bin.data(), bin.size(), i);
			msg_u8 += K::str_sprintf("%s [%d]\n", filename.c_str(), i);
			msg_u8 += "\tFamily   : " + _Test_getname(font, KFont::NID_FAMILY   ) + "\n";
			msg_u8 += "\tCopyRight: " + _Test_getname(font, KFont::NID_COPYRIGHT) + "\n";
			msg_u8 += "\tVersion  : " + _Test_getname(font, KFont::NID_VERSION  ) + "\n";
			msg_u8 += "\tTrademark: " + _Test_getname(font, KFont::NID_TRADEMARK) + "\n";
			msg_u8 += "\n";
		}
		msg_u8 += "\n";

		std::string outname = K::pathJoin(output_dir, K::pathGetLast(filename));
		outname = K::pathRenameExtension(outname, ".txt");

		KOutputStream output = KOutputStream::fromFileName(outname);
		output.write(msg_u8.data(), msg_u8.size());
	}
}
void Test_font_ex(const std::string &font_filename, float fontsize, const std::string &output_image_filename) {
	const KColor32 BG   = KColor(0.0f, 0.3f, 0.0f, 1.0f);
	const KColor32 LINE = KColor(0.0f, 0.2f, 0.0f, 1.0f);

	KFont font = KFont::createFromFileName(font_filename);
	if (font.isOpen()) {
		int img_w = 800;
		int img_h = (int)fontsize * 7;
		KImage imgText = KImage::createFromSize(img_w, img_h, BG);

		{
			KFont::Style style = KFont::STYLE_NONE;
			const wchar_t *text = L"普通の設定。スタンダード Default";

			int baseline = (int)fontsize*1;
			KImageUtils::horzLine(imgText, 0, img_w, baseline, LINE);

			float x = 0;
			for (int i=0; text[i]; i++) {
				wchar_t wc = text[i];
				KGlyph gm = font.getGlyph(wc, fontsize, style); // 描画開始位置に注意。メトリクス情報を使って、各文字のベースライン位置がそろうようにする
				KImage imgGlyph = font.getGlyphImage32(wc, fontsize, style, true);
				KImageUtils::blendAlpha(imgText, (int)x+gm.left, baseline-gm.top, imgGlyph);
				x += gm.advance;
			}
		}

		{
			KFont::Style style = KFont::STYLE_BOLD;
			const wchar_t *text = L"太字にしたもの。Bold style.";

			int baseline = (int)fontsize*2;
			KImageUtils::horzLine(imgText, 0, img_w, baseline, LINE);

			float x = 0;
			for (int i=0; text[i]; i++) {
				wchar_t wc = text[i];
				KGlyph gm = font.getGlyph(wc, fontsize, style); // 描画開始位置に注意。メトリクス情報を使って、各文字のベースライン位置がそろうようにする
				KImage imgGlyph = font.getGlyphImage32(wc, fontsize, style, true);
				KImageUtils::blendAlpha(imgText, (int)x+gm.left, baseline-gm.top, imgGlyph);
				x += gm.advance;
			}
		}

		{
			KFont::Style style = KFont::STYLE_NONE;
			const wchar_t *text = L"ALTA FIFA Finland Wedding (Kerning off)";

			int baseline = (int)fontsize*5;
			KImageUtils::horzLine(imgText, 0, img_w, baseline, LINE);

			float x = 0;
			for (int i=0; text[i]; i++) {
				wchar_t wc = text[i];
				KGlyph gm = font.getGlyph(wc, fontsize, style); // 描画開始位置に注意。メトリクス情報を使って、各文字のベースライン位置がそろうようにする
				KImage imgGlyph = font.getGlyphImage32(wc, fontsize, style, true);
				KImageUtils::blendAlpha(imgText, (int)x+gm.left, baseline-gm.top, imgGlyph);
				x += gm.advance;
			}
		}

		{
			KFont::Style style = KFont::STYLE_NONE;
			const wchar_t *text = L"ALTA FIFA Finland Wedding (Kerning on)";

			int baseline = (int)fontsize*6;
			KImageUtils::horzLine(imgText, 0, img_w, baseline, LINE);

			float x = 0;
			for (int i=0; text[i]; i++) {
				wchar_t wc = text[i];
				KGlyph gm = font.getGlyph(wc, fontsize, style); // 描画開始位置に注意。メトリクス情報を使って、各文字のベースライン位置がそろうようにする
				KImage imgGlyph = font.getGlyphImage32(wc, fontsize, style, true);
				KImageUtils::blendAlpha(imgText, (int)x+gm.left, baseline-gm.top, imgGlyph);
				x += gm.advance + font.getKerningAdvanve(text[i], text[i+1], fontsize);
			}
		}

		std::string png = imgText.saveToMemory();
		KOutputStream output = KOutputStream::fromFileName(output_image_filename);
		output.write(png.data(), png.size());
	}
}
void Test_font(const std::string &dir) {
#ifdef _WIN32
	Test_font_printInfo(dir, ""); // 存在しないファイルでも落ちない
	Test_font_printInfo(dir, "c:\\windows\\fonts\\msgothic.ttc");
	Test_font_printInfo(dir, "c:\\windows\\fonts\\meiryo.ttc");
	Test_font_printInfo(dir, "c:\\windows\\fonts\\arial.ttf");
	Test_font_printInfo(dir, "c:\\windows\\fonts\\tahoma.ttf");
#endif
}

} // Test


#pragma endregion // Font.cpp




#pragma region PlatformFonts.cpp


// フォントをファミリー名でソートする
struct Pred {
	bool operator()(const KPlatformFonts::INFO &a, const KPlatformFonts::INFO &b) const {
		return K::str_stricmp(a.family.c_str(), b.family.c_str()) < 0;
	}
};
std::string KPlatformFonts::getFontDirectory() {
	wchar_t dir[MAX_PATH];
	SHGetFolderPathW(nullptr, CSIDL_FONTS, nullptr, SHGFP_TYPE_CURRENT, dir);
	std::string s = K::strWideToUtf8(dir);
	return K::pathNormalize(s);
}
int KPlatformFonts::size() const {
	return m_list.size();
}
const KPlatformFonts::INFO & KPlatformFonts::operator [](int index) const {
	return m_list[index];
}
void KPlatformFonts::scan() {
	m_list.clear();

	// フォントフォルダ内のファイルを列挙
	std::string font_dir = getFontDirectory();
	std::vector<std::string> files = K::fileGetListInDir(font_dir);
	for (auto it=files.begin(); it!=files.end(); ++it) {
		std::string filename = K::pathJoin(font_dir, *it);
		KInputStream file = KInputStream::fromFileName(filename);
		std::string bin = file.readBin();
		int numfonts = KFont::getFontCollectionCount(bin.data(), bin.size());
		for (int i=0; i<numfonts; i++) {
			// Wingding などのシンボルテキストはロードできない。
			// ロードできないとわかっているので、エラーダイアログがいちいち出ないように抑制しておく
			KLog::muteDialog();
			KFont font = KFont::createFromMemory(bin.data(), bin.size(), i);
			KLog::unmuteDialog();

			INFO info;
			if (!font.isOpen()) {
				info.family = "(unsupported font)";
			} else {
				char tmp[256];
				// フォントファミリー
				// 日本語を優先、ダメだったら英語名を得る
				if (font.getNameInJapanese(KFont::NID_FAMILY, tmp, sizeof(tmp))) {
					info.family = tmp;
				} else if (font.getNameInEnglish(KFont::NID_FAMILY, tmp, sizeof(tmp))) {
					info.family = tmp;
				} else {
					info.family = "(no name)";
				}
				/*
				// サブファミリー "Regular" "Bold" など
				if (fo.getNameInEnglish(KFont::NID_SUBFAMILY, tmp, sizeof(tmp))) {
					info.subfamily = tmp;
				}
				*/
				info.tt_flags = font.getFlags(); // サブファミリー名ではなく、フラグから直接スタイルを取得する
				info.filename = *it; // filename
			//	info.filename = filename; // fullpath
				info.ttc_index = i;
				m_list.push_back(info);
			}
		}
	}
	// フォント名順でソート
	std::sort(m_list.begin(), m_list.end(), Pred());
}


namespace Test {

void Test_fontscan(const std::string &output_dir) {
	KPlatformFonts fonts;
	fonts.scan();

	std::string msg_u8;
	for (int i=0; i<fonts.size(); i++) {
		auto it = fonts[i];
		if (it.tt_flags == KFont::STYLE_NONE) {
			std::string fami_mb = K::strUtf8ToAnsi(it.family, "");
			std::string file_mb = K::strUtf8ToAnsi(it.filename, "");
			K::strReplace(fami_mb, "\\", "");
			K::strReplace(file_mb, "\\", "");
			msg_u8 += K::str_sprintf("%s : %s[%d]\n",
				fami_mb.c_str(),
				file_mb.c_str(),
				it.ttc_index
			);
		}
	}
	KOutputStream output = KOutputStream::fromFileName(K::pathJoin(output_dir, "Test_fontscan.txt"));
	output.write(msg_u8.data(), msg_u8.size());
}
} // Test

#pragma endregion // PlatformFonts.cpp

} // namespace
