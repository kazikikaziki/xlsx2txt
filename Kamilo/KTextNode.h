/// @file
/// Copyright (c) 2020 Heliodor
/// This software is released under the MIT License.
/// http://opensource.org/licenses/mit-license.php

#pragma once
#include "keng_game.h"
#include "KFont.h"
#include "KMeshDrawable.h"
#include "KRes.h"
#include "KImage.h"

namespace Kamilo {


class KTextBox;


/// 独自のカーニングを施す場合に使う
typedef void (*KTextBoxKerningFunc)(KTextBox *tb, wchar_t a, wchar_t b, float size, float *kerning);




/// 自動改行設定
enum KAutoWrapFlag {
	KAutoWrap_BLANK = 0x01, // 空白文字で改行する
	KAutoWrap_CALLBACK = 0x02, // 禁則文字問い合わせコールバックを使う @see 
};
typedef int KAutoWrapFlags;




/// テキストを表示するための仮想ボックス
/// @snippet KText.cpp test1
class KTextBox {
public:
	struct Attr {
		KColor32 color; ///< 文字色
		KFont font; ///< フォント
		KFont::Style style; ///< 文字スタイル
		float size; ///< 文字サイズ（ピクセル単位でフォントの標準高さを指定）
		float pitch; ///< 文字間の調整量（標準文字送りからの調整値。0で標準と同じ）
		int userdata;

		Attr() {
			color = KColor32::WHITE;
			style = KFont::STYLE_NONE;
			pitch = 0.0f;
			size = 20.0f;
			userdata = 0;
		}
	};
	struct Anim {
		/// 文字表示アニメの進行度。0.0 で文字が出現した瞬間、1.0で完全に文字が表示された状態になる
		float progress;

		Anim() {
			progress = 0.0f;
		}
	};
	struct Char {
		wchar_t code;
		Attr attr;
		Anim anim;
		KGlyph glyph; ///< この文字のグリフ情報
		KVec2 pos;    ///< テキストボックス原点からの相対座標。文字がグループ化されている場合は親文字からの相対座標
		int parent;  ///< 文字がグループ化されている場合、親文字のインデックス。グループ化されていない場合は -1
		Char() {
			code = L'\0';
			parent = -1;
		}
		bool no_glyph() const {
			return glyph.u0 == glyph.u1 || glyph.v0 == glyph.v1;
		}
	};

	KTextBox();
	void clear();

	/// 自動改行幅。0だと改行しない
	void setAutoWrapWidth(int w);
	int getAutoWrapWidth() const { return (int)auto_wrap_width_; }

	/// 自動改行設定
	void setAutoWrapFlags(KAutoWrapFlags flags) {
		auto_wrap_flags_ = flags;
	}
	
	/// カーソル位置をスタックに入れる
	void pushCursor();

	/// カーソル位置を復帰する
	void popCursor();

	/// カーソル位置を移動する
	void setCursor(const KVec2 &pos);

	/// カーソル位置を得る
	KVec2 getCursor() const;

	/// 現在の文字属性をスタックに入れる
	void pushFontAttr();

	/// 文字属性を復帰する
	void popFontAttr();

	/// フォントを設定する
	/// これは文字属性なので pushFontAttr/popFontAttr の影響を受ける
	/// @note この設定は、次に追加した文字から適用される
	void setFont(KFont &font);
	void setRubyFont(KFont &ruby);

	/// 文字色を設定する
	/// これは文字属性なので pushFontAttr/popFontAttr の影響を受ける
	/// @note この設定は、次に追加した文字から適用される
	void setFontColor(const KColor32 &color);

	/// 文字スタイルを設定する
	/// これは文字属性なので pushFontAttr/popFontAttr の影響を受ける
	/// @note この設定は、次に追加した文字から適用される
	void setFontStyle(KFont::Style style);

	/// 文字サイズを設定する
	/// これは文字属性なので pushFontAttr/popFontAttr の影響を受ける
	/// 文字サイズはピクセル単位で指定するが、必ずしも適切な行の高さと一致するわけではない。
	/// 行の推奨サイズ (LineHeight) を取得したければ KFont::getMetrics を使う。
	/// @note この設定は、次に追加した文字から適用される
	void setFontSize(float size);
	float getFontSize() const;
	void setRubyFontSize(float size);
	float getRubyFontSize() const;

