#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include "KDataGrid.h"

namespace Kamilo {

class KInputStream;
class KOutputStream;
class CCoreExcelReader; // internal class
class CCoreExcelReader2; // internal class


class KXlsxFile {
public:
	/// .XLSX ファイルをロードする
	static bool loadFromStream(KInputStream &file, const std::string &xlsx_name, std::vector<KDataGrid> &result);
	static bool loadFromFileName(const std::string &filename, std::vector<KDataGrid> &result);
	static bool loadFromMemory(const void *bin, size_t size, const std::string &name, std::vector<KDataGrid> &result);
};






class KExcelFile {
public:
	///行列インデックス（0起算）から "A1" や "AB12" などのセル名を得る
	static std::string encodeCellName(int col, int row);

	/// "A1" や "AB12" などのセル名から、行列インデックス（0起算）を取得する
	static bool decodeCellName(const std::string &s, int *col, int *row);

public:
	KExcelFile();

	bool empty() const;
	void clear();

	/// 元のファイル名を返す
	std::string getFileName() const;

	/// .XLSX ファイルをロードする
	bool loadFromStream(KInputStream &file, const std::string &xlsx_name);
	bool loadFromFileName(const std::string &name);
	bool loadFromMemory(const void *bin, size_t size, const std::string &name);

	/// シート数を返す
	int getSheetCount() const;

	/// シート名からシートを探し、見つかればゼロ起算のシート番号を返す。
	/// 見つからなければ -1 を返す
	int getSheetByName(const std::string &name) const;

	/// シート名を得る
	std::string getSheetName(int sheet) const;

	/// シート内で有効なセルの存在する範囲を得る。みつかれば true を返す
	/// sheet   : シート番号（ゼロ起算）
	/// col     : 左上の列番号（ゼロ起算）
	/// row     : 左上の行番号（ゼロ起算）
	/// colconut: 列数
	/// rowcount: 行数
	bool getSheetDimension(int sheet, int *col, int *row, int *colcount, int *rowcount) const;

	/// 指定されたセルの文字列を返す。文字列を含んでいない場合は "" を返す
	/// sheet: シート番号（ゼロ起算）
	/// col: 列番号（ゼロ起算）
	/// row: 行番号（ゼロ起算）
	/// retval: utf8文字列またはnullptr
	std::string getDataString(int sheet, int col, int row) const;

	/// 文字列を完全一致で検索する。みつかったらセルの行列番号を row, col にセットして true を返す
	/// 空文字列を検索することはできない。
	/// col: 見つかったセルの列番号（ゼロ起算）
	/// row: 見つかったセルの行番号（ゼロ起算）
	bool getCellByText(int sheet, const std::string &s, int *col, int *row) const;

	/// 全てのセルを巡回する
	/// sheet   : シート番号（ゼロ起算）
	/// cb      : セル巡回時に呼ばれるコールバックオブジェクト
	void scanCells(int sheet, KDataGridCallback *cb) const;
	
	/// セル文字列を XML 形式でエクスポートする
	std::string exportXmlString(bool with_header=true, bool with_comment=true);
	std::string exportText();

	const std::vector<KDataGrid> & getSheets() const;
	const KDataGrid & getSheet(int i) const;
	const KDataGrid * findSheet(const std::string &name) const;

private:
#if 0
	std::shared_ptr<CCoreExcelReader> m_Impl;
#else
	std::shared_ptr<CCoreExcelReader2> m_Impl;
#endif
};


namespace Test {
void Test_excel(const std::string &filename);
}


} // namespace
