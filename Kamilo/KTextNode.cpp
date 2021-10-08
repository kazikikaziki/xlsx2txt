#include "KTextNode.h"
//
#include "KFont.h"
#include "KImGui.h"
#include "KInternal.h"

namespace Kamilo {



#define TEXT_USE_INT_POS 1
#define TEXT_USE_INT_CUR 0
#define PARENT_NONE (-1)
#define PAERNT_SELF (-2)
#define BLANK_ADVANCE_FACTOR 0.3f


#pragma region KTextBox
KTextBox::KTextBox() {
	clear();
}
void KTextBox::clear() {
	curstack_.clear();
	attrstack_.clear();
	sequence_.clear();

	curattr_ = Attr();
	curattr_.color = KColor32::WHITE;
	curattr_.size = 24;

	rubyattr_ = Attr();
	rubyattr_.color = KColor32::WHITE;
	rubyattr_.size = 16;

	lineheight_ = 24;
	curx_ = 0;
	cury_ = 0;
	auto_wrap_width_ = 0;
	auto_wrap_flags_ = KAutoWrap_BLANK|KAutoWrap_CALLBACK;
	curlinestart_ = 0;
	curparent_ = PARENT_NONE;
	rowcount_ = 1;
	progress_animation_ = false;
	escape_ = true;
	kerning_ = true;
	kerningfunc_ = nullptr;
}
float KTextBox::getCharProgress(int index) const {
	return sequence_[index].anim.progress;
}
void KTextBox::setCharProgress(int index, float progress) {
	sequence_[index].anim.progress = KMath::clamp01(progress);
}
void KTextBox::setAutoWrapWidth(int w) {
	auto_wrap_width_ = (float)w;
}
void KTextBox::pushCursor() {
	curstack_.push_back(KVec2(curx_, cury_));
}
void KTextBox::popCursor() {
	curx_ = curstack_.back().x;
	cury_ = curstack_.back().y;
	curstack_.pop_back();
}
void KTextBox::setCursor(const KVec2 &pos) {
	curx_ = pos.x;
	cury_ = pos.y;
}
KVec2 KTextBox::getCursor() const {
	return KVec2(curx_, cury_);
}
void KTextBox::setFont(KFont &font) {
	curattr_.font = font;
}
void KTextBox::setRubyFont(KFont &font) {
	rubyattr_.font = font;
}
void KTextBox::setFontSize(float size) {
	curattr_.size = size;
}
void KTextBox::setRubyFontSize(float size) {
	rubyattr_.size = size;
}
float KTextBox::getFontSize() const {
	return curattr_.size;
}
float KTextBox::getRubyFontSize() const {
	return rubyattr_.size;
}
void KTextBox::setFontStyle(KFont::Style style) {
	curattr_.style = style;
}
void KTextBox::setFontPitch(float pitch) {
	curattr_.pitch = pitch;
}
void KTextBox::setFontColor(const KColor32 &color) {
	curattr_.color = color;
}
void KTextBox::setUserData(int userdata) {
	curattr_.userdata = userdata;
}
void KTextBox::setLineHeight(float height) {
	lineheight_ = height;
}
void KTextBox::setKerning(bool value) {
	kerning_ = value;
}
void KTextBox::setKerningFunc(KTextBoxKerningFunc fn) {
	kerningfunc_ = fn;
}
void KTextBox::setEscape(bool value) {
	escape_ = value;
}
float KTextBox::getLineHeight() const {
	return lineheight_;
}
void KTextBox::pushFontAttr() {
	attrstack_.push_back(curattr_);
}
void KTextBox::popFontAttr() {
	curattr_ = attrstack_.back();
	attrstack_.pop_back();
}
int KTextBox::beginGroup() {
	curparent_ = sequence_.size();

	// グループの親としてふるまう文字を入れておく
	Char chr;
	chr.pos.x = curx_;
	chr.pos.y = cury_;
	chr.parent = PAERNT_SELF; // 自分がグループの親になっている
	sequence_.push_back(chr);
	return curparent_;
}
void KTextBox::endGroup() {
	curparent_ = PARENT_NONE;
}
KVec2 KTextBox::getCharPos(int index) const {
	const Char &chr = sequence_[index];
	if (chr.parent >= 0) {
		// グループ文字になっている。基準文字からの相対座標が設定されている
		const Char &parent = sequence_[chr.parent];
		return parent.pos + chr.pos;
	} else {
		// グループ文字ではない。テキストボックス内座標が設定されている
		return chr.pos;
	}
}
void KTextBox::addString(const wchar_t *text) {
	for (const wchar_t *s=text; *s; s++) {
		if (*s == L'\n') {
			newLine();
		} else if (K::str_iswprint(*s)) {
			addChar(*s);
		}
	}
}
void KTextBox::addChar(wchar_t code) {
	addCharEx(code, curattr_);
}

void KTextBox::addCharEx(wchar_t code, const Attr &attr) {
	if (!attr.font.isOpen()) {
	//	KLog::printWarning(u8"フォントが未設定です");
		K__ERROR("NO FONT LOADED"); // フォントがロードされていない。何も描画できない
		return;
	}

	// カーニング適用
	if (kerning_) {
		if (sequence_.size() > 0) {
			const Char &last = sequence_.back();
			// 自分のすぐ左かつベースライン、フォントサイズが同じならカーニングを適用する
			if (last.pos.x < curx_ && last.pos.y == cury_) {
				if (last.attr.size == attr.size) {
					if (last.attr.font.id() == attr.font.id()) {
						// カーニングを適用し、カーソルを後退させる
						float kern = last.attr.font.getKerningAdvanve(last.code, code, last.attr.size);

						// ユーザーがカーニングに介入したい場合はここで行う
						onKerning(last.code, code, last.attr.size, &kern);

						if (kerningfunc_) {
							kerningfunc_(this, last.code, code, last.attr.size, &kern);
						}

						// カーニング適用
						if (kern != 0.0f) {
							curx_ += kern;
						}
					}
				}
			}
		}
	}

	// 文字情報を得る
	KGlyph glyph = attr.font.getGlyph(code, attr.size, attr.style, true);

	// 空白文字の文字送り量を調整する
	if (1) {
		if (code == ' ') {
			glyph.advance = attr.size * BLANK_ADVANCE_FACTOR;
		}
	}

	float bound_right  = curx_ + glyph.right;
//	float bound_bottom = cury_ + _min(glyph.bottom, 0); // bottom がベースラインよりも上側にある場合 (bottom < 0) は 0 扱いにする

	// 文字の位置を決定する
	// 文字の右側が自動改行位置を超えてしまう場合は、先に改行しておく
	if (auto_wrap_width_ > 0) {
		if (auto_wrap_width_ <= bound_right) {
			if (curparent_ >= 0) {
				// グループ文字を処理中なので、途中で改行できない。
				// グループ先頭までさかのぼって改行させる
				if (curlinestart_ == curparent_) {
					// すでにグループ先頭が行頭になっている。改行しても意味がない。何もしない
				
				} else {
					// グループ親
					Char *parent = &sequence_[curparent_];

					// 現在のカーソル位置をグループ親からの相対座標で記録
					float relx = curx_ - parent->pos.x;
					float rely = cury_ - parent->pos.y;

					// グループ親の位置までカーソルを戻す
					curx_ = parent->pos.x;
					cury_ = parent->pos.y;

					// 改行する。この呼び出しで curx_, cury_ が改行後の位置になる
					newLine();
					curlinestart_ = curparent_; // 現在の行の先頭文字インデックス

					// 改行後のカーソル位置にグループ親を置く
					parent->pos.x = curx_;
					parent->pos.y = cury_;

					// カーソル位置を復帰する。
					// カーソル位置とグループ親位置の相対座標が、改行前と変化しないようにする
					curx_ += relx;
					cury_ += rely;
				}

			} else {
				// 空白文字での改行を試みる。
				// 現在の行の最後にある空白文字を探す
				int space_index = -1;
				if (auto_wrap_flags_ & KAutoWrap_BLANK) {
					// 末尾の文字（改行位置をはみ出してしまった文字）から現在の行の先頭文字までさかのぼりながら、改行ポイントを探す
					int lineLen = (int)sequence_.size() - curlinestart_; // この行の長さ

					// 改行できる個所を探すために最大でどこまでさかのぼるか？
				//	int min_index = curlinestart_; // 現在の行の先頭まで。極端な話、行の先頭から１文字目でいきなり自動改行する場合もありうる
				//	int min_index = curlinestart_ + lineLen / 2; // 行の中間ぐらい
					int min_index = curlinestart_ + lineLen * 3 / 4; // 行の 3/4 ぐらい

					// 文字を行頭に向かって辿りながら、改行可能場所を探す
					for (int i=(int)sequence_.size()-1; i>=min_index; i--) {
						const Char *chr = &sequence_[i];
						if (chr->parent >= 0) {
							// グループ化されている場合は途中改行できない
						} else {
							if (K::str_iswblank(sequence_[i].code)) {
								space_index = i;
								break;
							}
						}
					}
				}

				// 最後の空白文字が先頭でなかったなら、そこで改行する
				if (space_index > 0 && space_index+1 < (int)sequence_.size()) {
					// 空白文字が見つかった。
					// 空白文字の「次の文字」を改行させる。
					// （空白文字で改行させると行の先頭が不自然に開いてしまう）
					int break_index = space_index + 1;
					Char *break_chr = &sequence_[break_index];

					// 改行位置文字から、現在の文字までを次の行に移動させる

					// 改行位置文字から、現在の文字までの相対距離
					float relx = curx_ - break_chr->pos.x;
					float rely = cury_ - break_chr->pos.y;

					// 現在の改行文字位置
					float oldx = break_chr->pos.x;
					float oldy = break_chr->pos.y;

					// 改行する。この呼び出しで curx_, cury_ が改行後の位置になる
					newLine();
					curlinestart_ = break_index; // 新しい行の先頭に来る文字のインデックス

					// 改行前と改行後で、改行文字位置文字がどのくらい移動したか
					float deltax = curx_ - oldx;
					float deltay = cury_ - oldy;

					// 現時点で、次の行に移動したのはカーソルだけであり、文字はまだ移動していない。
					// 改行しなければならない文字列を次の行へ移動する
					for (int i=break_index; i<(int)sequence_.size(); i++) {
						// 親がいる場合、親からの相対座標で管理しているので、改行しようがしまいが
						// 座標を変更してはならない。
						// 変更すべきなのは親の座標である
						// ゆえに parent < 0 な文字だけを移動させる ※parentは親も時のインデックスなので 0 が入る場合もある
						if (sequence_[i].parent < 0) {
							sequence_[i].pos.x += deltax;
							sequence_[i].pos.y += deltay;
						}
					}

					// この時点で、カーソルはまだ新しい行の先頭にいる。
					// これが文字列末尾になるよう移動する
					curx_ += relx;
					cury_ += rely;
				
				} else {
					if (auto_wrap_flags_ & KAutoWrap_CALLBACK) {
						// 空白文字が存在しない。日本語準拠の処理を行う
						// 基本的にはそのまま改行してもよいが、禁則文字などを考慮して問い合わせを行う
						if (dontBeStartOfLine(code)) {
							// 新しい文字は行頭禁止文字である。改行してはいけない
				
						} else if (sequence_.size() > 0 && dontBeLastOfLine(sequence_.back().code)) {
							// 最後に追加した文字は行末禁止文字だった。改行してはいけない

						} else {
							// 無条件改行
							newLine();
						}
					} else {
						// 無条件改行
						newLine();
					}
				}
			}
		}
	}

	// スケープシーケンスを処理する
	if (escape_) {
		if (code == L'\n') {
			// 無条件改行
			newLine();
			return;
		}
	}

	Char chr;
	chr.code = code;
	chr.anim = Anim();
	chr.attr = curattr_;
	chr.glyph = glyph;
	chr.parent = curparent_; // グループ番号は、そのグループの開始インデックスを表している。同時に、グループのローカル座標基準も表す

	if (chr.parent >= 0) {
		// グループ内文字の場合、文字座標はグループ先頭の文字からの相対座標で指定する
		KVec2 parentpos = sequence_[chr.parent].pos; // グループ先頭の文字位置
		KVec2 relpos = KVec2(curx_, cury_) - parentpos;
		chr.pos.x = relpos.x;
		chr.pos.y = relpos.y;

	} else {
		// テキストボックス左上からの相対座標
		chr.pos.x = curx_;
		chr.pos.y = cury_;
	}
	if (TEXT_USE_INT_POS) {
		// 座標を整数化する（カーソルは実数のまま、描画位置だけ調整する）
		#if 0
		chr.pos.x = floorf(chr.pos.x);
		chr.pos.y = floorf(chr.pos.y);
		#else
		chr.pos.x = KMath::round(chr.pos.x);
		chr.pos.y = KMath::round(chr.pos.y);
		#endif
	}

	sequence_.push_back(chr);

	if (TEXT_USE_INT_CUR) {
		// カーソルの位置を常に整数に合わせる
		curx_ += floorf(chr.glyph.advance + attr.pitch);
	//	curx_ += KMath::round(chr.glyph.advance + attr.pitch);
	} else {
		// advance, pitch をそのまま信用し、適用する
		curx_ += chr.glyph.advance + attr.pitch;
	}
}
void KTextBox::setRuby(int group_id, const wchar_t *text, int count) {
	pushCursor();

	// 対象文字列の矩形
	KVec2 topleft, bottomright;
	getGroupBoundRect(group_id, &topleft, &bottomright);

	// ルビ対象文字列の左上にカーソルを移動
	setCursor(topleft);

	// ルビ文字列を追加
	int ruby_start = getCharCount();
	for (int i=0; i<count; i++) {
		addCharEx(text[i], rubyattr_);
	}

	// この段階では、ルビ文字はルビ対象の文字列に対して左詰めになっている。
	// ルビ対象文字列の中心位置を基準にして、ルビ文字列を中央ぞろえにする
	float center = (topleft.x + bottomright.x) / 2;
	horzCentering(ruby_start, count, center);
	
	popCursor();
}
void KTextBox::newLine() {
	curx_ = 0;
	cury_ += lineheight_;
	rowcount_++;
	curlinestart_ = (int)sequence_.size(); // 現在の行の先頭文字インデックス
}
const KTextBox::Char & KTextBox::getCharByIndex(int index) const {
	static const Char s_char;
	if (0 <= index && index < (int)sequence_.size()) {
		return sequence_[index];
	}
	return s_char;
}
bool KTextBox::getBoundRect(int start, int count, KVec2 *out_topleft, KVec2 *out_bottomright) const {
	int s = KMath::max(start, 0);
	int e = KMath::min(start + count, (int)sequence_.size());
	if (s < e) {
		float box_l =  FLT_MAX;
		float box_r = -FLT_MAX;
		float box_t =  FLT_MAX;
		float box_b = -FLT_MAX;
		for (int i=s; i<e; i++) {
			const Char &elm = sequence_[i];

			KVec2 elmpos = getCharPos(i);

			// グリフの座標系はベースポイント基準のY軸下向き
			float char_l = elmpos.x + elm.glyph.left;
			float char_r = elmpos.x + elm.glyph.right;
			float char_t = elmpos.y + elm.glyph.top;
			float char_b = elmpos.y + elm.glyph.bottom;

			box_l = KMath::min(box_l, char_l);
			box_r = KMath::max(box_r, char_r);
			box_t = KMath::min(box_t, char_t);
			box_b = KMath::max(box_b, char_b);
		}
		if (out_topleft    ) *out_topleft     = KVec2(box_l, box_t);
		if (out_bottomright) *out_bottomright = KVec2(box_r, box_b);
		return true;
	}
	return false;
}
bool KTextBox::getGroupBoundRect(int parent, KVec2 *out_topleft, KVec2 *out_bottomright) const {
	if (parent < 0) return false;

	// グループ先頭インデックスと、グループ長さを得る。
	int start = parent + 1; // 親自身をカウントしないように注意
	int count = 0;
	for (int i=start; i<(int)sequence_.size(); i++) {
		if (sequence_[i].parent == parent) {
			count++;
		} else {
			break;
		}
	}
	// 矩形を取得する
	return getBoundRect(start, count, out_topleft, out_bottomright);
}
bool KTextBox::horzCentering(int start, int count, float x) {
	KVec2 topleft, bottomright;
	if (getBoundRect(start, count, &topleft, &bottomright)) {
		float width = bottomright.x - topleft.x; // 全体の幅
		float left = x - width / 2; // 中央ぞろえにしたときの左端座標
		float offset = left - topleft.x; // 左端が dest_x に一致するための移動量
		for (int i=start; i<start+count; i++) {
			sequence_[i].pos.x += offset;
		}
	}
	return false;
}
KImage KTextBox::exportImage(const ExportDesc *pDesc) {
	ExportDesc desc;
	if (pDesc) {
		desc = *pDesc;
	}

	// テキスト座標系からビットマップ座標系に変換
	#define TEXT2BMP_X(x) origin_x + (int)(x)
	#define TEXT2BMP_Y(y) origin_y + (int)(y)

	// 全テキストのバウンドボックスを求める
	// バウンドボックスの原点は、１行目１文字目のベースライン始点にある。
	// 1行目に文字が存在するなら、ほとんどの場合バウンドボックスの topleft.y は
	// 負の値（１行目のベースラインより上）になる
	int numchars = getCharCount();
	KVec2 topleft, bottomright;
	getBoundRect(0, numchars, &topleft, &bottomright);

	// ビットマップの大きさを決める
	if (desc.xsize <= 0) desc.xsize = (int)(bottomright.x - topleft.x);
	if (desc.ysize <= 0) desc.ysize = (int)(bottomright.y - topleft.y);
	int bmp_w = desc.margin_l + desc.xsize + desc.margin_r;
	int bmp_h = desc.margin_top + desc.ysize + desc.margin_btm;

	// ビットマップを作成
	KImage img = KImage::createFromSize(bmp_w, bmp_h, KColor32::BLACK);

	// テキストボックスの原点がビットマップのどこにくるか？
	// テキストボックスの原点は、１行目１文字目のベースラインに当たる。
	// 従って、テキストボックスの原点をビットマップ左上に重ねてしまうと、1行目がビットマップの上側にはみ出てしまう。
	// 正しくビットマップ内に収めるためには、テキスト全体を1行分下に下げないといけない。

	// 今回の場合は、テキスト全体バウンドボックスの左上が
	// ビットマップ左上よりもマージン分だけ内側になるように重ねる
	int origin_x = desc.margin_l - (int)topleft.x;
	int origin_y = desc.margin_top - (int)topleft.y;
//	int origin_y = desc.margin_top + lineheight_;
	// 行線を描画
	if (desc.show_rowlines) {
		int x0 = TEXT2BMP_X(0);
		int x1 = TEXT2BMP_X(desc.xsize);
		KColor32 linecolor(255, 50, 50, 50);
		for (int i=0; i<rowcount_; i++) {
			int y = TEXT2BMP_Y(lineheight_ * i);
			KImageUtils::horzLine(img, x0, x1, y, linecolor);
		}
	}

	// マージンが設定されている場合、その範囲を描画
	if (desc.show_margin) {
		int x0 = desc.margin_l;   //TEXT2BMP_X(0);
		int y0 = desc.margin_top; //TEXT2BMP_Y(0);
		int x1 = bmp_w - desc.margin_r; //TEXT2BMP_X(desc.xsize);
		int y1 = bmp_h - desc.margin_btm; //TEXT2BMP_Y(desc.ysize);
		KColor32 wrapcolor(255, 75, 0, 0);
		KImageUtils::horzLine(img, x0, x1, y0, wrapcolor);
		KImageUtils::horzLine(img, x0, x1, y1, wrapcolor);
		KImageUtils::vertLine(img, x0, y0, y1, wrapcolor);
		KImageUtils::vertLine(img, x1, y0, y1, wrapcolor);
	}

	// 自動折り返しが設定されているなら、その線を描画
	if (desc.show_wrap && auto_wrap_width_ > 0) {
		int x  = TEXT2BMP_X(auto_wrap_width_);
		int y0 = TEXT2BMP_Y(topleft.y);
		int y1 = TEXT2BMP_Y(bottomright.y);
		KColor32 wrapcolor(255, 75, 0, 0);
		KImageUtils::vertLine(img, x, y0, y1, wrapcolor);
	}

	// バウンドボックスを描画
	if (desc.show_boundbox) {
		int x0 = TEXT2BMP_X(topleft.x);
		int y0 = TEXT2BMP_Y(topleft.y);
		int x1 = TEXT2BMP_X(bottomright.x);
		int y1 = TEXT2BMP_Y(bottomright.y);
		KColor32 bordercolor(255, 120, 120, 120);
		KImageUtils::horzLine(img, x0, x1, y0, bordercolor);
		KImageUtils::horzLine(img, x0, x1, y1, bordercolor);
		KImageUtils::vertLine(img, x0, y0, y1, bordercolor);
		KImageUtils::vertLine(img, x1, y0, y1, bordercolor);
	}

	// 文字を描画
	for (size_t i=0; i<sequence_.size(); i++) {
		const Char &chr = sequence_[i];
		KImage glyph = chr.attr.font.getGlyphImage32(chr.code, chr.attr.size, chr.attr.style, true);
		KVec2 chrpos = getCharPos(i);
		int x = origin_x + (int)chrpos.x + chr.glyph.left;
		int y = origin_y + (int)chrpos.y + chr.glyph.top;
		KImageUtils::blendAlpha(img, x, y, glyph, chr.attr.color);
	}
	return img;

	#undef TEXT2BMP_X
	#undef TEXT2BMP_Y
}

/// アドレス p に2個の float を書き込む
static inline void kk_WriteFloat2(void *p, float x, float y) {
	float *pFloats = (float *)p;
	pFloats[0] = x;
	pFloats[1] = y;
}
static inline void kk_WriteFloat4(void *p, const KColor &color) {
	float *pFloats = (float *)p;
	pFloats[0] = color.r;
	pFloats[1] = color.g;
	pFloats[2] = color.b;
	pFloats[3] = color.a;
}
int KTextBox::getCharCount() const {
	return (int)sequence_.size();
}
int KTextBox::getMeshCharCount() const {
	int num = 0;
	for (size_t i=0; i<sequence_.size(); i++) {
		const Char &chr = sequence_[i];
		if (!chr.no_glyph()) { // no_glyph な文字は数えない。getMeshPositionArray での頂点設定と合わせておくこと
			num++;
		}
	}
	return num;
}

#define NUM_VERTEX_PER_GLPYH 6 // 1グリフにつき6頂点消費する (TriangleList)

int KTextBox::getMeshVertexCount() const {
	return getMeshCharCount() * NUM_VERTEX_PER_GLPYH;
}
const KVertex * KTextBox::getMeshVertexArray() const {
	vertices_.resize(sequence_.size() * NUM_VERTEX_PER_GLPYH); // 1グリフにつき6頂点消費する
	size_t glyph_count = 0;
	for (size_t i=0; i<sequence_.size(); i++) {
		const Char &chr = sequence_[i];
		if (!chr.no_glyph()) {
			KVertex *vdst = vertices_.data();
			// 座標
			{
				KVec2 chrpos = getCharPos(i);
				float left   = chrpos.x + (float)chr.glyph.left;
				float right  = chrpos.x + (float)chr.glyph.right;
				float top    = chrpos.y + (float)chr.glyph.top;
				float bottom = chrpos.y + (float)chr.glyph.bottom;
				// 1グリフにつき6頂点消費する
				// 利便性のため、6頂点のうちの最初の4頂点は、左上を基準に時計回りに並ぶようにする
				//
				// 0 ---- 1 左上から時計回りに 0, 1, 2, 3 になる
				// |      |
				// |      |
				// 3------2
				if (progress_animation_) {
					onGlyphMesh(i, chr, &left, &top, &right, &bottom);
				}
				vdst[glyph_count*6+0].pos = KVec3(left,  top,    0.0f); // 0:左上
				vdst[glyph_count*6+1].pos = KVec3(right, top,    0.0f); // 1:右上
				vdst[glyph_count*6+2].pos = KVec3(right, bottom, 0.0f); // 2:右下
				vdst[glyph_count*6+3].pos = KVec3(left,  bottom, 0.0f); // 3:左下
				vdst[glyph_count*6+4].pos = KVec3(left,  top,    0.0f); // 0:左上
				vdst[glyph_count*6+5].pos = KVec3(right, bottom, 0.0f); // 2:右下
			}
			// UV
			{
				float u0 = chr.glyph.u0;
				float v0 = chr.glyph.v0;
				float u1 = chr.glyph.u1;
				float v1 = chr.glyph.v1;
				// 1グリフにつき6頂点消費する
				// 頂点の順番については getMeshPositionArray を参照
				vdst[glyph_count*6+0].tex = KVec2(u0, v0); // 0:左上
				vdst[glyph_count*6+1].tex = KVec2(u1, v0); // 1:右上
				vdst[glyph_count*6+2].tex = KVec2(u1, v1); // 2:右下
				vdst[glyph_count*6+3].tex = KVec2(u0, v1); // 3:左下
				vdst[glyph_count*6+4].tex = KVec2(u0, v0); // 0:左上
				vdst[glyph_count*6+5].tex = KVec2(u1, v1); // 2:右下
			}
			// COLOR
			{
				KColor32 color32 = chr.attr.color;
				if (progress_animation_) {
					color32.a = (uint8_t)((float)color32.a * chr.anim.progress);
				}
				vdst[glyph_count*6+0].dif32 = color32; // 0:左上
				vdst[glyph_count*6+1].dif32 = color32; // 1:右上
				vdst[glyph_count*6+2].dif32 = color32; // 2:右下
				vdst[glyph_count*6+3].dif32 = color32; // 3:左下
				vdst[glyph_count*6+4].dif32 = color32; // 0:左上
				vdst[glyph_count*6+5].dif32 = color32; // 2:右下
			}
			glyph_count++; // glyph_count==i とは限らない！
		}
	}
	return vertices_.data();
}
KTEXID KTextBox::getMeshTexture(int index) const {
	int idx = 0;
	for (size_t i=0; i<sequence_.size(); i++) {
		const Char &chr = sequence_[i];
		if (!chr.no_glyph()) { // no_glyph な文字は数えない。getMeshPositionArray での頂点設定と合わせておくこと
			if (idx == index) {
				return chr.glyph.texture;
			}
			idx++;
		}
	}
	return nullptr;
}
void KTextBox::setCharProgressEnable(bool value) {
	progress_animation_ = value;
}
bool KTextBox::dontBeStartOfLine(wchar_t code) {
	static const wchar_t *list = L"｝〕〉》）」』】。、！？ァィゥェォッャュョ!?.,";
	return wcschr(list, code) != nullptr;
}
bool KTextBox::dontBeLastOfLine(wchar_t code) {
	static const wchar_t *list = L"｛〔〈《（「『【";
	return wcschr(list, code) != nullptr;
}
#pragma endregion // KTextBox





#pragma region KTextParser
KTextParser::KTextParser() {
	escape_ = L'\\'; // エスケープ文字
	box_ = nullptr;
}
void KTextParser::addStyle(int id, const wchar_t *start) {
	addStyle(id, start, L"\n");
}
void KTextParser::addStyle(int id, const wchar_t *start, const wchar_t *end) {
	K__ASSERT(id > 0); // id == 0 はデフォルト書式を表す。開始、終端文字は登録できない
	K__ASSERT(start && start[0]); // 空文字列は登録できない
	K__ASSERT(end && end[0]); // 空文字列は登録できない
	K__ASSERT(wcslen(start) < STYLE::MAXTOKENLEN);
	K__ASSERT(wcslen(end) < STYLE::MAXTOKENLEN);

	STYLE fmt;
	fmt.id = id;
	wcscpy_s(fmt.start_token, STYLE::MAXTOKENLEN, start);
	wcscpy_s(fmt.end_token, STYLE::MAXTOKENLEN, end);
	styles_.push_back(fmt);
}

// n 文字を比較する
static int K__CompareStringW(const wchar_t *ws1, const wchar_t *ws2, int compareWCharCount) {
	// wcsncmp による比較はダメ。全角記号と半角記号を区別してくれない。例えば
	// 全角の L"＜" と 半角である L"<" を同じと判定してしまう。
	// 代わりに CompareString を使う
	// https://www.s34.co.jp/cpptechdoc/article/comparestring/
	//
	int len1 = wcslen(ws1);
	int len2 = wcslen(ws2);
	int cmpLen1 = KMath::min(len1, compareWCharCount);
	int cmpLen2 = KMath::min(len2, compareWCharCount);
	int cmpLen = KMath::min(cmpLen1, cmpLen2);
	int ret = memcmp(ws1, ws2, sizeof(wchar_t)*cmpLen); // ただの記号と比較するだけなので、違いを許容する必要はない。厳密に一致するかどうかだけ調べる
//	std::wstring w1(ws1, cmpLen);
//	std::wstring w2(ws2, cmpLen);
//	int ret = CompareStringW(LOCALE_USER_DEFAULT, 0, w1.c_str(), -1, w2.c_str(), -1);
//	int ret = CompareStringW(LOCALE_USER_DEFAULT, 0, ws1, cmpLen, ws2, cmpLen);
	return ret;
}

const KTextParser::STYLE * KTextParser::isStart(const wchar_t *text) const {
	K__ASSERT(text);
	// 書式範囲の開始記号に一致するか調べる
	// 複数の記号が該当する場合は最も長いものを返す
	const STYLE *style = nullptr;
	size_t maxlen = 0;
	size_t textlen = wcslen(text);
	for (auto it=styles_.begin(); it!=styles_.end(); it++) {
		size_t n = wcslen(it->start_token);
		// wcsncmp による比較はダメ。全角記号と半角記号を区別してくれない。例えば
		// 全角の L"＜" と 半角である L"<" を同じと判定してしまう。
		if (K__CompareStringW(text, it->start_token, n) == 0) {
			if (maxlen < n) {
				maxlen = n;
				style = &(*it);
			}
		}
	}
	return style;
}
const KTextParser::STYLE * KTextParser::isEnd(const wchar_t *text) const {
	K__ASSERT(text);

	// 現在の書式の終了記号に一致するか調べる
	if (stack_.empty()) {
		return nullptr; // 適用されている書式なし
	}

	// 現在の書式（スタックトップにある書式）
	const STYLE *current_style = stack_.back().style;
	K__ASSERT(current_style);
	
	// 現在の書式の終端記号にマッチすれば OK
	const wchar_t *end = current_style->end_token;
	if (end && end[0]) {
		int n = wcslen(end);
		// wcsncmp による比較はダメ。全角記号と半角記号を区別してくれない。例えば
		// 全角の L"＜" と 半角である L"<" を同じと判定してしまう。
		if (K__CompareStringW(text, end, n) == 0) {
			return current_style;
		}
	}

	// 終端記号にマッチしなかったか、もしくは文末を終端記号とする特殊書式だった
	return nullptr;
}
void KTextParser::startStyle(const STYLE *style, const wchar_t *pos) {
	K__ASSERT(style);
	K__ASSERT(pos);
	K__ASSERT(box_);

	onStartStyle(box_, style->id);

	// スタイルと、その適用開始位置をスタックに積む
	MARK mark = {style, pos};
	stack_.push_back(mark);
}
void KTextParser::endStyle(const STYLE *style, const wchar_t *pos) {
	K__ASSERT(style);
	K__ASSERT(pos);
	K__ASSERT(box_);

	MARK mark = stack_.back();
	stack_.pop_back();

	if (mark.style == style) {
		// 最後に push したスタイルが pop されている。
		// スタイル開始と終端の対応関係が正しいということ
		const wchar_t *sep = nullptr;
		for (const wchar_t *s=mark.start; s<pos; s++) {
			if (*s == L'|') {
				sep = s;
				break;
			}
		}
		std::wstring left, right;
		if (sep) {
			// 区切り文字が見つかった。これを境にして前半と後半に分ける
			left.assign(mark.start, sep - mark.start);
			right.assign(sep+1, pos - sep - 1);
		} else {
			// 区切り文字なし。区切り文字の前半だけが存在し、後半は空文字列とする
			left.assign(mark.start, pos - mark.start);
		}
		onEndStyle(box_, style->id, left.c_str(), right.c_str());

	} else {
		// 最後に push したスタイルとは異なるスタイルを pop した。
		// スタイル開始と終端の対応関係が正しくない
		onEndStyle(box_, style->id, nullptr, 0);
	}
}
void KTextParser::parse(KTextBox *textbox, const wchar_t *text) {
	K__ASSERT(text);
	box_ = textbox;

	onBeginParse(box_);

	// 一番最初は必ず書式番号 0 で始まる
	STYLE default_style = {0, L"", L""};
	startStyle(&default_style, text);

	for (const wchar_t *it=text; *it!=L'\0'; ) {
		const STYLE *style;

		// エスケープ文字がある場合、その直後の文字を処理する
		if (*it == escape_) {
			it++;
			if (*it == L'\n') {
				// 改行文字だったら、この改行を無視する
			} else {
				// 非改行文字だったら、特別な意味を持たないただの文字として扱う
				onChar(box_, *it);
			}
			it++;
			continue;
		}

		// 開始記号にマッチすれば書式開始イベントを呼ぶ
		style = isStart(it);
		if (style) {
			it += wcslen(style->start_token);
			startStyle(style, it);
			continue;
		}

		// 終了記号にマッチすれば書式終了イベントを呼ぶ
		style = isEnd(it);
		if (style) {
			endStyle(style, it);
			it += wcslen(style->end_token);
			continue;
		}
		
		// 通常文字
		onChar(box_, *it);

		it++;
	}
	// 一番最後は必ず書式番号 0 で終わる
	endStyle(&default_style, text + wcslen(text) /*末尾のヌル文字のアドレス*/);

	onEndParse(box_);

	// 後始末
	box_ = nullptr;
	stack_.clear();
}
#pragma endregion // KTextParser





#pragma region KTextLayout
KTextLayout::KTextLayout() {
	m_textbox = nullptr;
	m_parser = nullptr;
	m_textarea_w = 0;
	m_textarea_h = 0;
	m_margin_left = 10;
	m_margin_right = 10;
	m_margin_top = 10;
	m_margin_bottom = 10;
	m_default_style = Style();
	m_real_pitch = 0;
	m_real_fontsize = 0;
	m_real_linepitch = 0;
	m_has_adjusted = false;
	m_auto_wrap_width = 0;
	m_talkbox_w = 0;
	m_talkbox_h = 0;
	m_horzalign = -1;
	m_vertalign = -1;
}
void KTextLayout::setParser(KTextParser *ps) {
	m_parser = ps;
}
void KTextLayout::setTextBox(KTextBox *tb) {
	m_textbox = tb;
}
KTextBox * KTextLayout::getTextBox() {
	return m_textbox;
}
void KTextLayout::clearText() {
	if (m_textbox) {
		m_textbox->clear();
	}
}
int KTextLayout::numChar() const {
	if (m_textbox) {
		return m_textbox->getCharCount();
	}
	return 0;
}
int KTextLayout::textarea_w() const {
	return m_textarea_w;
}
int KTextLayout::textarea_h() const {
	return m_textarea_h;
}
int KTextLayout::textbound_w() const {
	return (int)m_textboundsize.x;
}
int KTextLayout::textbound_h() const {
	return (int)m_textboundsize.y;
}
int KTextLayout::talkbox_w() const {
	return m_talkbox_w;
}
int KTextLayout::talkbox_h() const {
	return m_talkbox_h;
}
bool KTextLayout::has_adjusted() const {
	return m_has_adjusted;
}
KVec3 KTextLayout::getOffset() const {
	// 現時点では、最初の文字のベースライン（文字左下の基準点）が
	// テキストレイアウトの原点にぴったり位置している。
	// つまり、原点の右上側に１行目が存在することになる
	KVec3 offset;
	
	switch (m_horzalign) {
	case -1: // left
		offset.x = (float)(-m_talkbox_w/2 + m_margin_left); // テキスト原点（１文字目の基準点）をテキスト表示領域の左上に合わせる
		break;

	case 0: // center
		offset.x = (float)(-m_textboundsize.x/2);
		break;

	case 1: // right
		offset.x = (float)m_talkbox_w/2 - (float)m_margin_right - m_textboundsize.x;
		break;
	}

	switch (m_vertalign) {
	case -1: // top
		offset.y = (float)(m_talkbox_h/2 - m_margin_top); // １行目のベースラインをテキスト領域の上端に合わせる（ベースラインが上端なので、１行目は全体が上橋よりも上側にあることに注意）
		offset.y -= m_real_fontsize; // 現時点では上端に１行目のベースラインが重なっているので、１文字分下げる
		offset.y -= m_real_linepitch; // 行間分さらに下げる。これをしないと文字の上側に余白がなくなり、ルビがふれなくなる
		offset.y *= -1; // テキスト座標系はY軸下向きになっているので、これを反転する
		break;

	case 0: // center
		offset.y = -m_textboundsize.y/2;
		offset.y -= m_real_fontsize; // 現時点では上端に１行目のベースラインが重なっているので、１文字分下げる
		offset.y -= m_real_linepitch; // 行間分さらに下げる。これをしないと文字の上側に余白がなくなり、ルビがふれなくなる
		offset.y *= -1; // テキスト座標系はY軸下向きになっているので、これを反転する
		break;

	case 1: // bottom
		offset.y = -m_talkbox_h/2 + m_margin_bottom + m_textboundsize.y;
		offset.y *= -1; // テキスト座標系はY軸下向きになっているので、これを反転する
		break;
	}
	return offset;
}
void KTextLayout::setFont(KFont &font) {
	m_font = font;
}
void KTextLayout::setDefaultStyle(const Style &s) {
	m_default_style = s;
}
const KTextLayout::Style & KTextLayout::getDefaultStyle() const {
	return m_default_style;
}
void KTextLayout::setMargin(int left, int right, int top, int bottom) {
	m_margin_left = left;
	m_margin_right = right;
	m_margin_top = top;
	m_margin_bottom = bottom;

	// テキストエリア更新
	m_textarea_w = m_talkbox_w - m_margin_left - m_margin_right;
	m_textarea_h = m_talkbox_h - m_margin_top - m_margin_bottom;
}
void KTextLayout::getMargin(int *p_left, int *p_right, int *p_top, int *p_bottom) const {
	if (p_left)   *p_left   = m_margin_left;
	if (p_right)  *p_right  = m_margin_right;
	if (p_top)    *p_top    = m_margin_top;
	if (p_bottom) *p_bottom = m_margin_bottom;
}
void KTextLayout::getTextArea(int *p_left, int *p_right, int *p_top, int *p_bottom) const {
	// ベースポイント（１行１文字目の左下）を原点としたときのテキスト配置可能エリア
	if (p_left)   *p_left   = m_margin_left;
	if (p_right)  *p_right  = m_talkbox_w - m_margin_right;
	if (p_top)    *p_top    = -(m_real_fontsize + m_real_linepitch) - m_margin_top;
	if (p_bottom) *p_bottom = -(m_real_fontsize + m_real_linepitch) + m_textarea_h;
}
void KTextLayout::setBoxSize(int w, int h) {
	m_talkbox_w = w;
	m_talkbox_h = h;
	m_textarea_w = m_talkbox_w - m_margin_left - m_margin_right;
	m_textarea_h = m_talkbox_h - m_margin_top - m_margin_bottom;
}
void KTextLayout::setText(const std::string &text_u8) {
	std::wstring ws = K::strUtf8ToWide(text_u8);
	setText(ws);
}
void KTextLayout::setText(const std::wstring &text_w) {
	K__ASSERT(m_textbox);
	K__ASSERT(m_default_style.fontsize >= 1);

	m_source_text = text_w;

	// 自動調整なしのスタイルにリセットする
	resetToDefaultStyle();
}
void KTextLayout::resetToDefaultStyle() { // 自動調整なしのスタイルにリセットする
	m_real_pitch = m_default_style.pitch;
	m_real_fontsize = m_default_style.fontsize;
	m_real_linepitch = m_default_style.linepitch;
}
void KTextLayout::layout(bool auto_adjust) {
	// 現在の設定でレイアウトする
	layout_raw();

	// 自動補正が有効ならば補正する
	if (auto_adjust) {
		adjust();
	}
}
void KTextLayout::layout_raw() {
	K__ASSERT(m_textbox);
	K__ASSERT(m_parser);
	m_has_adjusted = false;
	m_textbox->clear();

	// 自動折り返しを設定
	if (m_auto_wrap_width > 0) {
		m_textbox->setAutoWrapWidth(m_auto_wrap_width);
	} else if (m_auto_wrap_width == 0) {
		m_textbox->setAutoWrapWidth(m_textarea_w);
	} else {
		m_textbox->setAutoWrapWidth(0);
	}
	
	// フォントサイズと間隔
	m_textbox->setFont(m_font);
	m_textbox->setFontSize(m_real_fontsize);
	m_textbox->setFontPitch(m_real_pitch);

	m_textbox->setRubyFont(m_font);
	m_textbox->setRubyFontSize(m_default_style.fontsize_ruby);

	// 行間設定
	m_textbox->setLineHeight(m_real_fontsize + m_real_linepitch);

	// 文字の描画位置に介入できるように
	m_textbox->setCharProgressEnable(true); // KTextBox::onGlyphMesh が呼ばれるようにする

	// パーサーを実行し、文字レイアウト等を決定する
	m_parser->parse(m_textbox, m_source_text.c_str());

	// テキスト表示範囲を得る
	int numchars = m_textbox->getCharCount();
	KVec2 topleft, bottomright;
	m_textbox->getBoundRect(0, numchars, &topleft, &bottomright);
	m_textboundsize.x = bottomright.x - topleft.x;
	m_textboundsize.y = bottomright.y - topleft.y;
}
bool KTextLayout::isOverRight() const {
	float MAX_X = (float)(m_textarea_w + m_margin_right/2); // マージンの半分ぐらいまでなら許すことにする
	return m_textboundsize.x >= MAX_X;
}
bool KTextLayout::isOverBottom() const {
#if 1
	// 1行目のベースライン(テキストボックス上端基準でY軸下向き)
	int baseY = (int)(m_margin_top + m_real_fontsize + m_real_linepitch);

	// バウンドボックスを得る。
	// このボックスは、文字が置いてある最初の行（ルビではなく、その元のテキストのほう）のベースライン左端を原点としている
	int numchars = m_textbox->getCharCount();
	KVec2 topleft, bottomright;
	m_textbox->getBoundRect(0, numchars, &topleft, &bottomright);

#if 1
	int t, b;
	getTextArea(nullptr, nullptr, &t, &b);
	if (bottomright.y >= b + m_margin_bottom/2) { // 下側余白へのはみだしをチェック。半分ぐらいなら許す
		return true; // 下側がはみ出ている
	}
	return false;
#endif

	// 実際にテキストボックス内に置いたときの座標を得る
	float top = baseY + topleft.y;
	float btm = baseY + bottomright.y;
	if (top < m_margin_top/2) { // 上側余白へのはみだしをチェック。半分ぐらいなら許す
	//	return true; // 上側がはみ出ている
	}
	if (btm >= m_talkbox_h - m_margin_bottom/2) { // 下側余白へのはみだしをチェック。半分ぐらいなら許す
		return true; // 下側がはみ出ている
	}
	return false;
#else
	float MAX_Y = (float)(m_textarea_h + m_margin_bottom/2); // マージンの半分ぐらいまでなら許すことにする
	return m_textboundsize.y >= MAX_Y;
#endif
}
// 文字範囲が既定のサイズに収まるように調整する
void KTextLayout::adjust() {

	// １回の調整量。小さいと細かい調整ができるが、その分試行回数も増えて時間がかかる
//	const float LINEPITCH_STEP = 0.5f;
	const float LINEPITCH_STEP = 0.2f;
	const float PITCH_STEP = 0.2f;
//	const float PITCH_STEP = 1.0f;
	const float SIZE_STEP = 1.0f;

	// 文字範囲をチェック
	while (1) {
		// 下側が範囲を超えているなら、まずは行間調整での解決を試みる
		while (isOverBottom() && m_real_linepitch > m_default_style.linepitch_adjustable_min) {
			m_real_linepitch = KMath::max(m_real_linepitch - LINEPITCH_STEP, m_default_style.linepitch_adjustable_min);
			layout_raw();
			m_has_adjusted = true;
		}

		// まだ下側が超えているなら、ピッチを調整してみる。
		// ピッチは 1 ピクセル小さくするだけでかなり詰まって見えるため、控えめにしておく
		while (isOverBottom() && m_real_pitch > m_default_style.charpitch_adjustable_min) {
			m_real_pitch = KMath::max(m_real_pitch - PITCH_STEP, m_default_style.charpitch_adjustable_min);
			layout_raw();
			m_has_adjusted = true;
		}

		// まだ下側が超えているなら、文字サイズを小さくしてみる
		if (isOverBottom() && m_real_fontsize > m_default_style.fontsize_adjustable_min) {
			// 行間と文字ピッチをいったんリセットする
			m_real_linepitch = m_default_style.linepitch;
			m_real_pitch = m_default_style.pitch;
			// 文字サイズを縮める
			m_real_fontsize = KMath::max(m_real_fontsize - SIZE_STEP, m_default_style.fontsize_adjustable_min);
			layout_raw();
			m_has_adjusted = true;
			continue; // 再び調整ループする
		}

		// 縦方向の調整を終了する。
		// まだ超えていたとしても、これ以上の調整はあきらめる
		break;
	}

	// 横方向を調整する
	{
		while (isOverRight() && m_real_pitch > m_default_style.charpitch_adjustable_min) {
			m_real_pitch = KMath::max(m_real_pitch - PITCH_STEP, m_default_style.charpitch_adjustable_min);
			layout_raw();
			m_has_adjusted = true;
		}
	}
}
#pragma endregion // TextLayout




#pragma region Test1
namespace Test {
void Test_textbox1(const char *output_dir) {
	const wchar_t *text =
		L"Hello, World.\n"
		L"ウィキペディアは　百科事典を作成するプロジェクトです。"
		L"その記事に執筆者独自の意見や研究内容が含まれてはならず（Wikipedia:独自研究は載せないを参照）、"
		L"その記事の内容は、信頼できる文献を参照することによって検証可能でなければなりません（Wikipedia:検証可能性を参照）。"
		L"したがって、記事の執筆者は、複数の信頼できる検証可能な文献を参照し、その内容に即して記事を執筆することが要求されます。"
		L"一方で、参考文献に掲載されている文章をそのまま引き写すことは、"
		L"剽窃（盗作）であり、場合によっては著作権の侵害という法律上の問題も生じることから、"
		L"各執筆者は、独自の表現で記事を執筆しなければなりません。";
	KTextBox box;
//	KFont font = KFont::createFromFileName("C:\\windows\\fonts\\msgothic.ttc", 0); // ＭＳ ゴシック
//	KFont font = KFont::createFromFileName("C:\\windows\\fonts\\msgothic.ttc", 1); // MS UI Gothic
//	KFont font = KFont::createFromFileName("C:\\windows\\fonts\\msgothic.ttc", 2); // ＭＳ Ｐ ゴシック
	KFont font = KFont::createFromFileName("C:\\windows\\fonts\\meiryo.ttc"); // メイリオ
	box.setFont(font);
	box.setFontSize(22);
	box.setAutoWrapWidth(400);
	for (const wchar_t *it=text; *it!=L'\0'; it++) {
		box.addChar(*it);
	}
	KImage img = box.exportImage(nullptr);
	std::string png = img.saveToMemory();

	std::string path = K::pathJoin(output_dir, "Test_textbox1.png");
	KOutputStream file;
	file.openFileName(path);
	file.write(png.data(), png.size());
}
/// [test1] <--- Doxgen @snippet コマンドからの参照用なので削除してはいけない
#pragma endregion // Test1




#pragma region Test2
/// [test2] <--- Doxgen @snippet コマンドからの参照用なので削除してはいけない
class SampleParser: public KTextParser {
public:
	enum STYLE {
		S_NONE,
		S_YELLOW,
		S_RED,
		S_BOLD,
		S_BIG,
		S_WEAK,
		S_RUBY,
		S_COMMENT,
	};
	int comment_depth_;
	bool is_ruby_; // ルビ書式を処理中