	/// 文字ピッチ（標準文字送り量からのオフセット）を設定する
	/// これは文字属性なので pushFontAttr/popFontAttr の影響を受ける
	/// @note この設定は、次に追加した文字から適用される
	void setFontPitch(float pitch);

	/// 現在のスタイルにユーザー定義のデータを関連付ける
	/// これは文字属性なので pushFontAttr/popFontAttr の影響を受ける
	/// @note この設定は、次に追加した文字から適用される
	void setUserData(int userdata);

	/// 行の高さを設定する。
	/// @note これは文字属性ではない。pushFontAttr, popFontAttr の影響を受けない
	/// @note この設定は、次に追加した文字から適用される
	void setLineHeight(float height);

	/// カーニング
	void setKerning(bool value);
	void setKerningFunc(KTextBoxKerningFunc fn);

	/// エスケープシーケンス (\n) を処理する
	void setEscape(bool value);

	float getLineHeight() const;

	/// 文字を追加する
	/// 改行したい時は明示的に newLine を呼ぶ
	void addChar(wchar_t code);
	void addCharEx(wchar_t code, const Attr &attr);

	void addString(const wchar_t *text);

	/// グループ化を開始する。
	/// ここから endGroup() を呼ぶまでの間に addChar した文字が１グループとして扱われる。
	/// グループ化すると途中で自動改行されなくなる
	/// このグループのグループ番号を返す
	int beginGroup();

	/// グループ化を終了する
	void endGroup();

	/// 指定されたグループにルビを振る。
	/// group_id ルビを振る対象となるグループ番号。このグループの文字列の上側中央寄せでルビ文字を設定する
	/// text ルビ文字列
	/// count ルビ文字列の長さ
	/// ルビ文字のサイズやスタイルには現在の文字スタイルがそのまま適用されるので、
	/// 事前にルビ用のサイズやスタイルを適用しておく。呼び出し例は以下の通り
	/// 
	/// textbox.pushFontAttr(); // 現在の文字設定を退避
	/// textbox.setFontSize(rubyfontsize); // ルビ文字のサイズ
	/// textbox.setFontStyle(KFont::STYLE_NONE); // ルビ文字には通常スタイルの文字を適用
	/// textbox.setRuby(groupid, rubytext, rubysize); // ルビを設定
	/// textbox.popFontAttr(); // 文字設定戻す
	/// 
	void setRuby(int group_id, const wchar_t *text, int count);

	/// 改行する
	void newLine();

	/// インデックスを指定して文字情報を得る
	const Char & getCharByIndex(int index) const;

	/// このボックスが含んでいる文字数を得る
	int getCharCount() const;

	/// 行数
	int getLineCount() const { return rowcount_; }

	/// 指定した範囲にあるテキストのバウンドボックスを得る
	bool getBoundRect(int start, int count, KVec2 *out_topleft, KVec2 *out_bottomright) const;

	/// 指定したグループに文字列のバウンドボックスを得る
	bool getGroupBoundRect(int parent, KVec2 *out_topleft, KVec2 *out_bottomright) const;

	/// 指定した範囲にあるテキストを x を中心にした中央揃えになるように移動する
	bool horzCentering(int start, int count, float x);
	
	/// テキストを画像として取得する
	struct ExportDesc {
		ExportDesc() {
			xsize = 0;
			ysize = 0;
			margin_l = margin_r = margin_top = margin_btm = 4;
			show_margin = true;
			show_rowlines = true;
			show_boundbox = true;
			show_wrap = true;
		}
		int xsize;
		int ysize;
		int margin_l;
		int margin_r;
		int margin_top;
		int margin_btm;
		bool show_margin;
		bool show_rowlines;
		bool show_boundbox;
		bool show_wrap;
	};
	KImage exportImage(const ExportDesc *desc);

	/// 文字の実際の位置（テキストボックス原点からの相対座標）を返す
	KVec2 getCharPos(int index) const;

