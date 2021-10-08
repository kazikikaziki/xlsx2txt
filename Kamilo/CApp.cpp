
// シンプルアプリケーションクラス
#define USE_IMGUI

#include "CApp.h"
#include <assert.h>
#include <d3d9.h>
#include <vector>
#include <string>

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx9.h"
static const char *g_ImFontFile = "c:\\windows\\fonts\\meiryo.ttc";
static int g_ImFontTTC = 2; // (0=Regular, 1=Italic, 2=Bold, 3=BoldItalic)
static float g_ImFontSize = 16;

// ImGui は一部の漢字しか対応していない。
// 追加で使いたい感じがある場合はここに登録する
// http://itpro.nikkeibp.co.jp/article/COLUMN/20091209/341831/?rt=nocnt
static const wchar_t *g_ImExtraChars = 
	// 2010年に常用漢字に追加された196文字。
	// 一部SJIS非対応の漢字があるので注意。SJISで保存しようとすると文字化けする
	//
	// @todo 以下を参考にして、完全な漢字のリストを作成する
	// imgui で日本語が「?」になる場合の対処
	// https://qiita.com/benikabocha/items/a25571c1b059eaf952de/
	//
	L"挨曖宛嵐畏萎椅彙茨咽淫唄鬱怨媛艶旺岡臆俺苛牙瓦楷潰諧崖蓋"
	L"骸柿顎葛釜鎌韓玩伎亀毀畿臼嗅巾僅錦惧串窟熊詣憬稽隙桁拳鍵"
	L"舷股虎錮勾梗喉乞傲駒頃痕沙挫采塞埼柵刹拶斬恣摯餌鹿　嫉腫" // 「叱」の旧字体は SJIS 範囲外なので除外した
	L"呪袖羞蹴憧拭尻芯腎須裾凄醒脊戚煎羨腺詮箋膳狙遡曽爽痩踪捉"
	L"遜汰唾堆戴誰旦綻緻酎貼嘲捗椎爪鶴諦溺　妬賭藤瞳栃頓貪丼那" // 「填」の旧字体は SJIS 範囲外なので除外した
	L"奈梨謎鍋匂虹捻罵　箸氾汎阪斑眉膝肘訃阜蔽餅璧蔑哺蜂貌　睦" // 「剥」「頬」の旧字体は SJIS 範囲外なので除外した
	L"勃昧枕蜜冥麺冶弥闇喩湧妖瘍沃拉辣藍璃慄侶瞭瑠呂賂弄籠麓脇"
	// それ以外の文字
	L"※←→↑↓牢礫贄杖碧蒼橙鞘綾堰憑楕矩"
	L"①②③④⑤⑥⑦⑧⑨發牌蓮么盃飜雀嶺槓晒"
;
static std::vector<ImWchar> g_ImKanji;
#endif


// Utils
static void _MakeOrthoMatrix(D3DMATRIX *mout, float w, float h, float znear, float zfar) {
	assert(mout);
	assert(w > 0);
	assert(h > 0);
	assert(znear < zfar);
	// 平行投影行列（左手系）
	// http://www.opengl.org/sdk/docs/man2/xhtml/glOrtho.xml
	// http://msdn.microsoft.com/ja-jp/library/cc372881.aspx
	float x = 2.0f / w;
	float y = 2.0f / h;
	float a = 1.0f / (zfar - znear);
	float b = znear / (znear - zfar);
	float tmp[] = {
		x, 0, 0, 0,
		0, y, 0, 0,
		0, 0, a, 0,
		0, 0, b, 1,
	};
	memcpy(mout, tmp, sizeof(float)*16);
}

static WNDCLASSEXW g_WC = {0};
static HWND g_hWnd = NULL;
static IDirect3D9 *g_D3D9 = NULL;
static IDirect3DDevice9 *g_D3D9_Dev = NULL;
static IDirect3DStateBlock9 *g_D3D9_Block = NULL;
static D3DPRESENT_PARAMETERS g_D3D9_PP = {0};
static bool g_ShouldResetDevice = false;
static bool g_ShouldExit = false;
static std::wstring g_DropFileNameW;

