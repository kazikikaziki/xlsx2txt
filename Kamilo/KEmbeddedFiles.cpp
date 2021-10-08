#include "KEmbeddedFiles.h"
#include <Windows.h>
#include <vector>
#include "KInternal.h"

namespace Kamilo {

class CWin32ResourceFiles {
	static BOOL CALLBACK nameCallback(HMODULE module, LPCWSTR type, LPWSTR name, LONG_PTR user) {
		CWin32ResourceFiles *obj = reinterpret_cast<CWin32ResourceFiles *>(user);
		obj->onEnum(module, type, name);
		return TRUE;
	}
	static BOOL CALLBACK typeCallback(HMODULE module, LPWSTR type, LONG_PTR user) {
		EnumResourceNamesW(module, type, nameCallback, user);
		return TRUE;
	}
private:
	struct ITEM {
		char name_u8[MAX_PATH];
		wchar_t name_w[MAX_PATH];
		HMODULE hModule;
		HRSRC hRsrc;
		ITEM() {
			name_u8[0] = 0;
			name_w[0] = 0;
			hModule = nullptr;
			hRsrc = nullptr;
		}
	};
	void onEnum(HMODULE module, LPCWSTR type, LPWSTR name) {
		// https://msdn.microsoft.com/ja-jp/library/windows/desktop/ms648038(v=vs.85).aspx
		if (IS_INTRESOURCE(name)) return; // 文字列ではなくリソース番号を表している場合は無視
		if (IS_INTRESOURCE(type)) return; // 文字列ではなくリソース番号を表している場合は無視

		CWin32ResourceFiles::ITEM item;
		wcscpy_s(item.name_w, sizeof(item.name_w)/sizeof(wchar_t), name);
		wcscat_s(item.name_w, sizeof(item.name_w)/sizeof(wchar_t), L".");
		wcscat_s(item.name_w, sizeof(item.name_w)/sizeof(wchar_t), type);
		item.hModule = module;
		item.hRsrc = FindResourceW(module, name, type);
		K::strWideToUtf8(item.name_u8, sizeof(item.name_u8), item.name_w);
		K__ASSERT(item.hRsrc);
		m_Items.push_back(item);
	}

private:
	std::vector<ITEM> m_Items;

public:
	/// この実行ファイルに埋め込まれているリソースデータの一覧を得る
	void update() {
		m_Items.clear();
		EnumResourceTypesW(NULL, typeCallback, (LONG_PTR)this);
	}

	/// リソースデータの個数
	int getCount() const {
		return (int)m_Items.size();
	}

	/// リソース情報
	const ITEM * getItem(int index) const {
		return &m_Items[index];
	}

	/// リソースデータの先頭アドレスとサイズを取得する。
	/// ここで取得できるアポインタはアプリケーション終了時まで常に有効である
	const void * getData(int index, int *size) const {
		if (0 <= index && index < (int)m_Items.size()) {
			const ITEM &item = m_Items[index];
			HGLOBAL hGlobal = LoadResource(item.hModule, item.hRsrc);
			if (hGlobal) {
				if (size) *size = (int)SizeofResource(item.hModule, item.hRsrc);
				return LockResource(hGlobal); // アンロック不要
			}
		}
		return NULL;
	}

public:
	int getIndexOfName(const std::string &name) {
		if (m_Items.empty()) {
			update();
		}
		for (size_t i=0; i<m_Items.size(); i++) {
			if (K::pathCompare(m_Items[i].name_u8, name, true, true) == 0) {
				return (int)i;
			}
		}
		return -1; // not found
	}
	KInputStream createStreamByName(const std::string &name) {
		KInputStream file;
		int size = 0;
		const void *data = getData(getIndexOfName(name), &size);
		if (data) {
			file.openMemory(data, size);
		}
		return file;
	}
	const char * getNameByIndex(int index) {
		const ITEM *item = getItem(index);
		return item ? item->name_u8: NULL;
	}
};

static CWin32ResourceFiles g_ResFiles;

bool KEmbeddedFiles::contains(const std::string &name) {
	return g_ResFiles.getIndexOfName(name) >= 0;
}
KInputStream KEmbeddedFiles::createInputStream(const std::string &name) {
	return g_ResFiles.createStreamByName(name);
}
const char * KEmbeddedFiles::name(int index) {
	return g_ResFiles.getNameByIndex(index);
}
int KEmbeddedFiles::count() {
	return g_ResFiles.getCount();
}


} // namespace
