/// @file
/// Copyright (c) 2020 Heliodor
/// This software is released under the MIT License.
/// http://opensource.org/licenses/mit-license.php

#pragma once
#include "KImage.h"
#include "KMatrix.h"
#include "KRef.h"

#pragma region Core Video
#define MO_VIDEO_ENABLE_HALF_PIXEL  1

/// ピクセルパーフェクトにするために座標を調整する
///
/// テクセルからピクセルへの直接的なマッピング (Direct3D 9)
/// "https://docs.microsoft.com/ja-jp/previous-versions/direct-x/ee417850(v=vs.85)"
#define K_HALF_PIXEL 0.5f



namespace Kamilo {

struct KMaterial;
struct KDrawListItem;
struct K__TEXID { int dummy; };
struct K__SHADERID { int dummy; };

/// テクスチャ識別子
typedef K__TEXID * KTEXID;

/// シェーダー識別子
/// @see KVideo::createShaderFromHLSL
typedef K__SHADERID * KSHADERID;

enum KPrimitive {
	KPrimitive_POINTS,
	KPrimitive_LINES,
	KPrimitive_LINE_STRIP,
	KPrimitive_TRIANGLES,
	KPrimitive_TRIANGLE_STRIP,
	KPrimitive_TRIANGLE_FAN,
	KPrimitive_ENUM_MAX
};

enum KBlend {
	KBlend_INVALID = -1,
	KBlend_ONE,
	KBlend_ADD,
	KBlend_ALPHA,
	KBlend_SUB,
	KBlend_MUL,
	KBlend_SCREEN,
	KBlend_MAX,
	KBlend_ENUM_MAX
};

enum KFilter {
	KFilter_NONE,
	KFilter_LINEAR,
	KFilter_ENUM_MAX
};

enum KColorChannel {
	KColorChannel_A = 1,
	KColorChannel_R = 2,
	KColorChannel_G = 4,
	KColorChannel_B = 8,
	KColorChannel_RGB  = KColorChannel_R|KColorChannel_G|KColorChannel_B,
	KColorChannel_RGBA = KColorChannel_R|KColorChannel_G|KColorChannel_B|KColorChannel_A,
};
typedef int KColorChannels;

struct KVideoRect {
	float xmin;
	float ymin;
	float xmax;
	float ymax;

	KVideoRect() {
		xmin = ymin = xmax = ymax = 0;
	}
	KVideoRect(float _xmin, float _ymin, float _xmax, float _ymax) {
		xmin = _xmin;
		ymin = _ymin;
		xmax = _xmax;
		ymax = _ymax;
	}
	KVideoRect expand(float dx, float dy) const {
		return KVideoRect(xmin-dx, ymin-dy, xmax+dx, ymax+dy);
	}
};

struct KShaderArg {
	KShaderArg() {
		texture  = nullptr;
		color = KColor(1.0f, 1.0f, 1.0f, 1.0f);
		specular = KColor(0.0f, 0.0f, 0.0f, 0.0f);
		blend = KBlend_ALPHA;
	}
	KTEXID texture;
	KBlend blend;
	KColor color;
	KColor specular;
};

/// 頂点構造体
///
/// この構造体のメモリ配置は Direct3D9 の頂点フォーマット
/// D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1 と完全な互換性がある
struct KVertex {
	KVec3 pos;
	KColor32 dif32; ///< ディフューズ色 テクスチャ色に乗算する
	KColor32 spe32; ///< スペキュラ色。テクスチャ色に加算する。
	KVec2 tex; /// テクスチャ座標1
	KVec2 tex2; /// テクスチャ座標2

	KVertex() {
		dif32 = KColor32::WHITE;
	}
};

class KTexture: public virtual KRef {
public:
	enum Format {
		FMT_NONE,
		FMT_ARGB32,  // A8 R8 G8 B8 (uint8_t)
		FMT_ARGB64F, // A16 R16 G16 B16 (half float)
	};
	struct Desc {
		/// テクスチャのサイズ
		int w;
		int h;
	
		/// このテクスチャの元画像のサイズ
		/// 元画像のサイズが 2^n でなかった場合、テクスチャ化するときに右端と下端に余白が追加される場合がある。
		/// このメソッドは余白調整する前のビットマップ画像のサイズが必要な時に使う。
		int original_w;
		int original_h;

		/// このテクスチャ上で、元画像が占めている部分を uv 値で示したもの。
		/// w / original_w, h / original_h と同じ。
		float original_u;
		float original_v;

		/// テクスチャのピッチバイト数（ある行の先頭ピクセルから、その次の行の先頭ピクセルに移動するためのバイト数）
		int pitch;
	
		int size_in_bytes;
	
		/// このテクスチャがレンダーテクスチャかどうか
		bool is_render_target;

		KColorFormat pixel_format;

		/// Direct3D を使用している場合、
		/// このテクスチャに関連付けられた IDirect3DTexture9 オブジェクト
		void *d3dtex9;

