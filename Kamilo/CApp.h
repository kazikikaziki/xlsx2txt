#pragma once

class CApp {
public:
	CApp();
	void run(int cw, int ch, const wchar_t *title);
	void postExit();

	enum Type {
		T_NONE,
		T_D3D9,
		T_D3DDEV9,
		T_HWND,
		T_SIZE_W,
		T_SIZE_H,
	};
	void * getValuePtr(Type t);
	int getValueInt(Type t);

	// メインループ開始時に呼ばれる
	virtual void onStart() {
	//	void *hWnd = getValuePtr(T_HWND);
	//	void *d3d9 = getValuePtr(T_D3D9);
	//	void *d3ddev9 = getValuePtr(T_D3DDEV9);
	//	KVideo::init(hWnd, d3d9, d3ddev9);
	}

	// メインループ終了時に呼ばれる
	virtual void onEnd() {
	//	KVideo::shutdown();
	}

	// メインループ更新時に呼ばれる
	virtual void onStep() {}

	// ここで Direct3D を使う
	// 例) dev->Clear(0, NULL, D3DCLEAR_TARGET, 0xFF0000FF, 0, 0);
	virtual void onDraw() {}

	// ここで ImGui を使う
	// 例) ImGui::ShowDemoWindow();
	virtual void onGUI() {}

	// イベント発生時に呼ばれる
	enum Event {
		E_NONE,
		E_DEVICE_LOST,  // デバイスロストした。引数なし
		E_DEVICE_RESET, // デバイスリセットした。引数なし
		E_DROPFILE,     // ファイルがドロップされた。引数はファイル名 (wchar_t*)
	};
	virtual void onEvent(Event e, void *data) {
	//	switch (e) {
	//	case E_DEVICE_LOST: KVideo::resetDevice_lost(); break;
	//	case E_DEVICE_RESET: KVideo::resetDevice_reset(); break;
	//	}
	}
};
