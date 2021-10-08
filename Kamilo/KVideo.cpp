#include "KVideo.h"

#include <mutex>
#include "KMath.h"
#include "KMatrix.h"
#include "KInternal.h"


// Use Direct3D9
#define K_USE_D3D9

// Check error strictly
#define K_USE_STRICT_CHECK 1

#ifdef K_USE_D3D9
#	include <d3d9.h>
#	include <d3dx9.h>
#	include <dxerr9.h> // dxerr9.lib
#	define _DXGetErrorStringW(hr)  DXGetErrorString9W(hr)
#endif


#ifdef K_USE_D3D9

#define K__VIDEO_ERR(fmt, ...)    K__ERROR(fmt, ##__VA_ARGS__)
#define K__VIDEO_WRN(fmt, ...)    K::print(fmt, ##__VA_ARGS__)
#define K__VIDEO_PRINT(fmt, ...)  K::print(fmt, ##__VA_ARGS__)


#define K__DX9_SCREENTEX_TEST    1
#define K__DX9_STRICT_CHECK      K_USE_STRICT_CHECK
#define K__DX9_LOCK_GUARD(mtx)   std::lock_guard<std::recursive_mutex> __lock__(mtx)
#define K__DX9_RELEASE(x)        {if (x) (x)->Release(); x=nullptr;}


#define K__EPS (1.0f / 8192) // 0.00012207031
#define K__EQUALS(x, y)  (fabsf((x) - (y)) <= K__EPS)
#define K__STREQ(x, y)   (strcmp(x, y) == 0)


namespace Kamilo {

static const uint32_t K__COLOR32_WHITE = 0xFFFFFFFF;
static const uint32_t K__COLOR32_ZERO = 0x00000000;


const KColor K_GIZMO_LINE_COLOR(0, 1, 0, 1);


#pragma region Def/Type/Utils
/// 定義済みシェーダーパラメータ名
/// この名前のパラメータは、パラメータ名とその型、役割があらかじめ決まっている。
/// （アンダースコアで始まる or 終わる名前は GLSL でコンパイルエラーになることに注意）
static const char * K__DX9_SHADERPARAM_MAIN_TEXTURE        = "mo__Texture";
static const char * K__DX9_SHADERPARAM_MAIN_TEXTURE_SIZE   = "mo__TextureSize";
static const char * K__DX9_SHADERPARAM_SCREEN_TEXTURE      = "mo__ScreenTexture";
static const char * K__DX9_SHADERPARAM_SCREEN_TEXTURE_SIZE = "mo__ScreenTextureSize";
static const char * K__DX9_SHADERPARAM_DIFFUSE             = "mo__Color";
static const char * K__DX9_SHADERPARAM_SPECULAR            = "mo__Specular";
static const char * K__DX9_SHADERPARAM_MATRIX_VIEW         = "mo__MatrixView";
static const char * K__DX9_SHADERPARAM_MATRIX_PROJ         = "mo__MatrixProj";
static const char * K__DX9_SHADERPARAM_TIME_SEC            = "mo__TimeSec";

static const DWORD K__DX9_COLOR_WHITE = 0xFFFFFFFF;
static const DWORD K__DX9_COLOR_ZERO  = 0x00000000;
static const DWORD K__DX9_FVF_VERTEX  = D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1 | D3DFVF_TEX2;

struct DX9_VERTEX {
	float xyz[3];
	DWORD dif;
	DWORD spe;
	float uv1[2];
	float uv2[2];
};

static std::string K__StdSprintf(const char *fmt, ...) {
	char s[1024 * 4];
	va_list args;
	va_start(args, fmt);
	vsprintf_s(s, sizeof(s), fmt, args);
	va_end(args);
	return s;
}
static float K__GetTimeSec() {
	return GetTickCount() / 1000.0f;
}
static std::string K__GetErrorStringUtf8(HRESULT hr) {
	const wchar_t *ws = _DXGetErrorStringW(hr);
	char s[1024];
	WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, ws, -1, s, sizeof(s), nullptr, nullptr);
	return std::string(s);
}
static float K__Clamp01(float t) {
	if (t < 0) return 0;
	if (t < 1) return t;
	return 1;
}
static uint32_t K__Color32FromRgba(const float *rgba) {
	uint8_t r = (uint8_t)(K__Clamp01(rgba[0]) * 255.0f);
	uint8_t g = (uint8_t)(K__Clamp01(rgba[1]) * 255.0f);
	uint8_t b = (uint8_t)(K__Clamp01(rgba[2]) * 255.0f);
	uint8_t a = (uint8_t)(K__Clamp01(rgba[3]) * 255.0f);
	return ((a & 0xFF) << 24) | ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF);
}

struct SVideoLimit {
	SVideoLimit() {
		max_texture_size_require = 2048;
		limit_texture_size = 0;
		use_square_texture_only = false;
		disable_pixel_shader = false;
	}
	/// 必要なテクスチャサイズ。
	/// この値は、ゲームエンジンの要求する最低スペック要件になる。
	/// このサイズ未満のテクスチャにしか対応しない環境で実行しようとした場合、正しい描画は保証されない
	int max_texture_size_require = 2048;

	/// テクスチャのサイズ制限
	/// 実際の環境とは無関係に適用する
	/// 0 を指定すると制限しない
	int limit_texture_size = 2048;

	/// 正方形のテクスチャしか作れないようにする。チープな環境をエミュレートするために使う
	bool use_square_texture_only = false;

	/// ピクセルシェーダーが使えないようにする。チープな環境をエミュレートするために使う
	bool disable_pixel_shader = false;
};
static SVideoLimit g_video_limit;

static const float K__MAT4_IDENTITY[] = {
	1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 1, 0,
	0, 0, 0, 1,
};

/// 必要な頂点ストリーム数。
/// この値は、ゲームエンジンの要求する最低スペック要件になる。
/// この数未満の頂点ストリームにしか対応していない環境で実行しようとした場合、正しい描画は保証されない
static const int SYSTEMREQ_MAX_VERTEX_ELMS = 8;

#pragma endregion // Def/Type/Utils


#pragma region D3D9 Functions
/// HRESULT 表示用のマクロ
/// S_OK ならば通常ログを出力し、エラーコードであればエラーログを出力する
static void DX9_log(const char *msg_u8, HRESULT hr) {
	std::string text; // UTF8
	text = msg_u8;
	text += ": ";
	text += K__GetErrorStringUtf8(hr);
	if (SUCCEEDED(hr)) {
		K__VIDEO_PRINT(text.c_str());
	} else {
		K__VIDEO_ERR("%s", text.c_str());
	}
}
static void DX9_printPresentParameter(const char *label, const D3DPRESENT_PARAMETERS &pp) {
	K__VIDEO_PRINT("--");
	K__VIDEO_PRINT("%s", label);
	K__VIDEO_PRINT("- BackBufferWidth           : %d", pp.BackBufferWidth);
	K__VIDEO_PRINT("- BackBufferHeight          : %d", pp.BackBufferHeight);
	K__VIDEO_PRINT("- BackBufferFormat          : (D3DFORMAT)%d", pp.BackBufferFormat);
	K__VIDEO_PRINT("- BackBufferCount           : %d", pp.BackBufferCount);
	K__VIDEO_PRINT("- MultiSampleType           : (D3DMULTISAMPLE_TYPE)%d", pp.MultiSampleType);
	K__VIDEO_PRINT("- MultiSampleQuality        : %d", pp.MultiSampleQuality);
	K__VIDEO_PRINT("- SwapEffect                : (D3DSWAPEFFECT)%d", pp.SwapEffect);
	K__VIDEO_PRINT("- hDeviceWindow             : 0x%08x", pp.hDeviceWindow);
	K__VIDEO_PRINT("- Windowed                  : %d", pp.Windowed);
	K__VIDEO_PRINT("- EnableAutoDepthStencil    : %d", pp.EnableAutoDepthStencil);
	K__VIDEO_PRINT("- AutoDepthStencilFmt       : (D3DFORMAT)%d", pp.AutoDepthStencilFormat);
	K__VIDEO_PRINT("- Flags                     : %d (0x%08x)", pp.Flags, pp.Flags);
	K__VIDEO_PRINT("- FullScreen_RefreshRateInHz: %d", pp.FullScreen_RefreshRateInHz);
	K__VIDEO_PRINT("- PresentationInterval      : 0x%08x", pp.PresentationInterval);
	K__VIDEO_PRINT("");
}
static void DX9_printAdapter(IDirect3D9 *d3d9) {
	K__ASSERT(d3d9);
	D3DADAPTER_IDENTIFIER9 adap;
	ZeroMemory(&adap, sizeof(adap));
	d3d9->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &adap);
	K__VIDEO_PRINT("--");
	K__VIDEO_PRINT("Adapter");
	K__VIDEO_PRINT("- Driver     : %s", adap.Driver);
	K__VIDEO_PRINT("- Description: %s", adap.Description);
	K__VIDEO_PRINT("- DeviceName : %s", adap.DeviceName);
	K__VIDEO_PRINT("");
}
static void DX9_printFullscreenResolutions(IDirect3D9 *d3d9) {
	K__ASSERT(d3d9);
	K__VIDEO_PRINT("Supported Fullscreen Resolutions:");
	int num = d3d9->GetAdapterModeCount(D3DADAPTER_DEFAULT, D3DFMT_X8R8G8B8);
	for (int i=0; i<num; i++) {
		D3DDISPLAYMODE mode;
		if (d3d9->EnumAdapterModes(D3DADAPTER_DEFAULT, D3DFMT_X8R8G8B8, i, &mode) == D3D_OK) {
			K__VIDEO_PRINT("- [%2d] %dx%d D3DFORMAT(%d)", i, mode.Width, mode.Height, mode.Format);
		}
	}
	K__VIDEO_PRINT("");
}
static void DX9_makeDefaultPresentParameters(IDirect3D9 *d3d9, HWND hWnd, D3DPRESENT_PARAMETERS *pp) {
	K__ASSERT(d3d9);
	K__ASSERT(pp);
	D3DDISPLAYMODE disp;
	ZeroMemory(&disp, sizeof(disp));
	d3d9->GetAdapterDisplayMode(0, &disp);
	// D3DPRESENT_PARAMETERS で EnableAutoDepthStencil を TRUE にしたら、
	// たとえ D3DRS_ZENABLE を FALSE にしていたとしても Z バッファーの生成を省略してはいけない。
	// 特にレンダーターゲット設定時にはZバッファーもレンダーターゲットに応じた大きさのものに差し替えておかないと、
	// 古いほうのZバッファサイズでクリッピングされてしまう！（大丈夫な環境もある）
	D3DPRESENT_PARAMETERS d3dpp = {
		0,                             // BackBufferWidth
		0,                             // BackBufferHeight
		disp.Format,                   // BackBufferFormat
		1,                             // BackBufferCount
		D3DMULTISAMPLE_NONE,           // MultiSampleType アンチエイリアス。使用の際には D3DRS_MULTISAMPLEANTIALIAS を忘れずに
		0,                             // MultiSampleQuality
		D3DSWAPEFFECT_DISCARD,         // SwapEffect
		hWnd,                          // hDeviceWindow
		TRUE,                          // Windowed
		TRUE,                          // EnableAutoDepthStencil
		D3DFMT_D24S8,                  // AutoDepthStencilFormat
		0,                             // Flags
		D3DPRESENT_RATE_DEFAULT,       // FullScreen_RefreshRateInHz
		D3DPRESENT_INTERVAL_IMMEDIATE, // PresentationInterval
	};
	if (1) {
		// http://maverickproj.web.fc2.com/pg11.html
		// この時点で m_d3dpp はマルチサンプリングを使わない設定なっている。
		// 最高品質から順番に調べていき、サポートしているならその設定を使う
		// ※アンチエイリアスが有効なのはバックバッファに描画する時であって、レンダーターゲットに描画する時には無効という噂あり。
		// 当然だがポリゴンエッジに対してAAがかかるのであって、テクスチャ画像に対してではないので、あまり意味はない。
		D3DMULTISAMPLE_TYPE mst = D3DMULTISAMPLE_16_SAMPLES;
		while (mst > 0) {
			DWORD back_msq;
			DWORD depth_msq;
			HRESULT hr1 = d3d9->CheckDeviceMultiSampleType(
				D3DADAPTER_DEFAULT,
				D3DDEVTYPE_HAL, 
				d3dpp.BackBufferFormat,
				d3dpp.Windowed,
				mst,
				&back_msq);
			HRESULT hr2 = d3d9->CheckDeviceMultiSampleType(
				D3DADAPTER_DEFAULT,
				D3DDEVTYPE_HAL, 
				d3dpp.AutoDepthStencilFormat,
				d3dpp.Windowed,
				mst,
				&depth_msq);
			if (SUCCEEDED(hr1) && SUCCEEDED(hr2)) {
				d3dpp.MultiSampleType = mst;
				d3dpp.MultiSampleQuality = std::min(back_msq-1, depth_msq-1);
				break;
			}
			mst = (D3DMULTISAMPLE_TYPE)(mst - 1);
		}
		K__VIDEO_PRINT("--");
		K__VIDEO_PRINT("Multi sampling check:");
		K__VIDEO_PRINT("- Max multi sampling type: (D3DMULTISAMPLE_TYPE)%d", mst);
		K__VIDEO_PRINT("");
	}
	*pp = d3dpp;
}
/// D3DCAPS9 が D3DPTEXTURECAPS_SQUAREONLY フラグを持っているかどうか
///
/// この関数はテスト用フラグの影響を受ける。テスト中でない場合は D3DPTEXTURECAPS_SQUAREONLY フラグの有無をそのまま返すが、
/// 正方形テクスチャのテスト中だった場合は caps の値とは無関係に必ず false を返す。
static bool DX9_hasD3DPTEXTURECAPS_SQUAREONLY(const D3DCAPS9 &caps) {
	if (g_video_limit.use_square_texture_only) {
		return false;
	}
	return (caps.TextureCaps & D3DPTEXTURECAPS_SQUAREONLY) != 0;
}

/// D3DCAPS9 に設定されている MaxTextureWidth の値を返す
///
/// この関数はテスト用フラグの影響を受ける。テスト中でない場合は D3DCAPS9::MaxTextureWidth の値をそのまま返すが、
/// サイズ制限が有効な場合は g_video_limit.limit_texture_size と比較して小さいほうの値を返す。
static int DX9_getMaxTextureWidth(const D3DCAPS9 &caps) {
	int lim = g_video_limit.limit_texture_size;
	if (lim > 0) {
		if (lim < (int)caps.MaxTextureWidth) {
			return lim;
		}
	}
	return (int)caps.MaxTextureWidth;
}

/// D3DCAPS9 に設定されている MaxTextureHeight の値を返す
///
/// この関数はテスト用フラグの影響を受ける。テスト中でない場合は D3DCAPS9::MaxTextureHeight の値をそのまま返すが、
/// サイズ制限が有効な場合は g_video_limit.limit_texture_size と比較して小さいほうの値を返す。
static int DX9_getMaxTextureHeight(const D3DCAPS9 &caps) {
	int lim = g_video_limit.limit_texture_size;
	if (lim > 0) {
		if (lim < (int)caps.MaxTextureHeight) {
			return lim;
		}
	}
	return (int)caps.MaxTextureHeight;
}
static void DX9_checkRenderIOTexture(IDirect3DDevice9 *d3ddev, IDirect3DTexture9 *tex) {
	K__ASSERT(d3ddev);
	static bool err_raised = false; // 最初のエラーだけを調べる
	if (K__DX9_STRICT_CHECK) {
		if (!err_raised && tex) {
			IDirect3DSurface9 *surf_target;
			d3ddev->GetRenderTarget(0, &surf_target);
			IDirect3DSurface9 *surf_tex;
			tex->GetSurfaceLevel(0, &surf_tex);
			if (surf_target == surf_tex) {
				K__VIDEO_ERR(u8"E_D3D_SOURCE_AND_DEST_TEXTURE_ARE_SAME: 描画するテクスチャと描画先レンダーテクスチャが同じになっています。動作が未確定になります");
				err_raised = true;
			}
			K__DX9_RELEASE(surf_tex);
			K__DX9_RELEASE(surf_target);
		}
	}
}
static void DX9_printDeviceCaps(IDirect3DDevice9 *d3ddev9) {
	K__ASSERT(d3ddev9);
	D3DCAPS9 caps;
	d3ddev9->GetDeviceCaps(&caps);

	K__VIDEO_PRINT("--");
	K__VIDEO_PRINT("Check Caps");
	K__VIDEO_PRINT("- VertexShaderVersion                : %d.%d", D3DSHADER_VERSION_MAJOR(caps.VertexShaderVersion), D3DSHADER_VERSION_MINOR(caps.VertexShaderVersion));
	K__VIDEO_PRINT("- PixelShaderVersion                 : %d.%d", D3DSHADER_VERSION_MAJOR(caps.PixelShaderVersion),  D3DSHADER_VERSION_MINOR(caps.PixelShaderVersion));
	K__VIDEO_PRINT("- MaxTextureWidth                    : %d", DX9_getMaxTextureWidth(caps));
	K__VIDEO_PRINT("- MaxTextureHeight                   : %d", DX9_getMaxTextureHeight(caps));
	K__VIDEO_PRINT("- D3DPTEXTURECAPS_ALPHA              : %s", (caps.TextureCaps & D3DPTEXTURECAPS_ALPHA) ? "Yes" : "No");
	K__VIDEO_PRINT("- D3DPTEXTURECAPS_SQUAREONLY         : %s", DX9_hasD3DPTEXTURECAPS_SQUAREONLY(caps) ? "Yes" : "No");
	K__VIDEO_PRINT("- D3DPTEXTURECAPS_POW2               : %s", (caps.TextureCaps & D3DPTEXTURECAPS_POW2) ? "Yes" : "No");
	K__VIDEO_PRINT("- D3DPTEXTURECAPS_NONPOW2CONDITIONAL : %s", (caps.TextureCaps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL) ? "Yes" : "No");
	K__VIDEO_PRINT("- D3DTEXOPCAPS_MULTIPLYADD           : %s", (caps.TextureOpCaps & D3DTEXOPCAPS_MULTIPLYADD) ? "Yes" : "No"); // D3DTOP_MULTIPLYADD
	K__VIDEO_PRINT("- D3DPMISCCAPS_PERSTAGECONSTANT      : %s", (caps.PrimitiveMiscCaps & D3DPMISCCAPS_PERSTAGECONSTANT) ? "Yes" : "No"); // D3DTSS_CONSTANT
	K__VIDEO_PRINT("- D3DPMISCCAPS_COLORWRITEENABLE      : %s", (caps.PrimitiveMiscCaps & D3DPMISCCAPS_COLORWRITEENABLE) ? "Yes" : "No");
	K__VIDEO_PRINT("- D3DPMISCCAPS_SEPARATEALPHABLEND    : %s", (caps.PrimitiveMiscCaps & D3DPMISCCAPS_SEPARATEALPHABLEND) ? "Yes" : "No");
	K__VIDEO_PRINT("- [Src] D3DPBLENDCAPS_BLENDFACTOR    : %s", (caps.SrcBlendCaps  & D3DPBLENDCAPS_BLENDFACTOR) ? "Yes" : "No");
	K__VIDEO_PRINT("- [Dst] D3DPBLENDCAPS_BLENDFACTOR    : %s", (caps.DestBlendCaps & D3DPBLENDCAPS_BLENDFACTOR) ? "Yes" : "No");
	K__VIDEO_PRINT("- NumSimultaneousRTs                 : %d", caps.NumSimultaneousRTs); // Multi path rendering
	K__VIDEO_PRINT("- MaxStreams                         : %d", caps.MaxStreams); // 頂点ストリーム数
	K__VIDEO_PRINT("- MaxTextureBlendStages              : %d", caps.MaxTextureBlendStages);
	K__VIDEO_PRINT("- AvailableTextureMem                : %d[MB]", d3ddev9->GetAvailableTextureMem()/1024/1024);
	K__VIDEO_PRINT("");
}
/// フルスクリーン解像度のチェック
/// 指定したパラメータでフルスクリーンできるなら true を返す
static bool DX9_checkFullScreenParams(IDirect3D9 *d3d9, int w, int h) {
	K__ASSERT(d3d9);
	K__ASSERT(w > 0);
	K__ASSERT(h > 0);
	int num = d3d9->GetAdapterModeCount(D3DADAPTER_DEFAULT, D3DFMT_X8R8G8B8);
	for (int i=0; i<num; i++) {
		D3DDISPLAYMODE mode;
		if (d3d9->EnumAdapterModes(D3DADAPTER_DEFAULT, D3DFMT_X8R8G8B8, i, &mode) == D3D_OK) {
			if ((int)mode.Width == w && (int)mode.Height == h) {
				return true;
			}
		}
	}
	return false;
}
static void DX9_setBlend(IDirect3DDevice9 *d3ddev, KBlend blend) {
	// RGB と Alpha の双方に同一のブレンド式を設定する。
	// これを別々にしたい場合は D3DRS_SEPARATEALPHABLENDENABLE を参照せよ
	K__ASSERT(d3ddev);
	switch (blend) {
	case KBlend_ONE:
		d3ddev->SetRenderState(D3DRS_BLENDOP,   D3DBLENDOP_ADD);
		d3ddev->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_ONE);
		d3ddev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);
		break;
	case KBlend_ADD: // add + alpha
		d3ddev->SetRenderState(D3DRS_BLENDOP,   D3DBLENDOP_ADD);
		d3ddev->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
		d3ddev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
		break;
	case KBlend_SUB: // sub + alpha
		d3ddev->SetRenderState(D3DRS_BLENDOP,   D3DBLENDOP_REVSUBTRACT);
		d3ddev->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
		d3ddev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCCOLOR);
		break;
	case KBlend_MUL:
		d3ddev->SetRenderState(D3DRS_BLENDOP,   D3DBLENDOP_ADD);
		d3ddev->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_ZERO);
		d3ddev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_SRCCOLOR);
		break;
	case KBlend_ALPHA:
		d3ddev->SetRenderState(D3DRS_BLENDOP,   D3DBLENDOP_ADD);
		d3ddev->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
		d3ddev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
		break;
	case KBlend_SCREEN:
		d3ddev->SetRenderState(D3DRS_BLENDOP,   D3DBLENDOP_ADD);
		d3ddev->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_INVDESTCOLOR);
		d3ddev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
		break;
	case KBlend_MAX:
		d3ddev->SetRenderState(D3DRS_BLENDOP,   D3DBLENDOP_MAX);
		d3ddev->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_ONE);
		d3ddev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
		break;
	}
}
static void DX9_setFilter(IDirect3DDevice9 *d3ddev, KFilter filter) {
	switch (filter) {
	case KFilter_NONE:
		d3ddev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
		d3ddev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
		break;

	case KFilter_LINEAR:
		d3ddev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		d3ddev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
		break;
	}
}
static void DX9_setTextureAddressing(IDirect3DDevice9 *d3ddev, bool wrap) {
	d3ddev->SetSamplerState(0, D3DSAMP_ADDRESSU, wrap ? D3DTADDRESS_WRAP : D3DTADDRESS_CLAMP);
	d3ddev->SetSamplerState(0, D3DSAMP_ADDRESSV, wrap ? D3DTADDRESS_WRAP : D3DTADDRESS_CLAMP);
}

static IDirect3D9 * DX9_init() {
	IDirect3D9 *d3d9 = Direct3DCreate9(D3D_SDK_VERSION);
	DX9_log("Direct3DCreate9", d3d9 ? S_OK : E_FAIL);
	if (d3d9) {
		// アダプター情報
		if (1) DX9_printAdapter(d3d9);

		// サポートしているフルスクリーン解像度
		if (0) DX9_printFullscreenResolutions(d3d9);

		return d3d9;
	}
	K__VIDEO_ERR(
		u8"E_FAIL: Direct3DCreate9\n"
		u8"グラフィックの初期化に失敗しました。\n"
		u8"システム要件を満たし、最新版の Direct3D がインストールされていることを確認してください\n"
		u8"・念のため、他のソフトを全て終了させるか、パソコンを再起動してからもう一度ゲームを実行してみてください\n\n"
	);
	return nullptr;
}
static IDirect3DDevice9 * DX9_initDevice(IDirect3D9 *d3d9, HWND hWnd, D3DPRESENT_PARAMETERS *d3dpp) {
	IDirect3DDevice9 *d3ddev9 = nullptr;
	DX9_makeDefaultPresentParameters(d3d9, hWnd, d3dpp);
	DX9_printPresentParameter("Present Parameters (Input)", *d3dpp);
	if (d3ddev9 == nullptr) {
		HRESULT hr = d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING|D3DCREATE_MULTITHREADED, d3dpp, &d3ddev9);
		DX9_log("IDirect3DDevice9::CreateDevice (HAL/HARD)", hr);
	}
	if (d3ddev9 == nullptr) {
		HRESULT hr = d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING|D3DCREATE_MULTITHREADED, d3dpp, &d3ddev9);
		DX9_log("IDirect3DDevice9::CreateDevice (HAL/SOFT)", hr);
	}
	if (d3ddev9 == nullptr) {
		HRESULT hr = d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_REF, hWnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING|D3DCREATE_MULTITHREADED, d3dpp, &d3ddev9);
		DX9_log("IDirect3DDevice9::CreateDevice (REF/SOFT)", hr);
	}
	if (d3ddev9 == nullptr) {
		// デバイスを初期化できなかった。
		// [[デバッグ方法]]
		// このエラーを意図的に出すには F5 で実行した直後に Ctrl+Alt+Delete でタスクマネージャのフルスクリーン画面を出し、
		// その間に裏でプログラムが起動するのを待てばよい。起動した頃を見計らってフルスクリーンを解除すれば、このダイアログが出ているはず。
		K__VIDEO_ERR(
			u8"E_FAIL: IDirect3DDevice9::CreateDevice\n"
			u8"Direct3D デバイスの初期化に失敗しました。\n"
			u8"Direct3D はインストールされていますが、利用することができません\n"
			u8"--\n"
			u8"・他のアプリケーションによって画面がフルスクリーンになっている最中にゲームが起動した場合、このエラーが出ます\n"
			u8"・念のため、他のソフトを全て終了させるか、パソコンを再起動してからもう一度ゲームを実行してみてください\n\n"
		);
	}
	return d3ddev9;
}
#pragma endregion // D3D9 Functions


