#include "KTable.h"

#include <string>
#include <vector>
#include "KExcel.h"
#include "KInternal.h"

namespace Kamilo {

static bool _IsBeginCell(const std::string &s) {
	if (s.compare("@BEGIN") == 0) {
		return true;
	}
	if (K::strStartsWith(s, "@BEGIN_")) {
		return true;
	}
	return false;
}
static bool _IsEndCell(const std::string &s) {
	return s.compare("@END") == 0;
}

// テーブルの左上セル番号 (col, row) を指定し、そこを @begin としたときのテーブルサイズを得る
// 対応する @end が無いなど、テーブルとして認識できない場合は false を返す
static bool _GetTableRange(const KDataGrid &grid, int col, int row, int *p_colcount, int *p_rowcount) {
	int c0, r0, ccnt, rcnt;
	if (!grid.getDimension(&c0, &r0, &ccnt, &rcnt)) {
		return false; // 範囲が取得できない or サイズ0である
	}

	// 調べるべき範囲
	int end_col = c0 + ccnt;
	int end_row = r0 + rcnt;

	// @begin のセルから真下に向かって検索し、 @end が見つかればそこが終端
	int num_rows = 0;
	for (int r=row+1; r<end_row; r++) {
		std::string s;
		if (grid.getCell(col, r, &s)) {
			if (_IsBeginCell(s)) { // @end が見つかる前に別の @begin が見つかってしまった。
				return false; // このテーブルは正しくない
			}
			if (_IsEndCell(s)) {
				num_rows = r - row;
				break;
			}
		}
	}
	if (num_rows == 0) {
		return false; // @endが見つからない
	}

	// @begin のセルから右に向かって検索し、空白でないセルどこまで続くか調べる。
	// 連続する非空白セルの範囲が、テーブルの横幅である
	// （空白セルが見つかるまで、ではダメ。c = end_col-1 までみっちりテキストが入っている場合がある）
	int num_cols = 0;
	for (int c=col; c<end_col; c++) { // @begin を含めた列を数える
		if (grid.getCell(c, row, nullptr)) {
			num_cols++;
		} else {
			// 空白セルが見つかった。ここで表は終了
			// ※空白セルが見つからない場合に注意。その場合は
			// c = end_col-1 までみっちりテキストが入っている
			break;
		}
	}
	if (num_cols <= 1) {
		return false; // データ列が無い
	}

	if (p_colcount) *p_colcount = num_cols; // @begin, @end がある列を含む列数
	if (p_rowcount) *p_rowcount = num_rows; // カラムヘッダを含む行数
	K__ASSERT(num_cols > 0 && num_rows > 0);
	return true;
}



class KTable::Impl {
	KDataGrid m_Grid;
	KTableCallback *m_CB;
	int m_KeyCol;
	int m_SourceCol, m_SourceRow; // このテーブルを含む親テーブル内での位置（このテーブルの @begin に対応するセル位置）
public:
	Impl() {
		m_KeyCol = 0;
		m_CB = nullptr;
		m_SourceCol = 0;
		m_SourceRow = 0;
	}
	~Impl() {
		clear();
	}
	bool empty() const {
		return m_Grid.empty();
	}
	void clear() {
		m_Grid.clear();
		m_KeyCol = 0;
		m_CB = nullptr;
		m_SourceCol = 0;
		m_SourceRow = 0;
	}
	void setCallback(KTableCallback *cb) {
		m_CB = cb;
	}
	bool loadFromExcelFile(const KDataGrid &sheet, const std::string &top_cell_text) {
		int c, r;
		if (!sheet.findCell(top_cell_text, &c, &r)) {
			return false;
		}
		int csize, rsize;
		if (!_GetTableRange(sheet, c, r, &csize, &rsize)) {
			return false;
		}
		m_Grid = sheet.copy(c, r, csize, rsize);
		m_SourceCol = c;
		m_SourceRow = r;
		return true;
	}
	std::string getSourceLocation(int *p_col, int *p_row) const {
		if (p_col) *p_col = m_SourceCol;
		if (p_row) *p_row = m_SourceRow;
		return m_Grid.getSourceLocation() + "@" + m_Grid.getName();
	}
	bool getCellCoordInSource(int col, int row, int *col_in_file, int *row_in_file) const {
		if (col < 0) return false;
		if (row < 0) return false;
		if (col_in_file) *col_in_file = m_SourceCol + 1 + col;
		if (row_in_file) *row_in_file = m_SourceRow + 1 + row;
		return true;
	}
	std::string getColumnName(int col) const {
		std::string s;
		m_Grid.getCell(1+col, 0, &s); // 一番上の行の @begin 右側を起点とする
		return s;
	}
	int findColumnByName(const std::string &column_name) const {
		int r = m_Grid.findCellInRow(0, column_name, 1);
		if (r > 0) {
			return r - 1;
		}
		return -1;
	}
	int getDataColCount() const {
		int csize = 0;
		m_Grid.getDimension(nullptr, nullptr, &csize, nullptr);
		return csize-1; // 一番左の列(@BEGIN @END などがある）を除く
	}
	int getDataRowCount() const {
		int rsize = 0;
		m_Grid.getDimension(nullptr, nullptr, nullptr, &rsize);
		return rsize-1; // カラムヘッダを除く
	}