		Desc() {
			w = 0;
			h = 0;
			original_w = 0;
			original_h = 0;
			original_u = 0;
			original_v = 0;
			pitch = 0;
			size_in_bytes = 0;
			is_render_target = false;
			pixel_format = (KColorFormat)0;
			d3dtex9 = NULL;
		}
	};
	static KTexture * create(int w, int h, Format f=FMT_ARGB32);
	static KTexture * createFromImage(const KImage &image);
	static KTexture * createRenderTarget(int w, int h, Format f);
	virtual int getWidth() const = 0;
	virtual int getHeight() const = 0;
	virtual Format getFormat() const = 0;
	virtual KTEXID getId() const = 0;
	virtual bool isRenderTarget() const = 0;
	virtual int getSizeInBytes() const = 0;
	virtual void fill(const KColor &color) = 0;
	virtual void fillEx(const KColor &color, KColorChannels channels) = 0;
	virtual void getDesc(Desc *desc) const = 0;
	virtual void * lockData() = 0;
	virtual void unlockData() = 0;
	virtual void * getDirect3DTexture9() = 0;
	virtual KImage exportTextureImage(int channel=-1) = 0;
	virtual void writeImageToTexture(const KImage &image, float u, float v) = 0;
	virtual bool writeImageFromBackBuffer() = 0;
	virtual void clearRenderTargetStencil(int stencil) = 0;
	virtual void clearRenderTargetDepth(float depth) = 0;
	virtual KVec2 getTextureUVFromOriginalUV(const KVec2 &orig_uv) = 0;
	virtual KVec2 getTextureUVFromOriginalPoint(const KVec2 &pixel) = 0;
	virtual void blit(KTexture *src, KMaterial *mat) = 0;
	virtual void blitEx(KTexture *src, KMaterial *mat, const KVideoRect *src_rect, const KVideoRect *dst_rect, const KMatrix4 *transform) = 0;
	virtual KVec4 getPixelValue(int x, int y) = 0;
};

class KShader: public virtual KRef {
public:
	struct Desc {
		int num_technique;
		char type[16];
		void *d3dxeff;

		Desc() {
			num_technique = 0;
			type[0] = '\0';
			d3dxeff = NULL;
		}
	};
	static KShader * create(const char *hlsl_u8, const char *name);

	virtual KSHADERID getId() const = 0;
	virtual void getDesc(Desc *desc) const = 0;
};


class KVideo {
public:
	enum StencilFunc {
		STENCILFUNC_NEVER,        // NEVER
		STENCILFUNC_LESS,         // stencil <  val
		STENCILFUNC_EQUAL,        // stencil == val
		STENCILFUNC_LESSEQUAL,    // stencil <= val
		STENCILFUNC_GREATER,      // stencil >  val
		STENCILFUNC_NOTEQUAL,     // stencil != val
		STENCILFUNC_GREATEREQUAL, // stencil >= val
		STENCILFUNC_ALWAYS,       // ALWAYS
	};
	enum StencilOp {
		STENCILOP_KEEP,
		STENCILOP_REPLACE,
		STENCILOP_INC,
		STENCILOP_DEC,
	};
	enum Param {
		PARAM_NONE,       ///< 何もしない
		PARAM_HAS_SHADER, ///< プログラマブルシェーダーが利用可能か？ {int}
		PARAM_HAS_HLSL,   ///< シェーダー言語としてHLSLが利用可能か？ {int}
		PARAM_HAS_GLSL,   ///< シェーダー言語としてGLSLが利用可能か？ {int}
		PARAM_IS_FULLSCREEN, ///< フルスクリーンかどうか？ {int} 1 or 0
		PARAM_D3DDEV9,    ///< IDirect3DDevice9 オブジェクト {IDirect3DDevice9*}
		PARAM_HWND,       ///< HWND
		PARAM_DEVICELOST, ///< デバイスロストしているか？ {int}
		PARAM_VS_VER,     ///< 頂点シェーダーのバージョン番号 {int major, int minor}
		PARAM_PS_VER,     ///< ピクセルシェーダーのバージョン番号　{int major, int minor}
		PARAM_MAXTEXSIZE, ///< 作成可能な最大テクスチャサイズ {int w, int h}
		PARAM_DRAWCALLS,  ///< 描画関数の累積呼び出し回数。この値を取得すると内部カウンタがリセットされる。{int 回数}
		
		/// 必要なテクスチャサイズ。
		/// この値は、ゲームエンジンの要求する最低スペック要件になる。
		/// このサイズ未満のテクスチャにしか対応しない環境で実行しようとした場合、正しい描画は保証されない
		PARAM_MAX_TEXTURE_REQUIRE,

		/// テクスチャのサイズ制限
		/// 実際の環境とは無関係に適用する
		/// 0 を指定すると制限しない
		PARAM_MAX_TEXTURE_SUPPORT,

		/// 正方形のテクスチャしか作れないようにする。
		/// チープな環境をエミュレートするために使う
		PARAM_USE_SQUARE_TEXTURE_ONLY,

		/// ピクセルシェーダーが使えないようにする。
		/// チープな環境をエミュレートするために使う
		PARAM_DISABLE_PIXEL_SHADER,

		/// ビデオアダプタ名
		PARAM_ADAPTERNAME,
	};

	static bool init(void *hWnd, void *d3d9, void *d3ddev9);
	static void shutdown();
	static bool isInit();

	static void setParameter(Param param, intptr_t data);
	static void getParameter(Param param, void *data);
	static void command(const char *cmd);