#pragma region CD3DTex

// dst に src をマテリアル mat で描画する
// dst = nullptr だった場合はバックバッファに描画する
static void _BlitEx(KTexture *dst, KTexture *src, KMaterial *mat, const KVideoRect *src_rect, const KVideoRect *dst_rect, const KMatrix4 *transform) {
	K__ASSERT_RETURN(src != nullptr);
	K__ASSERT_RETURN(src != dst);
	static const KMatrix4 identity;
	KVideo::pushRenderState();
	if (dst) KVideo::pushRenderTarget(dst->getId());
	// ブレンドモード
	if (mat) {
		mat->texture = src->getId();
		KVideo::setProjection(identity.m);
		KVideo::setTransform(identity.m);

		if (mat->shader) {
			KVideo::setShader(mat->shader);

			KShaderArg arg;
			arg.blend = mat->blend;
			arg.texture = mat->texture;
			arg.color = mat->color;
			arg.specular = mat->specular;
			KVideo::setDefaultShaderParams(arg);
			// ここのコールバックを削るときはayakashiの雷天気がちゃんと再生されるか確認すること
			if (mat && mat->cb) {
				mat->cb->onMaterial_Begin(mat);
				mat->cb->onMaterial_SetShaderVariable(mat);
			}
		} else {
			KVideo::setShader(nullptr);
			KVideo::setBlend(mat->blend);
			KVideo::setFilter(mat->filter);
			KVideo::setTextureAddressing(mat->wrap);
			KVideo::setColor(mat->color32());
			KVideo::setSpecular(mat->specular32());
			KVideo::setTexture(mat->texture);
		}
		KVideo::beginShader();
	} else {
		// 変換済み頂点 XYZEHW で描画するため行列設定は不要
		// m_d3ddev->SetTransform(D3DTS_PROJECTION, ...);
		// m_d3ddev->SetTransform(D3DTS_VIEW, ...);
		KVideo::setFilter(KFilter_NONE);
		KVideo::setTextureAddressing(false);
		KVideo::setProjection(identity.m);
		KVideo::setTransform(identity.m);
		KVideo::setBlend(KBlend_ONE);
		KVideo::setColor(K__COLOR32_WHITE);
		KVideo::setSpecular(K__COLOR32_ZERO);
		KVideo::setTexture(src->getId());
	}

	KVideo::drawTexture(src->getId(), src_rect, dst_rect, transform);

	// 後始末
	if (mat) {
		if (mat && mat->cb) {
			mat->cb->onMaterial_End(mat);
		}
		KVideo::endShader();
		KVideo::setShader(nullptr);
	}
	if (dst) KVideo::popRenderTarget();
	KVideo::popRenderState();
}

// channel: -1=RGBA, 0=R, 1=G, 2=B, 3=A
static void _BmpWrite(KBmp *bmp, const void *rgba, int pitch, int width, int height, int channel) {
	K__ASSERT_RETURN(bmp);
	K__ASSERT_RETURN(rgba);
	K__ASSERT_RETURN(bmp);
	K__ASSERT_RETURN(bmp->w == width);
	K__ASSERT_RETURN(bmp->h == height);
	if (bmp->pitch == pitch) {
		// ピッチが同じ。 memcpy で一括コピーできる
		memcpy(bmp->data, rgba, pitch * height);
	} else {
		// ピッチが異なる。ラインごとにコピーする
		for (int y=0; y<height; y++) {
			memcpy(
				bmp->data + (bmp->pitch * y),
				(uint8_t*)rgba + (pitch * y),
				width * 4
			);
		}
	}
	// Direct3Dのテクスチャには色が BGRA の順で格納されている (ARGBのリトルエンディアン)
	// これを RGBA の順番に並び替える
	if (channel == 3) { //0=R, 1=G, 2=B, 3=A
		// アルファチャンネルが指定された。
		// アルファチャンネルをグレースケールに変えておく
		// BGRA ==> RGBA
		for (int y=0; y<bmp->h; y++) {
			for (int x=0; x<bmp->w; x++) {
				const int i = bmp->get_offset(x, y);
			//	const uint8_t b = bmp->data[i+0];
			//	const uint8_t g = bmp->data[i+1];
			//	const uint8_t r = bmp->data[i+2];
				const uint8_t a = bmp->data[i+3];
				bmp->data[i+0] = a;
				bmp->data[i+1] = a;
				bmp->data[i+2] = a;
				bmp->data[i+3] = 255;
			}
		}
	} else if (channel == -1) {
		// BGRA ==> RGBA
		for (int y=0; y<bmp->h; y++) {
			for (int x=0; x<bmp->w; x++) {
				const int i = bmp->get_offset(x, y);
				const uint8_t b = bmp->data[i+0];
				const uint8_t g = bmp->data[i+1];
				const uint8_t r = bmp->data[i+2];
				const uint8_t a = bmp->data[i+3];
				bmp->data[i+0] = r;
				bmp->data[i+1] = g;
				bmp->data[i+2] = b;
				bmp->data[i+3] = a;
			}
		}
	}
}


static int g_NewTexId = 0;

class CD3DTex: public KTexture {
public:
	D3DSURFACE_DESC m_desc;
	D3DCAPS9 m_caps;
	IDirect3DDevice9  *m_d3ddev;
	IDirect3DTexture9 *m_d3dtex;
	IDirect3DSurface9 *m_color_surf;
	IDirect3DSurface9 *m_depth_surf;
	IDirect3DSurface9 *m_target_surf;
	IDirect3DSurface9 *m_lockable_surf; // ロック可能なサーフェス。レンダーターゲットからのコピー作業用
	UINT m_pitch;
	UINT m_original_w;
	UINT m_original_h;
	UINT m_required_w;
	UINT m_required_h;
	IDirect3DTexture9 *m_backup_tex; // デバイスリセット時にレンダーターゲットの内容を保持するためのテクスチャ

public:
	KTexture::Format m_kfmt;
	KTEXID m_ktexid;

	CD3DTex() {
		zero_clear();
		g_NewTexId++;
		m_ktexid = (KTEXID)g_NewTexId;
	}
	virtual ~CD3DTex() {
	}
	void zero_clear() {
		m_d3ddev = nullptr;
		m_d3dtex = nullptr;
		m_color_surf = nullptr;
		m_depth_surf = nullptr;
		m_target_surf = nullptr;
		m_lockable_surf = nullptr;
		m_pitch = 0;
		m_original_w = 0;
		m_original_h = 0;
		m_required_w = 0;
		m_required_h = 0;
		ZeroMemory(&m_desc, sizeof(m_desc));
		ZeroMemory(&m_caps, sizeof(m_caps));
		m_kfmt = KTexture::FMT_NONE;
		m_backup_tex = nullptr;
	}
	bool make(IDirect3DDevice9 *dev, int w, int h, KTexture::Format fmt, bool is_render_target) {
		zero_clear();
		K__ASSERT_RETURN_ZERO(dev);
		K__ASSERT_RETURN_ZERO(w > 0);
		K__ASSERT_RETURN_ZERO(h > 0);
		m_d3ddev = dev;
		HRESULT hr = m_d3ddev->GetDeviceCaps(&m_caps);
		if (FAILED(hr)) {
			std::string msg = K::win32_GetErrorString(hr);
			K::print(msg.c_str());
		}
		D3DSURFACE_DESC desc;
		{
			ZeroMemory(&desc, sizeof(desc));
			desc.Width  = w;
			desc.Height = h;
			if (fmt == KTexture::FMT_ARGB64F) {
				desc.Format = D3DFMT_A16B16G16R16F;
			} else {
				desc.Format = D3DFMT_A8R8G8B8;
			}
			if (is_render_target) {
				desc.Usage = D3DUSAGE_RENDERTARGET;
				desc.Pool  = D3DPOOL_DEFAULT;
				if (1) { // デバイスロスト時にレンダーターゲット内容が失われないようにする
					HRESULT hr = m_d3ddev->CreateTexture(w, h, 1, 0, desc.Format, D3DPOOL_SYSTEMMEM, &m_backup_tex, nullptr);
					if (FAILED(hr)) {
						std::string msg = K::win32_GetErrorString(hr);
						K::print(msg.c_str());
					}
				}
			} else {
				desc.Usage = 0;
				desc.Pool  = D3DPOOL_MANAGED;
			}
		}
		if (loadByDesc(&desc)) {
			return true;
		}
		destroy();
		return false;
	}
	void destroy(bool keep_backup=false) {
		K__DX9_RELEASE(m_color_surf);
		K__DX9_RELEASE(m_depth_surf);
		K__DX9_RELEASE(m_lockable_surf);
		K__DX9_RELEASE(m_d3dtex);
		K__DX9_RELEASE(m_target_surf);
		m_pitch = 0;
		if (!keep_backup) {
			K__DX9_RELEASE(m_backup_tex);
			zero_clear();
		}
	}
	virtual int getWidth() const override {
		return m_desc.Width;
	}
	virtual int getHeight() const override {
		return m_desc.Height;
	}
	virtual Format getFormat() const override {
		return m_kfmt;
	}
	virtual KTEXID getId() const override {
		return m_ktexid;
	}
	virtual bool isRenderTarget() const override {
		return (m_desc.Usage & D3DUSAGE_RENDERTARGET) != 0;
	}
	virtual int getSizeInBytes() const override {
		return m_pitch * m_desc.Height;
	}
	virtual void fill(const KColor &color) override {
		m_d3ddev->ColorFill(m_color_surf, nullptr, color.toColor32().toUInt32());
	}
	virtual void fillEx(const KColor &color, KColorChannels channels) override {
		K__ASSERT(m_d3ddev);
		DWORD color32 = color.toColor32().toUInt32();
		if (channels == KColorChannel_RGBA) {
			// 普通の塗りつぶし
			IDirect3DSurface9 *d3dsurf;
			if (m_d3dtex && m_d3dtex->GetSurfaceLevel(0, &d3dsurf) == S_OK) {
				m_d3ddev->ColorFill(d3dsurf, nullptr, color32);
				K__DX9_RELEASE(d3dsurf);
			}
		} else {
			// チャンネルを指定した塗りつぶし
			// ターゲット指定
			KVideo::pushRenderState();
			KVideo::pushRenderTarget(m_ktexid);
			// マスク設定
			DWORD oldmask;
			m_d3ddev->GetRenderState(D3DRS_COLORWRITEENABLE, &oldmask);
			KVideo::setColorWriteMask(channels);
			// ポリゴン
			struct _Vert {
				KVec4 xyzw;
				DWORD dif;
			};
			_Vert vertex[4];
			memset(vertex, 0, sizeof(vertex));
			{
				const DWORD diffuse = color32;
				int x, y, w, h;
				KVideo::getViewport(&x, &y, &w, &h);
				float x0 = (float)x;
				float y0 = (float)y;
				float x1 = (float)x + w;
				float y1 = (float)y + h;
				if (1 /*MO_VIDEO_ENABLE_HALF_PIXEL*/) {
					vertex[0].xyzw = KVec4(x0-K_HALF_PIXEL, y0-K_HALF_PIXEL, 0.0f, 1.0f); // ピクセル単位で指定することに注意
					vertex[1].xyzw = KVec4(x1-K_HALF_PIXEL, y0-K_HALF_PIXEL, 0.0f, 1.0f);
					vertex[2].xyzw = KVec4(x0-K_HALF_PIXEL, y1-K_HALF_PIXEL, 0.0f, 1.0f);
					vertex[3].xyzw = KVec4(x1-K_HALF_PIXEL, y1-K_HALF_PIXEL, 0.0f, 1.0f);
				} else {
					vertex[0].xyzw = KVec4(x0, y0, 0.0f, 1.0f); // ピクセル単位で指定することに注意
					vertex[1].xyzw = KVec4(x1, y0, 0.0f, 1.0f);
					vertex[2].xyzw = KVec4(x0, y1, 0.0f, 1.0f);
					vertex[3].xyzw = KVec4(x1, y1, 0.0f, 1.0f);
				}
				vertex[0].dif = diffuse;
				vertex[1].dif = diffuse;
				vertex[2].dif = diffuse;
				vertex[3].dif = diffuse;
			}
			// 描画
			m_d3ddev->SetTexture(0, nullptr);
			m_d3ddev->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1);
			m_d3ddev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, &vertex[0], sizeof(_Vert));
			// マスク戻す
			m_d3ddev->SetRenderState(D3DRS_COLORWRITEENABLE, oldmask);
			// ターゲット戻す
			KVideo::popRenderTarget();
			KVideo::popRenderState();
		}
	}
	virtual void getDesc(Desc *desc) const override {
		if (desc == nullptr) return;
		memset(desc, 0, sizeof(*desc));
		desc->w = m_desc.Width;
		desc->h = m_desc.Height;
		desc->original_w = m_original_w;
		desc->original_h = m_original_h;
		desc->original_u = (float)m_original_w / m_desc.Width;
		desc->original_v = (float)m_original_h / m_desc.Height;
		desc->pitch = m_pitch;
		desc->size_in_bytes = m_pitch * m_desc.Height;
		desc->pixel_format = getPixelFormat();
		desc->is_render_target = isRenderTarget();
		desc->d3dtex9 = m_d3dtex;
	}
	virtual void * getDirect3DTexture9() override {
		return m_d3dtex;
	}

	// 点 x, y におけるピクセル値を得る
	// 浮動小数テクスチャの場合に色値が 0..1 でクランプされないよう KColor ではなく KVec4 で返す
	virtual KVec4 getPixelValue(int x, int y) override {
		KVec4 ret;
		if (0 <= x && x < (int)m_desc.Width && 0 <= y && y < (int)m_desc.Height) {
			void *ptr = lockData();
			if (ptr) {
				switch (m_desc.Format) {
				case D3DFMT_A8R8G8B8:
					{
						uint32_t val32 = ((uint32_t*)ptr)[m_desc.Width * y + x];
						KColor32 col32 = KColor32::fromARGB32(val32);
						KColor col = col32.toColor();
						ret = KVec4(col.r, col.g, col.b, col.a);
						break;
					}
				case D3DFMT_A16B16G16R16F:
					// 16ビットfloat は C++ で直接扱えないので、
					// 半精度浮動小数をバイナリ形式から自力でデコードする必要があることに注意
					{
						break;
					}
				}
				unlockData();
			}
		}
		return ret;
	}
	bool loadByDesc(const D3DSURFACE_DESC *_desc) {
		if (m_d3ddev == nullptr) {
			return false;
		}
		K__ASSERT(m_d3ddev);
		K__ASSERT(m_color_surf == nullptr);
		K__ASSERT(m_depth_surf == nullptr);
		K__ASSERT(m_lockable_surf == nullptr);
		K__ASSERT(m_d3dtex == nullptr);
		K__ASSERT(m_pitch == 0);
		D3DSURFACE_DESC param = _desc ? *_desc : m_desc;

		m_required_w = param.Width;
		m_required_h = param.Height;

		// 作成しようとしているサイズがサポートサイズを超えているなら、作成可能なサイズまで小さくしておく。
		// テクスチャは正規化されたUV座標で描画するため、テクスチャ自体のサイズは関係ない。
		// ただし、メッシュなどでテクスチャ範囲がピクセル単位で指定されている場合は、テクスチャの実際のサイズを考慮しないといけないので注意。
		// --> mo__Texture_writeImageStretch()
		UINT maxW = DX9_getMaxTextureWidth(m_caps);
		UINT maxH = DX9_getMaxTextureHeight(m_caps);
		if (param.Width > maxW) {
			param.Width = maxW;
		}
		if (param.Height > maxH) {
			param.Height = maxH;
		}

		// テクスチャを作成
		HRESULT hr = m_d3ddev->CreateTexture(param.Width, param.Height, 1, param.Usage, param.Format, param.Pool, &m_d3dtex, nullptr);
		if (FAILED(hr)) {
			K__VIDEO_ERR("E_FAIL_D3D: Failed to CreateTexture: Size=(%d, %d), Format=(D3DFORMAT)%d, HRESULT='%s'",
				param.Width, param.Height, param.Format, K__GetErrorStringUtf8(hr).c_str());
			return false;
		}
		// テクスチャ情報を取得
		m_d3dtex->GetLevelDesc(0, &m_desc);

		if (m_desc.Usage == D3DUSAGE_RENDERTARGET) {
			// レンダーテクスチャを生成した
			// ターゲットサーフェスと深度バッファを取得しておく
			m_d3dtex->GetSurfaceLevel(0, &m_color_surf);
			hr = m_d3ddev->CreateDepthStencilSurface(m_desc.Width, m_desc.Height, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, FALSE, &m_depth_surf, nullptr);
			if (FAILED(hr)) {
				K__VIDEO_ERR("E_FAIL_D3D: Failed to CreateDepthStencilSurface for render target: Size=(%d, %d), Format=(D3DFORMAT)%d, HRESULT='%s'",
					m_desc.Width, m_desc.Height, param.Format, K__GetErrorStringUtf8(hr).c_str());
				return false;
			}
			// レンダーターゲットのバッファは初期化済みとは限らない
			// ゼロで塗りつぶしておく
			m_d3ddev->ColorFill(m_color_surf, nullptr, 0);
		}
		switch (m_desc.Format) {
		case D3DFMT_A8R8G8B8:
			m_pitch = m_desc.Width * 4;
			break;
		case D3DFMT_A16B16G16R16F:
			m_pitch = m_desc.Width * 8; // half float
			break;
		default:
			K__ASSERT(0);
			m_pitch = 0;
			break;
		}
		return true;
	}
	void onDeviceLost() {
		if (m_desc.Pool == D3DPOOL_DEFAULT) {
			// テクスチャ内容を退避する
			if (m_backup_tex) {
				void *src_bits = lockData();
				if (src_bits) {
					D3DLOCKED_RECT dst;
					if (m_backup_tex->LockRect(0, &dst, nullptr, 0) == D3D_OK) {
						K__ASSERT(dst.Pitch == m_pitch);
						memcpy(dst.pBits, src_bits, dst.Pitch * m_desc.Height);
						m_backup_tex->UnlockRect(0);
					}
					unlockData();
				}
			}
			destroy(true);
		}
	}
	void onDeviceReset() {
		if (m_desc.Pool == D3DPOOL_DEFAULT) {
			loadByDesc(&m_desc);

			// テクスチャ内容を復帰する
			if (m_backup_tex) {
				m_backup_tex->AddDirtyRect(nullptr);
				HRESULT hr = m_d3ddev->UpdateTexture(m_backup_tex, m_d3dtex); // srcのフォーマットは D3DPOOL_SYSTEMMEM であること
				if (FAILED(hr)) {
					std::string msg = K__GetErrorStringUtf8(hr);
				//	K__VIDEO_ERR(msg.c_str());
				}
			}
		}
	}
	virtual void * lockData() override {
		if (m_d3dtex == nullptr) {
			K__VIDEO_ERR("E_LOCK: null device");
			return nullptr;
		}
		if (m_desc.Width == 0 || m_desc.Height == 0) {
			K__VIDEO_ERR("E_LOCK: invalid texture params");
			return nullptr;
		}
		if (m_pitch == 0) {
			K__VIDEO_ERR("E_LOCK: invalid pitch");
			return nullptr;
		}
		if (isRenderTarget()) {
			HRESULT hr;
			if (m_target_surf == nullptr) {
				hr = m_d3ddev->CreateOffscreenPlainSurface(
					m_desc.Width, m_desc.Height, m_desc.Format,
					D3DPOOL_SYSTEMMEM, &m_target_surf, nullptr);  // コピー用サーフェスを作成
				if (FAILED(hr)) {
					DX9_log("E_LOCK: CreateOffscreenPlainSurface failed", hr);
					return nullptr;
				}
			}
			IDirect3DSurface9 *tmp_surf = nullptr;
			hr = m_d3dtex->GetSurfaceLevel(0, &tmp_surf);
			if (FAILED(hr)) {
				DX9_log("E_LOCK: GetSurfaceLevel failed", hr);
				return nullptr;
			}
			hr = m_d3ddev->GetRenderTargetData(tmp_surf, m_target_surf);
			K__DX9_RELEASE(tmp_surf);
			if (FAILED(hr)) {
				DX9_log("E_LOCK: Sruface GetRenderTargetData failed", hr);
				return nullptr;
			}
			D3DLOCKED_RECT lrect;
			hr = m_target_surf->LockRect(&lrect, nullptr, D3DLOCK_NOSYSLOCK);
			if (FAILED(hr)) {
				DX9_log("E_LOCK: RenderTexture LockRect failed", hr);
				return nullptr;
			}
			return lrect.pBits;

		} else {
			D3DLOCKED_RECT lrect;
			HRESULT hr = m_d3dtex->LockRect(0, &lrect, nullptr, 0);
			if (FAILED(hr)) {
				DX9_log("E_LOCK: Texture2D LockRect failed", hr);
				return nullptr;
			}
			return lrect.pBits;
		}
	}
	virtual void unlockData() override {
		if (isRenderTarget()) {
			if (m_target_surf) {
				m_target_surf->UnlockRect();
			}
		} else {
			if (m_d3dtex) {
				m_d3dtex->UnlockRect(0);
			}
		}
	}
	bool exportTextureImage(int channel, KBmp *bmp) {
		// RGBA配列を取得
		if (getImage(bmp)) {
			if (channel == 3) { //0=R, 1=G, 2=B, 3=A
				// アルファチャンネルが指定された。
				// アルファチャンネルをグレースケールに変えておく
				for (int y=0; y<bmp->h; y++) {
					for (int x=0; x<bmp->w; x++) {
						const int i = bmp->get_offset(x, y);
					//	const uint8_t b = bmp->data[i+0];
					//	const uint8_t g = bmp->data[i+1];
					//	const uint8_t r = bmp->data[i+2];
						const uint8_t a = bmp->data[i+3];
						bmp->data[i+0] = a;
						bmp->data[i+1] = a;
						bmp->data[i+2] = a;
						bmp->data[i+3] = 255;
					}
				}
				return true;
			}
		}
		return false;
	}
	virtual KImage exportTextureImage(int channel) override {
		KImage img = KImage::createFromPixels(m_desc.Width, m_desc.Height, KColorFormat_RGBA32, nullptr);
		KBmp bmp;
		img.lock(&bmp);
		exportTextureImage(channel, &bmp);
		img.unlock();
		return img;
	}
	virtual void writeImageToTexture(const KImage &image, float u, float v) override {
		KBmp bmp;
		image.lock(&bmp);
		writeImage(&bmp, u, v);
		image.unlock();
	}
	virtual bool writeImageFromBackBuffer() override {
		K__ASSERT(m_d3ddev);

		// バックバッファの内容を「ロック可能なサーフェス」 m_lockable_surf に転送する
		updateLockableSurfaceFromBackBuffer();

		// 「ロック可能サーフェス」の内容を指定されたテクスチャに転送する
		if (m_lockable_surf) {
			HRESULT hr = m_d3ddev->UpdateSurface(
				m_lockable_surf, nullptr, // Source
				m_color_surf, nullptr // Destination
			);
			K__ASSERT(SUCCEEDED(hr));
			return true;
		}
		return false;
	}
	/// 元画像のUV単位での座標に対する。現在のテクスチャのUV座標を返す
	/// orig_u 元画像の横幅を 1.0 としたときの座標。
	/// orig_v 元画像の高さを 1.0 としたときの座標。
	/// 戻り値は、現在のテクスチャのサイズを (1.0, 1.0) としたときの座標 (UV値)
	virtual KVec2 getTextureUVFromOriginalUV(const KVec2 &orig_uv) override {
		K__ASSERT(m_desc.Width > 0 && m_desc.Height > 0);
		return KVec2(
			orig_uv.x * m_original_w / (float)m_desc.Width,
			orig_uv.y * m_original_h / (float)m_desc.Height
		);
	}
	virtual KVec2 getTextureUVFromOriginalPoint(const KVec2 &pixel) override {
		K__ASSERT(m_desc.Width > 0 && m_desc.Height > 0);
		return KVec2(
			pixel.x / (float)m_desc.Width,
			pixel.y / (float)m_desc.Height
		);
	}
	virtual void blit(KTexture *src, KMaterial *mat) override {
		_BlitEx(this, src, mat, nullptr, nullptr, nullptr);
	}
	virtual void blitEx(KTexture *src, KMaterial *mat, const KVideoRect *src_rect, const KVideoRect *dst_rect, const KMatrix4 *transform) override {
		_BlitEx(this, src, mat, src_rect, dst_rect, transform);
	}
	KColorFormat getPixelFormat() const {
		switch (m_desc.Format) {
		case D3DFMT_A8R8G8B8: return KColorFormat_RGBA32;
		case D3DFMT_R8G8B8:   return KColorFormat_RGB24;
		}
		return KColorFormat_NOFMT;
	}
	void writeImage(const KBmp *bmp) {
		if (!KBmp::isvalid(bmp)) return;
		int w = KMath::min((int)m_desc.Width, bmp->w);
		int h = KMath::min((int)m_desc.Height, bmp->h);
		int srcPitch = bmp->pitch;
		int dstPitch = m_pitch;
		const uint8_t *srcBuf = bmp->data;
		uint8_t *dstBuf = (uint8_t *)lockData();
		if (srcBuf && dstBuf) {
			for (int y=0; y<h; y++) {
				auto src = srcBuf + srcPitch * y;
				auto dst = dstBuf + dstPitch * y;
				for (int x=0; x<w; x++) {
					const int srcIdx = x * 4;
					const int dstIdx = x * 4;
					const uint8_t r = src[srcIdx + 0];
					const uint8_t g = src[srcIdx + 1];
					const uint8_t b = src[srcIdx + 2];
					const uint8_t a = src[srcIdx + 3];
					dst[dstIdx + 0] = b;
					dst[dstIdx + 1] = g;
					dst[dstIdx + 2] = r;
					dst[dstIdx + 3] = a;
				}
			}
			unlockData();
		}
	}
	void writeImageStretch(const KBmp *bmp, float u, float v) {
		if (!KBmp::isvalid(bmp)) return;

		int srcW = bmp->w;
		int srcH = bmp->h;
		int srcPitch = bmp->pitch;
		int dstPitch = m_pitch;
		const uint8_t *srcBuf = bmp->data;
		uint8_t *dstBuf = (uint8_t *)lockData();
		if (srcBuf == nullptr) return;
		if (dstBuf == nullptr) return;

		// BGRA 順で書き込む
		for (int y=0; y<(int)m_desc.Height; y++) {
			auto src = srcBuf + srcPitch * (int)(srcH / m_desc.Height * u * y);
			auto dst = dstBuf + dstPitch * y;
			for (int x=0; x<(int)m_desc.Width; x++) {
				const int srcIdx = 4 * (int)(srcW / m_desc.Width * v * x);
				const int dstIdx = 4 * x;
				const uint8_t r = src[srcIdx + 0];
				const uint8_t g = src[srcIdx + 1];
				const uint8_t b = src[srcIdx + 2];
				const uint8_t a = src[srcIdx + 3];
				dst[dstIdx + 0] = b;
				dst[dstIdx + 1] = g;
				dst[dstIdx + 2] = r;
				dst[dstIdx + 3] = a;
			}
		}
		unlockData();
	}
	void writeImage(const KBmp *bmp, float u, float v) {
		if (!KBmp::isvalid(bmp)) return;

		m_original_w = bmp->w;
		m_original_h = bmp->h;

		if (K__EQUALS(u, 0.0f) || K__EQUALS(v, 0.0f)) {
			// コピー元、コピー先のサイズに関係なく、ピクセルが1対1で対応するように拡縮なしでコピーする
			writeImage(bmp);
			return;
		}

		if (m_desc.Width  == m_required_w &&
			m_desc.Height == m_required_h &&
			K__EQUALS(u, 1.0f) && K__EQUALS(v, 1.0f)
		) {
			// コピー元とコピー先のサイズが一致していて、ＵＶ範囲が 1.0 x 1.0 になっている場合は
			// ピクセルが1対1で対応するように拡縮なしでコピーできる
			writeImage(bmp);
			return;
		}

		// 要求されたサイズと、実際に作成されたテクスチャサイズが異なるか、
		// コピー UV 範囲が異なる。拡縮ありでコピーする
		writeImageStretch(bmp, u, v);
	}
	bool getImage(KBmp *bmp) {
		if (!KBmp::isvalid(bmp)) {
			return false;
		}
		if (bmp->w != m_desc.Width || bmp->h != m_desc.Height) {
			return false; // サイズが違う
		}

		// ピクセルデータをコピーする
		const uint8_t *locked_ptr = (const uint8_t *)lockData();
		if (locked_ptr == nullptr) {
			return false; // ロックできない
		}

		// テクスチャデータを転送する
		_BmpWrite(bmp, locked_ptr, m_pitch, m_desc.Width, m_desc.Height, -1);

		unlockData();
		return true;
	}
	// m_lockable_surf に バックバッファの内容を転送する
	void updateLockableSurfaceFromBackBuffer() {
		HRESULT hr;

		// 現在のバックバッファを取得
		IDirect3DSurface9 *back_surf = nullptr;
		hr = m_d3ddev->GetRenderTarget(0, &back_surf);
		K__ASSERT(SUCCEEDED(hr));
		K__ASSERT(back_surf);

		// バックバッファのパラメータを得る
		D3DSURFACE_DESC back_desc;
		memset(&back_desc, 0, sizeof(back_desc));
		hr = back_surf->GetDesc(&back_desc);
		K__ASSERT(SUCCEEDED(hr));

		// 現在持っているロック可能サーフェスが再利用できるか調べる
		bool recycle = false;
		if (m_lockable_surf) {
			D3DSURFACE_DESC lockable_desc;
			memset(&lockable_desc, 0, sizeof(lockable_desc));
			m_lockable_surf->GetDesc(&lockable_desc);
			if (lockable_desc.Width == back_desc.Width   &&
			    lockable_desc.Height == back_desc.Height &&
			    lockable_desc.Format == back_desc.Format)
			{
				// サイズとフォーマットが同じ。再利用する
				recycle = true;
			}
		}

		// 再利用できなければ古いものを廃棄して新規作成する
		if (!recycle) {
			K__DX9_RELEASE(m_lockable_surf);
			// バックバッファは D3DPOOL_DEFAULT になっているので Lock できない。
			// バックバッファと同じサイズ＆フォーマットで「ロック可能なサーフェス」を作成する
			hr = m_d3ddev->CreateOffscreenPlainSurface(back_desc.Width, back_desc.Height, back_desc.Format, D3DPOOL_SYSTEMMEM, &m_lockable_surf, nullptr);
			K__ASSERT(SUCCEEDED(hr));
			K__ASSERT(m_lockable_surf);
		}

		// バックバッファの内容をロック可能サーフェスに転送する
		hr = m_d3ddev->GetRenderTargetData(back_surf, m_lockable_surf);
		K__ASSERT(SUCCEEDED(hr));

		K__DX9_RELEASE(back_surf);
	}

	virtual void clearRenderTargetStencil(int stencil) {
		if (isRenderTarget()) {
			KVideo::pushRenderState();
			KVideo::pushRenderTarget(m_ktexid);
			KVideo::clearStencil(stencil);
			KVideo::popRenderTarget();
			KVideo::popRenderState();
		}
	}
	virtual void clearRenderTargetDepth(float depth) {
		if (isRenderTarget()) {
			KVideo::pushRenderState();
			KVideo::pushRenderTarget(m_ktexid);
			KVideo::clearDepth(depth);
			KVideo::popRenderTarget();
			KVideo::popRenderState();
		}
	}
};
#pragma endregion // CD3DTex