	/// テキストを描画するために必要な頂点数を返す (TriangleList)
	int getMeshVertexCount() const;

	/// テキストを描画の対象になる文字の数（コントロール文字を除外した文字数）
	int getMeshCharCount() const;

	/// 頂点配列を作成する（ワールド座標系 左下原点 Y軸上向き）
	const KVertex * getMeshVertexArray() const;

	/// index 番目の文字を描画するために必要なテクスチャを返す
	KTEXID getMeshTexture(int index) const;

	float getCharProgress(int index) const;
	void setCharProgress(int index, float progress);

	/// 文字の出現アニメを有効にするかどうか。
	/// true にすると setCharProgress での設定が有効になり、onGlyphMesh が呼ばれるようになる
	void setCharProgressEnable(bool value);
	
	/// 自動改行位置を決める時、code 位置の直前で改行してもよいか問い合わせるときに呼ばれる。
	/// 基本的には chr.code が行頭禁則文字であれば true を返すように実装する
	virtual bool dontBeStartOfLine(wchar_t code);

	/// 自動改行位置を決める時、code 位置の直後で改行してもよいか問い合わせるときに呼ばれる。
	/// 基本的には chr.code が行末禁則文字であれば true を返すように実装する
	virtual bool dontBeLastOfLine(wchar_t code);

	/// 独自のカーニングを施す場合に使う
	virtual void onKerning(wchar_t a, wchar_t b, float size, float *kerning) {}

	/// 文字を描画する直前に呼ばれる。
	/// chr.progress の値に応じて、実際の文字の色や矩形を設定する。
	/// setCharProgressEnable が true の場合のみ呼ばれる
	/// left, top, right, bottom にはこれから表示しようとしてる文字の矩形座標が入る。
	/// 必要に応じて、これらの値を書き換える
	virtual void onGlyphMesh(int index, const Char &chr, float *left, float *top, float *right, float *bottom) const {}

private:
	std::vector<KVec2> curstack_;
	std::vector<Attr> attrstack_;
	std::vector<Char> sequence_;
	mutable std::vector<KVertex> vertices_;
	Attr curattr_;
	Attr rubyattr_;
	float curx_;
	float cury_;
	float lineheight_;
	float auto_wrap_width_;
	int curlinestart_; // 現在の行頭にある文字のインデックス
	int curparent_;
	int rowcount_;
	bool progress_animation_; // true にした場合、progress の値によって文字の出現アニメーションが適用される
	bool kerning_; // カーニング使う
	bool escape_; // エスケープシーケンス使う
	KTextBoxKerningFunc kerningfunc_;
	KAutoWrapFlags auto_wrap_flags_;
};




/// 独自の構文を用いて、属性付テキストを作成するための構文解析器
/// @snippet KText.cpp test2
class KTextParser {
public:
	struct TEXT {
		const wchar_t *ws;
		int len;
	};

	KTextParser();

	/// 一行書式を追加する
	/// id 書式を識別するための番号.
	/// token 書式の開始記号。2文字以上でもよい。この記号から行末までがこの書式の適用範囲になる。例: "#" "//" 
	void addStyle(int id, const wchar_t *start);

	/// 範囲指定の書式を追加する
	/// id 書式を識別するための番号。
	/// start 書式の始まりを表す記号。2文字以上でもよい。例: "[" "[[" "<"
	/// end 書式の終了を表す記号。2文字以上でもよい。例: "]" "]]" ">"
	void addStyle(int id, const wchar_t *start, const wchar_t *end);

	/// パーサーを実行し、対象のテキストボックスにテキストを設定する
	void parse(KTextBox *box, const wchar_t *text);

	virtual void onBeginParse(KTextBox *tb) {}
	virtual void onEndParse(KTextBox *tb) {}

	/// 書式開始記号を見つけたときに呼ばれる
	virtual void onStartStyle(KTextBox *tb, int id) {}

	/// 書式終了記号を見つけたときに呼ばれる
	/// id 書式番号
	/// text 書式対象文字列。文字列が区切り文字 | で区切られている場合、最初の区切り文字よりも前半の部分が入る
	/// more 書式対象文字列が区切り文字 | で区切られている場合、最初の区切り文字よりも後ろの部分が入る
	virtual void onEndStyle(KTextBox *tb, int id, const wchar_t *text, const wchar_t *more) {}

