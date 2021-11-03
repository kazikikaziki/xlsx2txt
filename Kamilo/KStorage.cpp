#include "KStorage.h"

#include <vector>
#include "KDirectoryWalker.h"
#include "KEmbeddedFiles.h"
#include "KInternal.h"
#include "KPac.h"
#include "KZip.h"


// 大小文字の指定ミスを検出
#ifndef K_CASE_CHECK
#	define K_CASE_CHECK 1
#endif

const int PAC_CASE_CEHCK = 1;


namespace Kamilo {




#pragma region CWin32ResourceArchive
class CWin32ResourceArchive: public KArchive {
public:
	virtual bool contains(const std::string &name) override {
		return KEmbeddedFiles::contains(name);
	}
	virtual KInputStream createFileReader(const std::string &name) override {
		return KEmbeddedFiles::createInputStream(name);
	}
	virtual int getFileCount() override {
		return KEmbeddedFiles::count();
	}
	virtual const char * getFileName(int index) override {
		return KEmbeddedFiles::name(index);
	}
};
KArchive * KArchive::createEmbeddedReader() {
	return new CWin32ResourceArchive();
}
#pragma endregion // CWin32ResourceArchive


#pragma region CFolderArchive
class CFolderArchive: public KArchive, public KDirectoryWalker::Callback {
	std::string m_Dir;
	std::vector<std::string> m_Names;
public:
	CFolderArchive(const std::string &dir, int *err) {
		m_Dir = dir;
		if (!K::pathIsDir(dir)) {
			K__ERROR("CFolderArchive: Directory not exists: '%s'", dir.c_str());
			*err = 1;
		} else {
			*err = 0;
		}
	}

	virtual void onFile(const std::string &name_u8, const std::string &parent_u8) override {
		// KDirectoryWalker::walk でファイル（ディレクトリではない）を列挙するたびに呼ばれる。ファイルリストに登録する
		std::string path = K::pathJoin(parent_u8, name_u8);
		m_Names.push_back(path);
	}

	// 大小文字だけが異なる同名ファイルがあった時に警告する
	void check_filename_case(const std::string &name) {
		if (m_Names.empty()) {
			KDirectoryWalker::walk(m_Dir.c_str(), this);
		}
		for (auto it=m_Names.begin(); it!=m_Names.end(); ++it) {
			if (K::pathCompare(*it, name, true, false) == 0) {
				// [大小文字区別なし]で比較した
				// 念のために。[大小文字区別あり]でも比較する
				if (K::pathCompare(*it, name, false, false) != 0) {
					K::print(
						u8"W_FILEANME_CASE: ファイル名 '%s' が指定されましたが、実際のファイル名は '%s' です。大小文字だけが異なる同名ファイルは"
						u8"アーカイブ化したときに正しくロードできない可能性があります。必ず大小文字も一致させてください", name, it->c_str()
					);
				}
			}
		}
	}