static CD3DTex * CD3D9_findTexture(KTEXID tex);


#pragma region CD3DShader

static KTEXID g_ScreenCopyTex = nullptr;
static int g_NewShaderId = 0;

class CD3DShader: public KShader {
public:
	enum PARAMHANDLE {
		PH_MATRIX_PROJ,
		PH_MATRIX_VIEW,
		PH_DIFFUSE,
		PH_SPECULAR,
		PH_TEXTURE,
		PH_TEX_SIZE,
		PH_SCREEN_TEX,
		PH_SCREEN_TEX_SIZE,
		PH_TIME_SEC,
		PH_ENUM_MAX
	};
	IDirect3DDevice9 *m_d3ddev;
	ID3DXEffect *m_d3deff;
	D3DXEFFECT_DESC m_desc;
	D3DXHANDLE m_param_handles[PH_ENUM_MAX];
	KSHADERID m_id;
public:
	CD3DShader() {
		zero_clear();
		g_NewShaderId++;
		m_id = (KSHADERID)g_NewShaderId;
	}
	void zero_clear() {
		m_d3ddev = nullptr;
		m_d3deff = nullptr;
		ZeroMemory(m_param_handles, sizeof(m_param_handles));
		ZeroMemory(&m_desc, sizeof(m_desc));
	}
	virtual KSHADERID getId() const override {
		return m_id;
	}
	virtual void getDesc(Desc *out_desc) const override {
		if (out_desc) {
			memset(out_desc, 0, sizeof(*out_desc));
			out_desc->num_technique = m_desc.Techniques;
			out_desc->d3dxeff = m_d3deff;
			strcpy_s(out_desc->type, sizeof(out_desc->type), "HLSL");
		}
	}
	bool make(IDirect3DDevice9 *d3ddev, const char *hlsl_u8, const char *name_debug) {
		K__ASSERT(m_d3ddev == nullptr);
		K__ASSERT(m_d3deff == nullptr);
		K__ASSERT(d3ddev);
		K__ASSERT(hlsl_u8);
		K__ASSERT(name_debug);
		DWORD flags = 0;// = D3DXSHADER_PARTIALPRECISION; <-- もしかして、時間小数計算の精度が悪かったのでこれが原因じゃね？？
	#ifdef _DEBUG
		flags |= D3DXSHADER_DEBUG; // デバッグ有効
	#endif
		ID3DXBuffer *dxerr = nullptr;
		ID3DXEffect *dxeff = nullptr;

		SetLastError(0);
		HRESULT hr = D3DXCreateEffect(d3ddev, hlsl_u8, strlen(hlsl_u8), nullptr, nullptr, flags, nullptr, &dxeff, &dxerr);

		if (FAILED(hr)) {
			void *msg_p = dxerr ? dxerr->GetBufferPointer() : nullptr;
			const char *msg = msg_p ? (const char *)msg_p : "";
			K__VIDEO_ERR(
				"E_FAIL_D3D: Failed to D3DXCreateEffect (HRESULT=%s): %s %s",
				K__GetErrorStringUtf8(hr).c_str(),
				name_debug,
				msg
			);
			K__DX9_RELEASE(dxerr);
			return false;
		}
		dxeff->GetDesc(&m_desc);
		m_d3ddev = d3ddev;
		m_d3deff = dxeff;
		// よく使うシェーダー変数のIDを取得
		m_param_handles[PH_MATRIX_PROJ]     = m_d3deff->GetParameterByName(nullptr, K__DX9_SHADERPARAM_MATRIX_PROJ);
		m_param_handles[PH_MATRIX_VIEW]     = m_d3deff->GetParameterByName(nullptr, K__DX9_SHADERPARAM_MATRIX_VIEW);
		m_param_handles[PH_TIME_SEC]        = m_d3deff->GetParameterByName(nullptr, K__DX9_SHADERPARAM_TIME_SEC);
		m_param_handles[PH_TEXTURE]         = m_d3deff->GetParameterByName(nullptr, K__DX9_SHADERPARAM_MAIN_TEXTURE);
		m_param_handles[PH_TEX_SIZE]        = m_d3deff->GetParameterByName(nullptr, K__DX9_SHADERPARAM_MAIN_TEXTURE_SIZE);
		m_param_handles[PH_SCREEN_TEX]      = m_d3deff->GetParameterByName(nullptr, K__DX9_SHADERPARAM_SCREEN_TEXTURE);
		m_param_handles[PH_SCREEN_TEX_SIZE] = m_d3deff->GetParameterByName(nullptr, K__DX9_SHADERPARAM_SCREEN_TEXTURE_SIZE);
		m_param_handles[PH_DIFFUSE]         = m_d3deff->GetParameterByName(nullptr, K__DX9_SHADERPARAM_DIFFUSE);
		m_param_handles[PH_SPECULAR]        = m_d3deff->GetParameterByName(nullptr, K__DX9_SHADERPARAM_SPECULAR);
		return true;
	}
	void destroy() {
		K__DX9_RELEASE(m_d3deff);
		zero_clear();
	}
	void setParameter_TimeSec() {
		K__ASSERT(m_d3deff);
		if (m_param_handles[PH_TIME_SEC]) {
			// time_sec の値が数万程度になるとHLSLでの浮動小数計算の情報落ちが顕著になるので注意
			// HLSLに渡す値があまり大きくならないよう、一定の周期で区切っておく
			// int msec = KClock::getSystemTimeMsec() % (60 * 1000);
			// float time_sec = msec / 1000.0f;
			float time_sec = K__GetTimeSec();
			m_d3deff->SetFloat(m_param_handles[PH_TIME_SEC], time_sec);
		}
	}
	void setParameter_Diffuse(const float *rgba) {
		K__ASSERT(m_d3deff);
		if (m_param_handles[PH_DIFFUSE]) {
			m_d3deff->SetFloatArray(m_param_handles[PH_DIFFUSE], rgba, 4);
		}
	}
	void setParameter_Specular(const float *rgba) {
		K__ASSERT(m_d3deff);
		if (m_param_handles[PH_SPECULAR]) {
			m_d3deff->SetFloatArray(m_param_handles[PH_SPECULAR], rgba, 4);
		}
	}
	void setParameter_Texture(KTEXID texid) {
		K__ASSERT(m_d3deff);
		if (m_param_handles[PH_TEXTURE]) {
			CD3DTex *tex = CD3D9_findTexture(texid);
			if (tex) {
				m_d3deff->SetTexture(m_param_handles[PH_TEXTURE], tex->m_d3dtex); // d3dtex は nullptr の場合もある
				DX9_checkRenderIOTexture(m_d3ddev, tex->m_d3dtex);
			}
		}
	}
	void setParameter_TexSize(KTEXID texid) {
		K__ASSERT(m_d3deff);
		if (m_param_handles[PH_TEX_SIZE]) {
			CD3DTex *tex = CD3D9_findTexture(texid);
			if (tex) {
				float val[] = {
					(float)tex->getWidth(),
					(float)tex->getHeight()
				};
				m_d3deff->SetFloatArray(m_param_handles[PH_TEX_SIZE], val, 2);
			}
		}
	}
	// 現時点でスクリーンとして書き出している内容をテクスチャとして取得する
	void setParameter_ScreenTexture() {
		#if K__DX9_SCREENTEX_TEST
		K__ASSERT(m_d3deff);
		if (m_param_handles[PH_SCREEN_TEX]) {
			// 現在のレンダーターゲットの内容をテクスチャとして使う

			// 現在のレンダーターゲットの設定
			D3DSURFACE_DESC rt_desc;
			{
				memset(&rt_desc, 0, sizeof(rt_desc));
				IDirect3DSurface9 *rt_surf = nullptr;
				m_d3ddev->GetRenderTarget(0, &rt_surf);
				rt_surf->GetDesc(&rt_desc);
				rt_surf->Release();
			}

			// スクリーンテクスチャが存在する場合、それが再利用できるか判定する
			bool recycle = false;
			if (g_ScreenCopyTex) {
				CD3DTex *rtex = CD3D9_findTexture(g_ScreenCopyTex);
				if (rtex) {
					if (rtex->getWidth() == rt_desc.Width && rtex->getHeight() == rt_desc.Height) {
						recycle = true;
					}
				}
			}

			// スクリーンテクスチャがまだ存在しないか、既存のものを再利用できない場合は新規作成する
			if (!recycle) {
				if (g_ScreenCopyTex) {
					KVideo::deleteTexture(g_ScreenCopyTex);
					g_ScreenCopyTex = nullptr;
				}
				g_ScreenCopyTex = KVideo::createRenderTexture(rt_desc.Width, rt_desc.Height);
			}
			K__ASSERT(g_ScreenCopyTex);
			
			// Clear
			{
				KTexture *tex = CD3D9_findTexture(g_ScreenCopyTex);
				if (tex) tex->fill(KColor::ZERO);
			}

			// 現在のレンダーターゲットの内容を取得
			CD3DTex *screen_texobj = CD3D9_findTexture(g_ScreenCopyTex);
			K__ASSERT(screen_texobj);
			screen_texobj->writeImageFromBackBuffer();

			// スクリーンテクスチャとして設定する
			m_d3deff->SetTexture(m_param_handles[PH_SCREEN_TEX], screen_texobj->m_d3dtex);
			DX9_checkRenderIOTexture(m_d3ddev, screen_texobj->m_d3dtex);
		}
		#endif // K__DX9_SCREENTEX_TEST
	}
	void setParameter_ScreenTexSize() {
		#if K__DX9_SCREENTEX_TEST
		K__ASSERT(m_d3deff);
		if (m_param_handles[PH_SCREEN_TEX_SIZE]) {
			// 現在のターゲットのサイズを得る
			IDirect3DSurface9 *surf_target = nullptr;
			m_d3ddev->GetRenderTarget(0, &surf_target);
			if (surf_target) {
				D3DSURFACE_DESC desc;
				if (surf_target->GetDesc(&desc) == S_OK) {
					float val[] = {(float)desc.Width, (float)desc.Height};
					m_d3deff->SetFloatArray(m_param_handles[PH_SCREEN_TEX_SIZE], val, 2);
				}
				surf_target->Release();
			}
		}
		#endif // K__DX9_SCREENTEX_TEST
	}
	void setParameter_ProjMatrix(const D3DXMATRIX &proj) {
		K__ASSERT(m_d3deff);
		if (m_param_handles[PH_MATRIX_PROJ]) {
			m_d3deff->SetMatrix(m_param_handles[PH_MATRIX_PROJ], &proj);
		}
	}
	void setParameter_ViewMatrix(const D3DXMATRIX &transform) {
		K__ASSERT(m_d3deff);
		if (m_param_handles[PH_MATRIX_VIEW]) {
			m_d3deff->SetMatrix(m_param_handles[PH_MATRIX_VIEW], &transform);
		}
	}
	void bindShader() {
		K__ASSERT(m_d3deff);
		HRESULT hr;
		D3DXHANDLE technique = m_d3deff->GetTechnique(0); // 最初のテクニック
		hr = m_d3deff->SetTechnique(technique);
		if (FAILED(hr)) {
			DX9_log("SetTechnique", hr);
		}
		m_d3deff->Begin(nullptr, 0);
		hr = m_d3deff->BeginPass(0);
		if (FAILED(hr)) {
			DX9_log("BeginPass", hr);
		}
	}
	void unbindShader() {
		if (m_d3deff) {
			m_d3deff->EndPass();
			m_d3deff->End();
		}
	}
	void setInt( const char *name, const int *values, int count) {
		K__ASSERT(m_d3deff);
		D3DXHANDLE handle = m_d3deff->GetParameterByName(nullptr, name);
		m_d3deff->SetIntArray(handle, values, count);
	}
	void setFloat(const char *name, const float *values, int count) {
		K__ASSERT(m_d3deff);
		D3DXHANDLE handle = m_d3deff->GetParameterByName(nullptr, name);
		m_d3deff->SetFloatArray(handle, values, count);
	}
	void setTexture(const char *name, IDirect3DTexture9 *d3dtex) {
		K__ASSERT(m_d3deff);
		D3DXHANDLE handle = m_d3deff->GetParameterByName(nullptr, name);
		m_d3deff->SetTexture(handle, d3dtex);
		DX9_checkRenderIOTexture(m_d3ddev, d3dtex);
	}
	void onDeviceLost() {
		if (m_d3deff) {
			m_d3deff->OnLostDevice();
		}
	#if K__DX9_SCREENTEX_TEST
		if (g_ScreenCopyTex) {
			KVideo::deleteTexture(g_ScreenCopyTex);
			g_ScreenCopyTex = nullptr;
		}
	#endif
	}
	void onDeviceReset() {
		if (m_d3deff) {
			m_d3deff->OnResetDevice();
		}
	}
};
#pragma endregion // CD3DShader


#pragma region CD3D9
class CD3D9 {
	struct SSurfItem {
		SSurfItem() {
			color_surf = nullptr;
			depth_surf = nullptr;
		}
		IDirect3DSurface9 *color_surf;
		IDirect3DSurface9 *depth_surf;
	};
	struct SFloat3 {
		SFloat3() { x=y=z=0; }
		float x, y, z;
	};
	struct SFloat2 {
		SFloat2() { x=y=0; }
		float x, y;
	};
	D3DXMATRIX m_projection_matrix;
	D3DXMATRIX m_transform_matrix;
	std::vector<SSurfItem> m_rendertarget_stack;
	std::vector<IDirect3DStateBlock9 *> m_d3dblocks;
	std::unordered_map<KTEXID, CD3DTex*> m_texlist;
	std::unordered_map<KSHADERID, CD3DShader*> m_shaderlist;
	std::vector<DX9_VERTEX> m_vtmp;
	std::vector<int> m_itmp;
	std::vector<SFloat3> m_buf_pos;
	std::vector<DWORD> m_buf_dif;
	std::vector<DWORD> m_buf_spe;
	std::vector<SFloat2> m_buf_uv1;
	std::vector<SFloat2> m_buf_uv2;
	CD3DShader *m_curr_shader;
	DWORD m_diffuse;
	DWORD m_specular;
	KTEXID m_texture;
	D3DCAPS9 m_devcaps;
	HWND m_hWnd;
	D3DPRESENT_PARAMETERS m_d3dpp;
	IDirect3D9 *m_d3d9;
	IDirect3DDevice9 *m_d3ddev;
	std::recursive_mutex m_mutex;
	uint32_t m_video_thread_id;
	int m_drawcalls;
	int m_drawbuf_maxvert;
	bool m_drawbuf_using_index;
	bool m_shader_available;
	bool m_color_tex_changed;
public:
	CD3D9() {
		init_init(false);
	}
	~CD3D9() {
		shutdown();
	}
	void init_init(bool check_flag) {
		m_hWnd = nullptr;
		ZeroMemory(&m_d3dpp, sizeof(m_d3dpp));
		ZeroMemory(&m_devcaps, sizeof(m_devcaps));
		m_rendertarget_stack.clear();
		m_d3dblocks.clear();
		m_texlist.clear();
		m_shaderlist.clear();
		m_vtmp.clear();
		m_itmp.clear();
		m_buf_pos.clear();
		m_buf_dif.clear();
		m_buf_spe.clear();
		m_buf_uv1.clear();
		m_buf_uv2.clear();
		m_video_thread_id = 0;
		m_curr_shader = nullptr;
		m_d3d9 = nullptr;
		m_d3ddev = nullptr;
		m_drawcalls = 0;
		m_drawbuf_maxvert = 0;
		m_drawbuf_using_index = false;
		m_shader_available = false;
		m_color_tex_changed = false;
		m_diffuse = K__DX9_COLOR_WHITE;
		m_specular = K__DX9_COLOR_ZERO;
		m_texture = nullptr;
		m_projection_matrix = D3DXMATRIX();
		m_transform_matrix = D3DXMATRIX();
		if (check_flag) {
			if (g_video_limit.use_square_texture_only) {
				K__VIDEO_WRN(u8"use_square_texture_only スイッチにより、テクスチャサイズは正方形のみに制限されました");
			}
			if (g_video_limit.limit_texture_size > 0) {
				K__VIDEO_WRN(u8"limit_texture_size スイッチにより、テクスチャサイズが %d 以下に制限されました", g_video_limit.limit_texture_size);
			}
			if (g_video_limit.disable_pixel_shader) {
				K__VIDEO_WRN(u8"disable_pixel_shader によりプログラマブルシェーダーが無効になりました");
			}
		}
	}
	bool init(HWND window_handle, IDirect3D9 *d3d9, IDirect3DDevice9 *d3ddev9) {
		// 初期化
		ZeroMemory(&m_d3dpp, sizeof(m_d3dpp));
		m_d3d9 = nullptr;
		m_d3ddev = nullptr;
		m_projection_matrix = D3DXMATRIX();
		m_transform_matrix = D3DXMATRIX();
		m_curr_shader = nullptr;
		// ウィンドウハンドル。nullptrが指定された場合はこのスレッドに関連付けられているアクティブウィンドウを使う
		if (window_handle) {
			m_hWnd = window_handle;
		} else {
			m_hWnd = ::GetActiveWindow();
		}
		m_video_thread_id = K::sysGetCurrentThreadId();

		// Direct3D9 オブジェクト。nullptr が指定された場合は新規作成する
		if (!_initD3D9(d3d9)) {
			return false;
		}
		// Direct3DDevice9 オブジェクト。nullptr が指定された場合は新規作成する
		if (!_initD3DDevice9(d3ddev9)) {
			return false;
		}
		// レンダーステートを初期化
		setupDeviceStates();
		// デバイスが必要条件を満たしているかどうかを返す
		// （デバイスの作成に失敗したわけではないので、一応成功したとみなす）
		std::string msg_u8;
		if (! _checkCaps(&msg_u8)) {
			std::string text;
			text  = u8"【注意】\n";
			text += u8"お使いのパソコンは、必要なグラフィック性能を満たしていません。\n";
			text += u8"このまま起動すると、一部あるいは全ての画像が正しく表示されない可能性があります\n";
			text += u8"\n";
			text += msg_u8;
		#if 1
			K__VIDEO_ERR("%s", text.c_str());
		#else
			K::dialog(text);
		#endif
		}
		return true;
	}
	bool isInit() {
		return m_d3d9 != nullptr;
	}
	void shutdown() {
		m_diffuse = K__DX9_COLOR_WHITE;
		m_specular = K__DX9_COLOR_ZERO;
		m_texture = nullptr;
		K__DROP(m_curr_shader);
		K__DX9_LOCK_GUARD(m_mutex);
		for (auto it=m_texlist.begin(); it!=m_texlist.end(); ++it) {
			CD3DTex *tex = it->second;
			tex->destroy();
			tex->drop();
		}
		for (auto it=m_shaderlist.begin(); it!=m_shaderlist.end(); ++it) {
			CD3DShader *sh = it->second;
			sh->destroy();
			sh->drop();
		}
		m_texlist.clear();
		K__ASSERT(m_d3dblocks.empty()); // pushRenderState と popRenderState が等しく呼ばれていること
		m_d3dblocks.clear();
		K__DX9_RELEASE(m_d3ddev);
		K__DX9_RELEASE(m_d3d9);
		init_init(false);
	}
	bool _initD3D9(IDirect3D9 *d3d9) {
		K__ASSERT(m_d3d9 == nullptr);
		if (d3d9) {
			// 与えられた IDirect3D9 を使う
			m_d3d9 = d3d9;
			m_d3d9->AddRef();
			return true;
		}
		m_d3d9 = DX9_init();
		return m_d3d9 != nullptr;
	}
	bool _initD3DDevice9(IDirect3DDevice9 *d3ddev) {
		if (m_d3d9 == nullptr) return false;
		K__ASSERT(m_d3ddev == nullptr);
		m_shader_available = false;

		if (d3ddev) {
			// 与えられた IDirect3DDevice9 を使う
			m_d3ddev = d3ddev;
			m_d3ddev->AddRef();
		
		} else {
			// 新しい IDirect3DDevice9 を作成する
			m_d3ddev = DX9_initDevice(m_d3d9, m_hWnd, &m_d3dpp);
		}
		if (m_d3ddev) {
			// デバイス能力を取得
			m_d3ddev->GetDeviceCaps(&m_devcaps);

			// 実際に適用されたプレゼントパラメータを表示
			DX9_printPresentParameter("Present Parameters (Applied)", m_d3dpp);

			// デバイス能力の取得を表示
			DX9_printDeviceCaps(m_d3ddev);
		}
		return m_d3ddev != nullptr;
	}
	void setupDeviceStates() {
		if (m_d3ddev == nullptr) return;
		m_d3ddev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
		m_d3ddev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		m_d3ddev->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
		m_d3ddev->SetRenderState(D3DRS_LIGHTING, FALSE);
		m_d3ddev->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
		m_d3ddev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		m_d3ddev->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
		m_d3ddev->SetRenderState(D3DRS_ALPHAREF, 8); 
		m_d3ddev->SetRenderState(D3DRS_BLENDOP,  D3DBLENDOP_ADD);
		m_d3ddev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		m_d3ddev->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);