	/// 普通の文字を見つけたときに呼ばれる
	virtual void onChar(KTextBox *tb, wchar_t chr) {}

private:
	struct STYLE {
		static const int MAXTOKENLEN = 8;
		int id;
		wchar_t start_token[MAXTOKENLEN];
		wchar_t end_token[MAXTOKENLEN];
	};
	/// スタイル開始
	/// stype スタイル開始位置などの情報
	/// pos スタイル開始の文字位置
	void startStyle(const STYLE *style, const wchar_t *pos);

	/// スタイル終了
	/// stype スタイル開始位置などの情報
	/// pos スタイル終了の文字位置
	void endStyle(const STYLE *style, const wchar_t *pos);

	/// text がスタイルの開始記号に一致するなら、そのスタイルを返す
	const STYLE * isStart(const wchar_t *text) const;

	/// text がスタイルの終了記号に一致するなら、そのスタイルを返す
	const STYLE * isEnd(const wchar_t *text) const;

	/// スタイルテーブル
	std::vector<STYLE> styles_;

	/// エスケープ文字として使う文字
	wchar_t escape_;

private:
	// パース実行時の一時変数
	// ここのフィールドの値は parse() 実行ごとに毎回ゼロクリアされる
	struct MARK {
		const STYLE *style;
		const wchar_t *start;
	};
	std::vector<MARK> stack_;
	KTextBox *box_;
};




class KTextLayout {
public:
	struct Style {
		Style() {
			pitch = 0;
			linepitch = 0;
			linepitch_adjustable_min = -2;
			fontsize = 20;
			fontsize_ruby = 12;
			fontsize_adjustable_min = 18;
			charpitch_adjustable_min = -2;
		}
		float fontsize; // 標準フォントサイズ
		float pitch; // 標準文字ピッチ
		float linepitch; // 標準行間
		float fontsize_ruby; // ルビ文字サイズ
		float fontsize_adjustable_min; // 自動調整時に許容できるフォントサイズの最小値
		float linepitch_adjustable_min; // 自動調整時に許容できる行間詰めの最小値
		float charpitch_adjustable_min; // 自動調整時に許容できる文字間詰めの最小値
	};

	KTextLayout();
	
	void setTextBox(KTextBox *tb);
	KTextBox * getTextBox();

	void setParser(KTextParser *ps);

	// テキストボックスにテキストを流し込む。
	// テキストは書式付きの物を指定することができる。
	// 例えば "<<Hello>> {World}" など。
	// マークダウンテキストの書式については内部クラスの CTalkScriptParser を参照すること
	void setText(const std::string &text_u8);
	void setText(const std::wstring &text_w);
//	void setText(const std::wstring &text_w) { setText(text_w); }

	// テキストボックスの大きさを指定する
	void setSize(int w, int h) { setBoxSize(w, h); }
	void setBoxSize(int w, int h);

	// 現在の設定に従って文字をレイアウトする
	// auto_adjust が true の場合は必要に応じて自動補正する。
	// このとき実際に自動補正したかどうかは has_adjusted() で知ることができる
	void layout(bool auto_adjust);

	void clearText();

	// setText() で設定したテキストが setSize() で設定した範囲に収まるように文字間や行間を調整する。
	// 範囲をはみ出てしまっていた場合はレイアウトを再調整する。
	// 既に範囲内にテキストが収まっている場合は何もしない。
	// レイアウトを再調整したかどうかは has_adjusted() で知ることができる
	void adjust();

	// 文字の表示範囲の右側が setSize() で設定した範囲を超えているなら true を返す
	bool isOverRight() const;

	// 文字の表示範囲の下側が setSize() で設定した範囲を超えているなら true を返す
	bool isOverBottom() const;

	// 文字数を返す
	// マークダウン用の制御文字は含まず、純粋にテキストとして表示する文字数を得る
	int numChar() const;

	int textarea_w() const;
	int textarea_h() const;
	int textbound_w() const;
	int textbound_h() const;
	int talkbox_w() const;
	int talkbox_h() const;
	bool has_adjusted() const;