	static KTEXID createTexture(int w, int h, KTexture::Format fmt=KTexture::FMT_ARGB32);
	static KTEXID createTextureFromImage(const KImage &img, KTexture::Format fmt=KTexture::FMT_ARGB32);
	static KTEXID createRenderTexture(int w, int h, KTexture::Format fmt=KTexture::FMT_ARGB32);
	static void deleteTexture(KTEXID tex);
	static KTexture * findTexture(KTEXID tex);
	static void fill(KTEXID target, const KColor &color, KColorChannels channels=KColorChannel_RGBA);

	static KSHADERID createShaderFromHLSL(const char *code, const char *name);
	static void deleteShader(KSHADERID s);
	static KShader * findShader(KSHADERID s);
	static void setShader(KSHADERID s);
	static void setShaderIntArray(const char *name, const int *values, int count);
	static void setShaderInt(const char *name, int value);
	static void setShaderFloatArray(const char *name, const float *values, int count);
	static void setShaderFloat(const char *name, float value);
	static void setShaderTexture(const char *name, KTEXID tex);
	static bool getShaderDesc(KSHADERID s, KShader::Desc *desc);
	static void setDefaultShaderParams(const KShaderArg &arg);
	static void beginShader();
	static void endShader();

	static void pushRenderState();
	static void popRenderState();
	static void pushRenderTarget(KTEXID render_target);
	static void popRenderTarget();
	static void setViewport(int x, int y, int w, int h);
	static void getViewport(int *x, int *y, int *w, int *h);
	static void setColorWriteMask(KColorChannels channels);

	static void setStencilEnabled(bool value);
	static void setStencilFunc(StencilFunc func, int val, StencilOp pass_op);
	static void setDepthTestEnabled(bool value);

	static void setProjection(const KMatrix4 &m);
	static void setTransform(const KMatrix4 &m);
	static void setFilter(KFilter filter);
	static void setBlend(KBlend blend);
	static void setColor(uint32_t color);
	static void setSpecular(uint32_t specular);
	static void setTexture(KTEXID texture);
	static void setTextureAddressing(bool wrap);
	static void setTextureAndColors();

	static bool canFullscreen(int w, int h);
	static bool resetDevice(int w, int h, int fullscreen);

	// resetDevice を使わずに外部から IDirect3DDevice9 をリセットした場合に
	// KVideo の内部情報を正しく更新するために呼ぶ。
	static void resetDevice_lost();
	static void resetDevice_reset();

	static bool shouldReset();
	static bool beginScene();
	static bool endScene();
	static void setupDeviceStates();
	static void clearColor(const KColor &color);
	static void clearDepth(float z);
	static void clearStencil(int s);
	static void drawTexture(KTEXID src, const KVideoRect *src_rect, const KVideoRect *dst_rect, const KMatrix4 *transform);
	static void drawUserPtrV(const KVertex *vertices, int count, KPrimitive primitive);
	static void drawIndexedUserPtrV(const KVertex *vertices, int vertex_count, const int *indices, int index_count, KPrimitive primitive);

	static KImage getBackbufferImage();

#if 1
	static int tex_getWidth(KTEXID id) {
		KTexture *tex = findTexture(id);
		return tex ? tex->getWidth() : 0;
	}
	static int tex_getHeight(KTEXID id) {
		KTexture *tex = findTexture(id);
		return tex ? tex->getHeight() : 0;
	}
	static KTexture::Format tex_getFormat(KTEXID id) {
		KTexture *tex = findTexture(id);
		return tex ? tex->getFormat() : KTexture::FMT_NONE;
	}
	static KTEXID tex_getId(KTEXID id) {
		KTexture *tex = findTexture(id);
		return tex ? tex->getId() : 0;
	}
	static bool tex_isRenderTarget(KTEXID id) {
		KTexture *tex = findTexture(id);
		return tex ? tex->isRenderTarget() : false;
	}
	static int tex_getSizeInBytes(KTEXID id) {
		KTexture *tex = findTexture(id);
		return tex ? tex->getSizeInBytes() : 0;
	}
	static void tex_fill(KTEXID id, const KColor &color) {
		KTexture *tex = findTexture(id);
		if (tex) tex->fill(color);
	}
	static void tex_fillEx(KTEXID id, const KColor &color, KColorChannels channels=KColorChannel_RGBA) {
		KTexture *tex = findTexture(id);
		if (tex) tex->fillEx(color, channels);
	}
	static void tex_getDesc(KTEXID id, KTexture::Desc *desc) {
		KTexture *tex = findTexture(id);
		if (tex) tex->getDesc(desc);
	}
	static void * tex_lockData(KTEXID id) {
		KTexture *tex = findTexture(id);
		return tex ? tex->lockData() : 0;
	}
	static void tex_unlockData(KTEXID id) {
		KTexture *tex = findTexture(id);
		if (tex) tex->unlockData();
	}
	static void * tex_getDirect3DTexture9(KTEXID id) {
		KTexture *tex = findTexture(id);
		return tex ? tex->getDirect3DTexture9() : 0;
	}
	static KImage tex_exportTextureImage(KTEXID id, int channel=-1) {
		KTexture *tex = findTexture(id);
		return tex ? tex->exportTextureImage() : KImage();
	}
	static void tex_writeImageToTexture(KTEXID id, const KImage &image, float u, float v) {
		KTexture *tex = findTexture(id);
		if (tex) tex->writeImageToTexture(image, u, v);
	}
	static bool tex_writeImageFromBackBuffer(KTEXID id) {
		KTexture *tex = findTexture(id);
		return tex ? tex->writeImageFromBackBuffer() : 0;
	}
	static void tex_clearRenderTargetStencil(KTEXID id, int stencil) {
		KTexture *tex = findTexture(id);
		if (tex) tex->clearRenderTargetStencil(stencil);
	}
	static void tex_clearRenderTargetDepth(KTEXID id, float depth) {
		KTexture *tex = findTexture(id);
		if (tex) tex->clearRenderTargetDepth(depth);
	}
	static KVec2 tex_getTextureUVFromOriginalUV(KTEXID id, const KVec2 &orig_uv) {
		KTexture *tex = findTexture(id);
		return tex ? tex->getTextureUVFromOriginalUV(orig_uv) : KVec2();
	}
	static KVec2 tex_getTextureUVFromOriginalPoint(KTEXID id, const KVec2 &pixel) {
		KTexture *tex = findTexture(id);
		return tex ? tex->getTextureUVFromOriginalPoint(pixel) : KVec2();
	}
	static void tex_blit(KTEXID id, KTexture *src, KMaterial *mat) {
		KTexture *tex = findTexture(id);
		if (tex) tex->blit(src, mat);
	}
	static void tex_blitEx(KTEXID id, KTexture *src, KMaterial *mat, const KVideoRect *src_rect, const KVideoRect *dst_rect, const KMatrix4 *transform) {
		KTexture *tex = findTexture(id);
		if (tex) tex->blitEx(src, mat, src_rect, dst_rect, transform);
	}
	static KVec4 tex_getPixelValue(KTEXID id, int x, int y) {
		KTexture *tex = findTexture(id);
		return tex ? tex->getPixelValue(x, y) : KVec4();
	}
#endif
};
#pragma endregion // Core Video





class KMaterialCallback {
public:
	/// シェーダー変数をセットするときに呼ばれる。
	/// 既に KVideo::setShader によりシェーダーが選択済みになっているので、
	/// KVideo::setShaderInt などを呼ぶだけでよい
	virtual void onMaterial_SetShaderVariable(const KMaterial *material) {
		/*
		// サンプル
		KTEXID tex = KVideo::findTexture(...);
		KVideo::setShaderTexture("MyTexture", tex);
		KVideo::setShaderFloat("MyTime", 0);
		*/
	}