		// スペキュラーは頂点カラーから外したので、デフォルトではOFFにしておく。
		// マテリアルによるスペキュラ指定はSetTextureStageStateでの指定を参照せよ
		m_d3ddev->SetRenderState(D3DRS_SPECULARENABLE, FALSE); 

		m_d3ddev->SetTexture(0, nullptr);
		m_d3ddev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
		m_d3ddev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		m_d3ddev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
		m_d3ddev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
		m_d3ddev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		m_d3ddev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
		m_d3ddev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
		m_d3ddev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
		m_d3ddev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
		m_d3ddev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
		D3DMATERIAL9 material;
		memset(&material, 0, sizeof(D3DMATERIAL9));
		m_d3ddev->SetMaterial(&material);
		D3DXMATRIX matrix;
		D3DXMatrixIdentity(&matrix);
		m_d3ddev->SetTransform(D3DTS_WORLD, &matrix);
		m_d3ddev->SetTransform(D3DTS_VIEW, &matrix);
		m_d3ddev->SetTransform(D3DTS_PROJECTION, &matrix);
	}
	bool _checkCaps(std::string *message_u8) {
		K__ASSERT(m_d3ddev);
		
		std::string text_u8;
		bool ok = true;

		// テクスチャ
		{
			int reqsize = g_video_limit.max_texture_size_require;
			if (reqsize > 0) {
				if (DX9_getMaxTextureWidth(m_devcaps) < reqsize || DX9_getMaxTextureHeight(m_devcaps) < reqsize) {
					const char *msg_u8 = u8"利用可能なテクスチャサイズが小さすぎます";
					K__VIDEO_ERR("E_LOW_SPEC: TEXTURE SIZE %dx%d NOT SUPPORTED -- %s", reqsize, reqsize, msg_u8);
					text_u8 += K__StdSprintf("- %s\n", msg_u8);
					ok = false;
				}
			}
			if (DX9_hasD3DPTEXTURECAPS_SQUAREONLY(m_devcaps)) {
				const char *msg_u8 = u8"非正方形のテクスチャをサポートしていません";
				K__VIDEO_ERR("E_LOW_SPEC: SQUARE TEXTURE ONLY -- %s", msg_u8);
				text_u8 += K__StdSprintf("- %s\n", msg_u8);
				ok = false;
			}
			if ((m_devcaps.PrimitiveMiscCaps & D3DPMISCCAPS_PERSTAGECONSTANT) == 0) {
				const char *msg_u8 = u8"テクスチャステージごとの定数色指定をサポートしていません";
				K__VIDEO_ERR("E_LOW_SPEC: PERSTAGECONSTANT NOT SUPPORTED -- %s", msg_u8);
				text_u8 += K__StdSprintf("- %s\n", msg_u8);
				ok = false;
			}
		}

		// シェーダー
		{
			m_shader_available = true;

			int vs_major_ver = D3DSHADER_VERSION_MAJOR(m_devcaps.VertexShaderVersion);
			if (vs_major_ver < 2) {
				const char *msg_u8 = u8"バーテックスシェーダーをサポートしていないか、バージョンが古すぎます";
				K__VIDEO_WRN("E_NO_VERTEX_SHADER: %s", msg_u8);
				text_u8 += K__StdSprintf("- %s\n", msg_u8);
				m_shader_available = false;
			}

			int ps_major_ver = D3DSHADER_VERSION_MAJOR(m_devcaps.PixelShaderVersion);
			if (ps_major_ver < 2) {
				const char *msg_u8 = u8"ピクセルシェーダーをサポートしていないか、バージョンが古すぎます";
				K__VIDEO_WRN("E_NO_VERTEX_SHADER: %s", msg_u8);
				text_u8 += K__StdSprintf("- %s\n", msg_u8);
				m_shader_available = false;
			}

			if (g_video_limit.disable_pixel_shader) {
				const char *msg_u8 = u8"disable_pixel_shader スイッチにより、プログラマブルシェーダーが無効になりました";
				K__VIDEO_WRN("W_SHADER_DISABLED: %s", msg_u8);
				text_u8 += K__StdSprintf("- %s\n", msg_u8);
				m_shader_available = false;
			}
		}

		// 描画性能
		{
			if ((m_devcaps.PrimitiveMiscCaps & D3DPMISCCAPS_SEPARATEALPHABLEND) == 0) {
				const char *msg_u8 = u8"セパレートアルファブレンドに対応していません";
				K__VIDEO_ERR("E_LOW_SPEC: NO SEPARATE ALPHA BLEND -- %s", msg_u8);
				text_u8 += K__StdSprintf("- %s\n", msg_u8);
				ok = false;
			}
			if ((int)m_devcaps.MaxStreams < SYSTEMREQ_MAX_VERTEX_ELMS) {
				const char *msg_u8 = u8"頂点ストリーム数が足りません";
				K__VIDEO_ERR("E_LOW_SPEC: NUMBER OF VERTEX STREAMS %d NOT SUPPORTED -- %s", SYSTEMREQ_MAX_VERTEX_ELMS, msg_u8);
				text_u8 += K__StdSprintf("- %s\n", msg_u8);
				ok = false;
			}
		}

		if (message_u8) {
			*message_u8 = text_u8;
		}
		return ok;
	}

	#pragma region texture
	KTexture * createTexture(int w, int h, KTexture::Format fmt, bool is_render_tex) {
		CD3DTex *tex = new CD3DTex();
		if (tex->make(m_d3ddev, w, h, fmt, is_render_tex)) {
			K__DX9_LOCK_GUARD(m_mutex);
			m_texlist[tex->m_ktexid] = tex;
			return tex;
		} else {
			tex->drop();
			return nullptr;
		}
	}
	KTexture * createTextureFromImage(const KImage &img, KTexture::Format fmt) {
		KTexture *tex = createTexture(img.getWidth(), img.getHeight(), fmt, false);
		if (tex) {
			tex->writeImageToTexture(img, 1.0f, 1.0f);
		}
		return tex;
	}
	void deleteTexture(KTEXID texid) {
		K__DX9_LOCK_GUARD(m_mutex);
		auto it = m_texlist.find(texid);
		if (it != m_texlist.end()) {
			CD3DTex *tex = it->second;
			tex->destroy();
			tex->drop();
			m_texlist.erase(it);
		}
	}
	CD3DTex * findTexture(KTEXID texid) {
		K__DX9_LOCK_GUARD(m_mutex);
		auto it = m_texlist.find(texid);
		if (it != m_texlist.end()) {
			return it->second;
		}
		return nullptr;
	}
	void setFilter(KFilter filter) {
		DX9_setFilter(m_d3ddev, filter);
	}
	void setBlend(KBlend blend) {
		DX9_setBlend(m_d3ddev, blend);
	}
	void setTextureAddressing(bool wrap) {
		DX9_setTextureAddressing(m_d3ddev, wrap);
	}
	void drawTexture(KTEXID src, const KVideoRect *src_rect, const KVideoRect *dst_rect, const KMatrix4 *matrix) {
		// 色とテクスチャの設定を反映
		if (m_color_tex_changed) {
			m_color_tex_changed = false;
			setTextureAndColors();
		}

		// ポリゴン
		struct _Vert {
			KVec4 xyzw;
			KVec2 uv;
		};
		_Vert vertex[4];

		float x0, y0, x1, y1;
		if (dst_rect) {
			// 出力 rect が指定されている
			x0 = dst_rect->xmin;
			y0 = dst_rect->ymin;
			x1 = dst_rect->xmax;
			y1 = dst_rect->ymax;
		} else {
			// 出力 rect が省略されている。ビューポート全体（≒レンダーターゲット全体）に描画する
			int x, y, w, h;
			getViewport(&x, &y, &w, &h);
			x0 = (float)x;
			y0 = (float)y;
			x1 = (float)x + w;
			y1 = (float)y + h;
		}
		if (1 /*MO_VIDEO_ENABLE_HALF_PIXEL*/) {
			vertex[0].xyzw = KVec4(x0-K_HALF_PIXEL, y0-K_HALF_PIXEL, 0.0f, 1.0f); // ピクセル単位で指定することに注意
			vertex[1].xyzw = KVec4(x1-K_HALF_PIXEL, y0-K_HALF_PIXEL, 0.0f, 1.0f);
			vertex[2].xyzw = KVec4(x0-K_HALF_PIXEL, y1-K_HALF_PIXEL, 0.0f, 1.0f);
			vertex[3].xyzw = KVec4(x1-K_HALF_PIXEL, y1-K_HALF_PIXEL, 0.0f, 1.0f);
		} else {
			vertex[0].xyzw = KVec4(x0, y0, 0.0f, 1.0f); // ピクセル単位で指定することに注意
			vertex[1].xyzw = KVec4(x1, y0, 0.0f, 1.0f);
			vertex[2].xyzw = KVec4(x0, y1, 0.0f, 1.0f);
			vertex[3].xyzw = KVec4(x1, y1, 0.0f, 1.0f);
		}
		if (matrix) {
			vertex[0].xyzw = matrix->transform(vertex[0].xyzw);
			vertex[1].xyzw = matrix->transform(vertex[1].xyzw);
			vertex[2].xyzw = matrix->transform(vertex[2].xyzw);
			vertex[3].xyzw = matrix->transform(vertex[3].xyzw);
		}

		float u0, v0, u1, v1;
		if (src_rect) {
			// 入力 rect が指定されている。ピクセル単位で指定されているものとして、UV範囲に直す
			CD3DTex *srctex = CD3D9_findTexture(src);
			K__ASSERT_RETURN(srctex);
			float src_w = (float)srctex->getWidth();
			float src_h = (float)srctex->getHeight();
			u0 = src_rect->xmin / src_w;
			v0 = src_rect->ymin / src_h;
			u1 = src_rect->xmax / src_w;
			v1 = src_rect->ymax / src_h;
		} else {
			// 入力 rect が省略されている。入力テクスチャ全体を指定する
			u0 = 0.0f;
			v0 = 0.0f;
			u1 = 1.0f;
			v1 = 1.0f;
		}
		vertex[0].uv = KVec2(u0, v0);
		vertex[1].uv = KVec2(u1, v0);
		vertex[2].uv = KVec2(u0, v1);
		vertex[3].uv = KVec2(u1, v1);

		// 描画
		m_d3ddev->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1);
		m_d3ddev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, &vertex[0], sizeof(_Vert));
	}
	void fill(KTEXID target, const float *color_rgba, KColorChannels channels) {
		K__ASSERT(m_d3ddev);
		DWORD color32 = K__Color32FromRgba(color_rgba);
		if (channels == KColorChannel_RGBA) {
			CD3DTex *tex = CD3D9_findTexture(target);
			IDirect3DSurface9 *d3dsurf;
			if (tex && tex->m_d3dtex && tex->m_d3dtex->GetSurfaceLevel(0, &d3dsurf) == S_OK) {
				m_d3ddev->ColorFill(d3dsurf, nullptr, color32);
				K__DX9_RELEASE(d3dsurf);
			}
			return;
		}
		// ターゲット指定
		pushRenderState();
		pushRenderTarget(target);
		// マスク設定
		DWORD oldmask;
		m_d3ddev->GetRenderState(D3DRS_COLORWRITEENABLE, &oldmask);
		setColorWriteMask(channels);
		// ポリゴン
		struct _Vert {
			KVec4 xyzw;
			DWORD dif;
		};
		_Vert vertex[4];
		memset(vertex, 0, sizeof(vertex));
		{
			const DWORD diffuse = color32;
			int x, y, w, h;
			getViewport(&x, &y, &w, &h);
			float x0 = (float)x;
			float y0 = (float)y;
			float x1 = (float)x + w;
			float y1 = (float)y + h;
			if (1 /*MO_VIDEO_ENABLE_HALF_PIXEL*/) {
				vertex[0].xyzw = KVec4(x0-K_HALF_PIXEL, y0-K_HALF_PIXEL, 0.0f, 1.0f); // ピクセル単位で指定することに注意
				vertex[1].xyzw = KVec4(x1-K_HALF_PIXEL, y0-K_HALF_PIXEL, 0.0f, 1.0f);
				vertex[2].xyzw = KVec4(x0-K_HALF_PIXEL, y1-K_HALF_PIXEL, 0.0f, 1.0f);
				vertex[3].xyzw = KVec4(x1-K_HALF_PIXEL, y1-K_HALF_PIXEL, 0.0f, 1.0f);
			} else {
				vertex[0].xyzw = KVec4(x0, y0, 0.0f, 1.0f); // ピクセル単位で指定することに注意
				vertex[1].xyzw = KVec4(x1, y0, 0.0f, 1.0f);
				vertex[2].xyzw = KVec4(x0, y1, 0.0f, 1.0f);
				vertex[3].xyzw = KVec4(x1, y1, 0.0f, 1.0f);
			}
			vertex[0].dif = diffuse;
			vertex[1].dif = diffuse;
			vertex[2].dif = diffuse;
			vertex[3].dif = diffuse;
		}
		// 描画
		m_d3ddev->SetTexture(0, nullptr);
		m_d3ddev->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1);
		m_d3ddev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, &vertex[0], sizeof(_Vert));
		// マスク戻す
		m_d3ddev->SetRenderState(D3DRS_COLORWRITEENABLE, oldmask);
		// ターゲット戻す
		popRenderTarget();
		popRenderState();
	}
	#pragma endregion // texture

	#pragma region shader
	KShader * createShaderFromHLSL_impl(const char *code, const char *name) {
		K__ASSERT(m_d3ddev);
		if (! m_shader_available) {
			K__VIDEO_WRN(u8"プログラマブルシェーダーは利用できません。'%s' はロードされませんでした", name);
			return nullptr;
		}
		CD3DShader *sh = new CD3DShader();
		if (sh->make(m_d3ddev, code, name)) {
			K__DX9_LOCK_GUARD(m_mutex);
			m_shaderlist[sh->m_id] = sh;
			return sh;
		} else {
			sh->drop();
			return nullptr;
		}
	}
	void deleteShader(KSHADERID s) {
		K__DX9_LOCK_GUARD(m_mutex);
		auto it = m_shaderlist.find(s);
		if (it != m_shaderlist.end()) {
			CD3DShader *sh = it->second;
			sh->destroy();
			sh->drop();
			m_shaderlist.erase(it);
		}
	}
	CD3DShader * findShader(KSHADERID sid) {
		K__DX9_LOCK_GUARD(m_mutex);
		auto it = m_shaderlist.find(sid);
		if (it != m_shaderlist.end()) {
			return it->second;
		}
		return nullptr;
	}
	void setDefaultShaderParams(const KShaderArg &arg) {
		if (m_curr_shader) {
			setBlend(arg.blend);
			m_curr_shader->setParameter_TimeSec();
			m_curr_shader->setParameter_Texture(arg.texture);
			m_curr_shader->setParameter_TexSize(arg.texture);
			m_curr_shader->setParameter_Diffuse(arg.color.floats());
			m_curr_shader->setParameter_Specular(arg.specular.floats());
			m_curr_shader->setParameter_ProjMatrix(m_projection_matrix);
			m_curr_shader->setParameter_ViewMatrix(m_transform_matrix);
			m_curr_shader->setParameter_ScreenTexture();
			m_curr_shader->setParameter_ScreenTexSize();
		}
	}
	void beginShader() {
		K__ASSERT(m_d3ddev);
		if (m_curr_shader) {
			m_curr_shader->bindShader();
		}
	}
	void endShader() {
		if (m_curr_shader) {
			m_curr_shader->unbindShader();
		}
	}
	void setShader(KSHADERID sid) {
		K__DROP(m_curr_shader);
		CD3DShader *sh = findShader(sid);
		if (sh) {
			m_curr_shader = sh;
			K__GRAB(m_curr_shader);
		} else {
			m_curr_shader = nullptr;
		}
	}
	void setShaderInt(const char *name, const int *values, int count) {
		if (m_curr_shader) {
			m_curr_shader->setInt(name, values, count);
		}
	}
	void setShaderFloat(const char *name, const float *values, int count) {
		if (m_curr_shader) {
			m_curr_shader->setFloat(name, values, count);
		}
	}
	void setShaderTexture(const char *name, KTEXID texid) {
		if (m_curr_shader) {
			CD3DTex *tex = CD3D9_findTexture(texid);
			if (tex) {
				m_curr_shader->setTexture(name, tex->m_d3dtex);
			} else {
				m_curr_shader->setTexture(name, nullptr);
			}
		}
	}
	bool getShaderDesc(KSHADERID s, KShader::Desc *desc) {
		K__DX9_LOCK_GUARD(m_mutex);
		CD3DShader *sh = findShader(s);
		if (sh) {
			sh->getDesc(desc);
			return true;
		}
		return false;
	}
	#pragma endregion // shader

	#pragma region render state
	void setTextureAndColors() {
		K__ASSERT(m_d3ddev);

		// シェーダーが有効になっている場合、テクスチャステージを上書きしないようにする
		if (m_curr_shader) {
			return;
		}

		// D3DTA_TFACTOR を使う上での注意
		//
		// https://msdn.microsoft.com/ja-jp/library/cc324251.aspx
		// > ハードウェアによっては、D3DTA_TFACTOR と D3DTA_DIFFUSE の同時使用をサポートしていない場合もある
		//
		// D3DTA_TFACTOR で使われる色を設定
		m_d3ddev->SetRenderState(D3DRS_TEXTUREFACTOR, m_diffuse);

		if (m_texture) {
			// テクスチャを使う

			// ステージ0
			// テクスチャ色と頂点カラーを乗算
			if (1) {
				CD3DTex *tex = findTexture(m_texture);
				if (tex) {
					m_d3ddev->SetTexture(0, tex->m_d3dtex);
				} else {
					m_d3ddev->SetTexture(0, nullptr);
				}
				m_d3ddev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
				m_d3ddev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE); // テクスチャの色
				m_d3ddev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE); // 頂点色（頂点フォーマットの D3DFVF_DIFFUSE に対応）
				m_d3ddev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
				m_d3ddev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
				m_d3ddev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
			}
			// ステージ1
			// 前ステージの結果に定数カラーを乗算
			if (1) {
				m_d3ddev->SetTexture(1, nullptr);
				m_d3ddev->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_MODULATE);
				m_d3ddev->SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_CURRENT); // 前ステージでの合成結果
				m_d3ddev->SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_TFACTOR); // レンダーステートの定数カラー（D3DRS_TEXTUREFACTORで設定した色）
				m_d3ddev->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
				m_d3ddev->SetTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
				m_d3ddev->SetTextureStageState(1, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);
			}
			// ステージ2
			// 前ステージの結果に頂点スペキュラを加算
			if (1) {
				// スペキュラは加算合成だが D3DTOP_ADD で加算してはいけない.
				// 加算合成用のテクスチャでは RGB=0 の部分が透明になることを期待しているが
				// 単純加算だと RGB=0 の部分にも色が足されてしまう。あらかじめアルファを乗案してから加算するようにするために
				// スペキュラ向けの演算子 D3DTOP_MODULATEALPHA_ADDCOLOR を使う
				m_d3ddev->SetTexture(2, nullptr);
				m_d3ddev->SetTextureStageState(2, D3DTSS_COLOROP, D3DTOP_MODULATEALPHA_ADDCOLOR); // Arg1.RGB + Arg1.A*Arg2.RGB (Arg2のアルファは関与しない)
				m_d3ddev->SetTextureStageState(2, D3DTSS_COLORARG1, D3DTA_CURRENT);   // 前ステージでの合成結果
				m_d3ddev->SetTextureStageState(2, D3DTSS_COLORARG2, D3DTA_SPECULAR);  // 頂点スペキュラ（頂点フォーマットの D3DFVF_SPECULAR に対応）
				m_d3ddev->SetTextureStageState(2, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1); // アルファ変更しない
				m_d3ddev->SetTextureStageState(2, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
				m_d3ddev->SetTextureStageState(2, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
			}
			// ステージ3
			// 前ステージの結果に定数カラーを加算（スペキュラ）
			if (m_devcaps.PrimitiveMiscCaps & D3DPMISCCAPS_PERSTAGECONSTANT) { // D3DTSS_CONSTANT のサポートを確認
				m_d3ddev->SetTexture(3, nullptr);
				m_d3ddev->SetTextureStageState(3, D3DTSS_CONSTANT, m_specular); // D3DTA_CONSTANT で参照される色
				m_d3ddev->SetTextureStageState(3, D3DTSS_COLOROP, D3DTOP_MODULATEALPHA_ADDCOLOR); // Arg1.RGB + Arg1.A*Arg2.RGB (Arg2のアルファは関与しない)
				m_d3ddev->SetTextureStageState(3, D3DTSS_COLORARG1, D3DTA_CURRENT);   // 前ステージでの合成結果
				m_d3ddev->SetTextureStageState(3, D3DTSS_COLORARG2, D3DTA_CONSTANT);  // テクスチャステージの定数カラー（D3DTA_CONSTANT が使えないカードが意外と多いので注意）
				m_d3ddev->SetTextureStageState(3, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1); // アルファ変更しない
				m_d3ddev->SetTextureStageState(3, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
				m_d3ddev->SetTextureStageState(3, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
			}

		} else {
			// テクスチャが設定されていない。
			// 頂点カラー D3DTA_DIFFUSE と、マテリアルによって指定された色 D3DTA_TFACTOR を直接合成する

			// ステージ0
			// 頂点カラーと定数カラーを乗算
			if (1) {
				m_d3ddev->SetTexture(0, nullptr);
				m_d3ddev->SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_MODULATE);
				m_d3ddev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR); // レンダーステートの定数カラー（D3DRS_TEXTUREFACTORで設定した色）
				m_d3ddev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE); // 頂点色（頂点フォーマットの D3DFVF_DIFFUSE に対応）
				m_d3ddev->SetTextureStageState(0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE);
				m_d3ddev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
				m_d3ddev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
			}
			// ステージ1
			// 前ステージの結果に定数カラーを加算（スペキュラ）
			if (1) {
				m_d3ddev->SetTexture(1, nullptr);
				m_d3ddev->SetTextureStageState(1, D3DTSS_CONSTANT, m_specular); // D3DTA_CONSTANT で参照される色
				m_d3ddev->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_ADD);
				m_d3ddev->SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_CURRENT);   // 前ステージでの合成結果
				m_d3ddev->SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_CONSTANT);  // テクスチャステージの定数カラー（D3DTA_CONSTANT が使えないカードが意外と多いので注意）
				m_d3ddev->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1); // アルファは変更しない
				m_d3ddev->SetTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
				m_d3ddev->SetTextureStageState(1, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
			}
			// ステージ2
			// 使わない
			if (1) {
				m_d3ddev->SetTexture(2, nullptr);
				m_d3ddev->SetTextureStageState(2, D3DTSS_COLOROP, D3DTOP_DISABLE);
				m_d3ddev->SetTextureStageState(2, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
			}
			// ステージ3
			// 使わない
			if (1) {
				m_d3ddev->SetTexture(3, nullptr);
				m_d3ddev->SetTextureStageState(3, D3DTSS_COLOROP, D3DTOP_DISABLE);
				m_d3ddev->SetTextureStageState(3, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
			}
		}
	}
	void setColor(uint32_t color) {
		m_diffuse = color;
		m_color_tex_changed = true;
	}
	void setSpecular(uint32_t specular) {
		m_specular = specular;
		m_color_tex_changed = true;
	}
	void setTexture(KTEXID texture) {
		m_texture = texture;
		m_color_tex_changed = true;
	}
	void setProjection(const float *matrix4x4) {
		m_projection_matrix = D3DXMATRIX(matrix4x4);
		m_d3ddev->SetTransform(D3DTS_PROJECTION, &m_projection_matrix);
	}
	void setTransform(const float *matrix4x4) {
		m_transform_matrix = D3DXMATRIX(matrix4x4);
		m_d3ddev->SetTransform(D3DTS_VIEW, &m_transform_matrix);
	}
	void pushRenderState() {
		K__ASSERT(m_d3ddev);
		IDirect3DStateBlock9 *block = nullptr;
		m_d3ddev->CreateStateBlock(D3DSBT_ALL, &block);
		m_d3dblocks.push_back(block);
	}
	void popRenderState() {
		if (m_d3dblocks.size() > 0) {
			IDirect3DStateBlock9 *block = m_d3dblocks.back();
			if (block) {
				block->Apply();
				block->Release();
			}
			m_d3dblocks.pop_back();
		}
	}
	void pushRenderTarget(KTEXID render_target) {
		K__DX9_LOCK_GUARD(m_mutex);
		K__ASSERT(m_d3ddev);
		// 現在のターゲットを退避
		SSurfItem item;
		m_d3ddev->GetRenderTarget(0, &item.color_surf);
		m_d3ddev->GetDepthStencilSurface(&item.depth_surf);
		m_rendertarget_stack.push_back(item);

		// CD3DTex を得る
		CD3DTex *texobj = nullptr;
		if (render_target) {
			K__DX9_LOCK_GUARD(m_mutex);
			texobj = findTexture(render_target);
			if (texobj && texobj->isRenderTarget()) {
				if (K__DX9_STRICT_CHECK) {
					// カラーバッファと深度バッファのサイズが同じであることを確認する
					D3DSURFACE_DESC cdesc;
					D3DSURFACE_DESC zdesc;
					if (texobj->m_color_surf && texobj->m_depth_surf) {
						texobj->m_color_surf->GetDesc(&cdesc);
						texobj->m_depth_surf->GetDesc(&zdesc);
						K__ASSERT(cdesc.Width == zdesc.Width);
						K__ASSERT(cdesc.Height == zdesc.Height);
					}
				}
			} else {
				// render_target が非nullptrなのに、存在しない or レンダーテクスチャではない
				K__VIDEO_ERR("E_VIDEO_INVALID_RENDER_TEXTURE");
			}
		}
		
		// ターゲットをスタックに入れる。
		// 整合性を保つため、無効なターゲットが指定された場合は nullptr を入れる
		if (texobj) {
			m_d3ddev->SetRenderTarget(0, texobj->m_color_surf);
			m_d3ddev->SetDepthStencilSurface(texobj->m_depth_surf);
		} else {
			m_d3ddev->SetRenderTarget(0, nullptr);
			m_d3ddev->SetDepthStencilSurface(nullptr);
		}
	}
	void popRenderTarget() {
		K__DX9_LOCK_GUARD(m_mutex);
		K__ASSERT(m_d3ddev);
		// 退避しておいたターゲットを元に戻す
		if (! m_rendertarget_stack.empty()) {
			SSurfItem item = m_rendertarget_stack.back();
			m_d3ddev->SetRenderTarget(0, item.color_surf);
			m_d3ddev->SetDepthStencilSurface(item.depth_surf);
			K__DX9_RELEASE(item.color_surf);
			K__DX9_RELEASE(item.depth_surf);
			m_rendertarget_stack.pop_back();
		} else {
			K__VIDEO_ERR("E_EMPTY_REDNER_STACK");
		}
	}
	void setViewport(int x, int y, int w, int h) {
		K__ASSERT(m_d3ddev);
		D3DVIEWPORT9 vp;
		vp.X = (DWORD)KMath::max(x, 0);
		vp.Y = (DWORD)KMath::max(y, 0);
		vp.Width  = (DWORD)KMath::max(w, 0);
		vp.Height = (DWORD)KMath::max(h, 0);
		vp.MinZ = 0.0f; // MinZ, MaxZ はカメラ空間ではなくスクリーン空間の値であることに注意
		vp.MaxZ = 1.0f;
		m_d3ddev->SetViewport(&vp);
	}
	void getViewport(int *x, int *y, int *w, int *h) {
		K__ASSERT(m_d3ddev);
		D3DVIEWPORT9 vp;
		m_d3ddev->GetViewport(&vp);
		if (x) *x = (int)vp.X;
		if (y) *y = (int)vp.Y;
		if (w) *w = (int)vp.Width;
		if (h) *h = (int)vp.Height;
	}
	void setColorWriteMask(KColorChannels channels) {
		K__ASSERT(m_d3ddev);
		DWORD d3dmask = 0;
		d3dmask |= ((channels & KColorChannel_R) ? D3DCOLORWRITEENABLE_RED   : 0);
		d3dmask |= ((channels & KColorChannel_G) ? D3DCOLORWRITEENABLE_GREEN : 0);
		d3dmask |= ((channels & KColorChannel_B) ? D3DCOLORWRITEENABLE_BLUE  : 0);
		d3dmask |= ((channels & KColorChannel_A) ? D3DCOLORWRITEENABLE_ALPHA : 0);
		m_d3ddev->SetRenderState(D3DRS_COLORWRITEENABLE, d3dmask);
	}
	void setDepthTestEnabled(bool value) {
		if (value) {
			m_d3ddev->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
		} else {
			m_d3ddev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
		}
	}
	void setStencilEnabled(bool value) {
		if (value) {
			m_d3ddev->SetRenderState(D3DRS_STENCILENABLE, TRUE); // ステンシルバッファへ書き込む
			m_d3ddev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE); // Zバッファ書き込み禁止 (D3DRS_ZENABLEだとZテスト自体が無効になってしまう。ここでは書き込みだけ禁止したい）
			m_d3ddev->SetRenderState(D3DRS_STENCILMASK, 0xff); // D3DFMT_D24S8 によりステンシルマスクを 8 ビットにしているので、下位 8 ビットのみ参照させる
		} else {
			m_d3ddev->SetRenderState(D3DRS_STENCILENABLE, FALSE); // ステンシルバッファ書き込み禁止
			m_d3ddev->SetRenderState(D3DRS_ZWRITEENABLE, TRUE); // Zバッファ書き込み許可
		}
	}
	void setStencilFunc(KVideo::StencilFunc func, int val, KVideo::StencilOp pass_op) {
		m_d3ddev->SetRenderState(D3DRS_STENCILREF, val);
		switch (func) {
		case KVideo::STENCILFUNC_NEVER:        m_d3ddev->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_NEVER); break;
		case KVideo::STENCILFUNC_LESS:         m_d3ddev->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_LESS); break;
		case KVideo::STENCILFUNC_EQUAL:        m_d3ddev->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_EQUAL); break;
		case KVideo::STENCILFUNC_LESSEQUAL:    m_d3ddev->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_LESSEQUAL); break;
		case KVideo::STENCILFUNC_GREATER:      m_d3ddev->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_GREATER); break;
		case KVideo::STENCILFUNC_NOTEQUAL:     m_d3ddev->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_NOTEQUAL); break;
		case KVideo::STENCILFUNC_GREATEREQUAL: m_d3ddev->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_GREATEREQUAL); break;
		case KVideo::STENCILFUNC_ALWAYS:       m_d3ddev->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_ALWAYS); break;
		}
		switch (pass_op) {
		case KVideo::STENCILOP_KEEP:    m_d3ddev->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILCAPS_KEEP); break;
		case KVideo::STENCILOP_REPLACE: m_d3ddev->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILCAPS_REPLACE); break;
		case KVideo::STENCILOP_INC:     m_d3ddev->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILCAPS_INCRSAT); break;
		case KVideo::STENCILOP_DEC:     m_d3ddev->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILCAPS_DECRSAT); break;
		}
		m_d3ddev->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILCAPS_KEEP);
		m_d3ddev->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILCAPS_KEEP);
	}
	#pragma endregion // render state

	#pragma region device
	void resetDevice_lost() {
		K__DX9_LOCK_GUARD(m_mutex);
		K__ASSERT(m_d3ddev);
		{
			// テクスチャーのデバイスロスト処理
			for (auto it=m_texlist.begin(); it!=m_texlist.end(); ++it) {
				it->second->onDeviceLost();
			}
			// シェーダーのデバイスロスト処理
			for (auto it=m_shaderlist.begin(); it!=m_shaderlist.end(); ++it) {
				it->second->onDeviceLost();
			}
		}
	}
	bool canFullscreen(int w, int h) {
		return DX9_checkFullScreenParams(m_d3d9, w, h);
	}
	void resetDevice_reset(D3DPRESENT_PARAMETERS *new_pp) {
		if (new_pp) {
			m_d3dpp = *new_pp;
		} else {
			// 新しいパラメータが未指定。自動取得する
			D3DPRESENT_PARAMETERS pp;
			ZeroMemory(&pp, sizeof(pp));
			IDirect3DSwapChain9 *sw = nullptr;
			K__ASSERT(m_d3ddev->GetNumberOfSwapChains() > 0);
			m_d3ddev->GetSwapChain(0, &sw);
			sw->GetPresentParameters(&pp);
		}
		{
			// シェーダーの復帰
			for (auto it=m_shaderlist.begin(); it!=m_shaderlist.end(); ++it) {
				it->second->onDeviceReset();
			}

			// テクスチャの復帰
			for (auto it=m_texlist.begin(); it!=m_texlist.end(); ++it) {
				it->second->onDeviceReset();
			}
		}
	}
	bool resetDevice(int w, int h, int fullscreen) {
		K::print("ResetDevice %dx%d", w, h);
		if (m_video_thread_id != K::sysGetCurrentThreadId()) {
			K__VIDEO_ERR("%s: Thread not matched", __FUNCTION__);
			::InvalidateRect(m_hWnd, nullptr, TRUE);
			return false;
		}
		resetDevice_lost();

		// サイズなど再設定
		D3DPRESENT_PARAMETERS new_pp = m_d3dpp;
		if (w > 0) new_pp.BackBufferWidth = w;
		if (h > 0) new_pp.BackBufferHeight = h;
		if (fullscreen >= 0) new_pp.Windowed = fullscreen ? FALSE : TRUE;
		DX9_printPresentParameter("ResetDevice", new_pp);
		if (! new_pp.Windowed) {
			if (! DX9_checkFullScreenParams(m_d3d9, new_pp.BackBufferWidth, new_pp.BackBufferHeight)) {
				K__VIDEO_ERR("Invalid resolution for fullscreen: Size=%dx%d",
					new_pp.BackBufferWidth, new_pp.BackBufferHeight);
				::InvalidateRect(m_hWnd, nullptr, TRUE);
				//return false;
				// 回復を試みる
				w = 640;
				h = 480;
				new_pp.BackBufferWidth = w;
				new_pp.BackBufferHeight = h;
				K__VIDEO_PRINT("Default resolution setted: Size=%dx%d", w, h);
			}
		}

		// リセット実行
		HRESULT hr = m_d3ddev->Reset(&new_pp);
		if (FAILED(hr)) {
			// このエラーは割とありがちなので、エラーではなく情報レベルでのログにしておく
			K__VIDEO_PRINT("E_FAIL_D3D_DEVICE_RESET: HRESULT='%s'", K__GetErrorStringUtf8(hr).c_str());
			::InvalidateRect(m_hWnd, nullptr, TRUE);
			return false;
		}

		resetDevice_reset(&new_pp);
		return true;
	}
	bool shouldReset() {
		return m_d3ddev->TestCooperativeLevel() == D3DERR_DEVICENOTRESET;
	}
	bool beginScene() {
		K__ASSERT(m_d3ddev);
		m_d3ddev->BeginScene();
		setupDeviceStates();
		return true;
	}
	bool endScene() {
		K__ASSERT(m_d3ddev);
		m_d3ddev->EndScene();
		HRESULT hr = m_d3ddev->Present(nullptr, nullptr, nullptr, nullptr);
		if (FAILED(hr)) {
			// このエラーは割とありがちなので、エラーではなく情報レベルでのログにしておく
			K__VIDEO_PRINT("E_FAIL_D3D_DEVICE_PRESENT: HRESULT='%s'", K__GetErrorStringUtf8(hr).c_str());
			return false;
		}
		return true;
	}
	void clearColor(const float *color_rgba) {
		K__ASSERT(m_d3ddev);
		m_d3ddev->Clear(0, nullptr, D3DCLEAR_TARGET, color_rgba ? K__Color32FromRgba(color_rgba) : 0, 0, 0);
	}
	void clearDepth(float z) {
		K__ASSERT(m_d3ddev);
		m_d3ddev->Clear(0, nullptr, D3DCLEAR_ZBUFFER, 0, z, 0);
	}
	void clearStencil(int s) {
		K__ASSERT(m_d3ddev);
		m_d3ddev->Clear(0, nullptr, D3DCLEAR_STENCIL, 0, 0, s);
	}
	#pragma endregion // device

	#pragma region draw
	void drawUserPtrV(const KVertex *vertices, int count, KPrimitive primitive) {
		K__ASSERT(m_d3ddev);
		K__ASSERT(count >= 0);

		// 色とテクスチャの設定を反映
		if (m_color_tex_changed) {
			m_color_tex_changed = false;
			setTextureAndColors();
		}

		m_drawcalls++;

		// プリミティブ数を計算
		D3DPRIMITIVETYPE d3dpt = D3DPT_TRIANGLELIST;
		int numface = 0;
		switch (primitive) {
		case KPrimitive_POINTS:         d3dpt=D3DPT_POINTLIST;     numface=count; break;
		case KPrimitive_LINES:          d3dpt=D3DPT_LINELIST;      numface=count / 2; break;
		case KPrimitive_LINE_STRIP:     d3dpt=D3DPT_LINESTRIP;     numface=count - 1; break;
		case KPrimitive_TRIANGLES:      d3dpt=D3DPT_TRIANGLELIST;  numface=count / 3; break;
		case KPrimitive_TRIANGLE_STRIP: d3dpt=D3DPT_TRIANGLESTRIP; numface=count - 2; break;
		case KPrimitive_TRIANGLE_FAN:   d3dpt=D3DPT_TRIANGLEFAN;   numface=count - 2; break;
		}

		// 描画
		if (numface > 0 && count > 0) {
			// DX9_VERTEX と KVertex は互換性がある
			K__ASSERT(sizeof(DX9_VERTEX) == sizeof(KVertex)); // 最低限、サイズが同じであることを確認
			m_d3ddev->SetFVF(K__DX9_FVF_VERTEX);
			m_d3ddev->DrawPrimitiveUP(d3dpt, numface, vertices, sizeof(DX9_VERTEX));
		}
	}
	void drawIndexedUserPtrV(const KVertex *vertices, int vertex_count, const int *indices, int index_count, KPrimitive primitive) {
		K__ASSERT(m_d3ddev);
		K__ASSERT(index_count >= 0);

		// 色とテクスチャの設定を反映
		if (m_color_tex_changed) {
			m_color_tex_changed = false;
			setTextureAndColors();
		}

		m_drawcalls++;

		// プリミティブ数を計算
		D3DPRIMITIVETYPE d3dpt = D3DPT_TRIANGLELIST;
		int numface = 0;
		switch (primitive) {
		case KPrimitive_POINTS:         d3dpt=D3DPT_POINTLIST;     numface=index_count; break;
		case KPrimitive_LINES:          d3dpt=D3DPT_LINELIST;      numface=index_count / 2; break;
		case KPrimitive_LINE_STRIP:     d3dpt=D3DPT_LINESTRIP;     numface=index_count - 1; break;
		case KPrimitive_TRIANGLES:      d3dpt=D3DPT_TRIANGLELIST;  numface=index_count / 3; break;
		case KPrimitive_TRIANGLE_STRIP: d3dpt=D3DPT_TRIANGLESTRIP; numface=index_count - 2; break;
		case KPrimitive_TRIANGLE_FAN:   d3dpt=D3DPT_TRIANGLEFAN;   numface=index_count - 2; break;
		}

		// 描画
		if (numface > 0) {
			// https://msdn.microsoft.com/ja-jp/library/ee422176(v=vs.85).aspx
			m_d3ddev->SetFVF(K__DX9_FVF_VERTEX);
			m_d3ddev->DrawIndexedPrimitiveUP(d3dpt, 0, vertex_count, numface, indices, D3DFMT_INDEX32, vertices, sizeof(DX9_VERTEX));
		}
	}
	#pragma endregion // draw

	KImage getBackbufferImage() {
		HRESULT hr;

		// 現在のバックバッファを取得
		IDirect3DSurface9 *back_surf = nullptr;
		hr = m_d3ddev->GetRenderTarget(0, &back_surf);
		K__ASSERT(SUCCEEDED(hr));

		// バックバッファのパラメータを得る
		D3DSURFACE_DESC back_desc;
		memset(&back_desc, 0, sizeof(back_desc));
		if (back_surf) {
			hr = back_surf->GetDesc(&back_desc);
			K__ASSERT(SUCCEEDED(hr));
		}

		// バックバッファは D3DPOOL_DEFAULT になっているので Lock できない。
		// バックバッファと同じサイズ＆フォーマットでロック可能サーフェスを作成する
		IDirect3DSurface9 *tmp_surf = nullptr;
		if (back_desc.Width>0 && back_desc.Height>0) {
			hr = m_d3ddev->CreateOffscreenPlainSurface(back_desc.Width, back_desc.Height, back_desc.Format, D3DPOOL_SYSTEMMEM, &tmp_surf, nullptr);
			K__ASSERT(SUCCEEDED(hr));
		}

		// バックバッファの内容をロック可能サーフェスに転送する
		if (tmp_surf) {
			hr = m_d3ddev->GetRenderTargetData(back_surf, tmp_surf);
			K__ASSERT(SUCCEEDED(hr));
		}

		// ロック可能サーフェスをロックしてピクセル情報を得る
		KImage img = KImage::createFromPixels(back_desc.Width, back_desc.Height, KColorFormat_RGBA32, nullptr);
		D3DLOCKED_RECT lrect;
		if (tmp_surf && SUCCEEDED(tmp_surf->LockRect(&lrect, nullptr, D3DLOCK_NOSYSLOCK))) {
			KBmp bmp;
			img.lock(&bmp);
			_BmpWrite(&bmp, lrect.pBits, lrect.Pitch, back_desc.Width, back_desc.Height, -1);
			img.unlock();
			tmp_surf->UnlockRect();
		}
		K__DX9_RELEASE(tmp_surf);
		K__DX9_RELEASE(back_surf);

		return img;
	}
	void command(const char *cmd) {
		K__ASSERT(m_d3ddev);
		if (K__STREQ(cmd, "wireframe on")) {
			m_d3ddev->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
			m_d3ddev->SetTexture(0, nullptr);
			return;
		}
		if (K__STREQ(cmd, "wireframe off")) {
			m_d3ddev->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
			return;
		}
		if (K__STREQ(cmd, "cull cw")) {
			m_d3ddev->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);
			return;
		}
		if (K__STREQ(cmd, "cull ccw")) {
			m_d3ddev->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
			return;
		}
		if (K__STREQ(cmd, "cull none")) {
			m_d3ddev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
			return;
		}
		K__VIDEO_ERR("Unknown video command: %s", cmd);
	}
	void getParameter(KVideo::Param param, void *data) {
		if (data == nullptr) return;
		void **ppData = (void **)data;
		int *pIntData = (int*)data;

		switch (param) {
		case KVideo::PARAM_ADAPTERNAME:
			if (m_d3d9) {
				D3DADAPTER_IDENTIFIER9 adap;
				ZeroMemory(&adap, sizeof(adap));
				m_d3d9->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &adap);
				strcpy((char*)data, adap.Description);
			}
			return;

		case KVideo::PARAM_HAS_SHADER:
			pIntData[0] = m_shader_available;
			return;

		case KVideo::PARAM_HAS_HLSL:
			pIntData[0] = 1; // OK
			return;

		case KVideo::PARAM_HAS_GLSL:
			pIntData[0] = 0; // not supported
			return;

		case KVideo::PARAM_IS_FULLSCREEN:
			pIntData[0] = m_d3dpp.Windowed ? 0 : 1;
			return;

		case KVideo::PARAM_D3DDEV9:
			ppData[0] = m_d3ddev;
			return;

		case KVideo::PARAM_HWND:
			ppData[0] = m_hWnd;
			return;

		case KVideo::PARAM_DEVICELOST:
			pIntData[0] = (m_d3ddev && m_d3ddev->TestCooperativeLevel() == D3DERR_DEVICENOTRESET) ? 1 : 0;
			return;

		case KVideo::PARAM_VS_VER:
			pIntData[0] = m_d3ddev ? D3DSHADER_VERSION_MAJOR(m_devcaps.VertexShaderVersion) : 0;
			pIntData[1] = m_d3ddev ? D3DSHADER_VERSION_MINOR(m_devcaps.VertexShaderVersion) : 0;
			return;

		case KVideo::PARAM_PS_VER:
			pIntData[0] = m_d3ddev ? D3DSHADER_VERSION_MAJOR(m_devcaps.PixelShaderVersion) : 0;
			pIntData[1] = m_d3ddev ? D3DSHADER_VERSION_MINOR(m_devcaps.PixelShaderVersion) : 0;
			return;

		case KVideo::PARAM_MAXTEXSIZE:
			pIntData[0] = m_d3ddev ? DX9_getMaxTextureWidth(m_devcaps)  : 0;
			pIntData[1] = m_d3ddev ? DX9_getMaxTextureHeight(m_devcaps) : 0;
			return;
	
		case KVideo::PARAM_DRAWCALLS:
			pIntData[0] = m_drawcalls;
			m_drawcalls = 0;
			return;

		case KVideo::PARAM_MAX_TEXTURE_REQUIRE:
			pIntData[0] = g_video_limit.max_texture_size_require;
			return;

		case KVideo::PARAM_MAX_TEXTURE_SUPPORT:
			pIntData[0] = g_video_limit.limit_texture_size;
			return;

		case KVideo::PARAM_USE_SQUARE_TEXTURE_ONLY:
			pIntData[0] = g_video_limit.use_square_texture_only;
			return;

		case KVideo::PARAM_DISABLE_PIXEL_SHADER:
			pIntData[0] = g_video_limit.disable_pixel_shader;
			return;
		}
	}
	void setParameter(KVideo::Param param, intptr_t data) {
		switch (param) {
		case KVideo::PARAM_MAX_TEXTURE_REQUIRE:
			g_video_limit.max_texture_size_require = data;
			break;
		case KVideo::PARAM_MAX_TEXTURE_SUPPORT:
			g_video_limit.limit_texture_size = data;
			break;
		case KVideo::PARAM_USE_SQUARE_TEXTURE_ONLY:
			g_video_limit.use_square_texture_only = (data!=0);
			break;
		case KVideo::PARAM_DISABLE_PIXEL_SHADER:
			g_video_limit.disable_pixel_shader = (data!=0);
			break;
		}
	}
};
#pragma endregion // CD3D9