	SampleParser() {
		comment_depth_ = 0;
		is_ruby_ = false;
		// [] を色変え系として定義
		addStyle(S_YELLOW, L"[", L"]");
		addStyle(S_RED,    L"[[", L"]]");
		// <--> を太字強調系として定義
		addStyle(S_BOLD,   L"<", L">");
		addStyle(S_BIG,    L"<<", L">>");
		// {} を弱める系として定義
		addStyle(S_WEAK,   L"{", L"}");
		// () を引数付として定義
		addStyle(S_RUBY,   L"(", L")");
		// コメントを定義
		addStyle(S_COMMENT, L"#"); // # から行末まで（方法１）
		addStyle(S_COMMENT, L"//", L"\n"); // // から行末まで（方法２）
		addStyle(S_COMMENT, L"/*", L"*/"); // /* */ で囲まれた部分
	}
	virtual void onStartStyle(KTextBox *tb, int id) override {
		// コメント内部だったら無視する
		if (comment_depth_ > 0) {
			return;
		}
		switch (id) {
		case S_YELLOW:
			tb->pushFontAttr();
			tb->setFontColor(KColor32(0xFF, 0xFF, 0x00, 0xFF));
			break;
		case S_RED:
			tb->pushFontAttr();
			tb->setFontColor(KColor32(0xFF, 0x00, 0x00, 0xFF));
			break;
		case S_BOLD:
			tb->pushFontAttr();
			tb->setFontStyle(KFont::STYLE_BOLD);
			break;
		case S_BIG:
			tb->pushFontAttr();
			tb->setFontStyle(KFont::STYLE_BOLD);
			tb->setFontSize(36);
			break;
		case S_WEAK:
			tb->pushFontAttr();
			tb->setFontSize(18);
			tb->setFontColor(KColor32(0x77, 0x77, 0x77, 0xFF));
			break;
		case S_RUBY:
			// ルビ範囲の始まり
			is_ruby_ = true;
			break;
		case S_COMMENT:
			// コメント部分
			comment_depth_++;
			break;
		}
	}
	virtual void onEndStyle(KTextBox *tb, int id, const wchar_t *text, const wchar_t *more) override {
		if (id == 0) {
			return;
		}
		if (id == S_COMMENT) {
			comment_depth_--;
			return;
		}
		// コメント内部だったら無視する
		if (comment_depth_ > 0) {
			return;
		}
		if (id == S_RUBY) {
			// ルビが終わった。ルビの書式では前半に対象文字列。後半にルビ文字列が入っている
			is_ruby_ = false;

			// ルビ範囲を決める＆自動改行禁止にするため、対象文字列をグループ化する
			int group_id = tb->beginGroup();

			// ルビ対象文字列を追加する
			for (int i=0; text[i]; i++) {
				tb->addChar(text[i]);
			}
			
			// ルビ文字を設定する
			if (more[0]) {
				tb->pushFontAttr();
				tb->setFontSize(12); // 文字サイズ小さく
				tb->setFontStyle(KFont::STYLE_NONE); // 通常スタイルにする
				tb->setRuby(group_id, more, wcslen(more));
				tb->popFontAttr();
			} else {
				// ルビ文字が省略されている。
				// この場合は、ルビなしの単なる自動改行禁止文字列として扱われることになる
			}

			tb->endGroup();
			return;
		}
		tb->popFontAttr();
	}
	virtual void onChar(KTextBox *tb, wchar_t chr) override {
		// コメント内部の場合は何もしない
		if (comment_depth_ > 0) {
			return;
		}
		// ルビの処理は最後に一括で行うので。ここでは何もしない
		if (is_ruby_) {
			return;
		}
		// 改行文字を処理
		if (chr == L'\n') {
			tb->newLine();
			return;
		}
		// 印字可能な文字だった場合はテキストボックスに追加
		if (K::str_iswprint(chr)) {
			tb->addChar(chr);
		}
	}
};

void Test_textbox2(const char *output_dir) {
	const wchar_t *s = 
		L"[[ウィキペディア]]は[(百科事典|ひゃっかじてん)]/*ルビを振るにはこうする*/を"
		L"<(作成|さくせい)>する<<(プロジェクト)>>/*この部分は自動改行されない。（ルビ付き文字列は自動改行されないため、ルビの指定を省略すれば自動改行されない普通の文字列を組むことができる）*/です。"
		L"その[[記事]]に[(執筆者独自|しっぴつしゃどくじ)]の[意見]や研究内容が含まれてはならず{（Wikipedia:独自研究は載せないを参照）}、"
		L"その記事の内容は、信頼できる文献を参照することによって検証可能でなければなりません{（Wikipedia:検証可能性を参照）}。"
		L"したがって、記事の執筆者は、複数の信頼できる<[検証可能]>な文献を参照し、その内容に即して記事を執筆することが要求されます。"
		L"<<一方で>>、(参考文献|さんこうぶんけん)に掲載されている文章をそのまま引き写すことは、剽窃（盗作）であり、"
		L"場合によっては著作権の侵害という法律上の問題も生じることから、"
		L"各執筆者は、[[<<独自の表現>>]]で/*ここはコメント*/記事を[執筆]しなければなりません。//これはコメント\n"
		L"#これはコメントです\n";

	// テキストボックスを用意する
	KTextBox box;
//	KFont font = KFont::createFromFileName("C:\\windows\\fonts\\msgothic.ttc", 0); // ＭＳ ゴシック
//	KFont font = KFont::createFromFileName("C:\\windows\\fonts\\msgothic.ttc", 1); // MS UI Gothic
//	KFont font = KFont::createFromFileName("C:\\windows\\fonts\\msgothic.ttc", 2); // ＭＳ Ｐ ゴシック
	KFont font = KFont::createFromFileName("C:\\windows\\fonts\\meiryo.ttc"); // メイリオ
	box.setFont(font);
	box.setRubyFont(font);
	box.setFontSize(22);
	box.setAutoWrapWidth(400);

	// パーサーを実行する
	SampleParser mydok;
	mydok.parse(&box, s);

	// この時点で、テキストボックスにはパーサーで処理された文字列が入っている。
	// 画像ファイルに保存する
	KImage img = box.exportImage(nullptr);
	std::string png = img.saveToMemory();

	std::string path = K::pathJoin(output_dir, "Test_textbox2.png");
	KOutputStream file;
	file.openFileName(path);
	file.write(png.data(), png.size());
}
/// [test2] <--- Doxgen @snippet コマンドからの参照用なので削除してはいけない


#pragma endregion // Test2

} // Test


#pragma region KTextDrawable
KTextDrawable::KTextDrawable() {
	m_should_update_mesh = false;
	m_shadow = false;
	m_tb_font = KBank::getFontBank()->getDefaultFont();
	m_tb_fontstyle = KFont::STYLE_NONE;
	m_tb_fontsize = 20;
	m_tb_pitch = 0.0f;
	m_tb_linepitch = 0.0f;
	m_kerning = false;
	m_kerningfunc = nullptr;
	m_horz_align = KHorzAlign_LEFT;
	m_vert_align = KVertAlign_TOP;
	m_linear_filter = true;
	setLocalTransform(KMatrix4::fromScale(1, -1, 1)); // Y軸下向きにしておく
}
void KTextDrawable::onDrawable_willDraw(const KNode *camera) {
	// 描画直前。必要ならメッシュを更新する
	if (m_should_update_mesh) {
		updateTextMesh();
	}
}
bool KTextDrawable::onDrawable_getBoundingAabb(KVec3 *min_point, KVec3 *max_point) {
	// メッシュ更新してからAABBを得るように
	if (m_should_update_mesh) {
		updateTextMesh();
	}
	return KMeshDrawable::onDrawable_getBoundingAabb(min_point, max_point);
}

void KTextDrawable::updateTextMesh() {
	m_should_update_mesh = false;

	// 現在設定されている文字属性を使って、テキストを再登録する
	m_textbox.clear();
	m_textbox.setKerning(m_kerning);
	m_textbox.setKerningFunc(m_kerningfunc);
	m_textbox.setFont(m_tb_font);
	m_textbox.setFontStyle(m_tb_fontstyle);
	m_textbox.setFontSize(m_tb_fontsize);
	m_textbox.setLineHeight(m_tb_fontsize + m_tb_linepitch);
	m_textbox.setFontColor(KColor32::WHITE); // フォント色はメッシュの master_color によって変化させるため、ここでは常に WHITE を指定しておく
	m_textbox.setFontPitch(m_tb_pitch);
	m_textbox.addString(m_text.c_str());

	// メッシュを更新する
	KMesh *mesh = getMesh();
	if (mesh == nullptr) return;

	mesh->clear();

	// テキストメッシュ用の頂点を追加する
	int num_points = m_textbox.getMeshVertexCount();
	if (num_points > 0) {
		const KVertex *v = m_textbox.getMeshVertexArray();
		mesh->setVertices(0, v, num_points);
	}

	// 描画する文字の数だけサブメッシュを用意する
	int num_glyph = m_textbox.getMeshCharCount();
	for (int i=0; i<num_glyph; i++) {
		KSubMesh sub;
		sub.start = i*6; // 1文字につき6頂点
		sub.count = 6;
		sub.primitive = KPrimitive_TRIANGLES;
		mesh->addSubMesh(sub);
	}

	// それぞれの文字について、テクスチャを設定する
	for (int i=0; i<num_glyph; i++) {
		KSubMesh *subMesh = getSubMesh(i);
		subMesh->material.texture = (KTEXID)m_textbox.getMeshTexture(i);
		subMesh->material.blend = KBlend_ALPHA;
		subMesh->material.filter = m_linear_filter ? KFilter_LINEAR : KFilter_NONE;
	}

	// メッシュのAABBを得る
	KVec3 minp, maxp;
	mesh->getAabb(&minp, &maxp);

	// アラインメントにしたがって位置を調整する。
	// 座標系に注意。左上を原点としてY軸下向きなので、
	// minp は top-left を表し、maxp は bottom-right を表す
	KVec3 offset;
	switch (m_horz_align) {
	case KHorzAlign_LEFT:
		offset.x = -minp.x;
		break;
	case KHorzAlign_CENTER:
		offset.x = -(maxp.x + minp.x)/2;
		break;
	case KHorzAlign_RIGHT:
		offset.x = -maxp.x;
		break;
	}
	int numchar = m_textbox.getCharCount();
	switch (m_vert_align) {
	case KVertAlign_TOP:
		if (0) {
			// AABB上側を基準にする
			offset.y = -minp.y; // これはダメ。1行目の文字のアセント具合によって、ベースラインがそろわなくなる
		} else {
			// 1行目のエムハイトを基準にする
			if (numchar > 0) {
				const KTextBox::Char &chr = m_textbox.getCharByIndex(0);
				offset.y = m_tb_fontsize;
			}
		}
		break;
	case KVertAlign_CENTER:
		offset.y = -(maxp.y + minp.y)/2;
		break;
	case KVertAlign_BOTTOM:
		if (0) {
			// AABB下側を基準にする
			offset.y = -maxp.y; // これはダメ。最終行の文字のデセントの値によってベースラインがそろわない
		} else {
			// 最終行のベースラインを基準にする
			if (numchar > 0) {
				const KTextBox::Char &chr = m_textbox.getCharByIndex(numchar - 1);
				offset.y = -chr.pos.y;
			}
		}
		break;
	}

	// オフセットを整数化しておく
	// そうしないと文字が微妙にぼやけてしまう事がある
	offset = offset.floor();
	setRenderOffset(offset);
	m_aabb_min = KVec3(minp + offset).floor();
	m_aabb_max = KVec3(maxp + offset).ceil();
}
void KTextDrawable::onDrawable_inspector() {

	KMeshDrawable::onDrawable_inspector(); // call super class method

	{
		const char *lbl[] = {"Left", "Center", "Right"};
		int al = m_horz_align - KHorzAlign_LEFT; // make zero based index
		if (ImGui::Combo("Horz align", &al, lbl, 3)) {
			m_horz_align = (KHorzAlign)(KHorzAlign_LEFT + al);
			updateTextMesh();
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Horizontal textbox alignment");
		}
	}
	{
		const char *lbl[] = {"Top", "Center", "Bottom"};
		int al = m_vert_align - KVertAlign_TOP; // make zero based index
		if (ImGui::Combo("Vert align", &al, lbl, 3)) {
			m_vert_align = (KVertAlign)(KVertAlign_TOP + al);
			updateTextMesh();
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Vertical textbox alignment");
		}
	}
	{
		std::vector<std::string> list = KBank::getFontBank()->getFontNames();
		if (list.size() > 0) {
			int sel = -1;
			for (size_t i=0; i<list.size(); i++) {
				if (KBank::getFontBank()->getFont(list[i]) == m_tb_font) {
					sel = i;
				}
			}
			if (KImGui::Combo("Font", &sel, list)) {
				m_tb_font = KBank::getFontBank()->getFont(list[sel]);
				updateTextMesh();
			}
		}
	}
	if (ImGui::DragFloat("Font height", &m_tb_fontsize, 0.5f, 1, 120)) {
		updateTextMesh();
	}
	if (ImGui::DragFloat("Pitch", &m_tb_pitch, 0.1f, -20, 20)) {
		updateTextMesh();
	}
	if (ImGui::DragFloat("Line Pitch", &m_tb_linepitch, 0.1f, -20, 20)) {
		updateTextMesh();
	}
	// 編集用の文字列を得る
	std::string s = K::strWideToUtf8(m_text);

	// サイズに余裕を持たせておく
	s.resize(s.size() + 256);

	if (ImGui::InputTextMultiline("Text", &s[0], s.size())) {
		m_text = K::strUtf8ToWide(s);
		updateTextMesh();
	}

	ImGui::Separator();
	if (1) {
		KNode *self = getNode();
		KColor color = self->getColor();
		if (ImGui::ColorEdit4("Color (MeshRenderer)", (float*)&color)) {
			// マスターカラーはメッシュから独立しているため、メッシュの再生成は不要
			self->setColor(color);
		}
	}
	if (1) {
		if (ImGui::Checkbox("Linear Fiter (MeshRenderer)", &m_linear_filter)) {
			updateTextMesh();
		}
	}
}
void KTextDrawable::setText(const std::string &text_u8) {
	std::wstring ws = K::strUtf8ToWide(text_u8);
	setText(ws.c_str());
}
void KTextDrawable::setText(const char *text_u8) {
	std::wstring ws = K::strUtf8ToWide(text_u8);
	setText(ws.c_str());
}
void KTextDrawable::setText(const wchar_t *text) {
	K__ASSERT(text);
	m_text = text;
	m_should_update_mesh = true;
}
void KTextDrawable::setFont(KFont &font) {
	m_tb_font = font;
	m_should_update_mesh = true;
}
void KTextDrawable::setFont(const std::string &alias) {
	KFont font = KBank::getFontBank()->getFont(alias, false);
	if (!font.isOpen()) {
		KLog::printError("E_FONT: NO FONT ALIASED '%s", alias.c_str());
	}
	setFont(font);
}
void KTextDrawable::setFontSize(float value) {
	m_tb_fontsize = value;
	m_should_update_mesh = true;
}
void KTextDrawable::setFontStyle(KFont::Style value) {
	m_tb_fontstyle = value;
	m_should_update_mesh = true;
}
void KTextDrawable::setFontPitch(float value) {
	m_tb_pitch = value;
	m_should_update_mesh = true;
}
void KTextDrawable::setHorzAlign(KHorzAlign align) {
	m_horz_align = align;
	m_should_update_mesh = true;
}
void KTextDrawable::setVertAlign(KVertAlign align) {
	m_vert_align = align;
	m_should_update_mesh = true;
}
void KTextDrawable::setKerning(bool kerning) {
	m_kerning = kerning;
	m_should_update_mesh = true;
}
void KTextDrawable::setKerningFunc(KTextBoxKerningFunc fn) {
	m_kerningfunc = fn;
	m_should_update_mesh = true;
}
void KTextDrawable::setLinePitch(float pitch) {
	m_tb_linepitch = pitch;
	m_should_update_mesh = true;
}
void KTextDrawable::updateMesh() {
	updateTextMesh();
}
void KTextDrawable::getAabb(KVec3 *minpoint, KVec3 *maxpoint) const {
	if (minpoint) *minpoint = m_aabb_min;
	if (maxpoint) *maxpoint = m_aabb_max;
}
KVec3 KTextDrawable::getAabbSize() const {
	KVec3 minp, maxp;
	getAabb(&minp, &maxp);
	return maxp - minp;
}

void KTextDrawable::attach(KNode *node) {
	if (node && !isAttached(node)) {
		KTextDrawable *dw = new KTextDrawable();
		KDrawable::_attach(node, dw);
		dw->drop();
	}
}
bool KTextDrawable::isAttached(KNode *node) {
	return of(node) != nullptr;
}
KTextDrawable * KTextDrawable::of(KNode *node) {
	KDrawable *dw = KDrawable::of(node);
	return dynamic_cast<KTextDrawable*>(dw ? dw : nullptr);
}
#pragma endregion // KTextDrawable



} // namespace