	// 検索キーを保持している列を設定する
	// @see queryDataStringByKey
	void setKeyColumn(int col) {
		m_KeyCol = col;
	}

	// row 行につけられたマーカー文字列を得る
	// 例えばコメントアウトを示す "//" など。
	std::string getRowMarker(int row) const {
		std::string s;
		m_Grid.getCell(0, 1+row, &s);
		return s;
	}

	// col 列 row 行の値を得る。セルに値が入っていない場合は false を返す
	// セル文字列が空白からのみ構成される場合（前後の空白を取り除くと空文字列になってしまう場合）も false を返す
	bool queryDataString(int col, int row, std::string *p_val) const {
		return m_Grid.getCell(1+col, 1+row, p_val);
	}
	bool queryDataInt(int col, int row, int *p_val) const {
		std::string s;
		if (!queryDataString(col, row, &s)) {
			return false;
		}

		// この文字列から整数を抜き出すための前処理
		if (m_CB) m_CB->onTableNumericText(s);

		// 実数形式で記述されている値を整数として取り出す可能性
		float f = 0.0f;
		if (K::strToFloat(s, &f)) {
			if (p_val) {
				int A = (int)f; // 単純な整数化
				int B = (int)(f + FLT_EPSILON); // 1 のつもりが 0.9999999999998 のように記録されている場合を考慮した整数化
				int C = (int)(f + 0.000001f); // もっと精度の荒い整数化
				K__ASSERT(A == B);
				K__ASSERT(B == C);
				*p_val = C;
			}
			return true;
		}
		// 8桁の16進数を取り出す可能性。この場合は符号なしで取り出しておかないといけない
		unsigned int u = 0;
		if (K::strToUInt32(s, &u)) {
			if (p_val) *p_val = (int)u;
			return true;
		}
		// 普通の整数として取り出す
		int i = 0;
		if (K::strToInt(s, &i)) {
			if (p_val) *p_val = i;
			return true;
		}
		return false;
	}
	bool queryDataFloat(int col, int row, float *p_val) const {
		std::string s;
		if (!queryDataString(col, row, &s)) {
			return false;
		}

		// この文字列から整数を抜き出すための前処理
		if (m_CB) m_CB->onTableNumericText(s);

		float f = 0.0f;
		if (K::strToFloat(s, &f)) {
			if (p_val) *p_val = f;
			return true;
		}
		return false;
	}

	// col 列から value に一致する行を探す
	int findRowInColumnByString(int col, const std::string &value) const {
		int numcols = getDataColCount();
		int numrows = getDataRowCount();
		if (col < 0 || numcols <= col) return -1;
		for (int i=0; i<numrows; i++) {
			std::string s;
			if (queryDataString(col, i, &s)) {
				K::strTrim(s);
				if (s.compare(value) == 0) {
					return i;
				}
			}
		}
		return -1;
	}
	int findRowInColumnByInt(int col, int value) const {
		int numcols = getDataColCount();
		int numrows = getDataRowCount();
		if (col < 0 || numcols <= col) return -1;
		for (int i=0; i<numrows; i++) {
			int n = 0;
			if (queryDataInt(col, i, &n)) {
				if (n == value) {
					return i;
				}
			}
		}
		return -1;
	}

	// キー列における値 value_in_keycol に対応する select_col 列の値を得る
	// @see setKeyColumn
	bool queryDataStringByKey(int select_col, const std::string &value_in_keycol, std::string *p_val) const {
		int r = findRowInColumnByString(m_KeyCol, value_in_keycol);
		return (r >= 0) && queryDataString(select_col, r, p_val);
	}
	bool queryDataIntByKey(int select_col, const std::string &value_in_keycol, int *p_val) const {
		int r = findRowInColumnByString(m_KeyCol, value_in_keycol);
		return (r >= 0) && queryDataInt(select_col, r, p_val);
	}
	bool queryDataFloatByKey(int select_col, const std::string &value_in_keycol, float *p_val) const {
		int r = findRowInColumnByString(m_KeyCol, value_in_keycol);
		return (r >= 0) && queryDataFloat(select_col, r, p_val);
	}

