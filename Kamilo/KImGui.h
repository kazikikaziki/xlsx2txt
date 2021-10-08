#pragma once
#include <inttypes.h>

// Dear ImGui
// - MIT lisence
// - https://github.com/ocornut/imgui
#include "imgui/imgui.h"

#include "KVideo.h"

namespace Kamilo {

struct KImGuiImageParams {
	KImGuiImageParams() {
		w = 0;
		h = 0;
		u0 = 0.0f;
		v0 = 0.0f;
		u1 = 1.0f;
		v1 = 1.0f;
		color = KColor(1, 1, 1, 1);
		border_color = KColor(0, 0, 0, 0);
		keep_aspect = true;
		linear_filter = false;
	}
	int w;
	int h;
	float u0;
	float v0;
	float u1;
	float v1;
	KColor color;
	KColor border_color;
	bool keep_aspect;
	bool linear_filter;
};

class KImGuiCombo {
	typedef std::pair<std::string, int> Pair;
	std::vector<Pair> mItems;
	std::vector<const char *> mPChars;
	int mUpdating;
public:
	KImGuiCombo();
	void begin();
	void end();
	void addItem(const char *s, int value=0);
	int indexOfText(const char *s) const;
	int indexOfValue(int value) const;
	const char * getText(int index) const;
	int getValue(int index) const;
	const char ** getItemData() const;
	int getCount() const;
	bool showGui(const char *label, std::string &value) const;
	bool showGui(const char *label, int *value) const;
};

/// @file
/// Dear ImGui のサポート関数群。
///
/// Dear ImGui ライブラリは既にソースツリーに同梱されているので、改めてダウンロード＆インストールする必要はない。
/// 利用可能な関数などについては imgui/imgui.h を直接参照すること。また、詳しい使い方などは imgui/imgui.cpp に書いてある。
namespace KImGui {
	static const ImVec4 COLOR_GRAYED_OUT = {0.5f, 0.5f, 0.5f, 1.0f}; // グレーアウト色
	static const ImVec4 COLOR_STRONG = {1.0f, 0.8f, 0.2f, 1.0f}; // 強調テキスト色
	static const ImVec4 COLOR_WARNING(1.0f, 0.8f, 0.2f, 1.0f); // 警告テキスト色
	static const ImVec4 COLOR_ERROR  (1.0f, 0.5f, 0.5f, 1.0f); // エラー色
	ImVec4 COLOR_DEFAULT();
	ImVec4 COLOR_DISABLED();

	bool ButtonRepeat(const char *label, ImVec2 size=ImVec2(0, 0));
	bool Combo(const std::string &label, int *current_item, const std::vector<std::string> &items, int height_in_items=-1);
	bool Combo(const std::string &label, const std::vector<std::string> &pathlist, const std::string &selected, int *out_selected_index);
	int  FontCount();
	void Image(KTEXID tex, KImGuiImageParams *p);
	void Image(KTEXID tex, const ImVec2& size, const ImVec2& uv0 = ImVec2(0,0), const ImVec2& uv1 = ImVec2(1,1), const ImVec4& tint_col = ImVec4(1,1,1,1), const ImVec4& border_col = ImVec4(0,0,0,0));
	void ImageExportButton(const char *label, KTEXID tex, const std::string &filename, bool alphamask);
	void HSpace(int size);
	void PushFont(int index);// index: an index of ImFont in ImGui::GetIO().Fonts->Fonts
	void PopFont();
	void StyleKK();
	void VSpace(int size=8);
	bool InitWithDX9(void *hWnd, void *d3ddev9);
	void Shutdown();
	bool IsActive();
	void DeviceLost();
	void DeviceReset();
	long WndProc(void *hWnd, uint32_t msg, uintptr_t wp, uintptr_t lp);
	void BeginRender();
	void EndRender();
	ImFont * AddFontFromMemoryTTF(const void *data, size_t size, float size_pixels);
	ImFont * AddFontFromFileTTF(const std::string &filename_u8, int ttc_index, float size_pixels);
	void SetFilterPoint(); // ImGui::Image のフィルターを POINT に変更
	void SetFilterLinear(); // ImGui::Image のフィルターを LINEAR に変更
	void PushTextColor(const ImVec4 &col);
	void PopTextColor();

	template <typename T> void * ID(T x) { return (void*)x; }

	void ShowExampleAppCustomNodeGraph(bool* opened);
} // namespace KImGui



} // namespace