	/// KVideo でシェーダーを適用するときに呼ばれる
	virtual void onMaterial_Begin(const KMaterial *material) {}

	/// KVideo でシェーダーを解除するときに呼ばれる
	virtual void onMaterial_End(const KMaterial *material) {}

	/// 描画リストの内容を描画するときに呼ばれる
	virtual void onMaterial_Draw(const KDrawListItem *drawlist, bool *done) {
		/*
		// 無加工描画するサンプル（KVideoを使用）
		KVideo::pushRenderState();
		KVideo::setProjection(item->projection);
		KVideo::setTransform(item->transform);
		KVideoUtils::beginMaterial(&item->material);
		KVideoUtils::drawMesh(item->cb_mesh, item->cb_submesh);
		KVideoUtils::endMaterial(&mat);
		KVideo::popRenderState();

		// 無加工描画するサンプル（CShaderEffectorを使用）
		CShaderEffector eff;
		eff.setSizeByTexture(KVideo::findTexture(drawlist->material.texture));
		eff.copy(KVideo::findTexture(drawlist->material.texture));
		KMaterial mat = drawlist->material; // COPY
		eff.renderItem(drawlist, drawlist->transform, mat);
		*/
	}
};

struct KMaterial {
	KMaterial() {
		color = KColor::WHITE;
		specular = KColor::ZERO;
		texture = NULL;
		shader = NULL;
		filter = KFilter_NONE;
		blend = KBlend_ALPHA;
		cb = NULL;
		cb_data = NULL;
		wrap = false;
	}
	void clear() {
		color = KColor::WHITE;
		specular = KColor::ZERO;
		texture = NULL;
		shader = NULL;
		filter = KFilter_NONE;
		blend = KBlend_ALPHA;
		cb = NULL;
		cb_data = NULL;
		wrap = false;
	}
	KColor color; // diffuse
	KColor specular;
	KFilter filter;
	KTEXID texture;
	KSHADERID shader;
	KBlend blend;
	KMaterialCallback *cb;
	void *cb_data;
	bool wrap; // Use Wrap texture addressing mode ? (False by default)

	uint32_t color32() const {
		return KColor32(color).toUInt32();
	}
	uint32_t specular32() const {
		return KColor32(specular).toUInt32();
	}

