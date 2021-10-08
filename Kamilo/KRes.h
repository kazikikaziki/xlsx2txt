/// @file
/// Copyright (c) 2020 Heliodor
/// This software is released under the MIT License.
/// http://opensource.org/licenses/mit-license.php

#pragma once
#include <mutex>
#include "KEdgeFile.h"
#include "KFont.h"
#include "KImagePack.h"
#include "KLua.h"
#include "KNamedValues.h"
#include "KStorage.h"
#include "KString.h"


namespace Kamilo {

class KEngine;
class KFont;
class KNode;

class KResource: public virtual KRef {
public:
	KPath mName;
	KNameList mTags;

	KResource() {}
	virtual void release() {}

	void addTag(const KName &tag) {
		KNameList_pushback_unique(mTags, tag);
	}
	bool hasTag(const KName &tag) const {
		return KNameList_contains(mTags, tag);
	}
	const KNameList & getTags() const {
		return mTags;
	}
	void setName(const KPath &name) {
		mName = name;
	}
	KPath getName() const {
		return mName;
	}
};
typedef KAutoRef<KResource> KResourceAuto;


class KTextureRes: public KResource {
public:
	KTEXID mTexId;
	int mWidth;
	int mHeight;
	bool mIsRenderTex;
	bool mProtected;

	KTextureRes();
	virtual void release() override;
	bool loadEmptyTexture(int w, int h, KTexture::Format fmt=KTexture::FMT_ARGB32, bool protect=false);
	bool loadRenderTexture(int w, int h, KTexture::Format fmt=KTexture::FMT_ARGB32, bool protect=false);
};
typedef KAutoRef<KTextureRes> KTextureAuto;


class KSpriteRes: public KResource {
public:
	KSpriteRes();
	void clear();
	KVec3 getRenderOffset() const;
	KVec3 bmpToLocal(const KVec3 &bmp) const;
	bool buildFromImageEx(int img_w, int img_h, const KPath &texture_name, int image_index, KBlend blend, bool packed);
	bool buildFromPng(const void *png_data, int png_size, const KPath &texname);

	KPath mTextureName;
	KMesh mMesh;
	int mSubMeshIndex;
	int mPaletteCount;
	int mImageW; ///< 作成元画像の横幅。テクスチャの幅ではなく、元画像のサイズであることに注意。サイズが 2^n とは限らない
	int mImageH;

	/// 元画像からの切り取り範囲
	/// ※テクスチャからの切り取り範囲ではない！
	/// あくまでも、元のBITMAP画像内での切り取り範囲であることに注意。
	/// テクスチャ化するときに再配置したり余白の切り落としなどがあったとしても atlas 範囲は変化しない
	int mAtlasX;
	int mAtlasY;
	int mAtlasW;
	int mAtlasH;

	/// ピボット座標。
	/// アトラス範囲の左上を原点としたときのピボット座標を指定する。
	/// 座標単位は mPivotInPixels によって決まる
	/// mPivotInPixels が true ならばピクセル単位で指定する。
	/// mPivotInPixels が false なら左上を(0, 0)右下を(1, 1)とする正規化ビットマップ座標で指定する。
	/// @note 「正規化ビットマップ座標」における右下座標(1, 1)は、テクスチャや元画像の右下ではなく、アトラス範囲での右下になることに注意。
	/// 元画像が 2^n サイズでない場合、自動的に余白が追加されたテクスチャになるため、元画像の右下とテクスチャの右下は一致しなくなる。

	/// 元画像内での mPivot 座標は次のようにして計算する
	/// if (mPivotInPixels) {
	///   point = KVec2(atlas_x_, atlas_y_) + spritepivot_;
	/// } else {
	///   point = KVec2(atlas_x_, atlas_y_) + KVec2(atlas_w_ * pivot_.x, atlas_h_ * pivot_.y);
	/// }
	KVec2 mPivot;