LRESULT CALLBACK _WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
	#ifdef USE_IMGUI
	extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wp, lp)) {
		// ここで ImGui_ImplWin32_WndProcHandler が 1 を返した場合、そのメッセージは処理済みであるため
		// 次に伝搬してはならない。これはマウスカーソルの形状設定などに影響する。
		// 正しく処理しないと ImGui のテキストエディタにカーソルを重ねてもカーソル形状が IBeam にならなかったりする
		return 1;
	}
	#endif

	switch (msg) {
	case WM_CREATE:
		DragAcceptFiles(hWnd, TRUE);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_SIZE:
		if (wp != SIZE_MINIMIZED) {
			g_D3D9_PP.BackBufferWidth = LOWORD(lp);
			g_D3D9_PP.BackBufferHeight  = HIWORD(lp);
			g_ShouldResetDevice = true;
		}
		break;
	case WM_DROPFILES:
		{	// シングルドロップファイルのみ対応
			HDROP hDrop = (HDROP)wp;
			wchar_t name[MAX_PATH] = {0};
			DragQueryFileW(hDrop, 0, name, MAX_PATH);
			g_DropFileNameW = name;
			DragFinish(hDrop);
			break;
		}
	}
	return DefWindowProcW(hWnd, msg, wp, lp);
}