	// マテリアル m1 と m2 が同一とみなせるかどうか（一括描画しても良いか）調べる
	// この関数が true を返してた場合、マテリアル m2 が指定されたオブジェクトはマテリアル m1 で描画される可能性がある（逆もあり）
	static bool is_compatible(const KMaterial &m1, const KMaterial &m2, float color_tolerance=0.0f) {
		// コールバックが指定されている場合は常に異なるマテリアルとみなす
		if (m1.cb || m2.cb) {
			return false;
		}
		if (m1.shader != m2.shader) {
			return false;
		}
		if (m1.color!=m2.color && !m1.color.equals(m2.color, color_tolerance)) {
			return false;
		}
		if (m1.specular!=m2.specular && !m1.specular.equals(m2.specular, color_tolerance)) {
			return false;
		}
		if (m1.texture != m2.texture) {
			return false;
		}
		if (m1.blend != m2.blend) {
			return false;
		}
		if (m1.filter != m2.filter) {
			return false;
		}
		return true;
	}
};

struct KSubMesh {
	int start; ///< 開始頂点インデックス
	int count; ///< 頂点数
	KPrimitive primitive; ///< 頂点結合タイプ
	KMaterial material;
	KMatrix4 transform;
	KSubMesh() {
		start = 0;
		count = 0;
		primitive = KPrimitive_TRIANGLE_STRIP;
	}
};

class KVertexArray {
	std::vector<KVertex> m_Vec;
	int m_Size;
public:
	KVertexArray() {
		m_Size = 0;
	}
	void clear() {
		m_Size = 0;
	}
	void resize(int cnt) {
		// vecotor::resize を使って reserve と似たようなことをやる。
		// vecotor::resize すると KVertex のコンストラクタが走ってしまうため、
		// 極力 resize を呼ばないように実装する
		m_Size = cnt;
		if ((int)m_Vec.size() < m_Size) {
			m_Vec.resize(m_Size);
		}
	}
	int size() const {
		return m_Size;
	}
	const KVertex * data() const {
		return m_Vec.data();
	}
	KVertex * data() {
		return m_Vec.data();
	}
};

class KMesh {
public:
	KMesh();

	/// 頂点をすべて削除する
	void clear();

	void setVertexCount(int n);

	/// 他のメッシュをコピーする
	bool copyFrom(const KMesh *other);

	/// サブメッシュを追加する
	bool addSubMesh(const KSubMesh &sub);

	/// サブメッシュ数を返す
	int getSubMeshCount() const;

	/// サブメッシュを取得する
	/// 範囲外のインデックスを指定した場合は初期状態の KSubMesh を返す
	const KSubMesh * getSubMesh(int index) const;
	KSubMesh * getSubMesh(int index);

	/// 現在の頂点配列によって定義される AABB を取得する。
	/// 取得に成功した場合は結果を min_point, max_point にセットして true を返す
	/// 頂点配列が存在しないかサイズが 0 の場合は false を返す
	bool getAabb(int offset, int count, KVec3 *minpoint, KVec3 *maxpoint) const;
	bool getAabb(KVec3 *minpoint, KVec3 *maxpoint) const;

	/// 頂点個数を返す
	int getVertexCount() const;

	/// インデックス個数を返す
	int getIndexCount() const;

	void setPosition  (int index, const KVec3  &pos) { setPositions (index, &pos, 0, 1); }
	void setColor     (int index, const KColor &dif) { setColors    (index, &dif, 0, 1); }
	void setSpecular  (int index, const KColor &spe) { setSpeculars (index, &spe, 0, 1); }
	void setTexCoord  (int index, const KVec2  &uv ) { setTexCoords (index, &uv,  0, 1); }
	void setTexCoords2(int index, const KVec2  &uv ) { setTexCoords2(index, &uv,  0, 1); }
	void setColorInt   (int index, const KColor32 &dif) { setColorInts (index, &dif, 0, 1); }
	void setSpecularInt(int index, const KColor32 &spe) { setSpecularInts(index, &spe, 0, 1); }

	void setPositions(int offset, const KVec3 *pos, int stride, int count);
	void setColors(int offset, const KColor *color, int stride, int count);
	void setSpeculars(int offset, const KColor *specular, int stride, int count);
	void setTexCoords(int offset, const KVec2 *uv, int stride, int count);
	void setTexCoords2(int offset, const KVec2 *uv, int stride, int count);
	void setColorInts(int offset, const KColor32 *color32, int stride, int count);
	void setSpecularInts(int offset, const KColor32 *specular32, int stride, int count);
	void setIndices(int offset, const int *indices, int count);
	void setVertices(int offset, const KVertex *v, int count);

	KVec3  getPosition (int index) const;
	KColor getColor    (int index) const;
	KColor getSpecular (int index) const;
	KVec2  getTexCoord (int index) const;
	KVec2  getTexCoord2(int index) const;
	KColor32 getColor32(int index) const;
	KColor32 getSpecular32(int index) const;

	const KVec3 * getPositions(int offset=0) const;
	const KVec2 * getTexCoords1(int offset=0) const;
	const KVec2 * getTexCoords2(int offset=0) const;
	const KColor32 * getColorInts(int offset=0) const;
	const KColor32 * getSpecularInts(int offset=0) const;

	const KVertex * getVertices(int offset=0) const;
	const int * getIndices(int offset=0) const;

	/// 頂点配列をロックして、書き込み可能なポインタを返す
	KVertex * lock(int offset, int count);
	
	/// 頂点配列のロックを解除する
	void unlock();

	/// インデックス配列をロックして、書き込み可能なポインタを返す
	int * lockIndices(int offset, int count);
	
	/// インデックス配列のロックを解除する
	void unlockIndices();

private:
	int readPositionsFloat3(int read_offset, int read_count, void *dest, int dest_stride) const;
	int readColorsFloat4(int read_offset, int read_count, void *dest, int dest_stride) const;
	int readSpecularsFloat4(int read_offset, int read_count, void *dest, int dest_stride) const;
	int readTexCoordsFloat2_1(int read_offset, int read_count, void *dest, int dest_stride) const;
	int readTexCoordsFloat2_2(int read_offset, int read_count, void *dest, int dest_stride) const;
	int readColorsUint32(int read_offset, int read_count, void *dest, int dest_stride) const;
	int readSpecularsUint32(int read_offset, int read_count, void *dest, int dest_stride) const;

