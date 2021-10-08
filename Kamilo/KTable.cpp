#include "KTable.h"

#include <string>
#include <vector>
#include "KExcel.h"
#include "KInternal.h"

namespace Kamilo {

class KTable::Impl {
	KExcelFile m_excel;
	std::string m_sheetname;
	std::vector<std::string> m_colnames; // 列の名前
	int m_sheet;   // 選択中のテーブルを含んでいるシートのインデックス
	int m_leftcol;   // テーブルの左端の列インデックス
	int m_toprow;    // テーブルの開始マークのある行インデックス
	int m_bottomrow; // テーブルの終端マークのある行インデックス
public:
	Impl() {
		m_sheet = -1;
		m_leftcol = 0;
		m_toprow = 0;
		m_bottomrow = 0;
	}
	~Impl() {
		clear();
	}
	bool empty() const {
		return m_excel.empty();
	}
	bool _loadFromExcelFile(const KExcelFile &file) {
		m_excel = file;
		return !file.empty();
	}
	void clear() {
		m_excel.clear();
		m_sheet = -1;
		m_leftcol = 0;
		m_toprow = 0;
		m_bottomrow = 0;
		m_sheetname = "";
	}
	bool _selectTable(const std::string &sheet_name, const std::string &top_cell_text, const std::string &bottom_cell_text, bool mute) {
		m_sheet = -1;
		m_leftcol = 0;
		m_toprow = 0;
		m_bottomrow = 0;

		if (m_excel.empty()) {
			K__ERROR("E_EXCEL: Null data");
			return false;
		}

		int sheet = 0;
		int col0 = 0;
		int col1 = 0;
		int row0 = 0;
		int row1 = 0;

		// シートを探す
		sheet = m_excel.getSheetByName(sheet_name);
		if (sheet < 0) {
			if (!mute) K__ERROR(u8"E_EXCEL: シート '%s' が見つかりません", sheet_name.c_str());
			return false;
		}

		// 開始セルを探す
		if (! m_excel.getCellByText(sheet, top_cell_text, &col0, &row0)) {
			K__ERROR(u8"E_EXCEL_MISSING_TABLE_BEGIN: シート '%s' にはテーブル開始セル '%s' がありません", 
				sheet_name.c_str(), top_cell_text.c_str());
			return false;
		}
		K__VERBOSE("TOP CELL '%s' FOUND AT %s", top_cell_text.c_str(), KExcelFile::encodeCellName(col0, row0).c_str());

		// セルの定義範囲
		int dim_row_top = 0;
		int dim_row_cnt = 0;
		if (! m_excel.getSheetDimension(sheet, nullptr, &dim_row_top, nullptr, &dim_row_cnt)) {
			K__ERROR(u8"E_EXCEL_MISSING_SHEET_DIMENSION: シート '%s' のサイズが定義されていません", sheet_name.c_str());
			return false;
		}

		// 終了セルを探す
		// 終了セルは開始セルと同じ列で、開始セルよりも下の行にあるはず
		for (int i=row0+1; i<dim_row_top+dim_row_cnt; i++) {
			const char *s = m_excel.getDataString(sheet, col0, i);
			if (s && bottom_cell_text.compare(s) == 0) {
				row1 = i;
				break;
			}
		}
		if (row1 == 0) {
			K__ERROR(u8"E_EXCEL_MISSING_TABLE_END: シート '%s' のセル '%s' に対応する終端セル '%s' が見つかりません",
				sheet_name.c_str(), top_cell_text.c_str(), bottom_cell_text.c_str());
			return false;
		}
		{
			std::string s = KExcelFile::encodeCellName(col0, row0);
			K__VERBOSE("BOTTOM CELL '%s' FOUND AT %s", bottom_cell_text.c_str(), s.c_str());
		}
		// 開始セルの右隣からは、カラム名の定義が続く
		std::vector<std::string> cols;
		{
			int c = col0 + 1;
			while (1) {
				std::string cellstr = m_excel.getDataString(sheet, c, row0);
				if (cellstr.empty()) break;
				cols.push_back(cellstr);
				std::string s = KExcelFile::encodeCellName(col0, row0);
				K__VERBOSE("ID CELL '%s' FOUND AT %s", cellstr.c_str(), s.c_str());
				c++;
			}
			if (cols.empty()) {
				K__ERROR(u8"E_EXCEL_MISSING_COLUMN_HEADER: シート '%s' のテーブル '%s' にはカラムヘッダがありません", 
					sheet_name.c_str(), top_cell_text.c_str());
			}
			col1 = c;
		}

		// テーブル読み取りに成功した
		m_colnames = cols;
		m_sheet  = sheet;
		m_toprow    = row0;
		m_bottomrow = row1;
		m_leftcol   = col0;
		m_sheetname = sheet_name;
		return true;
	}
	std::string getColumnName(int col) const {
		if (m_excel.empty()) {
			K__ERROR("E_EXCEL: No table loaded");
			return "";
		}
		if (m_sheet < 0) {
			K__ERROR("E_EXCEL: No table selected");
			return "";
		}
		if (col < 0 || (int)m_colnames.size() <= col) {
			return "";
		}
		return m_colnames[col];
	}
	int getDataColIndexByName(const std::string &column_name) const {
		if (m_excel.empty()) {
			K__ERROR("E_EXCEL: No table loaded");
			return -1;
		}
		if (m_sheet < 0) {
			K__ERROR("E_EXCEL: No table selected");
			return -1;
		}
		for (size_t i=0; i<m_colnames.size(); i++) {
			if (m_colnames[i].compare(column_name) == 0) {
				return (int)i;
			}
		}
		return -1;
	}
	int getDataColCount() const {
		if (m_excel.empty()) {
			K__ERROR("E_EXCEL: No table loaded");
			return 0;
		}
		if (m_sheet < 0) {
			K__ERROR("E_EXCEL: No table selected");
			return 0;
		}
		return (int)m_colnames.size();
	}
	int getDataRowCount() const {
		if (m_excel.empty()) {
			K__ERROR("E_EXCEL: No table loaded");
			return 0;
		}
		if (m_sheet < 0) {
			K__ERROR("E_EXCEL: No table selected");
			return 0;
		}
		// 開始行と終了行の間にある行数
		int rows = m_bottomrow - m_toprow - 1;
		K__ASSERT(rows > 0);

		return rows;
	}
	const char * getRowMarker(int data_row) const {
		if (m_excel.empty()) {
			K__ERROR("E_EXCEL: No table loaded");
			return nullptr;
		}
		if (m_sheet < 0) {
			K__ERROR("E_EXCEL: No table selected");
			return nullptr;
		}
		if (data_row < 0) return nullptr;

		int col = m_leftcol; // 行マーカーは一番左の列にあるものとする。（一番左のデータ列の、さらにひとつ左側）
		int row = m_toprow + 1 + data_row;
		return m_excel.getDataString(m_sheet, col, row);
	}
	const char * getDataString(int data_col, int data_row) const {
		if (m_excel.empty()) {
			K__ERROR("E_EXCEL: No table loaded");
			return nullptr;
		}
		if (m_sheet < 0) {
			K__ERROR("E_EXCEL: No table selected");
			return nullptr;
		}
		if (data_col < 0) return nullptr;
		if (data_row < 0) return nullptr;

		// 一番左の列 (m_leftcol) は開始・終端キーワードを置くためにある。
		// ほかに文字が入っていてもコメント扱いで無視される。
		// 実際のデータはその右隣の列から開始する
		if (data_col >= m_leftcol + 1 + (int)m_colnames.size()) return nullptr;
		int col = m_leftcol + 1 + data_col;

		// 一番上の行（m_toprow) は開始キーワードとカラム名を書くためにある。
		// 実際のデータ行はそのひとつ下から始まる
		if (data_row >= m_bottomrow) return nullptr;
		int row = m_toprow + 1 + data_row;

		return m_excel.getDataString(m_sheet, col, row);
	}
	bool getDataIntTry(int data_col, int data_row, int *p_val) const {
		const char *s = getDataString(data_col, data_row);
		// 実数形式で記述されている値を整数として取り出す可能性
		float f = 0.0f;
		if (K::strToFloat(s, &f)) {
			if (p_val) *p_val = (int)f;
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
	bool getDataFloatTry(int data_col, int data_row, float *p_val) const {
		const char *s = getDataString(data_col, data_row);
		float f = 0.0f;
		if (K::strToFloat(s, &f)) {
			if (p_val) *p_val = f;
			return true;
		}
		return false;
	}
	bool getDataSource(int data_col, int data_row, int *col_in_file, int *row_in_file) const {
		if (data_col < 0) return false;
		if (data_row < 0) return false;
		if (col_in_file) *col_in_file = m_leftcol + 1 + data_col;
		if (row_in_file) *row_in_file = m_toprow  + 1 + data_row;
		return true;
	}
};


#pragma region KTable
KTable::KTable() {
	m_impl = nullptr;
}
bool KTable::empty() const {
	return m_impl == nullptr || m_impl->empty();
}
void KTable::clear() {
	if (m_impl) {
		m_impl->clear();
	}
}
bool KTable::loadFromExcelFile(const KExcelFile &excel, const std::string &sheetname, const std::string &top_cell_text, const std::string &btm_cell_text, bool mute) {
	auto impl = std::make_shared<Impl>();
	if (impl->_loadFromExcelFile(excel)) {
		std::string top = (top_cell_text != "") ? top_cell_text : "@BEGIN";
		std::string btm = (btm_cell_text != "") ? btm_cell_text : "@END";
		if (impl->_selectTable(sheetname, top, btm, mute)) {
			m_impl = impl;
			return true;
		}
	}
	m_impl = nullptr;
	return false;
}
bool KTable::loadFromStream(KInputStream &xlsx, const std::string &filename, const std::string &sheetname, const std::string &top_cell_text, const std::string &btm_cell_text, bool mute) {
	KExcelFile excel;
	if (excel.loadFromStream(xlsx, filename)) {
		if (loadFromExcelFile(excel, sheetname, top_cell_text, btm_cell_text, mute)) {
			return true;
		}
	}
	m_impl = nullptr;
	return false;
}
bool KTable::loadFromExcelMemory(const void *xlsx_bin, size_t xlsx_size, const std::string &filename, const std::string &sheetname, const std::string &top_cell_text, const std::string &btm_cell_text, bool mute) {
	if (xlsx_bin && xlsx_size > 0) {
		KExcelFile excel;
		if (excel.loadFromMemory(xlsx_bin, xlsx_size, filename)) {
			if (loadFromExcelFile(excel, sheetname, top_cell_text, btm_cell_text, mute)) {
				return true;
			}
		}
	}
	m_impl = nullptr;
	return false;
}

int KTable::getDataColIndexByName(const std::string &column_name) const {
	if (m_impl) {
		return m_impl->getDataColIndexByName(column_name);
	}
	return -1;
}
std::string KTable::getColumnName(int col) const {
	if (m_impl) {
		return m_impl->getColumnName(col);
	}
	return "";
}
int KTable::getDataColCount() const {
	if (m_impl) {
		return m_impl->getDataColCount();
	}
	return 0;
}
int KTable::getDataRowCount() const {
	if (m_impl) {
		return m_impl->getDataRowCount();
	}
	return 0;
}
const char * KTable::getRowMarker(int row) const {
	if (m_impl) {
		return m_impl->getRowMarker(row);
	}
	return nullptr;
}
const char * KTable::getDataString(int col, int row) const {
	if (m_impl) {
		return m_impl->getDataString(col, row);
	}
	return nullptr;
}
std::string KTable::getDataStringStd(int col, int row) const {
	const char *s = getDataString(col, row);
	if (s && s[0]) {
		return s;
	}
	return "";
}
int KTable::getDataInt(int col, int row, int def) const {
	int val = def;
	getDataIntTry(col, row, &val);
	return val;
}
bool KTable::getDataIntTry(int col, int row, int *p_val) const {
	return m_impl && m_impl->getDataIntTry(col, row, p_val);
}
float KTable::getDataFloat(int col, int row, float def) const {
	float val = def;
	getDataFloatTry(col, row, &val);
	return val;
}
bool KTable::getDataFloatTry(int col, int row, float *p_val) const {
	return m_impl && m_impl->getDataFloatTry(col, row, p_val);
}
bool KTable::getDataSource(int col, int row, int *col_in_file, int *row_in_file) const {
	if (m_impl && m_impl->getDataSource(col, row, col_in_file, row_in_file)) {
		return true;
	}
	return false;
}
int KTable::findRowByIntData(int col, int value) const {
	int numcols = getDataColCount();
	int numrows = getDataRowCount();
	if (col < 0 || numcols <= col) return -1;
	for (int i=0; i<numrows; i++) {
		int n = 0;
		if (getDataIntTry(col, i, &n)) { // 正しく整数に変換できて、かつ指定された値と同じならばOK
			if (n == value) {
				return i;
			}
		}
	}
	return -1;
}
int KTable::findRowByStringData(int col, const std::string &value, bool trim) const {
	int numcols = getDataColCount();
	int numrows = getDataRowCount();
	if (col < 0 || numcols <= col) return -1;
	for (int i=0; i<numrows; i++) {
		std::string s = getDataStringStd(col, i);
		if (trim) {
			K::strTrim(s);
		}
		if (s.compare(value) == 0) {
			return i;
		}
	}
	return -1;
}

#pragma endregion // KTable

} // namespace