	// ファイルが存在する？
	virtual bool contains(const std::string &name) override {
		// 大小文字が異なるだけの同名ファイルをチェック
		if (K_CASE_CHECK) {
			check_filename_case(name);
		}

		// 実際のファイル名を得る
		std::string realname = K::pathJoin(m_Dir, name);

		// ファイルの存在を確認
		if (!K::pathExists(realname)) {
			return false;
		}

		// ディレクトリ名でないことを確認
		if (K::pathIsDir(realname)) {
			return false;
		}

		// ファイルとして存在する
		return true;
	}
	virtual KInputStream createFileReader(const std::string &name) override {
		// 大小文字が異なるだけの同名ファイルをチェック
		if (K_CASE_CHECK) {
			check_filename_case(name);
		}

		// 実際のファイル名を得る
		std::string realname = K::pathJoin(m_Dir, name);

		// KInputStream を取得
		KInputStream file;
		if (file.openFileName(realname)) {
			K::outputDebugStringFmt("%s(%d): %s ==> OK", __FILE__, __LINE__, realname.c_str());
			return file;
		}
		return KInputStream();
	}
	virtual int getFileCount() override {
		return m_Names.size();
	}
	virtual const char * getFileName(int index) override {
		return m_Names[index].c_str();
	}
};
KArchive * KArchive::createFolderReader(const std::string &dir) {
	int err = 0;
	CFolderArchive *ar = new CFolderArchive(dir, &err);
	if (err == 0) {
		return ar;
	}
	ar->drop();
	return nullptr;
}
#pragma endregion // CFolderArchive



#pragma region CZipArchive
class CZipArchive: public KArchive {
	KUnzipper m_Unzipper;
	std::string m_Password;
	std::string m_TmpString;
public:
	CZipArchive(KInputStream &input, const std::string &password, int *err) {
		m_Unzipper.open(input);
		if (!m_Unzipper.isOpen()) {
			*err = 1;
			return;
		}
		m_Password = password;
	}
	virtual bool contains(const std::string &filename) override {
		KInputStream r = createFileReader(filename);
		return r.isOpen();
	}
	virtual KInputStream createFileReader(const std::string &filename) override {
		KInputStream file;
		for (int i=0; i<m_Unzipper.getEntryCount(); i++) {
			const char *s = getFileName(i);
			if (K::pathCompare(s, filename, false, false) == 0) {
				std::string bin;
				m_Unzipper.getEntryData(i, m_Password.c_str(), &bin);
				file.openMemoryCopy(bin.data(), bin.size());
			}
		}
		return file;
	}
	virtual int getFileCount() override {
		return m_Unzipper.getEntryCount();
	}
	virtual const char * getFileName(int index) override {
		std::string rawname;
		m_Unzipper.getEntryName(index, &rawname);
		if (m_Unzipper.getEntryParamInt(index, KUnzipper::WITH_UTF8)) {
			// zip内のファイル名が utf8 で記録されている。変換しなくてよい
			m_TmpString = rawname;
		} else {
			// zip内のファイル名が utf8 以外で記録されている
			m_TmpString = K::strAnsiToUtf8(rawname, "");
		}
		return m_TmpString.c_str();
	}
};

KArchive * KArchive::createZipReader(const std::string &zip, const std::string &password) {
	KArchive *ar = nullptr;
	KInputStream file;
	if (file.openFileName(zip)) {
		int err = 0;
		ar = new CZipArchive(file, password, &err);
		if (err) {
			ar->drop();
			ar = nullptr;
		}
	}
	return ar;
}
#pragma endregion // CZipArchive


#pragma region CPacFile
class CPacFile: public KArchive {
	KPacFileReader m_PacReader;
	std::string m_TmpString;
public:
	explicit CPacFile(KPacFileReader &reader) {
		m_PacReader = reader;
	}

	// 大小文字だけが異なる同名ファイルがあった時に警告する
	void check_filename_case(const std::string &name) {
		if (m_PacReader.getIndexByName(name, false, false) < 0) {
			int i = m_PacReader.getIndexByName(name, true, false);
			if (i >= 0) {
				std::string s = m_PacReader.getName(i);
				K::print(
					u8"W_PAC_FILEANME_CASE: ファイル名 '%s' の代わりに '%s' が存在します。大小文字を間違えていませんか？", name.c_str(), s.c_str());
			}
		}
	}

