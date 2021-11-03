#include "KDataGrid.h"
#include "KInternal.h"

namespace Kamilo {


// 0 起算の列・行番号を "A10" や "C7" などのセル位置文字列に変換する
std::string KDataGrid::encodeCellCoord(int col, int row) {
	if (col < 0) return "";
	if (row < 0) return "";
	if (col < COL_ALPHABETS) { // A～Z
		char c = (char)('A' + col);
		char s[256];
		sprintf_s(s, sizeof(s), "%c%d", c, 1+row);
		return s;
	}
	if (col < COL_ALPHABETS*COL_ALPHABETS) { // AA～ZZ
		int off = col - COL_ALPHABETS; // AA=0 としたときのカラム番号
		char c1 = (char)('A' + (off / COL_ALPHABETS));
		char c2 = (char)('A' + (off % COL_ALPHABETS));
		char s[256];
		K__ASSERT(isalpha(c1));
		K__ASSERT(isalpha(c2));
		sprintf_s(s, sizeof(s), "%c%c%d", c1, c2, 1+row);
		return s;
	}
	return "";
}

// "A2" や "AM244" などのセル番号を 0 起算の行と列の値にデコードする
bool KDataGrid::decodeCellCoord(const std::string &s, int *col, int *row) {
	if (s.empty()) return false;
	int c = -1;
	int r = -1;

	if (isalpha(s[0]) && isdigit(s[1])) {
		// セル番号が [A-Z][0-9].* にマッチしている。
		// 例えば "A1" や "Z42" など。
		c = toupper(s[0]) - 'A';
		K__ASSERT(0 <= c && c < COL_ALPHABETS);
		r = strtol(s.c_str() + 1, nullptr, 0);
		r--; // １起算 --> 0起算

	} else if (isalpha(s[0]) && isalpha(s[1]) && isdigit(s[2])) {
		// セル番号が [A-Z][A-Z][0-9].* にマッチしている。
		// 例えば "AA42" や "KZ1217" など
		int idx1 = toupper(s[0]) - 'A';
		int idx2 = toupper(s[1]) - 'A';
		K__ASSERT(0 <= idx1 && idx1 < COL_ALPHABETS);
		K__ASSERT(0 <= idx2 && idx2 < COL_ALPHABETS);
		// AA は A～Z の次の要素(26番目)なので、26からカウントする
		c = COL_ALPHABETS + idx1 * COL_ALPHABETS + idx2;
		r = strtol(s.c_str() + 2, nullptr, 0);
		r--; // １起算 --> 0起算
	}
	if (c >= 0 && r >= 0) {
		// ここで、セル範囲として 0xFFFFF が入る可能性に注意。(LibreOffice Calc で現象を確認）
		// 0xFFFFF 以上の値が入っていたら範囲取得失敗とみなす
		if (r >= 0xFFFFF) {
			return false;
		}
		K__ASSERT(0 <= c && c < COL_LIMIT);
		K__ASSERT(0 <= r && r < ROW_LIMIT);
		if (col) *col = c;
		if (row) *row = r;
		return true;
	}

	return false;
}




#pragma region KDataGrid
KDataGrid::KDataGrid() {
	clear();
}
bool KDataGrid::empty() const {
	return m_RowLines.empty();
}
void KDataGrid::clear() {
	m_Col0 = m_Col1 = -1;
	m_Row0 = m_Row1 = -1;
	m_RecalcDim = false;
	m_Name.clear();
	m_SourceLoc.clear();
	m_SourceCol = 0;
	m_SourceRow = 0;
	m_RowLines.clear();
}
const std::string & KDataGrid::getName() const {
	return m_Name;
}
void KDataGrid::setName(const std::string &name) {
	m_Name = name;
}
const std::string & KDataGrid::getSourceLocation(int *col, int *row) const {
	if (col) *col = m_SourceCol;
	if (row) *row = m_SourceRow;
	return m_SourceLoc;
}
void KDataGrid::setSourceLocation(const std::string &loc, int col, int row) {
	m_SourceLoc = loc;
	m_SourceCol = col;
	m_SourceRow = row;
}
void KDataGrid::setCell(int col, int row, const std::string &s) {
	std::string ss = s;
	K::strTrim(ss);
	if (ss.empty() == false) {
		if (m_RowLines.size() <= row) {
			m_RowLines.resize(row + 1);
		}
		if (m_RowLines[row].size() <= col) {
			m_RowLines[row].resize(col + 1);
		}
		m_RowLines[row][col] = ss;
		m_RecalcDim = true;
	}
}
bool KDataGrid::getDimension(int *p_col, int *p_row, int *p_colcount, int *p_rowcount) const {
	if (m_RecalcDim) {
		m_RecalcDim = false;
		m_Col0 = m_Col1 = -1;
		m_Row0 = m_Row1 = -1;
		for (int r=0; r<m_RowLines.size(); r++) {
			if (m_RowLines[r].empty()) continue;
			m_Row0 = r;
			break;
		}
		m_Row1 = (int)m_RowLines.size() - 1;

		for (int r=m_Row0; r<=m_Row1; r++) {
			const auto &line = m_RowLines[r];
			for (int c=0; c<line.size(); c++) {
				if (line[c].empty()) continue;
				if (m_Col0 < 0 || c < m_Col0) {
					m_Col0 = c;
				}
				break;
			}
			for (int c=line.size()-1; c>=m_Col0; c--) {
				if (line[c].empty()) continue;
				if (m_Col1 < 0 || m_Col1 < c) {
					m_Col1 = c;
				}
				break;
			}
		}
	}
	if (empty()) return false;
	if (p_col) *p_col = m_Col0;
	if (p_row) *p_row = m_Row0;
	if (p_colcount) *p_colcount = m_Col1 - m_Col0 + 1;
	if (p_rowcount) *p_rowcount = m_Row1 - m_Row0 + 1;
	return true;
}
bool KDataGrid::getCell(int col, int row, std::string *p_val) const {
	if (row < m_RowLines.size()) {
		const auto &line = m_RowLines[row];
		if (col < line.size()) {
			const std::string &t = line[col];
			if (t.empty() == false) {
				if (p_val) *p_val = t;
				return true;
			}
		}
	}
	return false;
}
bool KDataGrid::findCell(const std::string &s, int *p_col, int *p_row) const {
	int col, row, ncol, nrow;
	if (getDimension(&col, &row, &ncol, &nrow)) {
		for (int r=row; r<row+nrow; r++) {
			for (int c=col; c<col+ncol; c++) {
				std::string t;
				if (getCell(c, r, &t)) {
					if (t == s) {
						if (p_col) *p_col = c;
						if (p_row) *p_row = r;
						return true;
					}
				}
			}
		}
	}
	return false;
}
int KDataGrid::findCellInRow(int row, const std::string &s, int col_start) const {
	int col, ncol;
	if (getDimension(&col, nullptr, &ncol, nullptr)) {
		int col0 = K::max(col, col_start);
		int col1 = col + ncol;
		for (int c=col0; c<col1; c++) {
			std::string t;
			if (getCell(c, row, &t)) {
				if (t == s) {
					return c;
				}
			}
		}
	}
	return -1;
}
int KDataGrid::findCellInCol(int col, const std::string &s, int row_start) const {
	int row, nrow;
	if (getDimension(nullptr, &row, nullptr, &nrow)) {
		int row0 = K::max(row, row_start);
		int row1 = row + nrow;
		for (int r=row0; r<row1; r++) {
			std::string t;
			if (getCell(col, r, &t)) {
				if (t == s) {
					return r;
				}
			}
		}
	}
	return -1;
}
void KDataGrid::scanCells(KDataGridCallback *cb) const {
	int col, row, ncol, nrow;
	if (getDimension(&col, &row, &ncol, &nrow)) {
		for (int r=row; r<row+nrow; r++) {
			for (int c=col; c<col+ncol; c++) {
				std::string t;
				if (getCell(c, r, &t)) {
					cb->onCell(c, r, t);
				}
			}
		}
	}
}
bool KDataGrid::getCellInt(int col, int row, int *p_val) const {
	std::string s;
	if (getCell(col, row, &s)) {
		if (K::strToInt(s, p_val)) {
			return true;
		}
	}
	return false;
}
bool KDataGrid::getCellFloat(int col, int row, float *p_val) const {
	std::string s;
	if (getCell(col, row, &s)) {
		if (K::strToFloat(s, p_val)) {
			return true;
		}
	}
	return false;
}
KDataGrid KDataGrid::copy(int col, int row, int colcount, int rowcount) const {
	KDataGrid subgrid;
	subgrid.setSourceLocation(getSourceLocation(), m_SourceCol + col, m_SourceRow + row);
	subgrid.setName(getName());
	for (int r=0; r<rowcount; r++) {
		for (int c=0; c<colcount; c++) {
			std::string s;
			if (getCell(col+c, row+r, &s)) {
				subgrid.setCell(c, r, s);
			}
		}
	}
	return subgrid;
}
#pragma endregion // KDataGrid


} // Kamilo
