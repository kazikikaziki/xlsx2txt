#include "KDirectoryWalker.h"

#include <vector>
#include <Windows.h>
#include <Shlwapi.h>
#include "KInternal.h"

namespace Kamilo {

void KDirectoryWalker::scanFiles(const std::string &top_u8, const std::string &dir_u8, std::vector<Item> &list) {
	std::wstring wtop = K::strUtf8ToWide(top_u8);
	std::wstring wdir = K::strUtf8ToWide(dir_u8);
	scanFilesW(wtop.c_str(), wdir.c_str(), list);
}
void KDirectoryWalker::scanFilesW(const std::wstring &wtop, const std::wstring &wdir, std::vector<Item> &list) {
	// 検索パターンを作成
	wchar_t wpattern[MAX_PATH] = {0};
	{
		wchar_t wtmp[MAX_PATH] = {0};
		PathAppendW(wtmp, wtop.c_str());
		PathAppendW(wtmp, wdir.c_str());
		GetFullPathNameW(wtmp, MAX_PATH, wpattern, NULL);
		PathAppendW(wpattern, L"*");
	}
	// ここで wpattern は
	// "d:\\system\\*"
	// のような文字列になっている。ワイルドカードに注意！！
	WIN32_FIND_DATAW fdata;
	HANDLE hFind = FindFirstFileW(wpattern, &fdata);
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			if (fdata.cFileName[0] != L'.') {
				Item fitem;
				fitem.namew = fdata.cFileName;
				fitem.parentw = wdir;
				fitem.nameu = K::strWideToUtf8(fitem.namew);
				fitem.parentu = K::strWideToUtf8(fitem.parentw);
				fitem.isdir = (fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
				list.push_back(fitem);
			}
		} while (FindNextFileW(hFind, &fdata));
		FindClose(hFind);
	}
}
void KDirectoryWalker::scanW(const std::wstring &wtop, const std::wstring &wdir, Callback *cb) {
	K__ASSERT(cb);
	std::vector<Item> list;
	scanFilesW(wtop, wdir, list);
	for (auto it=list.begin(); it!=list.end(); ++it) {
		if (it->isdir) {
			bool enter = false;
			cb->onDir(it->nameu, it->parentu, &enter);
			if (enter) {
				wchar_t wsub[MAX_PATH] = {0};
				wcscpy_s(wsub, MAX_PATH, wdir.c_str());
				PathAppendW(wsub, it->namew.c_str());
				scanW(wtop, wsub, cb);
				cb->onDirExit(it->nameu, it->parentu);
			}
		} else {
			cb->onFile(it->nameu, it->parentu);
		}
	}
}
void KDirectoryWalker::walk(const std::string &dir_u8, KDirectoryWalker::Callback *cb) {
	K__ASSERT(cb);
	std::wstring wdir = K::strUtf8ToWide(dir_u8);
	cb->onStart();
	scanW(wdir, L"", cb);
	cb->onEnd();
}

} // namespace