	KBlend mDefaultBlend;
	bool mPivotInPixels;
	bool mUsingPackedTexture;
};
typedef KAutoRef<KSpriteRes> KSpriteAuto;


enum KMark {
	KMark_END  = -1,
	KMark_NONE = 0,
	KMark_A,
	KMark_B,
	KMark_C,
	KMark_D,
	KMark_E,
	KMark_ENUM_MAX
};


inline void K_MarkToStr(KMark mark, char *s, bool shorten=false) {
	switch (mark) {
	case KMark_END:  strcpy(s, "END"); return;
	case KMark_NONE: strcpy(s, ""); return;
	case KMark_A:    strcpy(s, shorten ? "A" : "MARKA"); return;
	case KMark_B:    strcpy(s, shorten ? "B" : "MARKB"); return;
	case KMark_C:    strcpy(s, shorten ? "C" : "MARKC"); return;
	case KMark_D:    strcpy(s, shorten ? "D" : "MARKD"); return;
	case KMark_E:    strcpy(s, shorten ? "E" : "MARKE"); return;
	}
	strcpy(s, "");
	return;
}

inline bool K_StrEq(const char *s, const char *t) {
	return s && t && strcmp(s, t) == 0;
}
inline bool K_StrToMark(const char *s, KMark *mark) {
	if (K_StrEq(s, "END"  )) { *mark = KMark_END; return true; }
	if (K_StrEq(s, "MARKA")) { *mark = KMark_A; return true; }
	if (K_StrEq(s, "MARKB")) { *mark = KMark_B; return true; }
	if (K_StrEq(s, "MARKC")) { *mark = KMark_C; return true; }
	if (K_StrEq(s, "MARKD")) { *mark = KMark_D; return true; }
	if (K_StrEq(s, "MARKE")) { *mark = KMark_E; return true; }
	if (K_StrEq(s, "")) { *mark = KMark_NONE; return true; }
	return false;
}

class KClipRes: public KResource {
public:
	static const int MAX_LAYER_COUNT = 12;
	enum TRIGGER {
		TRIG_NEVER,
		TRIG_FRAME,
		TRIG_PAGE_ONCE,
		TRIG_CLIP_ONCE,
	};
	struct SPRITE_LAYER {
		KPath sprite;  // スプライト名
		KPath label;   // レイヤー名
		KPath command; // レイヤーコマンド
	};
	struct SPRITE_KEY {
		SPRITE_KEY() {
			duration = 0;
			time = 0;
			this_mark = KMark_NONE;
			next_mark = KMark_NONE;
			num_layers = 0;
			edge_page = -1;
			xml_data = NULL;
		}
		~SPRITE_KEY() {
		//	K_Drop(xml_data);
		}

		int time; // キー時刻（トラック先頭からの経過時間）
		int duration; // このキーから次のキーまでの時間（アニメディレイ）
		KMark this_mark; // このキーを特定するためのマーク番号（マークとして有効なのは 1 以上の値のみ）
		KMark next_mark; // このキーの直後にシークする場合、その行先マーク番号。使わないなら 0。-1を指定するとどこにも飛ばない（＝終了）の意味になる
		KPath next_clip; // このキーの直後にシークする場合、その行先クリップ名
		int num_layers; // レイヤー数
		int edge_page; // 元のEdgeのページ番号
		KPath edge_name; // 元のEdgeファイル名
		SPRITE_LAYER layers[MAX_LAYER_COUNT]; // レイヤー情報
		KNamedValues user_parameters; // ユーザー定義データ
		KXmlElement *xml_data;

		SPRITE_KEY clone() const {
			SPRITE_KEY copy = *this;
			copy.xml_data = xml_data->clone();
			return copy;
		}
	};

	enum Flag {
		FLAG_LOOP = 1, // ループ再生する
		FLAG_KILL_SELF  = 2, // 再生終了したときにエンティティを削除する
		FLAG_UNSET_CLIP = 4, // 再生終了したときにクリップを外す
	};
	typedef int Flags;

	KClipRes(const std::string &name);
	virtual ~KClipRes();
	const char * getName() const;
	void clear();

	int getLength() const;

	bool getFlag(Flag flag) const;
	void setFlag(Flag flag, bool value);

	void setTag(KName tag);
	bool hasTag(KName tag) const;
	const KNameList & getTags() const;

	void saveForEdge(KXmlElement *xml, const KPath &homeDir); // <EdgeAnimation> タグ向けに保存する
	void saveForClip(KXmlElement *xml, const KPath &homeDir); // <Clip> タグ向けに保存する

	void on_track_gui_state(float frame);
	void on_track_gui();

	const SPRITE_KEY * getKeyByFrame(float frame) const;
	SPRITE_KEY * getKeyByFrame(float frame);