static CD3D9 g_Video;


#pragma region KVideo
bool KVideo::init(void *hWnd, void *d3d9, void *d3ddev9) {
	// 有効な HWND であることを確認
	K__ASSERT_RETURN_ZERO(IsWindow((HWND)hWnd));

	g_Video.init_init(true);
	if (g_Video.init((HWND)hWnd, (IDirect3D9 *)d3d9, (IDirect3DDevice9 *)d3ddev9)) {
		return true;
	}
	// ERR
	g_Video.shutdown();
	return false;
}
void KVideo::shutdown() {
	g_Video.shutdown();
}
bool KVideo::isInit() {
	return g_Video.isInit();
}
void KVideo::setParameter(Param param, intptr_t data) {
	g_Video.setParameter(param, data);
}
void KVideo::getParameter(Param param, void *data) {
	g_Video.getParameter(param, data);
}
void KVideo::command(const char *cmd) {
	g_Video.command(cmd);
}
KTEXID KVideo::createTexture(int w, int h, KTexture::Format fmt) {
	KTexture *tex = g_Video.createTexture(w, h, fmt, false);
	return tex ? tex->getId() : nullptr;
}
KTEXID KVideo::createTextureFromImage(const KImage &img, KTexture::Format fmt) {
	KTexture *tex = nullptr;
	if (!img.empty()) {
		tex = g_Video.createTextureFromImage(img, fmt);
	}
	return tex ? tex->getId() : nullptr;
}
KTEXID KVideo::createRenderTexture(int w, int h, KTexture::Format fmt) {
	KTexture *tex = g_Video.createTexture(w, h, fmt, true);
	return tex ? tex->getId() : nullptr;
}
void KVideo::deleteTexture(KTEXID tex) {
	g_Video.deleteTexture(tex);
}
KTexture * KVideo::findTexture(KTEXID tex) {
	return g_Video.findTexture(tex);
}
void KVideo::fill(KTEXID target, const KColor &color, KColorChannels channels) {
	g_Video.fill(target, color.floats(), channels);
}
KSHADERID KVideo::createShaderFromHLSL(const char *code, const char *name) {
	KShader *shader = g_Video.createShaderFromHLSL_impl(code, name);
	return shader ? shader->getId() : nullptr;
}
void KVideo::deleteShader(KSHADERID s) {
	g_Video.deleteShader(s);
}
KShader * KVideo::findShader(KSHADERID s) {
	return g_Video.findShader(s);
}
void KVideo::setShader(KSHADERID s) {
	g_Video.setShader(s);
}
void KVideo::setShaderIntArray(const char *name, const int *values, int count) {
	g_Video.setShaderInt(name, values, count);
}
void KVideo::setShaderInt(const char *name, int value) { 
	g_Video.setShaderInt(name, &value, 1);
}
void KVideo::setShaderFloatArray(const char *name, const float *values, int count) {
	g_Video.setShaderFloat(name, values, count);
}
void KVideo::setShaderFloat(const char *name, float value) {
	g_Video.setShaderFloat(name, &value, 1);
}
void KVideo::setShaderTexture(const char *name, KTEXID tex) {
	g_Video.setShaderTexture(name, tex);
}
bool KVideo::getShaderDesc(KSHADERID s, KShader::Desc *desc) {
	return g_Video.getShaderDesc(s, desc);
}
void KVideo::setDefaultShaderParams(const KShaderArg &arg) {
	g_Video.setDefaultShaderParams(arg);
}
void KVideo::beginShader() {
	g_Video.beginShader();
}
void KVideo::endShader() {
	g_Video.endShader();
}
void KVideo::pushRenderState() {
	g_Video.pushRenderState();
}
void KVideo::popRenderState() {
	g_Video.popRenderState();
}
void KVideo::pushRenderTarget(KTEXID render_target) {
	g_Video.pushRenderTarget(render_target);
}
void KVideo::popRenderTarget() {
	g_Video.popRenderTarget();
}
void KVideo::setViewport(int x, int y, int w, int h) {
	g_Video.setViewport(x, y, w, h);
}
void KVideo::getViewport(int *x, int *y, int *w, int *h) {
	g_Video.getViewport(x, y, w, h);
}
void KVideo::setColorWriteMask(KColorChannels channels) {
	g_Video.setColorWriteMask(channels);
}
void KVideo::setStencilEnabled(bool value) {
	g_Video.setStencilEnabled(value);
}
void KVideo::setStencilFunc(StencilFunc func, int val, StencilOp pass_op) {
	g_Video.setStencilFunc(func, val, pass_op);
}
void KVideo::setDepthTestEnabled(bool value) {
	g_Video.setDepthTestEnabled(value);
}
void KVideo::setProjection(const KMatrix4 &m) {
	g_Video.setProjection(m.m);
}
void KVideo::setTransform(const KMatrix4 &m) {
	g_Video.setTransform(m.m);
}
void KVideo::setFilter(KFilter filter) {
	g_Video.setFilter(filter);
}
void KVideo::setBlend(KBlend blend) {
	g_Video.setBlend(blend);
}
void KVideo::setColor(uint32_t color) {
	g_Video.setColor(color);
}
void KVideo::setSpecular(uint32_t specular) {
	g_Video.setSpecular(specular);
}
void KVideo::setTexture(KTEXID texture) {
	g_Video.setTexture(texture);
}
void KVideo::setTextureAddressing(bool wrap) {
	g_Video.setTextureAddressing(wrap);
}
void KVideo::setTextureAndColors() {
	g_Video.setTextureAndColors();
}
void KVideo::resetDevice_lost() {
	g_Video.resetDevice_lost();
}
void KVideo::resetDevice_reset() {
	g_Video.resetDevice_reset(nullptr);
}
bool KVideo::resetDevice(int w, int h, int fullscreen) {
	return g_Video.resetDevice(w, h, fullscreen);
}
bool KVideo::canFullscreen(int w, int h) {
	return g_Video.canFullscreen(w, h);
}
bool KVideo::shouldReset() {
	return g_Video.shouldReset();
}
bool KVideo::beginScene() {
	return g_Video.beginScene();
}
bool KVideo::endScene() {
	return g_Video.endScene();
}
void KVideo::setupDeviceStates() {
	g_Video.setupDeviceStates();
}
void KVideo::clearColor(const KColor &color) {
	g_Video.clearColor(color.floats());
}
void KVideo::clearDepth(float z) {
	g_Video.clearDepth(z);
}
void KVideo::clearStencil(int s) {
	g_Video.clearStencil(s);
}
void KVideo::drawTexture(KTEXID src, const KVideoRect *src_rect, const KVideoRect *dst_rect, const KMatrix4 *transform) {
	g_Video.drawTexture(src, src_rect, dst_rect, transform);
}
void KVideo::drawUserPtrV(const KVertex *vertices, int count, KPrimitive primitive) {
	g_Video.drawUserPtrV(vertices, count, primitive);
}
void KVideo::drawIndexedUserPtrV(const KVertex *vertices, int vertex_count, const int *indices, int index_count, KPrimitive primitive) {
	g_Video.drawIndexedUserPtrV(vertices, vertex_count, indices, index_count, primitive);
}
KImage KVideo::getBackbufferImage() {
	return g_Video.getBackbufferImage();
}
#pragma endregion // KVideo


