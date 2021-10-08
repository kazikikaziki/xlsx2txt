#pragma once
#include <string>
#include <vector>

namespace Kamilo {

class KDirectoryWalker {
public:
	struct Item {
		std::wstring namew;
		std::wstring parentw;
		std::string nameu; // ファイル名部分 (utf8)
		std::string parentu; // 親ディレクトリ (utf8)
		bool isdir;
	};
	
	class Callback {
	public:
		/// ファイルを見つけた
		/// name_u8:   ファイル名（パスを含まない）
		/// parent_u8: 親ディレクトリ（検索開始フォルダからの相対パス）
		/// ※ディレクトリでないファイルの場合だけ呼ばれる。ディレクトリを見つけた場合は on_dir が呼ばれる
		virtual void onFile(const char *name_u8, const char *parent_u8) {}

		/// ディレクトリを見つけた
		/// enter: このディレクトリの中を再帰スキャンするなら true をセットする（何もしない場合のデフォルト値は false）
		///        trueをセットした場合はディレクトリ内をスキャンし、脱出する時に onDirExit が呼ばれる
		virtual void onDir(const char *name_u8, const char *parent_u8, bool *enter) {}

		/// ディレクトリから出る
		virtual void onDirExit(const char *name_u8, const char *parent_u8) {}
	};

	/// dir 直下にあるファイルおよびディレクトリを巡回する。
	/// ディレクトリまたはファイルを見つけるたびに cb が呼ばれる。
	/// KDirectoryWalker::Callback::onDir の実装によってサブフォルダを再帰スキャンするかどうかが決まる
	static void walk(const std::string &dir_u8, Callback *cb);

	// [current_dir]/wtop/wdir にあるファイルの一覧を得る。
	// wtop: 検索起点のパス（絶対パスまたは空文字列でも良い）
	// wdir: wtop 以下の検索パス（空文字列でも良い）
	// list: 得られたファイル名。このファイル名は wtop からの相対名になる
	// ※得られるファイル名は wtop 以下のパスである（wtop を含まない)
	//
	// 検索する場所 = [current_dir]/wtop/wdir
	// 得られるファイル名 = wdir/[filename]
	static void scanFilesW(const std::wstring &wtop, const std::wstring &wdir, std::vector<Item> &list);
	static void scanFiles(const std::string &top_u8, const std::string &dir_u8, std::vector<Item> &list);

	static void scanW(const std::wstring &wtop, const std::wstring &wdir, Callback *cb);
};

} // namespace