	int findPageByMark(int mark) const; /// mark されたページを探す
	void addKey(const SPRITE_KEY &key, int pos=-1); /// キーを追加する
	void deleteKey(int index);
	const SPRITE_KEY * getKey(int index) const;
	SPRITE_KEY * getKey(int index);
	int getKeyTime(int index) const; /// キーの時刻
	int getKeyCount() const; /// このアニメのキー数を返す
	int getMaxLayerCount() const; /// このアニメに含まれる最大のレイヤー数を返す
	int getPageByFrame(float frame, int *out_pageframe, int *out_pagedur=NULL) const; /// フレーム位置に対応したページ番号を返す
	int getSpritesInPage(int page, int max_layers, KPath *layer_sprite_names) const; /// ページに含まれるスプライト名を配列で返す（レイヤーが複数枚ある場合はスプライトも複数存在するため）
	int getLayerCount(int page) const;
	bool getNextPage(int page, int mark, std::string *p_new_clip, int *p_new_page) const;
	void recalculateKeyTimes();

	KPath mEdgeFile;
	KXmlElement *mEditInfoXml;

private:
	void gui_key(const SPRITE_KEY &seg, int framenumber) const;
	std::vector<SPRITE_KEY> mKeys;
	KPath mName;
	Flags mFlags;
	int mLength;
	KNameList mTags;
};
typedef KAutoRef<KClipRes> KClipAuto;



class KShaderRes: public KResource {
public:
	KShaderRes();
	void clear();
	KSHADERID getId();
	bool loadFromHLSL(const char *name, const char *code);
	bool loadFromStream(KInputStream &input, const char *name);
	virtual void release() override;

private:
	KSHADERID mShaderId;
};
typedef KAutoRef<KShaderRes> KShaderAuto;




class KFontRes: public KResource {
public:
	KFontRes();
	virtual ~KFontRes();
	virtual void release() override;
	bool loadFromFont(KFont &font);
	bool loadFromStream(KInputStream &input, int ttc_index=0);
	bool loadFromSystemFontDirectory(const char *filename, int ttc_index=0);
private:
	KFont mFont;
};




class KTextureBank {
public:
	enum Flag {
		/// 同名のテクスチャが存在した場合に上書きする
		F_OVERWRITE_ANYWAY = 1,

		/// 使用目的の異なる同名テクスチャが存在する場合に、テクスチャを再生成して上書きする
		/// （例：レンダーテクスチャを取得しようとしているが、同名の通常テクスチャがあった）
		F_OVERWRITE_USAGE_NOT_MATCHED = 2,

		/// サイズの異なる同名テクスチャが存在する場合に、テクスチャを再生成して上書きする
		///（例：256x256のレンダーテクスチャを取得しようとしたが、同名でサイズの異なるレンダーテクスチャが既に存在している）
		F_OVERWRITE_SIZE_NOT_MATCHED = 4,

		/// 保護テクスチャとして生成する。このフラグがついているテクスチャは clearTextures が呼ばれても破棄されない
		F_PROTECT = 8,

		F_FLOATING_FORMAT = 16,
	};
	typedef int Flags;

	virtual ~KTextureBank() {}
	virtual KTEXID addTextureFromSize(const KPath &name, int w, int h) = 0;
	virtual KTEXID addTextureFromImage(const KPath &name, const KImage &image) = 0;
	virtual KTEXID addTextureFromTexture(const KPath &name, KTEXID source) = 0;
	virtual KTEXID addTextureFromFileName(const KPath &name) = 0;
	virtual KTEXID addTextureFromStream(const KPath &name, KInputStream &input) = 0;

	/// レンダーテクスチャを追加する
	virtual KTEXID addRenderTexture(const KPath &name, int w, int h, Flags flags=0) = 0;

	virtual KTEXID getTexture(const KPath &tex_path) = 0;
	virtual KTEXID getTextureEx(const KPath &tex_path, int modifier, bool should_exist, KNode *node_for_mod=NULL) = 0;
	virtual bool isRenderTexture(const KPath &tex_path) = 0;

	/// 登録済みのテクスチャを探す
	/// @param name 
	///        テクスチャ名
	/// @param modifier
	///        0 以外の値を指定すると、登録されているテクスチャそのままではなく modifier の値に応じて改変されたテクスチャを返す。
	///        何の値を指定するとどういう改変になるかはユーザー指定コールバック KTextureBankCallback::on_videobank_modifier() で決まる
	/// @param should_exist
	///        見つからない時にエラーを出すかどうか
	/// @param node_for_mod 
	///        K_SIG_TEXBANK_MODIFIER イベントの node に渡される値
	virtual KTEXID findTexture(const KPath &name, int modifier=0, bool should_exist=true, KNode *node_for_mod=NULL) = 0;

	/// 指定された名前に一致するテクスチャが登録されているかどうかを調べる
	virtual KTEXID findTextureRaw(const KPath &name, bool should_exist) const = 0;
	
	/// KTEXID が登録済みであれば、その名前を返す
	virtual KPath getTextureName(KTEXID tex) const = 0;