CApp::CApp() {
}
void CApp::postExit() {
	g_ShouldExit = true;
}
void CApp::run(int cw, int ch, const wchar_t *title) {
	assert(g_hWnd == NULL);
	assert(g_D3D9 == NULL);
	assert(g_D3D9_Dev == NULL);
	g_ShouldResetDevice = false;
	g_ShouldExit = false;

	// Window
	ZeroMemory(&g_WC, sizeof(WNDCLASSEXW));
	g_WC.cbSize = sizeof(WNDCLASSEXW);
	g_WC.lpszClassName = L"KumaWindowClass";
	g_WC.lpfnWndProc = _WndProc;
	RegisterClassExW(&g_WC);
	g_hWnd = CreateWindowExW(0, g_WC.lpszClassName, NULL, 
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, NULL, NULL, NULL, NULL);
	if (g_hWnd == NULL) goto END;
	ShowWindow(g_hWnd, SW_SHOW);
	SetWindowTextW(g_hWnd, title);
	if (1) {
		// クライアント領域のサイズが cw * ch になるように調整する
		BOOL has_menu = GetMenu(g_hWnd) != NULL;
		LONG_PTR style = GetWindowLongPtrW(g_hWnd, GWL_STYLE);
		LONG_PTR exstyle = GetWindowLongPtrW(g_hWnd, GWL_EXSTYLE);
		RECT rect = {0, 0, cw, ch};
		AdjustWindowRectEx(&rect, (DWORD)style, has_menu, (DWORD)exstyle); // AdjustWindowRectEx では Window style を 32 ビットのまま扱っている
		int ww = rect.right - rect.left;
		int hh = rect.bottom - rect.top;
		SetWindowPos(g_hWnd, NULL, 0, 0, ww, hh, SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOMOVE);
	}
	{
		// この時点でたまっているメッセージを処理。WM_CREATE や上記ウィンドウ初期化時に設定したウィンドウサイズによる WM_SIZED など
		// 特に WM_SIZED はここで処理しておかないと D3D デバイスを作ってからリサイズ処理が発生してデバイスリセットするので二度手間になってしまう
		MSG msg;
		while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	}

	// Direct3D
	g_D3D9 = Direct3DCreate9(D3D_SDK_VERSION);
	if (g_D3D9 == NULL) goto END;
	ZeroMemory(&g_D3D9_PP, sizeof(g_D3D9_PP));
	g_D3D9_PP.SwapEffect = D3DSWAPEFFECT_DISCARD;
	g_D3D9_PP.Windowed = TRUE;
	g_D3D9_PP.EnableAutoDepthStencil = TRUE;
	g_D3D9_PP.AutoDepthStencilFormat = D3DFMT_D24S8;
	g_D3D9_PP.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
	g_D3D9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, g_hWnd, 
		D3DCREATE_HARDWARE_VERTEXPROCESSING|D3DCREATE_MULTITHREADED, &g_D3D9_PP, &g_D3D9_Dev);
	if (g_D3D9_Dev == NULL) goto END;
	g_D3D9_Dev->CreateStateBlock(D3DSBT_ALL, &g_D3D9_Block);

	// ImGui
	#ifdef USE_IMGUI
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui_ImplWin32_Init(g_hWnd);
		ImGui_ImplDX9_Init(g_D3D9_Dev);
		ImGuiIO &io = ImGui::GetIO();
		io.IniFilename = ""; // ImGui 独自の ini ファイルを抑制
		io.LogFilename = ""; // ImGui 独自の log ファイルを抑制
		ImFontConfig conf;
		conf.FontNo = g_ImFontTTC;
		if (0) {
			io.Fonts->AddFontFromFileTTF(g_ImFontFile, g_ImFontSize, &conf, io.Fonts->GetGlyphRangesJapanese());
		} else {
			if (g_ImKanji.empty()) {
				// ImGui 側で自動設定された日本語文字リストを追加する
				const ImWchar *jp_chars = ImGui::GetIO().Fonts->GetGlyphRangesJapanese();
				for (int i=0; jp_chars[i]; i++) {
					ImWchar imwc = (ImWchar)(jp_chars[i] & 0xFFFF);
					g_ImKanji.push_back(imwc);
				}
				// ImGui のリストに無い文字を追加する（2010年に常用漢字に追加された196文字）
				for (int i=0; g_ImExtraChars[i]; i++) {
					// 2回追加することに注意
					ImWchar imwc = (ImWchar)(g_ImExtraChars[i] & 0xFFFF);
					g_ImKanji.push_back(imwc);
					g_ImKanji.push_back(imwc);
				}
				g_ImKanji.push_back(0); // sentinel
			}
			io.Fonts->AddFontFromFileTTF(g_ImFontFile, g_ImFontSize, &conf, g_ImKanji.data());
		}
	}
	#endif
	
	onStart();
	while (!g_ShouldExit) {
		// ウィンドウメッセージを処理
		MSG msg;
		if (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) break;
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}

		// 特殊イベント
		if (!g_DropFileNameW.empty()) {
			onEvent(E_DROPFILE, (void *)g_DropFileNameW.c_str());
			g_DropFileNameW.clear();
		}

		// 更新
		onStep();

		// 描画開始
		g_D3D9_Dev->BeginScene();
		g_D3D9_Dev->Clear(0, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL, 0, 1.0f, 0);

		// メイン画面
		if (1) {
			D3DVIEWPORT9 vp = {0, 0, g_D3D9_PP.BackBufferWidth, g_D3D9_PP.BackBufferHeight, 0.0f, 1.0f};
			D3DMATRIX proj;
			_MakeOrthoMatrix(&proj, (float)vp.Width, (float)vp.Height, -1000, 1000);
			g_D3D9_Dev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
			g_D3D9_Dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
			g_D3D9_Dev->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
			g_D3D9_Dev->SetRenderState(D3DRS_LIGHTING, FALSE);
			g_D3D9_Dev->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
			g_D3D9_Dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
			g_D3D9_Dev->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
			g_D3D9_Dev->SetRenderState(D3DRS_ALPHAREF, 8); 
			g_D3D9_Dev->SetRenderState(D3DRS_BLENDOP,  D3DBLENDOP_ADD);
			g_D3D9_Dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
			g_D3D9_Dev->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
			g_D3D9_Dev->SetRenderState(D3DRS_SPECULARENABLE, FALSE); 
			g_D3D9_Dev->SetTexture(0, NULL);
			g_D3D9_Dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
			g_D3D9_Dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
			g_D3D9_Dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
			g_D3D9_Dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
			g_D3D9_Dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
			g_D3D9_Dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
			g_D3D9_Dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
			g_D3D9_Dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
			g_D3D9_Dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
			g_D3D9_Dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
			g_D3D9_Dev->SetViewport(&vp); // ビューポートを画面いっぱいにする
			g_D3D9_Dev->SetTransform(D3DTS_PROJECTION, &proj); // 全画面が映るように設定
			onDraw();
		}

		// GUI画面
		#ifdef USE_IMGUI
		if (ImGui::GetCurrentContext()) {
			ImGui_ImplDX9_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();
			onGUI();
			ImGui::EndFrame();
			g_D3D9_Block->Capture();
			g_D3D9_Dev->SetRenderState(D3DRS_ZENABLE, FALSE);
			g_D3D9_Dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
			g_D3D9_Dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
			ImGui::Render();
			ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
			g_D3D9_Block->Apply();
		}
		#endif

		// 描画終了
		g_D3D9_Dev->EndScene();
		if (g_D3D9_Dev->Present(NULL, NULL, NULL, NULL) == D3DERR_DEVICELOST) {
			if (g_D3D9_Dev->TestCooperativeLevel() == D3DERR_DEVICENOTRESET) {
				g_ShouldResetDevice = true;
			}
		}

		// デバイスリセット
		if (g_ShouldResetDevice) {
			onEvent(E_DEVICE_LOST, NULL);
			#ifdef USE_IMGUI
			ImGui_ImplDX9_InvalidateDeviceObjects();
			#endif
			g_D3D9_Block->Release();
			g_D3D9_Block = NULL;
			HRESULT hr = g_D3D9_Dev->Reset(&g_D3D9_PP);
			if (FAILED(hr)) {
				// 解放忘れのオブジェクトがある可能性。たとえば KVideo と連携しているのに KVideo::resetDevice 呼んでないとか
				OutputDebugStringW(L"デバイスをリセットできません"); 
				break;
			}
			g_D3D9_Dev->CreateStateBlock(D3DSBT_ALL, &g_D3D9_Block);
			#ifdef USE_IMGUI
			ImGui_ImplDX9_CreateDeviceObjects();
			#endif
			g_ShouldResetDevice = false;
			onEvent(E_DEVICE_RESET, NULL);
		}
	}
	onEnd();