	std::vector<int> m_indices;
	KVertexArray m_vertices;
	std::vector<KSubMesh> m_submeshes;

	mutable std::vector<KVec3> m_tmppos;
	mutable std::vector<KVec2> m_tmpuv1;
	mutable std::vector<KVec2> m_tmpuv2;
	mutable std::vector<KColor32> m_tmpdif;
	mutable std::vector<KColor32> m_tmpspe;
	mutable KVec3 m_whole_aabb_min;
	mutable KVec3 m_whole_aabb_max;

	enum {
		POS = 0x01,
		DIF = 0x02,
		SPE = 0x04,
		UV1 = 0x08,
		UV2 = 0x10,
		IDX = 0x20,
		AABB = 0x40,
		ALL = 0xFF,
	};
	mutable int m_changed;
};

namespace MeshShape {
	// p0 矩形の最小座標
	// p1 矩形の最大座標
	// uv0 p0 に対応するUV
	// uv1 p1 に対応するUV
	// color 色
	// uvRot90  true にするとテクスチャUVを90度回転させる
	void makeRect(KMesh *mesh, const KVec2 &p0, const KVec2 &p1, const KVec2 &uv0, const KVec2 &uv1, const KColor &color, bool uvRot90=false);

	// skew_dxdy = dx/dy
	void makeRectSkew(KMesh *mesh, const KVec2 &p0, const KVec2 &p1, const KVec2 &uv0, const KVec2 &uv1, const KColor &color, float skew_dxdy, float skew_dudv);
}

struct KDrawListItem {
	KMaterial material;
	KMatrix4 projection;
	KMatrix4 transform;
	KPrimitive prim;
	int start, count, index_number_offset;
	bool with_index;

	KVideo::StencilFunc stencil_fn;
	KVideo::StencilOp stencil_op;
	int stencil_ref;
	bool with_stencil;
	bool color_write;

	// ユーザーが自分で描画する時に使う情報。
	// マテリアルのコールバックなしで描画するなら、個々の情報は使わない
	const KMesh *cb_mesh;
	int cb_submesh;

	KDrawListItem() {
		index_number_offset = 0;
		start = count = 0;
		prim = KPrimitive_ENUM_MAX; // 無効な値を入れておく
		with_index = false;
		color_write = true;
		cb_mesh = NULL;
		cb_submesh = 0;

		with_stencil = false;
		stencil_fn = KVideo::STENCILFUNC_ALWAYS;
		stencil_op = KVideo::STENCILOP_REPLACE;
		stencil_ref = 0;
	}
};

/// 類似する描画コマンドを結合して、ドローコールを節約する
class KDrawList {
public:
	KDrawList();
	~KDrawList();

	void destroy();

	/// 描画アイテム及びレンダーステートを初期化する
	void clear();

	/// 描画アイテム数を返す
	size_t size() const;

	/// マテリアルを設定する。
	/// ここでの設定内容は clear() または endList() が呼ばれるまで持続する
	void setMaterial(const KMaterial &m);

	void setColorWrite(bool value);
	void setStencil(bool value);
	void setStencilOpt(KVideo::StencilFunc fn, int ref, KVideo::StencilOp op);

	/// 変形行列を設定する。
	/// ここでの設定内容は clear() または endList() が呼ばれるまで持続する
	void setTransform(const KMatrix4 &m);

	/// 投影行列を設定する。
	/// ここでの設定内容は clear() または endList() が呼ばれるまで持続する
	void setProjection(const KMatrix4 &m);

	/// プリミティブタイプを設定する。
	/// ここでの設定内容は clear() または endList() が呼ばれるまで持続する
	void setPrimitive(KPrimitive prim);

	/// 頂点配列を指定し、描画リストに新しい描画アイテムを追加する
	void addVertices(const KVertex *v, int count);
	
	/// インデックス付き頂点配列を指定し、描画リストに新しい描画アイテムを追加する
	void addVerticesWithIndex(const KVertex *v, int vcount, const int *i, int icount);

	/// メッシュを指定し、描画リストに新しい描画アイテムを追加する
	/// submesh 描画するサブメッシュ番号
	void addMesh(const KMesh *mesh, int submesh);

	/// 描画リストの終端処理を行う
	void endList();

	/// 描画リストの内容を現在のバックバッファまたはレンダーターゲットに描画する
	/// @see _pushRenderTarget, KVideo::popRenderTarget
	void draw();

	KDrawListItem & operator[](size_t index);

private:
	/// 前回と今回の描画設定を比較する。
	/// 同一のレンダーステートになっていて結合描画可能であれば true を返す
	bool is_changed();

	std::vector<KDrawListItem> m_items;
	KMesh m_unimesh;
	KDrawListItem m_next;
	KDrawListItem m_last;
	bool m_changed;