#pragma region KTexture
KTexture * KTexture::create(int w, int h, KTexture::Format fmt) {
	if (!KVideo::isInit()) {
		K__VIDEO_ERR("Video has not initialized");
		return nullptr;
	}
	return g_Video.createTexture(w, h, fmt, false);
}
KTexture * KTexture::createFromImage(const KImage &img) {
	KTexture *ret = g_Video.createTextureFromImage(img, FMT_ARGB32);
	return ret;
}
KTexture * KTexture::createRenderTarget(int w, int h, KTexture::Format fmt) {
	if (!KVideo::isInit()) {
		K__VIDEO_ERR("Video has not initialized");
		return nullptr;
	}
	return g_Video.createTexture(w, h, fmt, true);
}
#pragma endregion // KTexture


#pragma region KShader
KShader * KShader::create(const char *hlsl_u8, const char *name) {
	if (!KVideo::isInit()) {
		K__VIDEO_ERR("Video has not initialized");
		return nullptr;
	}
	return g_Video.createShaderFromHLSL_impl(hlsl_u8, name);
}
#pragma endregion // KShader


#pragma region Forward Impl
static CD3DTex * CD3D9_findTexture(KTEXID tex) {
	return g_Video.findTexture(tex);
}
#pragma endregion // Forward Impl


#endif // K_USE_D3D9


// 同一ステートをまとめて描画する
// かなり効果あり。あやかしの1-2ではFPSが2倍近く違う
#define DRAWLIST_UNIDRAW_TEST  1

// プリミティブを結合しない（テスト用）
#define DRAWLIST_NO_UNITE 0


#pragma region KMesh
KMesh::KMesh() {
	m_changed = 0;
}
bool KMesh::getAabb(KVec3 *minpoint, KVec3 *maxpoint) const {
	return getAabb(0, getVertexCount(), minpoint, maxpoint);
}
bool KMesh::getAabb(int offset, int count, KVec3 *minpoint, KVec3 *maxpoint) const {
	if (offset == 0 && count == m_vertices.size()) {
		// 全頂点のAABB
		if (m_changed & AABB) {
			m_changed &= ~AABB;
			m_whole_aabb_min = KVec3();
			m_whole_aabb_max = KVec3();
			if (m_vertices.size() > 0) {
				const KVertex *v = m_vertices.data();
				KVec3 minp, maxp;
				minp = v[0].pos;
				maxp = v[0].pos;
				for (int i=1; i<(int)m_vertices.size(); i++) {
					const KVec3 &p = v[i].pos;
					minp = minp.getmin(p);
					maxp = maxp.getmax(p);
				}
				m_whole_aabb_min = minp;
				m_whole_aabb_max = maxp;
			}
		}
		if (minpoint) *minpoint = m_whole_aabb_min;
		if (maxpoint) *maxpoint = m_whole_aabb_max;

	} else {
		// 部分AABB
		KVec3 minp, maxp;
		if (offset < 0) return false;
		if (offset + count > (int)m_vertices.size()) return false;
		if (count > 0) {
			const KVertex *v = m_vertices.data();
			minp = v[offset].pos;
			maxp = v[offset].pos;
			for (int i=offset+1; i<offset+count; i++) {
				const KVec3 &p = v[i].pos;
				minp = minp.getmin(p);
				maxp = maxp.getmax(p);
			}
		}
		if (minpoint) *minpoint = minp;
		if (maxpoint) *maxpoint = maxp;
	}
	return true;
}
void KMesh::clear() {
	m_whole_aabb_min = KVec3();
	m_whole_aabb_max = KVec3();
	m_changed = 0;
	m_indices.clear();
	m_vertices.clear();
	m_submeshes.clear();
	m_tmppos.clear();
	m_tmpuv1.clear();
	m_tmpuv2.clear();
	m_tmpdif.clear();
	m_tmpspe.clear();
}
void KMesh::setVertexCount(int n) {
	if (m_vertices.size() != n) {
		m_vertices.resize(n);
		m_changed = ALL;
	}
}
bool KMesh::copyFrom(const KMesh *other) {
	if (other && this != other) {
		if (0) {
			*this = *other;
		} else {
			m_indices = other->m_indices;
			m_vertices = other->m_vertices;
			m_submeshes = other->m_submeshes;
			m_tmppos.clear();
			m_tmpuv1.clear();
			m_tmpuv2.clear();
			m_tmpdif.clear();
			m_tmpspe.clear();
			m_changed = ALL;
		}
		return true;
	}
	return false;
}
bool KMesh::addSubMesh(const KSubMesh &sub) {
	if (sub.start >= 0 && sub.count >= 0) {
		m_submeshes.push_back(sub);
		return true;
	}
	return false;
}
int KMesh::getSubMeshCount() const {
	return (int)m_submeshes.size();
}
const KSubMesh * KMesh::getSubMesh(int index) const {
	if (0 <= index && index < (int)m_submeshes.size()) {
		return &m_submeshes[index];
	}
	return nullptr;
}
KSubMesh * KMesh::getSubMesh(int index) {
	if (0 <= index && index < (int)m_submeshes.size()) {
		return &m_submeshes[index];
	}
	return nullptr;
}
// p の型に関係なく x バイトだけずらしたアドレス
#define OFFSET_BYTE(p, x)  (void*)((char*)(p) + (x))
void KMesh::setPositions(int offset, const KVec3 *pos, int stride, int count) {
	m_changed |= POS;
	m_changed |= AABB;
	KVertex *v = lock(offset, count);
	for (int i=0; i<count; i++) {
		v[i].pos = *(const KVec3*)OFFSET_BYTE(pos, stride * i);
	}
	unlock();
}
void KMesh::setColors(int offset, const KColor *color, int stride, int count) {
	m_changed |= DIF;
	KVertex *v = lock(offset, count);
	for (int i=0; i<count; i++) {
		KColor colorf = *(KColor*)OFFSET_BYTE(color, stride * i);
		v[i].dif32 = KColor32(colorf);
	}
	unlock();
}
void KMesh::setColorInts(int offset, const KColor32 *color, int stride, int count) {
	m_changed |= DIF;
	KVertex *v = lock(offset, count);
	for (int i=0; i<count; i++) {
		v[i].dif32 = *(KColor32*)OFFSET_BYTE(color, stride * i);
	}
	unlock();
}
void KMesh::setSpeculars(int offset, const KColor *specular, int stride, int count) {
	m_changed |= SPE;
	KVertex *v = lock(offset, count);
	for (int i=0; i<count; i++) {
		KColor colorf = *(KColor*)OFFSET_BYTE(specular, stride * i);
		v[i].spe32 = KColor32(colorf);
	}
	unlock();
}
void KMesh::setSpecularInts(int offset, const KColor32 *specular, int stride, int count) {
	m_changed |= SPE;
	KVertex *v = lock(offset, count);
	for (int i=0; i<count; i++) {
		v[i].spe32 = *(KColor32*)OFFSET_BYTE(specular, stride * i);
	}
	unlock();
}
void KMesh::setTexCoords(int offset, const KVec2 *uv, int stride, int count) {
	m_changed |= UV1;
	KVertex *v = lock(offset, count);
	for (int i=0; i<count; i++) {
		v[i].tex = *(KVec2*)OFFSET_BYTE(uv, stride * i);
	}
	unlock();
}
void KMesh::setTexCoords2(int offset, const KVec2 *uv, int stride, int count) {
	m_changed |= UV2;
	KVertex *v = lock(offset, count);
	for (int i=0; i<count; i++) {
		v[i].tex2 = *(KVec2*)OFFSET_BYTE(uv, stride * i);
	}
	unlock();
}
void KMesh::setIndices(int offset, const int *indices, int count) {
	m_changed |= IDX;
	int *idx = lockIndices(offset, count);
	for (int i=0; i<count; i++) {
		idx[i] = indices[i];
	}
	unlockIndices();
}
void KMesh::setVertices(int offset, const KVertex *v, int count) {
	m_changed |= ALL;
	KVertex *dst = lock(offset, count);
#if 1
	memcpy(dst, v, sizeof(KVertex) * count);
#else
	for (int i=0; i<count; i++) {
		dst[offset + i] = v[i];
	}
#endif
	unlock();
}
int KMesh::readPositionsFloat3(int read_offset, int read_count, void *dest, int dest_stride) const {
	int cnt = KMath::min(read_count, (int)m_vertices.size() - read_offset);
	const KVertex *v = m_vertices.data();
	for (int i=0; i<cnt; i++) {
		memcpy(
			OFFSET_BYTE(dest, dest_stride * i),
			&v[read_offset + i].pos,
			sizeof(float)*3
		);
	}
	return cnt;
}
int KMesh::readColorsFloat4(int read_offset, int read_count, void *dest, int dest_stride) const {
	int cnt = KMath::min(read_count, (int)m_vertices.size() - read_offset);
	const KVertex *v = m_vertices.data();
	for (int i=0; i<cnt; i++) {
		KColor colorf = KColor(v[read_offset + i].dif32); // KColor32 --> KColor
		memcpy(
			OFFSET_BYTE(dest, dest_stride * i),
			&colorf,
			sizeof(float)*4
		);
	}
	return cnt;
}
int KMesh::readSpecularsFloat4(int read_offset, int read_count, void *dest, int dest_stride) const {
	int cnt = KMath::min(read_count, (int)m_vertices.size() - read_offset);
	const KVertex *v = m_vertices.data();
	for (int i=0; i<cnt; i++) {
		KColor colorf = KColor(v[read_offset + i].spe32); // KColor32 --> KColor
		memcpy(
			OFFSET_BYTE(dest, dest_stride * i),
			&colorf,
			sizeof(float)*4
		);
	}
	return cnt;
}
int KMesh::readTexCoordsFloat2_1(int read_offset, int read_count, void *dest, int dest_stride) const {
	int cnt = KMath::min(read_count, (int)m_vertices.size() - read_offset);
	const KVertex *v = m_vertices.data();
	for (int i=0; i<cnt; i++) {
		memcpy(
			OFFSET_BYTE(dest, dest_stride * i),
			&v[read_offset + i].tex,
			sizeof(float)*2
		);
	}
	return cnt;
}
int KMesh::readTexCoordsFloat2_2(int read_offset, int read_count, void *dest, int dest_stride) const {
	int cnt = KMath::min(read_count, (int)m_vertices.size() - read_offset);
	const KVertex *v = m_vertices.data();
	for (int i=0; i<cnt; i++) {
		memcpy(
			OFFSET_BYTE(dest, dest_stride * i),
			&v[read_offset + i].tex2,
			sizeof(float)*2
		);
	}
	return cnt;
}
int KMesh::readColorsUint32(int read_offset, int read_count, void *dest, int dest_stride) const {
	int cnt = KMath::min(read_count, (int)m_vertices.size() - read_offset);
	const KVertex *v = m_vertices.data();
	for (int i=0; i<cnt; i++) {
		memcpy(
			OFFSET_BYTE(dest, dest_stride * i),
			&v[read_offset + i].dif32,
			sizeof(uint32_t)
		);
	}
	return cnt;
}
int KMesh::readSpecularsUint32(int read_offset, int read_count, void *dest, int dest_stride) const {
	int cnt = KMath::min(read_count, (int)m_vertices.size() - read_offset);
	const KVertex *v = m_vertices.data();
	for (int i=0; i<cnt; i++) {
		memcpy(
			OFFSET_BYTE(dest, dest_stride * i),
			&v[read_offset + i].spe32,
			sizeof(uint32_t)
		);
	}
	return cnt;
}
const KVec3 * KMesh::getPositions(int offset) const {
	if (m_changed & POS) {
		m_changed &= ~POS;
		m_tmppos.resize(m_vertices.size());
		readPositionsFloat3(0, m_vertices.size(), m_tmppos.data(), sizeof(KVec3));
	}
	if (0 <= offset && offset < (int)m_tmppos.size()) {
		return m_tmppos.data() + offset;
	}
	return nullptr;
}
const KVec2 * KMesh::getTexCoords1(int offset) const {
	if (m_changed & UV1) {
		m_changed &= ~UV1;
		m_tmpuv1.resize(m_vertices.size());
		readTexCoordsFloat2_1(0, m_vertices.size(), m_tmpuv1.data(), sizeof(KVec2));
	}
	if (0 <= offset && offset < (int)m_tmpuv1.size()) {
		return m_tmpuv1.data() + offset;
	}
	return nullptr;
}
const KVec2 * KMesh::getTexCoords2(int offset) const {
	if (m_changed & UV2) {
		m_changed &= ~UV2;
		m_tmpuv2.resize(m_vertices.size());
		readTexCoordsFloat2_2(0, m_vertices.size(), m_tmpuv2.data(), sizeof(KVec2));
	}
	if (0 <= offset && offset < (int)m_tmpuv2.size()) {
		return m_tmpuv2.data() + offset;
	}
	return nullptr;
}
const KColor32 * KMesh::getColorInts(int offset) const {
	if (m_changed & DIF) {
		m_changed &= ~DIF;
		m_tmpdif.resize(m_vertices.size());
		readColorsUint32(0, m_vertices.size(), m_tmpdif.data(), sizeof(KColor32));
	}
	if (0 <= offset && offset < (int)m_tmpdif.size()) {
		return m_tmpdif.data() + offset;
	}
	return nullptr;
}
const KColor32 * KMesh::getSpecularInts(int offset) const {
	if (m_changed & SPE) {
		m_changed &= ~SPE;
		m_tmpspe.resize(m_vertices.size());
		readSpecularsUint32(0, m_vertices.size(), m_tmpspe.data(), sizeof(KColor32));
	}
	if (0 <= offset && offset < (int)m_tmpspe.size()) {
		return m_tmpspe.data() + offset;
	}
	return nullptr;
}
#undef OFFSET_BYTE

KVec3 KMesh::getPosition(int index) const {
	K__ASSERT(0 <= index && index < (int)m_vertices.size());
	return m_vertices.data()[index].pos;
}
KColor KMesh::getColor(int index) const {
	K__ASSERT(0 <= index && index < (int)m_vertices.size());
	return KColor(m_vertices.data()[index].dif32); // KColor32 --> KColor
}
KColor KMesh::getSpecular(int index) const {
	K__ASSERT(0 <= index && index < (int)m_vertices.size());
	return KColor(m_vertices.data()[index].spe32); // KColor32 --> KColor
}
KColor32 KMesh::getColor32(int index) const {
	K__ASSERT(0 <= index && index < (int)m_vertices.size());
	return m_vertices.data()[index].dif32;
}
KColor32 KMesh::getSpecular32(int index) const {
	K__ASSERT(0 <= index && index < (int)m_vertices.size());
	return m_vertices.data()[index].spe32;
}
KVec2 KMesh::getTexCoord(int index) const {
	K__ASSERT(0 <= index && index < (int)m_vertices.size());
	return m_vertices.data()[index].tex;
}
KVec2 KMesh::getTexCoord2(int index) const {
	K__ASSERT(0 <= index && index < (int)m_vertices.size());
	return m_vertices.data()[index].tex2;
}
const KVertex * KMesh::getVertices(int offset) const {
	K__ASSERT(0 <= offset && offset < (int)m_vertices.size());
	return m_vertices.data() + offset;
}
const int * KMesh::getIndices(int offset) const {
	// インデックス配列が存在せず、かつオフセット 0 が指定されているときのみ無言で nullptr を返す
	// それ以外はエラー扱いとする
	if (m_indices.empty() && offset == 0) {
		return nullptr;
	}
	K__ASSERT(0 <= offset && offset < (int)m_indices.size());
	return m_indices.data() + offset;
}
int KMesh::getVertexCount() const {
	return m_vertices.size();
}
int KMesh::getIndexCount() const {
	return m_indices.size();
}
KVertex * KMesh::lock(int offset, int count) {
	if (offset >= 0 && count > 0) {
		if ((int)m_vertices.size() <= offset + count) {
			m_vertices.resize(offset + count);
		}
		return m_vertices.data() + offset;
	}
	return nullptr;
}
void KMesh::unlock() {
	// 頂点情報のどれかが書き換えられたものとする
	m_changed |= ALL;
}
int * KMesh::lockIndices(int offset, int count) {
	if (offset >= 0 && count > 0) {
		if ((int)m_indices.size() <= offset + count) {
			m_indices.resize(offset + count);
		}
		return &m_indices[offset];
	}
	return nullptr;
}
void KMesh::unlockIndices() {
	// インデックスが書き換えられたものとする
	m_changed |= IDX;
}
#pragma endregion // KMesh


#pragma region MeshShape
void MeshShape::makeRect(KMesh *mesh, const KVec2 &p0, const KVec2 &p1, const KVec2 &uv0, const KVec2 &uv1, const KColor &color, bool uvRot90) {
	if (mesh == nullptr) return;
	mesh->clear();
	mesh->setVertexCount(4);
	if (1) {
		mesh->setPosition(0, KVec3(p0.x, p1.y, 0.0f));
		mesh->setPosition(1, KVec3(p1.x, p1.y, 0.0f));
		mesh->setPosition(2, KVec3(p0.x, p0.y, 0.0f));
		mesh->setPosition(3, KVec3(p1.x, p0.y, 0.0f));

		if (uvRot90) {
			mesh->setTexCoord(0, KVec2(uv1.x, uv0.y)); // 左上頂点にはテクスチャ右上
			mesh->setTexCoord(1, KVec2(uv1.x, uv1.y)); // 右上頂点にはテクスチャ右下
			mesh->setTexCoord(2, KVec2(uv0.x, uv0.y)); // 左下頂点にはテクスチャ左上
			mesh->setTexCoord(3, KVec2(uv0.x, uv1.y)); // 右下頂点にはテクスチャ左下
		} else {
			mesh->setTexCoord(0, KVec2(uv0.x, uv0.y));
			mesh->setTexCoord(1, KVec2(uv1.x, uv0.y));
			mesh->setTexCoord(2, KVec2(uv0.x, uv1.y));
			mesh->setTexCoord(3, KVec2(uv1.x, uv1.y));
		}
		KColor32 color32 = color; // KColor --> KColor32
		mesh->setColorInt(0, color32);
		mesh->setColorInt(1, color32);
		mesh->setColorInt(2, color32);
		mesh->setColorInt(3, color32);
	}
	{
		KSubMesh sub;
		sub.start = 0;
		sub.count = 4;
		sub.primitive = KPrimitive_TRIANGLE_STRIP;
		mesh->addSubMesh(sub);
	}
}
void MeshShape::makeRectSkew(KMesh *mesh, const KVec2 &p0, const KVec2 &p1, const KVec2 &uv0, const KVec2 &uv1, const KColor &color, float skew_dxdy, float skew_dudv) {
	if (mesh == nullptr) return;
	mesh->clear();
	mesh->setVertexCount(4);
	if (1) {
		float dy = p1.y - p0.y;
		float dv = uv1.y - uv0.y;

		mesh->setPosition(0, KVec3(p0.x+skew_dxdy/2*dy, p1.y, 0.0f));
		mesh->setPosition(1, KVec3(p1.x+skew_dxdy/2*dy, p1.y, 0.0f));
		mesh->setPosition(2, KVec3(p0.x-skew_dxdy/2*dy, p0.y, 0.0f));
		mesh->setPosition(3, KVec3(p1.x-skew_dxdy/2*dy, p0.y, 0.0f));

		mesh->setTexCoord(0, KVec2(uv0.x+skew_dudv/2*dv, uv0.y));
		mesh->setTexCoord(1, KVec2(uv1.x+skew_dudv/2*dv, uv0.y));
		mesh->setTexCoord(2, KVec2(uv0.x-skew_dudv/2*dv, uv1.y));
		mesh->setTexCoord(3, KVec2(uv1.x-skew_dudv/2*dv, uv1.y));

		KColor32 color32 = color; // KColor --> KColor32
		mesh->setColorInt(0, color32);
		mesh->setColorInt(1, color32);
		mesh->setColorInt(2, color32);
		mesh->setColorInt(3, color32);
	}
	{
		KSubMesh sub;
		sub.start = 0;
		sub.count = 4;
		sub.primitive = KPrimitive_TRIANGLE_STRIP;
		mesh->addSubMesh(sub);
	}
}
#pragma endregion // MeshShape