	virtual int getTextureCount() = 0;
	virtual std::vector<std::string> getTextureNameList() = 0;
	virtual std::vector<std::string> getRenderTextureNameList() = 0;

	virtual void clearTextures(bool remove_protected_textures=false) = 0;
	virtual void removeTexture(const KPath &name) = 0;
	virtual void removeTexture(KTEXID tex) = 0;
	virtual void removeTexturesByTag(const std::string &tag) = 0;
	virtual bool hasTexture(const KPath &name) const = 0;
	
	virtual void setProtect(KTEXID id, bool value) = 0;
	virtual bool getProtect(KTEXID id) = 0;

	virtual void setTag(KTEXID id, KName tag) = 0;
	virtual bool hasTag(KTEXID id, KName tag) const = 0;
	virtual const KNameList & getTags(KTEXID id) const = 0;

	/// name テクスチャに対して modifier を適用したときのテクスチャ名を返す
	virtual KPath makeTextureNameWithModifier(const KPath &name, int modifier) const = 0;

	/// modname に適用されている modifier を返す。未適用の場合は 0 を返す
	virtual int getTextureModifier(const KPath &modname) const = 0;

	/// KTextureBankCallback::on_videobank_modifier によって作成されたテクスチャを削除する
	virtual void clearTextureModifierCaches() = 0;

	/// KTextureBankCallback::on_videobank_modifier によって作成されたテクスチャのうち、
	/// modifier 番号が modifier_start 以上 modifier_start + count 未満のものを削除する
	virtual void clearTextureModifierCaches(int modifier_start, int count) = 0;

	virtual void guiTexture(const KPath &asset_path, int box_size) = 0;
	virtual void guiTextureInfo(KTEXID tex, int box_size, const KPath &texname) = 0;
	virtual void guiTextureHeader(const KPath &name, bool realsize, bool filter_rentex, bool filter_tex2d) = 0;
	virtual void guiTextureBank() = 0;
	virtual bool guiTextureSelector(const char *label, KTEXID *p_texture) = 0;
	virtual bool guiTextureSelector(const char *label, const KPath &selected, KPath *new_selected) = 0;
	virtual bool guiRenderTextureSelector(const char *label, const KPath &selected, KPath *new_selected) = 0;
};

class KSpriteBank {
public:
	virtual ~KSpriteBank() {}

	/// 新しいスプライトを追加する
	virtual bool addSpriteFromDesc(const KPath &name, KSpriteAuto sp, bool update_mesh=true) = 0;
	virtual bool addSpriteFromTextureRect(const KPath &spr_name, const KPath &tex_name, int x, int y, int w, int h, int ox, int oy) = 0;

	/// 新しいスプライトとテクスチャを追加する
	/// @param name スプライト名
	/// @param tex_name  テクスチャ名。これは既存のテクスチャ名ではなく、 img から新しく作成し登録テクスチャ名である
	/// @param img       テクスチャ画像
	/// @param ox, oy    ピボットの座標。ビットマップ座標系（左上を原点して Y 軸下向き、ピクセル単位）で指定する
	virtual bool addSpriteAndTextureFromImage(const KPath &spr_name, const KPath &tex_name, const KImage &img, int ox, int oy) = 0;

	/// 名前でスプライトを探す
	/// 見つからなかった場合、 should_exists が真であればエラーメッセージを出す
	virtual KSpriteAuto findSprite(const KPath &name, bool should_exists=true) = 0;

	/// スプライトのテクスチャを設定する
	virtual bool setSpriteTexture(const KPath &name, const KPath &texture) = 0;

	/// スプライトのテクスチャアトラスを取得する
	/// @param name スプライト名
	/// @param[out] u0, v0, u1, v1 テクスチャからの切り取り範を UV 座標で表したもの。テクスチャ左上を (0, 0) として右下を (1, 1) とする
	/// @return スプライトで使用しているテクスチャ
	virtual KTEXID getSpriteTextureAtlas(const KPath &name, float *u0, float *v0, float *u1, float *v1) = 0;

	virtual int getSpriteCount() = 0;
	virtual void removeSprite(const KPath &name) = 0;
	virtual void removeSpritesByTag(const std::string &tag) = 0;
	virtual void clearSprites() = 0;
	virtual bool hasSprite(const KPath &name) const = 0;
	virtual void guiSpriteBank(KTextureBank *texbank) = 0;
	virtual bool guiSpriteSelector(const char *label, KPath *path) = 0;
	virtual void guiSpriteInfo(const KPath &name, KTextureBank *texbank) = 0;
	virtual void guiSpriteTextureInfo(const KPath &name, KTextureBank *texbank) = 0;