	virtual bool contains(const std::string &name) override {
		if (PAC_CASE_CEHCK) {
			check_filename_case(name);
		}
		return m_PacReader.getIndexByName(name, false, false) >= 0;
	}
	virtual KInputStream createFileReader(const std::string &name) override {
		KInputStream file;
		if (PAC_CASE_CEHCK) {
			check_filename_case(name);
		}
		int index = m_PacReader.getIndexByName(name, false, false);
		if (index >= 0) {
			std::string bin = m_PacReader.getData(index);
			file.openMemoryCopy(bin.data(), bin.size());
		}
		return file;
	}
	virtual int getFileCount() override {
		return m_PacReader.getCount();
	}
	virtual const char * getFileName(int index) override {
		m_TmpString = m_PacReader.getName(index);
		return m_TmpString.c_str();
	}
};
KArchive * KArchive::createPacReader(const std::string &filename) {
	KArchive *archive = nullptr;
	KInputStream file;
	file.openFileName(filename);
	
	KPacFileReader reader = KPacFileReader::fromStream(file);
	if (reader.isOpen()) {
		archive = new CPacFile(reader);
	} else {
		K__ERROR("E_FILE_FAIL: Failed to open a pac file: '%s'", filename.c_str());
	}
	return archive;
}
#pragma endregion // CPacFile


// リソースに埋め込まれた zip ファイルからロード
KArchive * KArchive::createEmbeddedZipReader(const std::string &filename, const std::string &password) {
	// リソースとして埋め込まれた zip ファイルを得る
	KInputStream file = KEmbeddedFiles::createInputStream(filename);

	// KArchive インターフェースに適合させる
	KArchive *ar = nullptr;
	if (file.isOpen()) {
		int err = 0;
		ar = new CZipArchive(file, password, &err);
		if (err) {
			ar->drop();
			ar = nullptr;
		}
	}
	return ar;
}

// リソースに埋め込まれた pac ファイルからロード
KArchive * KArchive::createEmbeddedPacReader(const std::string &filename) {
	CPacFile *ar = nullptr;

	// リソースとして埋め込まれた pac ファイルを得る
	KInputStream file = KEmbeddedFiles::createInputStream(filename); // リソースとして埋め込まれた pac ファイル

	// その file を使って pac ファイルリーダーを作る
	KPacFileReader reader = KPacFileReader::fromStream(file);

	// KArchive インターフェースに適合させる
	if (reader.isOpen()) {
		ar = new CPacFile(reader);
	}
	return ar;
}







class CStorage: public KStorage {
	std::vector<KArchive *> m_Archives;
public:
	CStorage() {
	}
	virtual ~CStorage() {
		clear();
	}
	virtual void clear() override {
		for (size_t i=0; i<m_Archives.size(); i++) {
			m_Archives[i]->drop();
		}
		m_Archives.clear();
	}
	virtual bool empty() const override {
		return getLoaderCount() == 0;
	}
	virtual void addArchive(KArchive *ar) override {
		if (ar) {
			ar->grab();
			m_Archives.push_back(ar);
		}
	}
	virtual bool addFolder(const std::string &dir) override {
		KArchive *ar = KArchive::createFolderReader(dir.c_str());
		if (ar) {
			addArchive(ar);
			ar->drop();
			K::print(u8"検索パスにディレクトリ %s を追加しました", dir.c_str());
			return true;
		} else {
			K__WARNING("E_FILE_FAIL: Failed to open a dir: '%s'", dir.c_str());
			return false;
		}
	}
	virtual bool addZipFile(const std::string &filename, const std::string &password) override {
		KArchive *ar = KArchive::createZipReader(filename.c_str(), password.c_str());
		if (ar) {
			addArchive(ar);
			ar->drop();
			K::print(u8"検索パスにZIPファイル %s を追加しました", filename.c_str());
			return true;
		} else {
			K__WARNING("E_FILE_FAIL: Failed to open a zip file: '%s'", filename.c_str());
			return false;
		}
	}
	virtual bool addPacFile(const std::string &filename) override {
		KArchive *ar = KArchive::createPacReader(filename.c_str());
		if (ar) {
			addArchive(ar);
			ar->drop();
			K::print(u8"検索パスにPACファイル %s を追加しました", filename.c_str());
			return true;
		} else {
			K__WARNING("E_FILE_FAIL: Failed to open a pac file: '%s'", filename.c_str());
			return false;
		}
	}
	virtual bool addEmbeddedFiles() override {
		KArchive *ar = KArchive::createEmbeddedReader();
		if (ar) {
			addArchive(ar);
			ar->drop();
			K::print(u8"検索パスに埋め込みリソースを追加しました");
			return true;
		} else {
			K__WARNING("E_FILE_FAIL: Failed to open a embedded files");
			return false;
		}
	}
	virtual bool addEmbeddedPacFileLoader(const std::string &filename) override {
		KArchive *ar = KArchive::createEmbeddedPacReader(filename.c_str());
		if (ar) {
			addArchive(ar);
			ar->drop();
			K::print(u8"検索パスに埋め込み pac ファイルを追加しました: %s", filename.c_str());
			return true;
		} else {
			K__WARNING("E_FILE_FAIL: Failed to open a embedded pac file: '%s'", filename.c_str());
			return false;
		}
	}
	virtual KInputStream getInputStream(const std::string &filename, bool should_exists) const override {
		if (filename.empty()) {
			K__ERROR("Empty filename");
			return KInputStream();
		}

		// 絶対パスで指定されている場合は普通のファイルとして開く
		if (!K::pathIsRelative(filename)) {
			KInputStream file;
			file.openFileName(filename);
			return file;
		}

		if (m_Archives.empty()) {
			// ローダーが一つも設定されていない。
			// 一番基本的な方法で開く
			KInputStream file;
			if (file.openFileName(filename)) {
				return file;
			}
		} else {
			// ローダーを順番に試す
			for (size_t i=0; i<m_Archives.size(); i++) {
				KArchive *ar = m_Archives[i];
				KInputStream file = ar->createFileReader(filename);
				if (file.isOpen()) {
					return file;
				}
			}
		}
		if (should_exists) {
			K__ERROR("Failed to open file: %s", filename.c_str());
		}
		return KInputStream();
	}
	virtual bool contains(const std::string &filename) const override {
		KInputStream file = getInputStream(filename, false);
		return file.isOpen();
	}
	virtual std::string loadBinary(const std::string &filename, bool should_exists) const override {
		KInputStream file = getInputStream(filename, should_exists);
		return file.readBin();
	}
	virtual KArchive * getLoader(int index) override {
		return m_Archives[index];
	}
	virtual int getLoaderCount() const override {
		return (int)m_Archives.size();
	}
}; // CStorage


KStorage * createStorage() {
	return new CStorage();
}


} // namespace