END:
	#ifdef USE_IMGUI
	{
		ImGui_ImplDX9_Shutdown();
		ImGui_ImplWin32_Shutdown();
		if (ImGui::GetCurrentContext()) {
			ImGui::DestroyContext();
		}
	}
	#endif
	if (g_D3D9_Dev) {
		g_D3D9_Dev->Release();
		g_D3D9_Dev = NULL;
	}
	if (g_D3D9) {
		g_D3D9->Release();
		g_D3D9 = NULL;
	}
	ZeroMemory(&g_D3D9_PP, sizeof(g_D3D9_PP));
	if (g_hWnd) {
		DestroyWindow(g_hWnd);
		g_hWnd = NULL;
	}
	UnregisterClassW(g_WC.lpszClassName, g_WC.hInstance);
	ZeroMemory(&g_WC, sizeof(g_WC));
}
void * CApp::getValuePtr(Type t) {
	switch (t) {
	case T_D3D9: return g_D3D9;
	case T_D3DDEV9: return g_D3D9_Dev;
	case T_HWND: return g_hWnd;
	}
	return NULL;
}
int CApp::getValueInt(Type t) {
	switch (t) {
	case T_SIZE_W: return g_D3D9_PP.BackBufferWidth;
	case T_SIZE_H: return g_D3D9_PP.BackBufferHeight;
	}
	return 0;
}