	/// スプライトの生成元画像乗での座標（ビットマップ座標系。左上原点Y軸下向き）を指定して、
	/// スプライトのローカル座標（ピボット原点、Y軸上向き）を得る
	/// 指定されたスプライトが存在しない場合はゼロを返す
	virtual KVec3 bmpToLocal(const KPath &name, const KVec3 &bmp) = 0;

	/// 実際にスプライトとして表示されるときのスプライト画像を作成する
	virtual KImage exportSpriteImage(const KPath &sprite_path, KTextureBank *texbank, int modofier=0, KNode *node_for_mod=NULL) = 0;

	/// 実際に画面に描画する時のスプライト画像を反映させたテクスチャを得る
	virtual KTEXID getBakedSpriteTexture(const KPath &sprite_path, int modifier, bool should_exist, KTextureBank *texbank) = 0;
};

class KShaderBank {
public:
	virtual ~KShaderBank() {}
	virtual KPath getShaderName(KSHADERID shader) const = 0;
	virtual std::vector<std::string> getShaderNameList() const = 0;
	virtual void addShader(const KPath &name, KShaderAuto shader) = 0;

	/// HLSL で定義されたシェーダーを追加する
	virtual KSHADERID addShaderFromHLSL(const KPath &name, const char *code) = 0;
	virtual KSHADERID addShaderFromStream(KInputStream &input, const char *filename) = 0;

	/// GLSL で定義されたシェーダーを追加する
	virtual KSHADERID addShaderFromGLSL(const KPath &name, const char *vs_code, const char *fs_code) = 0;

	/// 名前でシェーダーを探す
	/// 見つからなかった場合、 should_exists が真であればエラーメッセージを出す
	virtual KSHADERID findShader(const KPath &name, bool should_exist=true) = 0;
	virtual KShaderAuto findShaderAuto(const KPath &name, bool should_exist=true) = 0;
	
	virtual void setTag(KSHADERID id, KName tag) = 0;
	virtual bool hasTag(KSHADERID id, KName tag) const = 0;
	virtual const KNameList & getTags(KSHADERID id) const = 0;

	virtual int getShaderCount() const = 0;
	virtual void removeShader(const KPath &name) = 0;
	virtual void removeShadersByTag(KName tag) = 0;
	virtual void clearShaders() = 0;
	virtual bool hasShader(const KPath &name) const = 0;

	virtual bool guiShaderSelector(const char *label, KSHADERID *p_shader) = 0;
	virtual bool guiShaderSelector(const char *label, const KPath &selected, KPath *new_selected) = 0;
};

class KFontBank {
public:
	virtual void clearFonts() = 0;

	/// フォントを作成して追加する
	/// @param alias このフォントにつける名前
	///
	/// @param filename フォントファイル名またはフォント名
	///    空文字列を指定した場合は、システムにインストールされているフォントから alias に一致するものを探す。
	///      "arialbd.ttf" を指定すると Arial フォントになる。
	///      "msgothic.ttc" を指定すると MSゴシックファミリーになる
	///      "ＭＳ ゴシック" を指定するとMSゴシックになる（ＭＳは全角だがスペースは半角なので注意！）
	///
	/// @param ttc_index
	///    TTC フォントが複数のフォントを含んでいる場合、そのインデックスを指定する。
	///    例えば MS ゴシックフォント msgothic.ttc をロードする場合は以下の値を指定する
	///    0 -- MSゴシック
	///    1 -- MS UI ゴシック
	///    2 -- MS P ゴシック
	///
	/// @return
	///    新しく追加したフォントを返す。
	///    既に同名フォントが追加済みである場合は、そのフォントを返す。
	///    どちらの場合でも、得られるフォントは借りた参照であり、呼び出し側で参照カウンタを減らす必要はない
	///    フォントの追加に失敗した場合は NULL を返す
	///
	/// @code
	///  addFontFromInstalledFonts("msgothic",    "msgothic.ttc", 0);
	///  addFontFromInstalledFonts("msgothic_ui", "msgothic.ttc", 1);
	///  addFontFromInstalledFonts("msgothic_p",  "msgothic.ttc", 2);
	/// @endcode
	virtual bool addFontFromStream(const std::string &alias, KInputStream &input, const std::string &filename, int ttc_index, KFont *out_font) = 0;
	virtual bool addFontFromFileName(const std::string &alias, const std::string &filename, int ttc_index, bool should_exists, KFont *out_font) = 0;
	virtual bool addFontFromInstalledFonts(const std::string &alias, const std::string &filename, int ttc_index, bool should_exists, KFont *out_font) = 0;