	// キー列内の値 value_in_keycol （整数）に対応する select_col 列の値を得る
	// ※Excel シートなどでは整数のつもりでも "1.00000001" のように記録されている場合があるので、
	// その場合は文字列で "1" を検索しても見つからない
	bool queryDataStringByIntKey(int select_col, int value_in_keycol, std::string *p_val) const {
		int r = findRowInColumnByInt(m_KeyCol, value_in_keycol);
		return (r >= 0) && queryDataString(select_col, r, p_val);
	}
	bool queryDataIntByIntKey(int select_col, int value_in_keycol, int *p_val) const {
		int r = findRowInColumnByInt(m_KeyCol, value_in_keycol);
		return (r >= 0) && queryDataInt(select_col, r, p_val);
	}
	bool queryDataFloatByIntKey(int select_col, int value_in_keycol, float *p_val) const {
		int r = findRowInColumnByInt(m_KeyCol, value_in_keycol);
		return (r >= 0) && queryDataFloat(select_col, r, p_val);
	}

};


#pragma region KTable
KTable::KTable() {
	m_impl = nullptr;
}
bool KTable::loadFromDataGrid(const KDataGrid &grid, const std::string &topleft, bool mute) {
	auto impl = std::make_shared<Impl>();
	std::string s = topleft.empty() ? "@BEGIN" : topleft;
	if (impl->loadFromExcelFile(grid, s)) {
		m_impl = impl;
		return true;
	}
	m_impl = nullptr;
	return false;
}
void KTable::setCallback(KTableCallback *cb) {
	if (m_impl) m_impl->setCallback(cb);
}
bool KTable::empty() const {
	return m_impl == nullptr || m_impl->empty();
}
void KTable::clear() {
	if (m_impl) m_impl->clear();
}
std::string KTable::getSourceLocation(int *p_col, int *p_row) const {
	return m_impl ? m_impl->getSourceLocation(p_col, p_row) : "";
}
int KTable::findColumnByName(const std::string &column_name) const {
	return m_impl ? m_impl->findColumnByName(column_name) : -1;
}
std::string KTable::getColumnName(int col) const {
	return m_impl ?  m_impl->getColumnName(col) : "";
}
int KTable::getDataColCount() const {
	return m_impl ? m_impl->getDataColCount() : 0;
}
int KTable::getDataRowCount() const {
	return m_impl ? m_impl->getDataRowCount() : 0;
}
std::string KTable::getRowMarker(int row) const {
	return m_impl ? m_impl->getRowMarker(row) : "";
}
bool KTable::queryDataString(int col, int row, std::string *p_val) const {
	return m_impl && m_impl->queryDataString(col, row, p_val);
}
bool KTable::queryDataInt(int col, int row, int *p_val) const {
	return m_impl && m_impl->queryDataInt(col, row, p_val);
}
bool KTable::queryDataFloat(int col, int row, float *p_val) const {
	return m_impl && m_impl->queryDataFloat(col, row, p_val);
}
bool KTable::getCellCoordInSource(int col, int row, int *col_in_file, int *row_in_file) const {
	return m_impl && m_impl->getCellCoordInSource(col, row, col_in_file, row_in_file);
}
int KTable::findRowInColumnByString(int col, const std::string &value) const {
	return m_impl ? m_impl->findRowInColumnByString(col, value) : -1;
}
int KTable::findRowInColumnByInt(int col, int value) const {
	return m_impl ? m_impl->findRowInColumnByInt(col, value) : -1;
}
void KTable::setKeyColumn(int col) {
	if (m_impl) m_impl->setKeyColumn(col);
}
bool KTable::queryDataStringByKey(int select_col, const std::string &value_in_keycol, std::string *p_val) const {
	return m_impl && m_impl->queryDataStringByKey(select_col, value_in_keycol, p_val);
}
bool KTable::queryDataIntByKey(int select_col, const std::string &value_in_keycol, int *p_val) const {
	return m_impl && m_impl->queryDataIntByKey(select_col, value_in_keycol, p_val);
}
bool KTable::queryDataFloatByKey(int select_col, const std::string &value_in_keycol, float *p_val) const {
	return m_impl && m_impl->queryDataFloatByKey(select_col, value_in_keycol, p_val);
}
bool KTable::queryDataStringByKey(int select_col, int value_in_keycol, std::string *p_val) const {
	return m_impl && m_impl->queryDataStringByIntKey(select_col, value_in_keycol, p_val);
}
bool KTable::queryDataIntByKey(int select_col, int value_in_keycol, int *p_val) const {
	return m_impl && m_impl->queryDataIntByIntKey(select_col, value_in_keycol, p_val);
}
bool KTable::queryDataFloatByKey(int select_col, int value_in_keycol, float *p_val) const {
	return m_impl && m_impl->queryDataFloatByIntKey(select_col, value_in_keycol, p_val);
}






#pragma endregion // KTable

} // namespace