#pragma region KDrawList
KDrawList::KDrawList() {
	clear();
}
KDrawList::~KDrawList() {
	clear();
}
void KDrawList::destroy() {
	clear();
}
bool KDrawList::is_changed() {
#if DRAWLIST_NO_UNITE
	return true;
#else
	// ユーザー定義描画がある場合は、常に変更済みとみなす
	if (m_next.material.cb) {
		return true;
	}
	// プリミティブが List 系の場合のみ結合描画できる
	if (m_next.prim == KPrimitive_LINES || m_next.prim == KPrimitive_TRIANGLES) {
		// OK
	} else {
		return true;
	}

	// マテリアル異なる？
	const float col_tolerance = 4.0f / 255;
	if (!KMaterial::is_compatible(m_last.material, m_next.material, col_tolerance)) {
		return true;
	}

	// プリミティブ異なる？
	if (m_last.prim != m_next.prim) {
		return true;
	}

	// ステンシル異なる？
	if (m_last.with_stencil != m_next.with_stencil) {
		return true;
	}
	if (m_last.with_stencil) {
		// ステンシルを使う設定の場合は、設定の中身を比較する
		if (m_last.stencil_fn != m_next.stencil_fn) return true;
		if (m_last.stencil_op != m_next.stencil_op) return true;
		if (m_last.stencil_ref!= m_next.stencil_ref)return true;
	}

	if (m_last.color_write != m_next.color_write) return true;

	// 行列異なる？
	const float mat_tolerance = 0.01f;
	if (m_last.transform != m_next.transform) {
		if (!m_last.transform.equals(m_next.transform, mat_tolerance)) {
			return true;
		}
	}
	if (m_last.projection != m_next.projection) {
		if (!m_last.projection.equals(m_next.projection, mat_tolerance)) {
			return true;
		}
	}
	// 変更なし
	return false;
#endif
}
void KDrawList::clear() {
	m_unimesh.clear();
	m_next = KDrawListItem();
	m_last = KDrawListItem();
	m_items.clear();
	m_changed = 0;
}
void KDrawList::setMaterial(const KMaterial &m) {
	m_next.material = m;
}
void KDrawList::setTransform(const KMatrix4 &m) {
	m_next.transform = m;
}
void KDrawList::setProjection(const KMatrix4 &m) {
	m_next.projection = m;
}
void KDrawList::setPrimitive(KPrimitive prim) {
	m_next.prim = prim;
}
void KDrawList::setColorWrite(bool value) {
	m_next.color_write = value;
}
void KDrawList::setStencil(bool value) {
	m_next.with_stencil = value;
}
void KDrawList::setStencilOpt(KVideo::StencilFunc fn, int ref, KVideo::StencilOp op) {
	m_next.stencil_fn = fn;
	m_next.stencil_ref = ref;
	m_next.stencil_op = op;
}
void KDrawList::addMesh(const KMesh *mesh, int submeshIndex) {
	if (mesh == nullptr) return;

	const KSubMesh *subMesh = mesh->getSubMesh(submeshIndex);
	if (subMesh == nullptr) return;

	// setPrimitive() を呼んだ時点でリストが登録されてしまうため、
	// メッシュパラメータは setPrimitive() よりも先に設定しておく
	m_next.cb_mesh = mesh;
	m_next.cb_submesh = submeshIndex;

	setPrimitive(subMesh->primitive);
	if (mesh->getIndexCount() > 0) {
		addVerticesWithIndex(mesh->getVertices(), mesh->getVertexCount(), mesh->getIndices()+subMesh->start, subMesh->count);
	} else {
		addVertices(mesh->getVertices()+subMesh->start, subMesh->count);
	}
}
static void copy_mem(void *dst, int dst_stride, const void *src, int src_stride, int copysize, int copycount) {
	// p の型に関係なく x バイトだけずらしたアドレス
	#define OFFSET_BYTE(p, x)  (void*)((char*)(p) + (x))

	K__ASSERT(dst);
	K__ASSERT(src);
	for (int i=0; i<copycount; i++) {
		memcpy(
			OFFSET_BYTE(dst, dst_stride*i), 
			OFFSET_BYTE(src, src_stride*i),
			copysize
		);
	}

	#undef OFFSET_BYTES
}
void KDrawList::addVertices(const KVertex *v, int count) {
	if (v == nullptr) return;
	if (count <= 0) return;
	if (DRAWLIST_UNIDRAW_TEST) {
		int numv = m_unimesh.getVertexCount();

		if (m_last.with_index==true || is_changed()) {
			// 前回 addPrimitive を呼んだ時とは描画設定が変わっている。
			// 前回 addPrimitive したときの描画アイテムを確定させ、新しい描画単位として登録する
			if (m_last.count > 0) {
				m_items.push_back(m_last);
			}
			m_next.index_number_offset = 0; // 使わないが念のため初期化
			m_next.start = numv;
			m_next.count = 0;
			m_next.with_index = false;
		}
		{
			m_unimesh.setVertices(numv, v, count);
		}
		m_next.count += count;
		m_last = m_next; // 今回の addPrimitive で更新された描画アイテム

	} else {
		#if 1
		KVideo::pushRenderState();
		KVideo::setProjection(m_next.projection);
		KVideo::setTransform(m_next.transform);
		KVideoUtils::beginMaterial(&m_next.material);
		KVideo::drawUserPtrV(v, count, m_next.prim);
		KVideoUtils::endMaterial(&m_next.material);
		KVideo::popRenderState();
		#endif
	}
}
void KDrawList::addVerticesWithIndex(const KVertex *v, int vcount, const int *i, int icount) {
	if (v == nullptr || vcount == 0) return;
	if (i == nullptr || icount == 0) return;
	if (DRAWLIST_UNIDRAW_TEST) {
		int numv = m_unimesh.getVertexCount();
		int numi = m_unimesh.getIndexCount();
		
		// インデックス付きの描画は結合できない（インデックスをずらさないといけないので）
		// 常に独立して描画する
		if (1/*m_last.with_index==false || ischanged()*/) {
			// 前回 addPrimitive を呼んだ時とは描画設定が変わっている。
			// 既存の描画をコミットし、新しい描画単位として登録する
			if (m_last.count > 0) {
				m_items.push_back(m_last);
			}
			m_next.index_number_offset = numv;
			m_next.start = numi;
			m_next.count = 0;
			m_next.with_index = true;
		}
		m_unimesh.setVertices(numv, v, vcount);
		m_unimesh.setIndices(numi, i, icount);
		m_next.count += icount;
		m_last = m_next; // 今回の addPrimitive で更新された描画アイテム

	} else {
		#if 1
		KVideo::pushRenderState();
		KVideo::setProjection(m_next.projection);
		KVideo::setTransform(m_next.transform);
		KVideoUtils::beginMaterial(&m_next.material);
		KVideo::drawIndexedUserPtrV(v, vcount, i, icount, m_next.prim);
		KVideoUtils::endMaterial(&m_next.material);
		KVideo::popRenderState();
		#endif
	}
}
void KDrawList::endList() {
	if (DRAWLIST_UNIDRAW_TEST) {
		if (m_last.count > 0) {
			m_items.push_back(m_last);
		}
		m_next = KDrawListItem();
		m_last = KDrawListItem();
	}
}
size_t KDrawList::size() const {
	return m_items.size();
}
void KDrawList::draw() {
	if (m_items.empty()) return;
	if (DRAWLIST_UNIDRAW_TEST) {
		// メッシュの内容を転送
		int vnum = m_unimesh.getVertexCount();
		int inum = m_unimesh.getIndexCount();
		KVideo::pushRenderState();
		for (size_t i=0; i<m_items.size(); i++) {
			const KDrawListItem &item = m_items[i];
			bool done = false;
			if (item.material.cb) {
				item.material.cb->onMaterial_Draw(&item, &done);
			}
			if (!done) { // ユーザーによって描画が完了していないなら、続ける
				KVideo::setProjection(item.projection);
				KVideo::setTransform(item.transform);

				if (item.with_stencil) {
					KVideo::setStencilEnabled(true);
					KVideo::setStencilFunc(item.stencil_fn, item.stencil_ref, item.stencil_op);
				}
				if (!item.color_write) {
					KVideo::setColorWriteMask(0);
				}

				KVideoUtils::beginMaterial(&item.material);
				if (item.with_index) {
					KVideo::drawIndexedUserPtrV(
						m_unimesh.getVertices() + item.index_number_offset,
						m_unimesh.getVertexCount() - item.index_number_offset,
						m_unimesh.getIndices() + item.start,
						item.count,
						item.prim
					);
				} else {
					KVideo::drawUserPtrV(
						m_unimesh.getVertices() + item.start,
						item.count,
						item.prim
					);
				}
				KVideoUtils::endMaterial(&item.material);

				if (!item.color_write) {
					KVideo::setColorWriteMask(KColorChannel_RGBA);
				}
				if (item.with_stencil) {
					KVideo::setStencilEnabled(false);
				}
			}
		}
		KVideo::popRenderState();
	}
}

KDrawListItem & KDrawList::operator[](size_t index) {
	return m_items[index];
}
#pragma endregion // KDrawList


namespace KVideoUtils {

/// テクスチャの内容をコピーする。コピーする際にマテリアルを適用することができる。
/// 複数のマテリアルを指定した場合は、最初のマテリアルでコピーした結果を、さらに次のマテリアルでコピーするという手順を繰り返す
/// @param dst コピー先のレンダーテクスチャ
/// @param src コピー元のテクスチャ
/// @param materials コピーの際に適用するマテリアル。不要な場合は nullptr でよい。
/// @param materials_count  適用するマテリアルの個数
void blitArray(KTexture *dst, KTexture *src, KMaterial **materials, int materials_count) {
	if (src == nullptr) return;
	K__ASSERT(src != dst);

	// マテリアルなしの場合は普通にコピーする
	if (materials == nullptr || materials_count == 0) {
		_BlitEx(dst, src, nullptr, nullptr, nullptr, nullptr);
		return;
	}

	// マテリアルが１つだけ指定されている場合も普通にコピーする
	if (materials && materials_count == 1) {
		_BlitEx(dst, src, materials[0], nullptr, nullptr, nullptr);
		return;
	}

	// 複数のマテリアルが指定されている場合は全てを適用する
	if (materials && materials_count >= 2) {
		KTexture *old_src = src;
		for (int m=0; m<materials_count; m++) {
			blit(dst, src, materials[m]);
			if (m+1 < materials_count) {
				std::swap(src, dst); // src <--> dst
			}
		}
		if (old_src != src) {
			// 奇数回 swap したために、最終結果を保持しているレンダーターゲットが入れ替わっている
			std::swap(src, dst); // src <--> dst
			_BlitEx(dst, src, nullptr, nullptr, nullptr, nullptr);
		}
	}
}
void blit(KTexture *dst, KTexture *src, KMaterial *mat) {
	_BlitEx(dst, src, mat, nullptr, nullptr, nullptr);
}
void blitEx(KTexture *dst, KTexture *src, KMaterial *mat, const KVideoRect *src_rect, const KVideoRect *dst_rect, const KMatrix4 *transform) {
	_BlitEx(dst, src, mat, src_rect, dst_rect, transform);
}


#pragma region strToBlend
static const char * s_blend_name_table[KBlend_ENUM_MAX+1] = {
	"one",    // KBlend_ONE
	"add",    // KBlend_ADD
	"alpha",  // KBlend_ALPHA
	"sub",    // KBlend_SUB
	"mul",    // KBlend_MUL
	"screen", // KBlend_SCREEN
	"max",    // KBlend_MAX
	nullptr,     // (sentinel)
};
KBlend strToBlend(const char *s, KBlend def) {
	if (s == nullptr || !s[0]) return def;
	for (int i=0; i<KBlend_ENUM_MAX; i++) {
		if (K__STREQ(s, s_blend_name_table[i])) {
			return (KBlend)i;
		}
	}
	K__VIDEO_ERR("Invalid blend name '%s'", s);
	return def;
}
bool strToBlend(const char *s, KBlend *blend) {
	if (s && s[0]) {
		for (int i=0; i<KBlend_ENUM_MAX; i++) {
			if (K__STREQ(s, s_blend_name_table[i])) {
				if (blend) *blend = (KBlend)i;
				return true;
			}
		}
	}
	return false;
}
#pragma endregion // strToBlend


void translateInPlace(KMatrix4 &m, float x, float y, float z) {
	// (* * * *) 
	// (* * * *)
	// (* * * *)
	// (x y z 1) <--- ここをずらす
	m._41 += x;
	m._42 += y;
	m._43 += z;
}
void translateHalfPixelInPlace(KMatrix4 &m) {
	translateInPlace(m, -K_HALF_PIXEL, K_HALF_PIXEL, 0.0f);
}
void translateHalfPixelInPlace(KVec3 &m) {
	m.x -= K_HALF_PIXEL;
	m.y += K_HALF_PIXEL;
}
void translateHalfPixelInPlace(KVec2 &m) {
	m.x -= K_HALF_PIXEL;
	m.y += K_HALF_PIXEL;
}
/// 変形行列の平行移動成分だけを整数化する
void translateFloorInPlace(KMatrix4 &m) {
	// (* * * *)
	// (* * * *)
	// (* * * *)
	// (x y z 1) <--- ここの x, y, z を整数化する
	m._41 = floorf(m._41);
	m._42 = floorf(m._42);
	m._43 = floorf(m._43); // キャビネット図法など、Z値によってX,Yの座標が変化する場合はZの整数化もしないとダメ
}
void translateFloorInPlace(KVec3 &m) {
	m.x = floorf(m.x);
	m.y = floorf(m.y);
	m.z = floorf(m.z);
}
void translateFloorInPlace(KVec2 &m) {
	m.x = floorf(m.x);
	m.y = floorf(m.y);
}



void KVideoUtils::beginMaterial(const KMaterial *material) {
	if (material) {
		if (material->shader) {
			KVideo::setShader(material->shader);
			KShaderArg arg;
			arg.blend = material->blend;
			arg.texture = material->texture;
			arg.color = material->color;
			arg.specular = material->specular;
			KVideo::setDefaultShaderParams(arg);
			if (material->cb) {
				material->cb->onMaterial_SetShaderVariable(material);
			}
			KVideo::beginShader();
		} else {
			KVideo::setShader(nullptr);
			KVideo::setBlend(material->blend);
			KVideo::setFilter(material->filter);
			KVideo::setTextureAddressing(material->wrap);
			KVideo::setColor(material->color32());
			KVideo::setSpecular(material->specular32());
			KVideo::setTexture(material->texture);
		}
	} else {
		KVideo::setShader(nullptr);
		KVideo::setBlend(KBlend_ALPHA);
		KVideo::setFilter(KFilter_NONE);
		KVideo::setTextureAddressing(false);
		KVideo::setColor(K__COLOR32_WHITE);
		KVideo::setSpecular(K__COLOR32_ZERO);
		KVideo::setTexture(nullptr);
		KVideo::beginShader();
	}
	if (material && material->cb) {
		material->cb->onMaterial_Begin(material);
	}
}
void KVideoUtils::endMaterial(const KMaterial *material) {
	if (material && material->cb) {
		material->cb->onMaterial_End(material);
	}
	KVideo::endShader();
	KVideo::setShader(nullptr);
}

void KVideoUtils::drawMesh(const KMesh *mesh, int start, int count, KPrimitive primitive) {
	if (mesh && mesh->getSubMeshCount() > 0) {
		if (mesh->getIndexCount() > 0) {
			KVideo::drawIndexedUserPtrV(mesh->getVertices(), mesh->getVertexCount(), mesh->getIndices(), count, primitive);

		} else {
			KVideo::drawUserPtrV(mesh->getVertices(), count, primitive);
		}
	}
}
void KVideoUtils::drawMesh(const KMesh *mesh, int submeshIndex) {
	if (mesh == nullptr) return;
	const KSubMesh *subMesh = mesh->getSubMesh(submeshIndex);
	if (subMesh) {
		drawMesh(mesh, subMesh->start, subMesh->count, subMesh->primitive);
	}
}

} // namespace KVideoUtils


#pragma region KGizmo
KGizmo::KGizmo() {
}
void KGizmo::add_aabb_2d(const KMatrix4 &matrix, float x0, float y0, float x1, float y1, float z, const KColor &color) {
	addLine(matrix, KVec3(x0, y0, z), KVec3(x1, y0, z), color);
	addLine(matrix, KVec3(x1, y0, z), KVec3(x1, y1, z), color);
	addLine(matrix, KVec3(x1, y1, z), KVec3(x0, y1, z), color);
	addLine(matrix, KVec3(x0, y1, z), KVec3(x0, y0, z), color);
}
void KGizmo::addLineDash(const KMatrix4 &matrix, const KVec3 &a, const KVec3 &b, const KColor &color) {
	const int span = 8; // 線分同士の距離（線分の先頭から、次の線分の先頭まで）
	const int seg  = 4; // 線分の長さ
	float len = (a - b).getLength();
	if (len < seg) return;
	int num = (int)len / span;
	for (int i=0; i<num; i++) {
		KVec3 p0 = a + (b - a) * (float)(span*i    ) / len;
		KVec3 p1 = a + (b - a) * (float)(span*i+seg) / len;
		addLine(matrix, p0, p1, color);
	}
	{
		KVec3 p0 = a + (b - a) * (float)(span * num) / len;
		KVec3 p1 = b;
		addLine(matrix, p0, p1, color);
	}
}

template <class T> T GetMax(const T &a, const T &b, const T &c, int *idx) {
	if (a > b && a > c) {
		*idx = 0;
		return a;
	}
	if (b > a && b > c) {
		*idx = 1;
		return b;
	}
//	if (c > a && c > b) {
		*idx = 2;
		return c;
//	}
}

void KGizmo::addArrow(const KMatrix4 &matrix, const KVec3 &from, const KVec3 &to, float head, const KColor &color) {
	KVec3 dir;
	if (!(to - from).getNormalizedSafe(&dir)) {
		return;
	}
	KVec3 arrow_axis = to - from;

	// ここでは軸ベクトルしか与えられていないので、先端の三角形の法線がどこに向くようにするか決めないといけない。
	// まず視線ベクトル (0, 0, 1) に一致するようにしてみる。ダメだったら（矢印の軸が視線ベクトルに重なる場合）
	// 別の法線向きを選ぶ
	KVec3 cr = arrow_axis.cross(KVec3(0, 0, 1));// カメラ目線に垂直になるようにしてみる
	KVec3 right;
	if (!cr.getNormalizedSafe(&right)) {
		cr = arrow_axis.cross(KVec3(0, 1, 0)); // 水平面に重なるように置いてみる
		if (!cr.getNormalizedSafe(&right)) {
			cr = KVec3(1, 0, 0);
		}
	}
	addLine(matrix, from, to, color, color);
	addLine(matrix, to-dir*head+right*head, to, color, color);
	addLine(matrix, to-dir*head-right*head, to, color, color);
}
static KVertex MAKE_VERT(const KVec3 &pos, const KColor32 &dif, const KVec2 &tex) {
	KVertex v;
	v.pos = pos;
	v.dif32 = dif;
	v.tex = tex;
	return v;
}

void KGizmo::addLine(const KMatrix4 &matrix, const KVec3 &a, const KVec3 &b, const KColor &color) {
	KVertex v[] = {
		MAKE_VERT(a, color, KVec2()),
		MAKE_VERT(b, color, KVec2()),
	};
	m_drawlist.setTransform(matrix);
	m_drawlist.setPrimitive(KPrimitive_LINES);
	m_drawlist.addVertices(v, 2);
}
void KGizmo::addLine(const KMatrix4 &matrix, const KVec3 &a, const KVec3 &b, const KColor &colora, const KColor &colorb) {
	KVertex v[] = {
		MAKE_VERT(a, colora, KVec2()),
		MAKE_VERT(b, colorb, KVec2()),
	};
	m_drawlist.setTransform(matrix);
	m_drawlist.setPrimitive(KPrimitive_LINES);
	m_drawlist.addVertices(v, 2);
}
void KGizmo::addAabbFill2D(const KMatrix4 &matrix, float x0, float y0, float x1, float y1, const KColor &color) {
	KVertex v[] = {
		MAKE_VERT(KVec3(x0, y0, 0.0f), color, KVec2()),
		MAKE_VERT(KVec3(x0, y1, 0.0f), color, KVec2()),
		MAKE_VERT(KVec3(x1, y1, 0.0f), color, KVec2()),
		MAKE_VERT(KVec3(x1, y0, 0.0f), color, KVec2()),
		MAKE_VERT(KVec3(x0, y0, 0.0f), color, KVec2()),
		MAKE_VERT(KVec3(x1, y1, 0.0f), color, KVec2()),
	};
	m_drawlist.setTransform(matrix);
	m_drawlist.setPrimitive(KPrimitive_TRIANGLES);
	m_drawlist.addVertices(v, 6);
}
void KGizmo::addLine2D(const KMatrix4 &matrix, float x0, float y0, float x1, float y1, const KColor &color) {
	addLine(matrix, KVec3(x0, y0, 0.0f), KVec3(x1, y1, 0.0f), color);
}
void KGizmo::addCircle(const KMatrix4 &matrix, const KVec3 &p, float r, int axis_index, const KColor &color) {
	int n = 16;
	KVec3 a, b;
	switch (axis_index) {
	case 0:
		// X axis
		break;
	case 1:
		// Y axis
		a = p + KVec3(r, 0.0f, 0.0f);
		for (int i=1; i<=n; i++) {
			float t = KMath::PI * 2 * i / n;
			b = p + KVec3(r * cosf(t), 0.0f, r * sinf(t));
			addLine(matrix, a, b, color, color);
			a = b;
		}
		break;
	case 2:
		// Z axis
		a = p + KVec3(r, 0.0f, 0.0f);
		for (int i=1; i<=n; i++) {
			float t = KMath::PI * 2 * i / n;
			b = p + KVec3(r * cosf(t), r * sinf(t), 0.0f);
			addLine(matrix, a, b, color, color);
			a = b;
		}
		break;
	}
}
void KGizmo::addAabbEllipse2D(const KMatrix4 &matrix, float x0, float y0, float x1, float y1, const KColor &color) {
	const int N = 32;
	const float rx = (x1 - x0) / 2;
	const float ry = (y1 - y0) / 2;
	const float ox = (x1 + x0) / 2;
	const float oy = (y1 + y0) / 2;
	KVec3 p0(x1, oy, 0.0f);
	for (int i=1; i<=N; i++) {
		float t = KMath::PI * 2 * i / N;
		KVec3 p1(ox + rx * cosf(t), oy + ry * sinf(t), 0.0f);
		addLine(matrix, p0, p1, color, color);
		p0 = p1;
	}
}
void KGizmo::addAabbFrame2D(const KMatrix4 &matrix, float x0, float y0, float x1, float y1, const KColor &color) {
	add_aabb_2d(matrix, x0, y0, x1, y1, 0.0f, color);
}
void KGizmo::addAabb(const KMatrix4 &matrix, const KVec3 &a, const KVec3 &b, const KColor &color) {
	KColor color2(color.r*0.7f, color.g*0.7f, color.b*0.7f, color.a);
	// 手前面
	addLine(matrix, KVec3(a.x, a.y, a.z), KVec3(b.x, a.y, a.z), color, color2);
	addLine(matrix, KVec3(b.x, a.y, a.z), KVec3(b.x, b.y, a.z), color, color2);
	addLine(matrix, KVec3(b.x, b.y, a.z), KVec3(a.x, b.y, a.z), color, color2);
	addLine(matrix, KVec3(a.x, b.y, a.z), KVec3(a.x, a.y, a.z), color, color2);

	// 奥面
	addLineDash(matrix, KVec3(a.x, a.y, b.z), KVec3(b.x, a.y, b.z), color);
	addLine    (matrix, KVec3(b.x, a.y, b.z), KVec3(b.x, b.y, b.z), color, color2);
	addLine    (matrix, KVec3(b.x, b.y, b.z), KVec3(a.x, b.y, b.z), color, color2);
	addLineDash(matrix, KVec3(a.x, b.y, b.z), KVec3(a.x, a.y, b.z), color);

	// Z方向ライン
	addLine    (matrix, KVec3(a.x, a.y, a.z), KVec3(a.x, a.y, b.z), color, color2);
	addLine    (matrix, KVec3(b.x, a.y, a.z), KVec3(b.x, a.y, b.z), color, color2);
	addLine    (matrix, KVec3(b.x, b.y, a.z), KVec3(b.x, b.y, b.z), color, color2);
	addLineDash(matrix, KVec3(a.x, b.y, a.z), KVec3(a.x, b.y, b.z), color);
}
void KGizmo::addAabbSheared(const KMatrix4 &matrix, const KVec3 &a, const KVec3 &b, float xshear, const KColor &color) {
	KColor color2(color.r*0.7f, color.g*0.7f, color.b*0.7f, color.a);
	// 手前面
	addLine(matrix, KVec3(a.x-xshear, a.y, a.z), KVec3(b.x-xshear, a.y, a.z), color, color2);
	addLine(matrix, KVec3(b.x-xshear, a.y, a.z), KVec3(b.x-xshear, b.y, a.z), color, color2);
	addLine(matrix, KVec3(b.x-xshear, b.y, a.z), KVec3(a.x-xshear, b.y, a.z), color, color2);
	addLine(matrix, KVec3(a.x-xshear, b.y, a.z), KVec3(a.x-xshear, a.y, a.z), color, color2);

	// 奥面
	addLineDash(matrix, KVec3(a.x+xshear, a.y, b.z), KVec3(b.x+xshear, a.y, b.z), color);
	addLine    (matrix, KVec3(b.x+xshear, a.y, b.z), KVec3(b.x+xshear, b.y, b.z), color, color2);
	addLine    (matrix, KVec3(b.x+xshear, b.y, b.z), KVec3(a.x+xshear, b.y, b.z), color, color2);
	addLineDash(matrix, KVec3(a.x+xshear, b.y, b.z), KVec3(a.x+xshear, a.y, b.z), color);

	// Z方向ライン
	addLineDash(matrix, KVec3(a.x-xshear, a.y, a.z), KVec3(a.x+xshear, a.y, b.z), color);
	addLine    (matrix, KVec3(b.x-xshear, a.y, a.z), KVec3(b.x+xshear, a.y, b.z), color, color2);
	addLine    (matrix, KVec3(b.x-xshear, b.y, a.z), KVec3(b.x+xshear, b.y, b.z), color, color2);
	addLine    (matrix, KVec3(a.x-xshear, b.y, a.z), KVec3(a.x+xshear, b.y, b.z), color, color2);
}
void KGizmo::addAxis(const KMatrix4 &matrix, const KVec3 &pos, float size) {
	if (1) {
		const KVec3 xarm(size, 0.0f, 0.0f);
		const KVec3 yarm(0.0f, size, 0.0f);
		const KVec3 zarm(0.0f, 0.0f, size);
		addLine(matrix, pos - xarm, pos + xarm, KColor(1, 0, 0, 1), KColor(1, 0, 0, 1)*1.0f);
		addLine(matrix, pos - yarm, pos + yarm, KColor(0, 1, 0, 1), KColor(0, 1, 0, 1)*1.0f);
		addLine(matrix, pos - zarm, pos + zarm, KColor(0, 0, 1, 1), KColor(0, 0, 1, 1)*0.5f);
	} else {
		float head = 8;
		addArrow(matrix, KVec3(-size, 0.0f, 0.0f), KVec3(size, 0.0f, 0.0f), head, KColor(1, 0, 0, 1));
		addArrow(matrix, KVec3(0.0f, -size, 0.0f), KVec3(0.0f, size, 0.0f), head, KColor(0, 1, 0, 1));
		addArrow(matrix, KVec3(0.0f, 0.0f, -size), KVec3(0.0f, 0.0f, size), head, KColor(0, 0, 1, 1));
	}
}
void KGizmo::addMesh(const KMatrix4 &matrix, KMesh *mesh, const KColor &color) {
}
void KGizmo::newFrame() {
	m_drawlist.clear();
}
void KGizmo::render(const KMatrix4 &projection) {
	m_drawlist.endList();

	for (size_t i=0; i<m_drawlist.size(); i++) {
		m_drawlist[i].projection = projection;
	}

	m_drawlist.draw();
}
#pragma endregion // KGizmo