	std::vector<KVec3> m_tmp_pos;
	std::vector<KColor32> m_tmp_dif;
	std::vector<KColor32> m_tmp_spe;
	std::vector<KVec2> m_tmp_uv1;
	std::vector<KVec2> m_tmp_uv2;
};

extern const KColor K_GIZMO_LINE_COLOR; // KColor(0, 1, 0, 1);

class KGizmo {
public:
	KGizmo();
	void addLine(const KMatrix4 &matrix, const KVec3 &a, const KVec3 &b, const KColor &color=K_GIZMO_LINE_COLOR);
	void addLine(const KMatrix4 &matrix, const KVec3 &a, const KVec3 &b, const KColor &color, const KColor &color2);
	void addLineDash(const KMatrix4 &matrix, const KVec3 &a, const KVec3 &b, const KColor &color=K_GIZMO_LINE_COLOR);
	void addLine2D(const KMatrix4 &matrix, float x0, float y0, float x1, float y1, const KColor &color=K_GIZMO_LINE_COLOR);
	void addAabbSheared(const KMatrix4 &matrix, const KVec3 &a, const KVec3 &b, float xshear, const KColor &color=K_GIZMO_LINE_COLOR);
	void addAabb(const KMatrix4 &matrix, const KVec3 &a, const KVec3 &b, const KColor &color=K_GIZMO_LINE_COLOR);
	void addAabbFrame2D(const KMatrix4 &matrix, float x0, float y0, float x1, float y1, const KColor &color=K_GIZMO_LINE_COLOR);
	void addAabbFill2D(const KMatrix4 &matrix, float x0, float y0, float x1, float y1, const KColor &color=K_GIZMO_LINE_COLOR);
	void addAabbEllipse2D(const KMatrix4 &matrix, float x0, float y0, float x1, float y1, const KColor &color=K_GIZMO_LINE_COLOR);
	void addAxis(const KMatrix4 &matrix, const KVec3 &pos, float size);
	void addMesh(const KMatrix4 &matrix, KMesh *mesh, const KColor &color=K_GIZMO_LINE_COLOR);
	void addCircle(const KMatrix4 &matrix, const KVec3 &p, float r, int axis, const KColor &color=K_GIZMO_LINE_COLOR);
	void addArrow(const KMatrix4 &matrix, const KVec3 &from, const KVec3 &to, float head, const KColor &color=K_GIZMO_LINE_COLOR);
	void newFrame();
	void render(const KMatrix4 &projection);
private:
	void add_aabb_2d(const KMatrix4 &matrix, float x0, float y0, float x1, float y1, float z, const KColor &color);
	KDrawList m_drawlist;
};

namespace KVideoUtils {

/// ブレンド値とブレンド名の相互変換を行う
KBlend strToBlend(const char *s, KBlend def);
bool strToBlend(const char *s, KBlend *blend);

void translateInPlace(KMatrix4 &m, float x, float y, float z);

void translateHalfPixelInPlace(KMatrix4 &m);
void translateHalfPixelInPlace(KVec3 &m);
void translateHalfPixelInPlace(KVec2 &m);

/// 変形行列の平行移動成分だけを整数化する
void translateFloorInPlace(KMatrix4 &m);
void translateFloorInPlace(KVec3 &m);
void translateFloorInPlace(KVec2 &m);


/// テクスチャの内容をコピーする。コピーする際にマテリアルを適用することができる。
/// KVideoUtils::blit とは異なって複数のマテリアルを指定することができる。
/// マテリアルは指定された順番通りに適用されていく
/// @param dst コピー先のレンダーテクスチャ
/// @param src コピー元のテクスチャ
/// @param materials コピーの際に適用するマテリアル。不要な場合は NULL でよい。
/// @param materials_count  適用するマテリアルの個数
void blitArray(KTexture *dst, KTexture *src, KMaterial **materials, int materials_count);
void blit(KTexture *dest, KTexture *src, KMaterial *mat);
void blitEx(KTexture *dest, KTexture *src, KMaterial *mat, const KVideoRect *src_rect, const KVideoRect *dst_rect, const KMatrix4 *transform);
void beginMaterial(const KMaterial *material);
void endMaterial(const KMaterial *material);
void drawMesh(const KMesh *mesh, int start, int count, KPrimitive primitive);
void drawMesh(const KMesh *mesh, int submesh_index);


} // KVideoUtils




class CMeshBuf {
public:
	CMeshBuf();
	void clear();
	size_t size() const;

	void resize2(KMesh *mesh, int vertexcount, int indexcount);
	
	void setIndex(int index, int vertex);

	void setPosition(int index, const KVec3 &pos);
	void setPosition(int index, float x, float y, float z) { setPosition(index, KVec3(x, y, z)); }
	void setColor(int index, float r, float g, float b, float a) { setColor32(index, KColor32(KColor(r, g, b, a))); }
	void setColor(int index, const KColor &color)                { setColor32(index, KColor32(color)); }
	void setColor32(int index, int r, int g, int b, int a)       { setColor32(index, KColor32(r, g, b, a)); }
	void setColor32(int index, const KColor32 &color);
	void setSpecular(int index, float r, float g, float b, float a=1.0f) { setSpecular32(index, KColor32(KColor(r, g, b, a))); }
	void setSpecular(int index, const KColor &color)                     { setSpecular32(index, KColor32(color)); }
	void setSpecular32(int index, int r, int g, int b, int a=255)        { setSpecular32(index, KColor32(r, g, b, a)); }
	void setSpecular32(int index, const KColor32 &color);
	void setTexCoord(int index, const KVec2 &uv);
	void setTexCoord(int index, float u, float v) { setTexCoord(index, KVec2(u, v)); }
	void setTexCoord2(int index, const KVec2 &uv);
	void setTexCoord2(int index, float u, float v) { setTexCoord2(index, KVec2(u, v)); }
	void copyVertex(int dest, int src);
	KSubMesh * addToMesh(KMesh *mesh, KPrimitive prim, const KMaterial *material=NULL) const;

private:
	KMesh *m_Mesh;
	KVertex *m_Vertices;
	int m_VStart;
	int m_VCount;
	int *m_Indices;
	int m_IStart;
	int m_ICount;
	mutable bool m_Locked;
};


class CPathDraw {
	std::vector<KVec2> m_Points;
	CMeshBuf m_MeshBuf;
public:
	float m_Z;
	float m_AutoLength;

