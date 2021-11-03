#pragma once
#include <string>
#include <unordered_map>

namespace Kamilo {

class KDataGridCallback {
public:
	/// セル巡回時に呼ばれる
	/// col 列番号（Excelの画面とは異なり、0起算であることに注意）
	/// row 行番号（Excelの画面とは異なり、0起算であることに注意）
	/// s セルの文字列(UTF8)
	virtual void onCell(int col, int row, const std::string &s) = 0;
};


class KDataGrid {
public:
	static constexpr int COL_ALPHABETS = 26; // A～Z
	static constexpr int COL_LIMIT     = COL_ALPHABETS + COL_ALPHABETS * COL_ALPHABETS; // A～Z + AA～ZZ
	static constexpr int ROW_LIMIT     = 1024 * 8;

	/// 行列インデックス（0起算）から "A1" や "AB12" などのセル名を得る
	/// 列の表記は A から ZZ まで対応する（A～Z, AA～ZZ)
	static std::string encodeCellCoord(int col, int row);

	/// "A1" や "AB12" などのセル名から、行列インデックス（0起算）を取得する
	/// 列の表記は A から ZZ まで対応する（A～Z, AA～ZZ)
	static bool decodeCellCoord(const std::string &s, int *col, int *row);

public:
	KDataGrid();
	bool empty() const;
	void clear();
	const std::string & getName() const;
	void setName(const std::string &name);
	const std::string & getSourceLocation(int *col=nullptr, int *row=nullptr) const;
	void setSourceLocation(const std::string &loc, int col, int row);
	bool getDimension(int *p_col, int *p_row, int *p_colcount, int *p_rowcount) const;
	bool getCell(int col, int row, std::string *p_val) const;
	void setCell(int col, int row, const std::string &s);
	bool findCell(const std::string &s, int *p_col, int *p_row) const;
	int  findCellInRow(int row, const std::string &s, int col_start=0) const;
	int  findCellInCol(int col, const std::string &s, int row_start=0) const;
	void scanCells(KDataGridCallback *cb) const;
	bool getCellInt(int col, int row, int *p_val) const;
	bool getCellFloat(int col, int row, float *p_val) const;
	KDataGrid copy(int col, int row, int colcount, int rowcount) const;


private:
	std::vector<std::vector<std::string>> m_RowLines; // [row][col] 行方向にイテレートしたいときに使う
	std::string m_Name;
	std::string m_SourceLoc;
	int m_SourceCol, m_SourceRow;
	mutable int m_Col0, m_Col1;
	mutable int m_Row0, m_Row1;
	mutable bool m_RecalcDim;
};

} // Kamilo
