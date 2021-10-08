#pragma once
#include <string>
#include <memory>

namespace Kamilo {

class KInputStream;
class KOutputStream;

/// セル巡回用のコールバック
class KExcelScanCellsCallback {
public:
	/// セル巡回時に呼ばれる
	/// col 列番号（Excelの画面とは異なり、0起算であることに注意）
	/// row 行番号（Excelの画面とは異なり、0起算であることに注意）
	/// s セルの文字列(UTF8)
	virtual void onCell(int col, int row, const char *s) = 0;
};

class CCoreExcelReader; // internal class

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
	const char * getDataString(int sheet, int col, int row) const;

	/// 文字列を完全一致で検索する。みつかったらセルの行列番号を row, col にセットして true を返す
	/// 空文字列を検索することはできない。
	/// col: 見つかったセルの列番号（ゼロ起算）
	/// row: 見つかったセルの行番号（ゼロ起算）
	bool getCellByText(int sheet, const std::string &s, int *col, int *row) const;

	/// 全てのセルを巡回する
	/// sheet   : シート番号（ゼロ起算）
	/// cb      : セル巡回時に呼ばれるコールバックオブジェクト
	void scanCells(int sheet, KExcelScanCellsCallback *cb) const;
	
	/// セル文字列を XML 形式でエクスポートする
	std::string exportXmlString(bool with_header=true, bool with_comment=true);
	std::string exportText();

private:
	std::shared_ptr<CCoreExcelReader> m_Impl;
};


namespace Test {
void Test_excel(const std::string &filename);
}


} // namespace