	CPathDraw();
	void pathClear();
	void pathInit(const KVec2 &p);
	void pathLineTo(const KVec2 &p);

	// center を中心とした半径 radius の弧を追加する
	void pathArcTo(const KVec2 &center, float radius, float rad_start, float rad_end, int segments=0);
	void pathArcToAuto(const KVec2 &center, float radius, float rad_start, float rad_end, int maxlevel);

	// ２次のベジェ補間を行う
	// p1, p2   追加の制御点。既存のパスの終端を始点として、３個の点で補間を行う
	// segments 線分個数. 0 を指定すると De Casteljau のアルゴリズムにより自動分割する。
	//          その場合に線分長さ（の近似）が mAutoLength 未満になったら計算を打ち切る
	void pathQuadricBezierTo(const KVec2 &p1, const KVec2 &p2, int segments=0);
	void pathQuadricBezierToAuto(const KVec2 &p0, const KVec2 &p1, const KVec2 &p2, int maxlevel);

	// ３次のベジェ補間を行う
	// p1, p2, p3  追加の制御点。既存のパスの終端を始点として、４個の点で補間を行う
	// segments    線分個数. 0 を指定すると De Casteljau のアルゴリズムにより自動分割する。
	//             その場合に線分長さ（の近似）が mAutoLength 未満になったら計算を打ち切る
	void pathCubicBezierTo(const KVec2 &p1, const KVec2 &p2, const KVec2 &p3, int segments=0);
	void pathCubicBezierToAuto(const KVec2 &p0, const KVec2 &p1, const KVec2 &p2, const KVec2 &p3, int maxlevel);

	// ４次のベジェ補間を行う
	// p1, p2, p3, p4  追加の制御点。既存のパスの終端を始点として、５個の点で補間を行う
	// segments        線分個数. 0 を指定すると De Casteljau のアルゴリズムにより自動分割する
	//                 その場合に線分長さ（の近似）が mAutoLength 未満になったら計算を打ち切る
	void pathQuinticBezierTo(const KVec2 &p1, const KVec2 &p2, const KVec2 &p3, const KVec2 &p4, int segments=0);
	void pathQuinticBezierToAuto(const KVec2 &p0, const KVec2 &p1, const KVec2 &p2, const KVec2 &p3, const KVec2 &p4, int maxlevel);

	// n次ベジェ補間を行う
	// p          追加の制御点。既存のパスの終端を始点として、n+1個の点で補間を行う
	// n          追加の制御店の個数（使いたい多項式の次数と同じ値になる）
	// maxlevel   最大再帰深さ
	void pathNthBezierToAuto(const KVec2 *p, int n, int maxlevel);

	// n次のベジェを２個のｎ次ベジェ曲線に分割し、新しい制御点を得る
	// p, n 元の制御点と個数
	// tmp0 始点から中間点までをベジェ曲線で表すときの制御点。個数は n と同じで、tmp0 の始点 (tmp0[0]) は p[0] と同じ
	// tmp1 中間点から終点までをベジェ曲線で表すときの制御点。個数は n と同じで、tmp1 の終点 (tmp1[n-1] は p[n-1] と同じ
	// idx0 tmp0 の書き込み先制御点インデックス(初期値は 0)
	// idx1 tmp1 の書き込み先制御点インデックス(初期値は n-1)
	void pathNthBezierGetPoints(const KVec2 *p, int n, KVec2 *tmp0, KVec2 *tmp1, int idx0, int idx1);

	// 任意の個数の点を指定し、イイ感じで補完する
	// polynominal 最大の補完次数（16次まで）
	void pathSmooth(const KVec2 *points, int size, int polynomial);

	// パスに従って線を引く。
	// mesh      サブメッシュの追加先
	// thickness 太さ。0を指定すると KPrimitive_LINE_STRIP を使う
	// color     頂点色
	// material  サブメッシュのマテリアル
	void stroke(KMesh *mesh, float thickness=1.0f, const KColor &color=KColor::WHITE, const KMaterial *material=NULL);

	// パスに従って線を引く。始点と終点の頂点色をそれぞれ指定し、グラデーションさせる
	void strokeGradient(KMesh *mesh, float thickness=1.0f, const KColor &color1=KColor::WHITE, const KColor &color2=KColor::WHITE, const KMaterial *material=NULL);


	void addCross(KMesh *mesh, const KVec2 &pos, float size, const KColor &color);
	void addLine(KMesh *mesh, const KVec2 &p0, const KVec2 &p1, const KColor &color);
	void addBox(KMesh *mesh, const KVec2 &pos, const KVec2 &halfsize, const KColor &fill_color, const KColor &stroke_color);
	void addEllipse(KMesh *mesh, const KVec2 &pos, const KVec2 &r, const KColor &fill_color, const KColor &stroke_color);

};

} // namespace