#pragma region CMeshBuf
CMeshBuf::CMeshBuf() {
	clear();
}
void CMeshBuf::clear() {
	m_Vertices = nullptr;
	m_VStart = 0;
	m_VCount = 0;
	m_Indices = nullptr;
	m_IStart = 0;
	m_ICount = 0;
	m_Mesh = nullptr;
	m_Locked = false;
}
size_t CMeshBuf::size() const {
	return m_VCount;
}
void CMeshBuf::resize2(KMesh *mesh, int vertexcount, int indexcount) {
	clear();
	if (mesh == nullptr) return;

	if (vertexcount > 0) {
		m_VStart = mesh->getVertexCount();
		m_VCount = vertexcount;
		m_Vertices = mesh->lock(m_VStart, m_VCount);
	}
	if (indexcount > 0) {
		m_IStart = 0;
		m_ICount = indexcount;
		m_Indices = mesh->lockIndices(m_IStart, m_ICount);
	}
	m_Locked = true;
	m_Mesh = mesh;
}
void CMeshBuf::setIndex(int index, int vertex) {
	m_Indices[index] = vertex;
}
void CMeshBuf::setPosition(int index, const KVec3 &pos) {
	m_Vertices[index].pos = pos;
}
void CMeshBuf::setColor32(int index, const KColor32 &color) {
	m_Vertices[index].dif32 = color;
}
void CMeshBuf::setSpecular32(int index, const KColor32 &color) {
	m_Vertices[index].spe32 = color;
}
void CMeshBuf::setTexCoord(int index, const KVec2 &uv) {
	m_Vertices[index].tex = uv;
}
void CMeshBuf::setTexCoord2(int index, const KVec2 &uv) {
	m_Vertices[index].tex2 = uv;
}
void CMeshBuf::copyVertex(int dest, int src) {
	m_Vertices[dest] = m_Vertices[src];
}
KSubMesh * CMeshBuf::addToMesh(KMesh *mesh, KPrimitive prim, const KMaterial *material) const {
	if (m_Mesh) {
		// resize2 で初期化されている
		// m_Vertices, m_Indices には頂点バッファを lock した値がそのまま入っているものとする
		// addToMesh は連続で呼ばれることがあるため、lock と unlock の整合性に注意
		if (m_Locked) {
			if (m_Vertices) m_Mesh->unlock();
			if (m_Indices) m_Mesh->unlockIndices();
			m_Locked = false; 
		}

		KSubMesh sub;
		if (m_Indices) { // with index buffer
			sub.start = m_IStart;
			sub.count = m_ICount;
		} else { // vertex only
			sub.start = m_VStart;
			sub.count = m_VCount;
		}
		sub.primitive = prim;
		if (material) {
			sub.material = *material;
		}
		if (m_Mesh->addSubMesh(sub)) {
			return m_Mesh->getSubMesh(m_Mesh->getSubMeshCount() - 1);
		}
		return nullptr;
	}
	return nullptr;
}
#pragma endregion // CMeshBuf




#pragma region CPathDraw

// ２次ベジェ曲線。３個の制御点を使う
static KVec2 _QuadricBezier(const KVec2 &p0, const KVec2 &p1, const KVec2 &p2, float t) {
	K__ASSERT(0 <= t && t <= 1);
	float T = 1.0f - t;
	float x = t*t*p2.x + 2*t*T*p1.x + T*T*p0.x;
	float y = t*t*p2.y + 2*t*T*p1.y + T*T*p0.y;
	return KVec2(x, y);
}

// ３次ベジェ曲線。４個の制御点を使う
static KVec2 _CubicBezier(const KVec2 &p0, const KVec2 &p1, const KVec2 &p2, const KVec2 &p3, float t) {
	K__ASSERT(0 <= t && t <= 1);
	float T = 1.0f - t;
	float x = t*t*t*p3.x + 3*t*t*T*p2.x + 3*t*T*T*p1.x + T*T*T*p0.x;
	float y = t*t*t*p3.y + 3*t*t*T*p2.y + 3*t*T*T*p1.y + T*T*T*p0.y;
	return KVec2(x, y);
}

// ４次ベジェ曲線。５個の制御点を使う
static KVec2 _QuinticBezier(const KVec2 &p0, const KVec2 &p1, const KVec2 &p2, const KVec2 &p3, const KVec2 &p4, float t) {
	K__ASSERT(0 <= t && t <= 1);
	float T = 1.0f - t;
	float x = t*t*t*t*p4.x + 4*t*t*t*T*p3.x + 6*t*t*T*T*p2.x + 4*t*T*T*T*p1.x + T*T*T*T*p0.x;
	float y = t*t*t*t*p4.y + 4*t*t*t*T*p3.y + 6*t*t*T*T*p2.y + 4*t*T*T*T*p1.y + T*T*T*T*p0.y;
	return KVec2(x, y);
}

CPathDraw::CPathDraw() {
	m_Z = 0;
	m_AutoLength = 4.0f;
}
void CPathDraw::pathClear() {
	m_Points.clear();
}
void CPathDraw::pathInit(const KVec2 &p) {
	m_Points.clear();
	m_Points.push_back(p);
}
void CPathDraw::pathLineTo(const KVec2 &p) {
	m_Points.push_back(p);
}

// center を中心とした半径 radius の弧を追加する
void CPathDraw::pathArcTo(const KVec2 &center, float radius, float rad_start, float rad_end, int segments) {
	if (segments > 0) {
		for (int i=1; i<=segments; i++) {
			float t = (float)i / segments;
			float rad = KMath::lerp(rad_start, rad_end, t);
			m_Points.push_back(KVec2(center.x + radius * cosf(rad), center.y + radius * sinf(rad)));
		}
	} else {
		pathArcToAuto(center, radius, rad_start, rad_end, 8);
	}
}
void CPathDraw::pathArcToAuto(const KVec2 &center, float radius, float rad_start, float rad_end, int maxlevel) {
	KVec2 p0 = center + KVec2(cosf(rad_start), sinf(rad_end)) * radius;
	KVec2 p1 = center + KVec2(cosf(rad_start), sinf(rad_end)) * radius;
	KVec2 d01 = p1 - p0;
	float lenx = fabs(d01.x);
	float leny = fabs(d01.y);
	if (lenx+leny < m_AutoLength) {
		m_Points.push_back(p1);
	} else if (maxlevel > 0) {
		float rad_mid = (rad_start + rad_end) * 0.5f;
		pathArcToAuto(center, radius, rad_start, rad_mid, maxlevel-1);
		pathArcToAuto(center, radius, rad_mid, rad_end, maxlevel-1);
	}
}

// ２次のベジェ補間を行う
// p1, p2   追加の制御点。既存のパスの終端を始点として、３個の点で補間を行う
// segments 線分個数. 0 を指定すると De Casteljau のアルゴリズムにより自動分割する。
//          その場合に線分長さ（の近似）が m_AutoLength 未満になったら計算を打ち切る
void CPathDraw::pathQuadricBezierTo(const KVec2 &p1, const KVec2 &p2, int segments) {
	K__ASSERT(!m_Points.empty());
	KVec2 p0 = m_Points.back();
	if (segments == 0) {
		segments = 8;
	}
	for (int i=1; i<=segments; i++) {
		float t = (float)i / segments;
		m_Points.push_back(_QuadricBezier(p0, p1, p2, t));
	}
}
void CPathDraw::pathQuadricBezierToAuto(const KVec2 &p0, const KVec2 &p1, const KVec2 &p2, int maxlevel) {
	KVec2 d01 = p1 - p0;
	KVec2 d12 = p2 - p1;
	float lenx = fabs(d01.x) + fabsf(d12.x);
	float leny = fabs(d01.y) + fabsf(d12.y);
	if (lenx+leny < m_AutoLength) {
		m_Points.push_back(p2);
	} else if (maxlevel > 0) {
		KVec2 p01 = (p0 + p1) * 0.5f;
		KVec2 p12 = (p1 + p2) * 0.5f;
		KVec2 p012 = (p01 + p12) * 0.5f;
		pathQuadricBezierToAuto(p0, p01, p012, maxlevel-1);
		pathQuadricBezierToAuto(p012, p12, p2, maxlevel-1);
	}
}

// ３次のベジェ補間を行う
// p1, p2, p3  追加の制御点。既存のパスの終端を始点として、４個の点で補間を行う
// segments    線分個数. 0 を指定すると De Casteljau のアルゴリズムにより自動分割する。
//             その場合に線分長さ（の近似）が m_AutoLength 未満になったら計算を打ち切る
void CPathDraw::pathCubicBezierTo(const KVec2 &p1, const KVec2 &p2, const KVec2 &p3, int segments) {
	K__ASSERT(!m_Points.empty());
	KVec2 p0 = m_Points.back();
	if (segments > 0) {
		for (int i=1; i<=segments; i++) {
			float t = (float)i / segments;
			m_Points.push_back(_CubicBezier(p0, p1, p2, p3, t));
		}
	} else {
		pathCubicBezierToAuto(p0, p1, p2, p3, 8);
	}
}
void CPathDraw::pathCubicBezierToAuto(const KVec2 &p0, const KVec2 &p1, const KVec2 &p2, const KVec2 &p3, int maxlevel) {
	KVec2 d01 = p1 - p0;
	KVec2 d12 = p2 - p1;
	KVec2 d23 = p3 - p2;
	float lenx = fabs(d01.x) + fabsf(d12.x) + fabsf(d23.x);
	float leny = fabs(d01.y) + fabsf(d12.y) + fabsf(d23.y);
	if (lenx+leny < m_AutoLength) {
		m_Points.push_back(p3);
	} else if (maxlevel > 0) {
		KVec2 p01 = (p0 + p1) * 0.5f;
		KVec2 p12 = (p1 + p2) * 0.5f;
		KVec2 p23 = (p2 + p3) * 0.5f;
		KVec2 p012 = (p01 + p12) * 0.5f;
		KVec2 p123 = (p12 + p23) * 0.5f;
		KVec2 p0123 = (p012 + p123) * 0.5f;
		pathCubicBezierToAuto(p0, p01, p012, p0123, maxlevel-1);
		pathCubicBezierToAuto(p0123, p123, p23, p3, maxlevel-1);
	}
}

// ４次のベジェ補間を行う
// p1, p2, p3, p4  追加の制御点。既存のパスの終端を始点として、５個の点で補間を行う
// segments        線分個数. 0 を指定すると De Casteljau のアルゴリズムにより自動分割する
//                 その場合に線分長さ（の近似）が m_AutoLength 未満になったら計算を打ち切る
void CPathDraw::pathQuinticBezierTo(const KVec2 &p1, const KVec2 &p2, const KVec2 &p3, const KVec2 &p4, int segments) {
	K__ASSERT(!m_Points.empty());
	KVec2 p0 = m_Points.back();
	if (segments > 0) {
		for (int i=1; i<=segments; i++) {
			float t = (float)i / segments;
			m_Points.push_back(_QuinticBezier(p0, p1, p2, p3, p4, t));
		}
	} else {
		pathQuinticBezierToAuto(p0, p1, p2, p3, p4, 8);
	}
}
void CPathDraw::pathQuinticBezierToAuto(const KVec2 &p0, const KVec2 &p1, const KVec2 &p2, const KVec2 &p3, const KVec2 &p4, int maxlevel) {
	KVec2 d01 = p1 - p0;
	KVec2 d12 = p2 - p1;
	KVec2 d23 = p3 - p2;
	KVec2 d34 = p4 - p3;
	float lenx = fabs(d01.x) + fabsf(d12.x) + fabsf(d23.x) + fabsf(d34.x);
	float leny = fabs(d01.y) + fabsf(d12.y) + fabsf(d23.y) + fabsf(d34.y);
	if (lenx+leny < m_AutoLength) {
		m_Points.push_back(p3);
	} else if (maxlevel > 0) {
		KVec2 p01 = (p0 + p1) * 0.5f;
		KVec2 p12 = (p1 + p2) * 0.5f;
		KVec2 p23 = (p2 + p3) * 0.5f;
		KVec2 p34 = (p3 + p4) * 0.5f;
		KVec2 p012 = (p01 + p12) * 0.5f;
		KVec2 p123 = (p12 + p23) * 0.5f;
		KVec2 p234 = (p23 + p34) * 0.5f;
		KVec2 p0123 = (p012 + p123) * 0.5f;
		KVec2 p1234 = (p123 + p234) * 0.5f;
		KVec2 p01234 = (p0123 + p1234) * 0.5f;
		pathQuinticBezierToAuto(p0, p01, p012, p0123, p01234, maxlevel-1);
		pathQuinticBezierToAuto(p01234, p1234, p234, p34, p4, maxlevel-1);
	}
}

// n次ベジェ補間を行う
// p          追加の制御点。既存のパスの終端を始点として、n+1個の点で補間を行う
// n          追加の制御店の個数（使いたい多項式の次数と同じ値になる）
// maxlevel   最大再帰深さ
void CPathDraw::pathNthBezierToAuto(const KVec2 *p, int n, int maxlevel) {
	if (n <= 0) {
		return;
	}
	if (n == 1) {
		m_Points.push_back(p[0]);
		return;
	}
	float lenx = 0;
	float leny = 0;
	for (int i=0; i+1<n; i++) {
		lenx += fabsf(p[i+1].x - p[i].x);
		leny += fabsf(p[i+1].y - p[i].y);
	}
	if (lenx+leny < m_AutoLength) {
		m_Points.push_back(p[n-1]);
	} else if (maxlevel > 0) {
		KVec2 tmp0[16];
		KVec2 tmp1[16];
		pathNthBezierGetPoints(p, n, tmp0, tmp1, 0, n-1);
		pathNthBezierToAuto(tmp0, n, maxlevel-1);
		pathNthBezierToAuto(tmp1, n, maxlevel-1);
	}
}

// n次のベジェを２個のｎ次ベジェ曲線に分割し、新しい制御点を得る
// p, n 元の制御点と個数
// tmp0 始点から中間点までをベジェ曲線で表すときの制御点。個数は n と同じで、tmp0 の始点 (tmp0[0]) は p[0] と同じ
// tmp1 中間点から終点までをベジェ曲線で表すときの制御点。個数は n と同じで、tmp1 の終点 (tmp1[n-1] は p[n-1] と同じ
// idx0 tmp0 の書き込み先制御点インデックス(初期値は 0)
// idx1 tmp1 の書き込み先制御点インデックス(初期値は n-1)
void CPathDraw::pathNthBezierGetPoints(const KVec2 *p, int n, KVec2 *tmp0, KVec2 *tmp1, int idx0, int idx1) {
	KVec2 tmp[16];
	for (int i=0; i+1<n; i++) {
		tmp[i] = (p[i] + p[i+1]) * 0.5f;
	}
	tmp0[idx0] = p[0];
	tmp1[idx1] = p[n-1];
	if (n >= 2) {
		pathNthBezierGetPoints(tmp, n-1, tmp0, tmp1, idx0+1, idx1-1);
	}
}

// 任意の個数の点を指定し、イイ感じで補完する
// polynominal 最大の補完次数（16次まで）
void CPathDraw::pathSmooth(const KVec2 *points, int size, int polynomial) {
	int poly = polynomial;
	int index = 0;
	while (poly >= 5) {
		// ５次以上の補完
		// polynomial次のベジェを処理するには poly+1個の点が必要だが、
		// 前回の終端点を始点として再利用するため実際には poly 個の点があれば良い。
		// 残りの点の個数が 5 個未満になったら、4, 3, 2, 1次と専用の補完関数に飛ばす
		while (index+poly-1 < size) {
			pathNthBezierToAuto(points+index, poly, 8); 
			index += poly;
		}
		poly--;
	}
	if (poly >= 4) {
		// 4次補間
		// 残りの点が足りなくて4次補間できなくなったら、次数を下げて補完するため次の処理へ行く
		while (index+3 < size) {
			KVec2 p1 = points[index  ];
			KVec2 p2 = points[index+1];
			KVec2 p3 = points[index+2];
			KVec2 p4 = points[index+3];
			pathQuinticBezierTo(p1, p2, p3, p4);
			index += 4;
		}
	}
	if (poly >= 3) {
		// 3次補間
		// 残りの点が足りなくて3次補間できなくなったら、次数を下げて補完するため次の処理へ行く
		while (index+2 < size) {
			KVec2 p1 = points[index  ];
			KVec2 p2 = points[index+1];
			KVec2 p3 = points[index+2];
			pathCubicBezierTo(p1, p2, p3);
			index += 3;
		}
	}
	if (poly >= 2) {
		// 2次補間
		// 残りの点が足りなくて2次補間できなくなったら、次数を下げて補完するため次の処理へ行く
		while (index+1 < size) {
			KVec2 p1 = points[index  ];
			KVec2 p2 = points[index+1];
			pathQuadricBezierTo(p1, p2);
			index += 2;
		}
	}
	if (poly >= 1) {
		// 1次補間
		// 最後の点まできっちり使って線形補間する
		while (index < size) {
			KVec2 p1 = points[index];
			pathLineTo(p1);
			index += 1;
		}
	}
}

// パスに従って線を引く。
// mesh      サブメッシュの追加先
// thickness 太さ。0を指定すると KPrimitive_LINE_STRIP を使う
// color     頂点色
// material  サブメッシュのマテリアル
void CPathDraw::stroke(KMesh *mesh, float thickness, const KColor &color, const KMaterial *material) {
	strokeGradient(mesh, thickness, color, color, material);
}

// パスに従って線を引く。始点と終点の頂点色をそれぞれ指定し、グラデーションさせる
void CPathDraw::strokeGradient(KMesh *mesh, float thickness, const KColor &color1, const KColor &color2, const KMaterial *material) {
	if (m_Points.size() < 2) return;

	if (thickness > 0) {
		m_MeshBuf.clear();
		m_MeshBuf.resize2(mesh, m_Points.size() * 2, 0);
		int N = m_Points.size();
		KVec2 nor0 = KVec2(1, 0);
		for (int i=0; i<N; i++) {
			float t = (float)i / (N - 1);
			const KVec2 &p0 = m_Points[i];

			KVec2 nor1 = nor0;
			if (i+1 < N) {
				const KVec2 &p1 = m_Points[i+1];
				KVec2 tang = p1 - p0;
				float tanglen = hypotf(tang.x, tang.y);
				if (tanglen > 0) {
					nor1 = KVec2(-tang.y/tanglen, tang.x/tanglen);
				}
			}

			KVec2 nor01 = (nor0 + nor1) * 0.5f;
			KVec3 pos(p0.x, p0.y, m_Z);
			KVec3 half(nor01.x*thickness*0.5f, nor01.y*thickness*0.5f, 0.0f);
			m_MeshBuf.setPosition(i*2+0, pos-half);
			m_MeshBuf.setPosition(i*2+1, pos+half);

			KColor32 color = KColor32::lerp(color1, color2, t);
			KColor32 col32(color);
			m_MeshBuf.setColor32(i*2+0, col32);
			m_MeshBuf.setColor32(i*2+1, col32);

			nor0 = nor1;
		}
		m_MeshBuf.addToMesh(mesh, KPrimitive_TRIANGLE_STRIP, material);
		m_Points.clear();

	} else {
		m_MeshBuf.clear();
		m_MeshBuf.resize2(mesh, m_Points.size(), 0);
		int N = m_Points.size();
		for (int i=0; i<N; i++) {
			float t = (float)i / (N - 1);

			const KVec2 &p = m_Points[i];
			KVec3 pos(p.x, p.y, m_Z);
			m_MeshBuf.setPosition(i, pos);

			KColor32 color = KColor32::lerp(color1, color2, t);
			KColor32 col32(color);
			m_MeshBuf.setColor32(i, col32);
		}
		m_MeshBuf.addToMesh(mesh, KPrimitive_LINE_STRIP, material);
		m_Points.clear();
	}
}

void CPathDraw::addCross(KMesh *mesh, const KVec2 &pos, float size, const KColor &color) {
	float half = size / 2;
	m_MeshBuf.clear();
	m_MeshBuf.resize2(mesh, 4, 0);
	m_MeshBuf.setPosition(0, KVec3(pos.x-half, pos.y, 0.0f));
	m_MeshBuf.setPosition(1, KVec3(pos.x+half, pos.y, 0.0f));
	m_MeshBuf.setPosition(2, KVec3(pos.x, pos.y-half, 0.0f));
	m_MeshBuf.setPosition(3, KVec3(pos.x, pos.y+half, 0.0f));
	m_MeshBuf.setColor32(0, KColor32::WHITE);
	m_MeshBuf.setColor32(1, KColor32::WHITE);
	m_MeshBuf.setColor32(2, KColor32::WHITE);
	m_MeshBuf.setColor32(3, KColor32::WHITE);
	KMaterial m;
	m.color = color;
	m_MeshBuf.addToMesh(mesh, KPrimitive_LINES, &m);
}

void CPathDraw::addLine(KMesh *mesh, const KVec2 &a, const KVec2 &b, const KColor &color) {
	m_MeshBuf.clear();
	if (color.a > 0) {
		m_MeshBuf.resize2(mesh, 2, 0);
		m_MeshBuf.setPosition(0, KVec3(a.x, a.y, 0.0f));
		m_MeshBuf.setPosition(1, KVec3(b.x, b.y, 0.0f));
		m_MeshBuf.setColor32(0, KColor32::WHITE);
		m_MeshBuf.setColor32(1, KColor32::WHITE);
		m_MeshBuf.setColor32(2, KColor32::WHITE);
		m_MeshBuf.setColor32(3, KColor32::WHITE);
		KMaterial m;
		m.color = color;
		m_MeshBuf.addToMesh(mesh, KPrimitive_LINES, &m);
	}
}

void CPathDraw::addBox(KMesh *mesh, const KVec2 &pos, const KVec2 &halfsize, const KColor &fill_color, const KColor &stroke_color) {
	m_MeshBuf.clear();
	if (fill_color.a > 0) {
		m_MeshBuf.resize2(mesh, 4, 0);
		m_MeshBuf.setPosition(0, KVec3(pos.x-halfsize.x, pos.y-halfsize.y, 0.0f));
		m_MeshBuf.setPosition(1, KVec3(pos.x+halfsize.x, pos.y-halfsize.y, 0.0f));
		m_MeshBuf.setPosition(2, KVec3(pos.x+halfsize.x, pos.y+halfsize.y, 0.0f));
		m_MeshBuf.setPosition(3, KVec3(pos.x-halfsize.x, pos.y+halfsize.y, 0.0f));
		m_MeshBuf.setColor32(0, KColor32::WHITE);
		m_MeshBuf.setColor32(1, KColor32::WHITE);
		m_MeshBuf.setColor32(2, KColor32::WHITE);
		m_MeshBuf.setColor32(3, KColor32::WHITE);
		KMaterial m;
		m.color = fill_color;
		m_MeshBuf.addToMesh(mesh, KPrimitive_TRIANGLE_FAN, &m);
	}
	if (stroke_color.a > 0) {
		m_MeshBuf.resize2(mesh, 5, 0);
		m_MeshBuf.setPosition(0, KVec3(pos.x-halfsize.x, pos.y-halfsize.y, 0.0f));
		m_MeshBuf.setPosition(1, KVec3(pos.x+halfsize.x, pos.y-halfsize.y, 0.0f));
		m_MeshBuf.setPosition(2, KVec3(pos.x+halfsize.x, pos.y+halfsize.y, 0.0f));
		m_MeshBuf.setPosition(3, KVec3(pos.x-halfsize.x, pos.y+halfsize.y, 0.0f));
		m_MeshBuf.setColor32(0, KColor32::WHITE);
		m_MeshBuf.setColor32(1, KColor32::WHITE);
		m_MeshBuf.setColor32(2, KColor32::WHITE);
		m_MeshBuf.setColor32(3, KColor32::WHITE);
		m_MeshBuf.copyVertex(4, 0);
		KMaterial m;
		m.color = stroke_color;
		m_MeshBuf.addToMesh(mesh, KPrimitive_LINE_STRIP, &m);
	}
}

void CPathDraw::addEllipse(KMesh *mesh, const KVec2 &pos, const KVec2 &r, const KColor &fill_color, const KColor &stroke_color) {
	m_MeshBuf.clear();
	if (r.isZero()) {
		return;
	}
#if 0
	int num_points = 32;
#else
	int num_points = 8; // 最低頂点数
	{
		float SEG_LEN = 10; // １辺の長さ
		float R = (r.x + r.y) / 2;
		int num = (int)(2 * KMath::PI * R / SEG_LEN);
		if (num_points < num) {
			num_points = num;
		}
	}
#endif
	if (fill_color.a > 0) {
		m_MeshBuf.resize2(mesh, num_points, 0);
		for (int i=0; i<num_points; i++) {
			float t = KMath::PI * 2 * i / num_points;
			m_MeshBuf.setPosition(i, KVec3(pos.x + r.x * cosf(t), pos.y + r.y * sinf(t), 0.0f));
			m_MeshBuf.setColor32(i, KColor32::WHITE);
		}
		KMaterial m;
		m.color = fill_color;
		m_MeshBuf.addToMesh(mesh, KPrimitive_TRIANGLE_FAN, &m);
	}
	if (stroke_color.a > 0) {
		m_MeshBuf.resize2(mesh, num_points + 1, 0);
		for (int i=0; i<num_points; i++) {
			float t = KMath::PI * 2 * i / num_points;
			m_MeshBuf.setPosition(i, KVec3(pos.x + r.x * cosf(t), pos.y + r.y * sinf(t), 0.0f));
			m_MeshBuf.setColor32(i, KColor32::WHITE);
		}
		m_MeshBuf.copyVertex(num_points, 0);
		KMaterial m;
		m.color = stroke_color;
		m_MeshBuf.addToMesh(mesh, KPrimitive_LINE_STRIP, &m);
	}
}
#pragma endregion // CPathDraw


} // namespace