	/// 指定されたフォントを追加する。内部で参照カウンタを増やす
	/// 古い同名フォントがある場合は、古いフォントを削除してから追加する。
	/// 追加に失敗した場合は false を返す
	/// @param alias このフォントにつける名前
	/// @param font フォントオブジェクト
	virtual bool addFont(const std::string &alias, KFont &font) = 0;

	virtual void deleteFont(const std::string &alias) = 0;

	/// 指定された名前のフォントを返す。
	/// フォントがない場合、fallback 引数にしたがって NULL または代替フォントを返す
	/// @param alias フォント名(addFontでフォントを追加するときに指定した、ユーザー定義のフォント名）
	/// @param fallback  フォントが見つからなかったとき、代替フォントを返すなら true. NULL を返すなら false
	virtual KFont getFont(const std::string &alias, bool fallback=true) const = 0;

	/// なんでも良いから適当なフォントを取得したい時に使う。
	/// setDefaultFont によってデフォルトフォントが設定されていれば、それを返す。
	/// 未設定の場合は、最初に追加されたフォントを返す。
	/// フォントが全くロードされていない場合は、Arial フォントを返す
	virtual KFont getDefaultFont() const = 0;

	/// ロード済みの内部フォント一覧を得る
	virtual const std::vector<std::string> & getFontNames() const = 0;

	/// フォントが見つからなかった場合に使う代替フォントを指定する。
	/// このフォントは既に addFont で追加済みでなければならない。
	/// @param alias フォント名(addFontでフォントを追加するときに指定した、ユーザー定義のフォント名）
	virtual void setDefaultFont(const std::string &alias) = 0;
};

class KAnimationBank {
public:
	virtual ~KAnimationBank() {}
	virtual void guiClips() const = 0;
	virtual bool guiClip(KClipRes *clip) const = 0;
	virtual void clearClipResources() = 0;
	virtual void removeClipResource(const std::string &name) = 0;
	virtual void removeClipResourceByTag(const std::string &tag) = 0;
	virtual void addClipResource(const std::string &name, KClipRes *clip) = 0;
	virtual KClipRes * getClipResource(const std::string &name) const = 0;
	virtual KClipRes * find_clip(const std::string &clipname) = 0;
	virtual KPathList getClipNames() const = 0;
};


class KLuaBankCallback {
public:
	virtual void on_luabank_load(lua_State *ls, const std::string &name) = 0;
};

class KLuaBank {
public:
	KLuaBank();
	~KLuaBank();

	void setStorage(KStorage &storage);
	void setCallback(KLuaBankCallback *cb);

	lua_State * addEmptyScript(const std::string &name);
	lua_State * findScript(const std::string &name) const;
	lua_State * queryScript(const std::string &name, bool reload=false);
	lua_State * makeThread(const std::string &name);
	bool contains(const std::string &name) const;
	void remove(const std::string &name);
	void clear();
	bool addScript(const std::string &name, const std::string &code);

private:
	std::unordered_map<std::string, lua_State *> m_items;
	mutable std::recursive_mutex m_mutex;
	KLuaBankCallback *m_cb;
	KStorage m_storage;
};


class KVideoBank: public virtual KRef {
public:
	static KVideoBank * create();
	virtual ~KVideoBank() {}
	virtual KAnimationBank * getAnimationBank() = 0;
	virtual KTextureBank * getTextureBank() = 0;
	virtual KSpriteBank * getSpriteBank() = 0;
	virtual KShaderBank * getShaderBank() = 0;
	virtual KFontBank * getFontBank() = 0;
	virtual void removeData(const KPath &name) = 0;
	virtual void guiMeshInfo(const KMesh *mesh) = 0;
	virtual bool guiMaterialInfo(KMaterial *mat) = 0;
};




class KSpriteList {
public:
	struct ITEM {
		ITEM() {
			page = 0;
			layer = 0;
		}
		ITEM(const ITEM &src) {
			def = src.def;
			page = src.page;
			layer = src.layer;
		}
		KSpriteAuto def;
		int page;
		int layer;
	};
	KSpriteList() {
	}
	KPath m_tex_name;
	KImage m_tex_image;
	std::vector<ITEM> m_items;
};

enum KPaletteImportFlag {
	KPaletteImportFlag_MAKE_INDEXED_TRUE  = 0x01, // パレットがあるとき、最初のパレットを適用してRGBA画像として作成する
	KPaletteImportFlag_MAKE_INDEXED_FALSE = 0x02, // パレットが1つならばパレットを適用してRGBA画像として作成し、複数ある場合はインデックス画像として作成する
	KPaletteImportFlag_MAKE_INDEXED_AUTO  = 0x04, // 必ずインデックス画像として作成する
	KPaletteImportFlag_SAVE_PALETTE_TRUE  = 0x10, // パレットがあれば必ずパレット情報も保存する
	KPaletteImportFlag_SAVE_PALETTE_FALSE = 0x20, // 複数のパレットがあるときだけパレット情報を保存する
	KPaletteImportFlag_SAVE_PALETTE_AUTO  = 0x40, // パレットを保存しない
	KPaletteImportFlag_DEFAULT = KPaletteImportFlag_MAKE_INDEXED_AUTO | KPaletteImportFlag_SAVE_PALETTE_AUTO,
};
typedef int KPaletteImportFlags;


enum KGameUpdateBankFlag {
	KGameUpdateBankFlags_SAVE_RGB = 0x01,
};
typedef int KGameUpdateBankFlags;

void K_GameUpdateBank(const char *bankDir, const char *dataDir, KPathList *updated_files=NULL, KGameUpdateBankFlags flags=0);

class KGameImagePack {
public:

	/// Edge ファイルまたはキャッシュをロードしてスプライトリストを得る。
	/// paths でキャッシュ名が指定されていれば、まずキャッシュからのロードを試みる。
	/// ロードできなかった場合は paths で指定された edg ファイルから直接ロードし、可能ならばキャッシュフォルダ内にキャッシュを保存する。
	/// キャッシュを作成する時には write_cellsize と write_flags の値を使ってパックファイルを作成する。
	/// スプライトをロードできたなら true を返す。
	/// paths: ファイル名とフォルダ名の情報
	/// cellsize: ブロック化サイズ。キャッシュファイルを保存する時に使う。
	/// flags: キャッシュ生成用のフラグ。キャッシュファイルを保存する時に使う。
	static bool loadSpriteList(KStorage &storage, KSpriteList *sprites, const std::string &imageListName);

	static void makeSpritelistFromPack(const KImgPackR &pack, const KImage &pack_image, const KPath &tex_name, KSpriteList *sprites);
	static KImgPackR loadPackR_fromCache(KStorage &storage, const std::string &imageListName, KImage *image);
};


struct KGameEdgeLayerLabel {
	static const int MAXCMDS = 4;
	KPath name;
	KPath cmd[MAXCMDS];
	int numcmds;

	KGameEdgeLayerLabel() {
		numcmds = 0;
	}
};


/// ゲームにEDGEドキュメントを使う時の操作関数群。
/// EDGEドキュメントをゲーム用のアニメファイルとして使う場合に必要な操作を行う
/// ※EDGEドキュメントはドット絵ツール Edge2 の標準ファイル形式。詳細は以下を参照
/// http://takabosoft.com/edge2
///
class KGameEdgeBuilder {
public:
	/// 無視をあらわす色かどうかを判定
	static bool isIgnorableLabelColor(const KColor32 &color);

	/// 無視可能なレイヤーならば true を返す（レイヤーのラベルの色によって判別）
	static bool isIgnorableLayer(const KEdgeLayer *layer);

	/// 無視可能なページならば true を返す（ページのラベルの色によって判別）
	static bool isIgnorablePage(const KEdgePage *page);

	/// 無視可能なレイヤーを全て削除する
	static void removeIgnorableLayers(KEdgePage *page);

	/// 無視可能なページを全て削除する
	static void removeIgnorablePages(KEdgeDocument *edge);

	/// 無視可能なページ、レイヤーを全て削除する
	static void removeIgnorableElements(KEdgeDocument *edge);

	/// ページまたはレイヤーのラベル文字列に式が含まれていれば、それを読み取る
	static bool parseLabel(const KPath &label, KGameEdgeLayerLabel *out);

	/// Edgeドキュメントをロードする
	/// ※使用後は drop で参照カウンタを捨てる必要がある
	static bool loadFromFileName(KEdgeDocument *edge, const std::string &filename);
	static bool loadFromStream(KEdgeDocument *edge, KInputStream &file, const std::string &debugname);
	static bool loadFromFileInMemory(KEdgeDocument *edge, const void *data, size_t size, const std::string &debugname);
};


class KGamePath {
public:
	/// パス文字列 name に含まれる危険文字を一定の法則で置換し、
	/// ファイル名として使っても安全な文字列にした結果を返す
	static KPath escapeFileName(const KPath &name);