	// アラインメントをそろえるための位置補正
	KVec3 getOffset() const;

	void setFont(KFont &font);

	// 自動調整なしの場合に適用する書式（フォントサイズ、文字ピッチ）
	void setDefaultStyle(const Style &s);
	const Style & getDefaultStyle() const;

	// 自動調整によって変化した書式を元に戻す（setDefaultStyle で設定した書式に戻す）
	void resetToDefaultStyle();

	void setMargin(int left, int right, int top, int bottom);
	void getMargin(int *p_left, int *p_right, int *p_top, int *p_bottom) const;
	void getTextArea(int *p_left, int *p_right, int *p_top, int *p_bottom) const;
	const std::wstring & getTextW() const { return m_source_text; }

	// < 0  自動改行しない
	// ==0  テキストボックスの幅に合わせて自動改行
	// > 0  指定されたサイズに合わせて自動改行
	int m_auto_wrap_width;

	int m_horzalign; // -1=left 0=center 1=right
	int m_vertalign; // -1=top 0=center 1=bottom

private:
	void layout_raw();

	KVec2 m_textboundsize;
	std::wstring m_source_text;
	int m_textarea_w;
	int m_textarea_h;
	bool m_has_adjusted;
	Style m_default_style;
	KTextBox *m_textbox;
	KTextParser *m_parser;
	KFont m_font;
	float m_real_pitch;
	float m_real_fontsize;
	float m_real_linepitch;
	int m_margin_left;
	int m_margin_right;
	int m_margin_top;
	int m_margin_bottom;
	int m_talkbox_w;
	int m_talkbox_h;
};

namespace Test {
void Test_textbox1(const char *output_dir);
void Test_textbox2(const char *output_dir);
}




enum KHorzAlign {
	KHorzAlign_LEFT   = -1,
	KHorzAlign_CENTER =  0,
	KHorzAlign_RIGHT  =  1,
};

enum KVertAlign {
	KVertAlign_TOP    = -1,
	KVertAlign_CENTER =  0,
	KVertAlign_BOTTOM =  1,
};


class KTextDrawable: public KMeshDrawable {
public:
	static void attach(KNode *node);
	static bool isAttached(KNode *node);
	static KTextDrawable * of(KNode *node);
public:
	KTextDrawable();
	void setText(const char *text_u8);
	void setText(const wchar_t *text);
	void setText(const std::string &text_u8);
	void setFont(KFont &font);
	void setFont(const std::string &alias); // alias = KTextMeshManager に登録したフォント名
	void setFontSize(float value);
	void setFontStyle(KFont::Style value);
	void setFontPitch(float value);
	void setHorzAlign(KHorzAlign align);
	void setVertAlign(KVertAlign align);
	void setKerning(bool kerning);
	void setKerningFunc(KTextBoxKerningFunc fn);
	void setLinePitch(float pitch);
	void updateMesh();
	void getAabb(KVec3 *minpoint, KVec3 *maxpoint) const;
	KVec3 getAabbSize() const;

	virtual void onDrawable_willDraw(const KNode *camera);
	virtual bool onDrawable_getBoundingAabb(KVec3 *min_point, KVec3 *max_point) override;
	virtual void onDrawable_inspector() override;
	void updateTextMesh();

private:
	std::wstring m_text;
	KTextBox m_textbox;
	KHorzAlign m_horz_align;
	KVertAlign m_vert_align;
	KVec3 m_aabb_min;
	KVec3 m_aabb_max;
	bool m_linear_filter;
	bool m_should_update_mesh;
	bool m_shadow;
	bool m_kerning;
	KTextBoxKerningFunc m_kerningfunc;

	// テキスト属性。
	// KTextBox に設定するテキスト属性は clear のたびに失われる。
	// また、KTextBox に設定するテキストは、それ以降に追加される文字にのみ適用される。
	// そのため、KTextBox に設定するフォント属性を保持しておく
	KFont m_tb_font;
	KFont::Style m_tb_fontstyle;
	float m_tb_fontsize;
	float m_tb_pitch;
	float m_tb_linepitch;
};




} // namespace