	/// 文字 c をアセット名として含めることができる文字かどうか。
	/// 利用可能な文字であれば true を返す。
	/// パス区切り文字など、名前として使用できない文字であれば false を返す
	static bool isSafeAssetChar(char c);

	/// 仮想ファイル名から、実際のファイル名を得る。仮想ファイル名とは、ファイル名の省略や、特定のファイルを表すための特別な識別子を許容するファイル名のこと
	/// name が省略または "AUTO" が指定されていれば、現在扱っている XML ファイル名から自動的に EDGE ファイル名を導き、それを返す
	/// それ以外の場合は現在のディレクトリを考慮したファイル名を返す
	static KPath evalPath(const KPath &expr, const KPath &curr_path, const KPath &def_ext);

	/// .edg ファイルから作成された .png を表すパス文字列を返す。
	/// ここでは、ひとつの edg につき一枚の png が対応するものとする
	///（ひとつの .edg から、それに含まれるすべての画像を展開したひとつの .png ができているものとする）
	/// 従って、ページ番号やレイヤー番号は考慮しない。
	/// .edg ファイルについては KEdgeFile.h を参照
	static KPath getEdgeAtlasPngPath(const KPath &edg_path);

	static KPath getTextureAssetPath(const KPath &base_path);

	/// 画像アーカイブ base_path の index1 ページの index2 レイヤーから作成されたスプライトを表すためのパス文字列を返す。
	/// 例えば chara/player.edg のページ[2] のレイヤー[0] から作成されたスプライトであれば、 "chara/player@2.0.sprite" を返す
	static KPath getSpriteAssetPath(const KPath &base_path, int index1, int index2);
};


struct SPRITE_ATTR {
	SPRITE_ATTR() {
		clear();
	}
	KBlend blend;
	float pivot_x;
	float pivot_y;
	int page;
	bool pivot_x_in_percent;
	bool pivot_y_in_percent;
	bool has_blend;
	bool has_pivot_x;
	bool has_pivot_y;

	void clear() {
		blend = KBlend_INVALID;
		pivot_x = 0;
		pivot_y = 0;
		pivot_x_in_percent = false;
		pivot_y_in_percent = false;
		page = -1;
		has_blend = false;
		has_pivot_x = false;
		has_pivot_y = false;
	}

	void readFromXmlAttr(KXmlElement *elm) {
		clear();

		page = elm->getAttrInt("page", -1);

		const char *blend_str = elm->getAttrString("blend");
		if (blend_str) {
			KBlend bl = KVideoUtils::strToBlend(blend_str, KBlend_INVALID);
			if (bl != KBlend_INVALID) {
				blend = bl;
				has_blend = true;
			}
		}
		const char *px_str = elm->getAttrString("pivotX");
		if (px_str) {
			KNumval numval(px_str);
			pivot_x = numval.numf;
			pivot_x_in_percent = numval.has_suffix("%");
			has_pivot_x = true;
		}

		const char *py_str = elm->getAttrString("pivotY");
		if (py_str) {
			KNumval numval(py_str);
			pivot_y = numval.numf;
			pivot_y_in_percent = numval.has_suffix("%");
			has_pivot_y = true;
		}
	}
	float getPivotXInPixels(int atlas_w) const {
		return pivot_x_in_percent ? (atlas_w * pivot_x / 100) : pivot_x;
	}
	float getPivotYInPixels(int atlas_h) const {
		return pivot_y_in_percent ? (atlas_h * pivot_y / 100) : pivot_y;
	}
};


class CXresLoaderCallback {
public:
	virtual bool onImageFilter(const std::string &filter, KImage &img) = 0;
	virtual void onLoadClip(KClipRes *clip) {}
};


class KXresLoader: public virtual KRef {
public:
	static KXresLoader * create(KStorage &storage, CXresLoaderCallback *cb);
	virtual void loadFromFile(const char *xml_name, bool should_exists) = 0;
	virtual void loadFromText(const char *raw_text, const char *xml_name) = 0;
};





// Edgeから直接アニメを作成する
bool K_makeClip(KClipRes **out_clip, KInputStream &edge_file, const KPath &edge_name, const KPath clip_name);


class KBank {
public:
	static void install();
	static void uninstall();
	static bool isInstalled();
	static KVideoBank * getVideoBank();
	static KTextureBank * getTextureBank();
	static KAnimationBank * getAnimationBank();
	static KSpriteBank * getSpriteBank();
	static KShaderBank * getShaderBank();
	static KFontBank * getFontBank();
};




} // namespace
