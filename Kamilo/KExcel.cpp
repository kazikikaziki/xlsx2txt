#include "KExcel.h"

#include <unordered_map>
#include "KStream.h"
#include "KInternal.h"
#include "KZip.h"
#include "KXml.h"


namespace Kamilo {


#define EXCEL_ALPHABET_NUM 26
#define EXCEL_COL_LIMIT    (EXCEL_ALPHABET_NUM + EXCEL_ALPHABET_NUM * EXCEL_ALPHABET_NUM) // A～Z + AA～ZZ
#define EXCEL_ROW_LIMIT    1024
#define HAS_CHAR(str, chr) (strchr(str, chr) != nullptr)


// 文字列 s を xml に組み込めるようにする
static void _EscapeString(std::string &s) {
	K::strReplace(s, "\\", "\\\\");
	K::strReplace(s, "\"", "\\\"");
	K::strReplace(s, "\n", "\\n");
	K::strReplace(s, "\r", "");
	if (s.find(',') != std::string::npos) {
		s.insert(0, "\"");
		s.append("\"");
	}
}

// 文字列 s をエスケープする必要がある？
static bool _ShouldEscapeString(const char *s) {
	K__ASSERT(s);
	return HAS_CHAR(s, '<') || HAS_CHAR(s, '>') || HAS_CHAR(s, '"') || HAS_CHAR(s, '\'') || HAS_CHAR(s, '\n');
}

// ZIP 内のファイルを探してインデックスを返す
static int _FindZipEntry(KUnzipper &zr, const std::string &name) {
	int num = zr.getEntryCount();
	for (int i=0; i<num; i++) {
		std::string s;
		zr.getEntryName(i, &s);
		if (s.compare(name) == 0) {
			return i;
		}
	}
	return -1;
}
static KXmlElement * _LoadXmlFromZip(KUnzipper &zr, const std::string &zip_name, const std::string &entry_name) {
	if (!zr.isOpen()) {
		K__ERROR("E_INVALID_ARGUMENT");
		return nullptr;
	}

	// XLSX の XML は常に UTF-8 で書いてある。それを信用する
	int fileid = _FindZipEntry(zr, entry_name);
	if (fileid < 0) {
		K__ERROR("E_FILE: Failed to open file '%s' from archive '%s'", entry_name.c_str(), zip_name.c_str());
		return nullptr;
	}

	std::string xml_u8;
	if (!zr.getEntryData(fileid, "", &xml_u8)) {
		K__ERROR("E_FILE: Failed to open file '%s' from archive '%s'", entry_name.c_str(), zip_name.c_str());
		return nullptr;
	}

	std::string doc_name = K::pathJoin(zip_name, entry_name);
	KXmlElement *xdoc = KXmlElement::createFromString(xml_u8, doc_name);
	if (xdoc == nullptr) {
		K__ERROR("E_XML: Failed to read xml document: '%s' from archive '%s'", entry_name.c_str(), zip_name.c_str());
	}
	return xdoc;
}



class CCoreExcelReader {
	static const int ROW_LIMIT = 10000;

	enum Type {
		TP_NUMBER,
		TP_STRINGID,
		TP_LITERAL,
		TP_OTHER,
	};
	std::string m_FileName;
	KXmlElement *m_WorkBookDoc;
	std::vector<KXmlElement *> m_WorkSheets;
	std::unordered_map<int, std::string> m_Strings;
	typedef std::unordered_map<int, const KXmlElement*> Int_Elms;
	typedef std::unordered_map<const KXmlElement*, Int_Elms> Sheet_RowElms;
	mutable Sheet_RowElms m_RowElements;
public:
	CCoreExcelReader() {
		m_WorkBookDoc = nullptr;
	}
	virtual ~CCoreExcelReader() {
		clear();
	}
	void clear() {
		for (size_t i=0; i<m_WorkSheets.size(); i++) {
			K__DROP(m_WorkSheets[i]);
		}
		m_WorkSheets.clear();
		m_Strings.clear();
		m_RowElements.clear();
		K__DROP(m_WorkBookDoc);
		m_FileName.clear();
	}
	bool empty() const {
		return m_WorkSheets.empty();
	}
	std::string getFileName() const {
		return m_FileName;
	}
	int getSheetCount() const {
		return (int)m_WorkSheets.size();
	}
	std::string getSheetName(int sheetId) const {
		const KXmlElement *root_elm = m_WorkBookDoc->getChild(0);
		K__ASSERT(root_elm);

		const KXmlElement *sheets_xml = root_elm->findNode("sheets");
		K__ASSERT(sheets_xml);

		int idx = 0;
		for (int iSheet=sheets_xml->findChildByTag("sheet"); iSheet>=0; iSheet=sheets_xml->findChildByTag("sheet", iSheet+1)) {
			const KXmlElement *xSheet = sheets_xml->getChild(iSheet);
			if (idx == sheetId) {
				const char *s = xSheet->getAttrString("name");
				return s;
			}
			idx++;
		}
		return "";
	}
	int getSheetByName(const std::string &name) const {
		const KXmlElement *root_elm = m_WorkBookDoc->getChild(0);
		K__ASSERT(root_elm);

		const KXmlElement *sheets_xml = root_elm->findNode("sheets");
		K__ASSERT(sheets_xml);

		int idx = 0;
		for (int iSheet=sheets_xml->findChildByTag("sheet"); iSheet>=0; iSheet=sheets_xml->findChildByTag("sheet", iSheet+1)) {
			const KXmlElement *xSheet = sheets_xml->getChild(iSheet);
			const char *name_u8 = xSheet->getAttrString("name");
			if (name_u8 && name.compare(name_u8) == 0) {
				return idx;
			}
			idx++;
		}
		return -1;
	}
	bool getSheetDimension(int sheet, int *col, int *row, int *colcount, int *rowcount) const {
		if (sheet < 0) return false;
		if (sheet >= (int)m_WorkSheets.size()) return false;

		const KXmlElement *xdoc = m_WorkSheets[sheet];
		K__ASSERT(xdoc);

		const KXmlElement *xroot = xdoc->getChild(0);
		K__ASSERT(xroot);

		const KXmlElement *xdim = xroot->findNode("dimension");
		K__ASSERT(xdim);

		// セルの定義範囲を表す文字列を取得する
		// この文字列は "A1:E199" のようにコロンで左上端セルと右下端セル番号が書いてある
		const char *attr = xdim->getAttrString("ref");

		// コロンを区切りにして分割
		char tmp[256];
		strcpy_s(tmp, sizeof(tmp), attr ? attr : "");
		
		// コロンの位置を見つける。
		char *colon = strchr(tmp, ':');
		if (colon) {
			// コロンの位置で文字列を分割する。コロンをヌル文字に置換する
			*colon = '\0';

			// minpos にはコロン左側の文字列を入れる
			// maxpos にはコロン右側の文字列を入れる
			const char *minpos = tmp;
			const char *maxpos = colon + 1;

			// セル番号から行列インデックスを得る
			int L=0, R=0, T=0, B=0;
			if (parse_cell_position(minpos, &L, &T) && parse_cell_position(maxpos, &R, &B)) {
				if (col) *col = L;
				if (row) *row = T;
				if (colcount) *colcount = R - L + 1;
				if (rowcount) *rowcount = B - T + 1;
				return true;
			}
		}

		// インデックスを得られなかった場合は自力で取得する
		//
		// 条件は分からないが、Libre Office Calc でシートを保存したとき、
		// 実際の範囲は "A1:M60" であるにもかかわらず "A1:AMJ60" といった記述になることがあったため。
		// 念のために自力での取得コードも書いておく。
		const KXmlElement *sheet_xml = xroot->findNode("sheetData");
		if (sheet_xml) {
			CScanDim dim;
			scan_cells(sheet_xml, &dim);
			if (col) *col = dim.col0_;
			if (row) *row = dim.row0_;
			if (colcount) *colcount = dim.col1_ - dim.col0_ + 1;
			if (rowcount) *rowcount = dim.row1_ - dim.row0_ + 1;
			return true;
		}

		return false;
	}
	const char * getDataString(int sheet, int col, int row) const {
		if (sheet < 0 || col < 0 || row < 0) return nullptr;
		const KXmlElement *s = get_sheet_xml(sheet);
		const KXmlElement *r = get_row_xml(s, row);
		const KXmlElement *c = get_cell_xml(r, col);
		return get_cell_text(c);
	}
	bool getCellByText(int sheet, const std::string &s, int *col, int *row) const {
		if (sheet < 0) return false;
		if (s.empty()) return false;

		// シートを選択
		const KXmlElement *sheet_xml = get_sheet_xml(sheet);
		if (sheet_xml == nullptr) return false;

		// 同じ文字列を持つセルを探す
		const KXmlElement *c_xml = find_cell(sheet_xml, s);
		if (c_xml == nullptr) return false;

		K__ASSERT(col);
		K__ASSERT(row);

		// 見つかったセルの行列番号を得る
		int icol = -1;
		int irow = -1;
		const char *r = c_xml->getAttrString("r");
		parse_cell_position(r, &icol, &irow);

		if (icol >= 0 && irow >= 0) {
			*col = icol;
			*row = irow;
			return true;
		}
		return false;
	}
	void scanCells(int sheet, KExcelScanCellsCallback *cb) const {
		if (sheet < 0) return;
		if (sheet >= (int)m_WorkSheets.size()) return;

		const KXmlElement *doc = m_WorkSheets[sheet];
		K__ASSERT(doc);

		const KXmlElement *root_elm = doc->getChild(0);
		K__ASSERT(root_elm);

		const KXmlElement *sheet_xml = root_elm->findNode("sheetData");
		if (sheet_xml) {
			scan_cells(sheet_xml, cb);
		}
	}
	bool loadFromFile(KInputStream &file, const std::string &xlsx_name) {
		m_RowElements.clear();

		if (!file.isOpen()) {
			K__ERROR("E_INVALID_ARGUMENT");
			return false;
		}

		// XMLS ファイルを ZIP として開く
		bool ok = false;
		{
			KUnzipper zr(file);
			ok = loadFromZipAsXlsx(zr, xlsx_name);
		}
		return ok;
	}

	bool loadFromZipAsXlsx(KUnzipper &zr, const std::string &xlsx_name) {
		// 文字列テーブルを取得
		const KXmlElement *strings_doc = _LoadXmlFromZip(zr, xlsx_name, "xl/sharedStrings.xml");
		if (strings_doc) {
			int stringId = 0;
			const KXmlElement *string_elm = strings_doc->getChild(0);

			for (int si=0; si<string_elm->getChildCount(); si++) {
				const KXmlElement *si_elm = string_elm->getChild(si);
				if (!si_elm->hasTag("si")) continue;

				// パターンA
				// <si>
				//   <t>テキスト</t>
				// </si>
				const KXmlElement *t_elm = si_elm->findNode("t");
				if (t_elm) {
					const char *s = t_elm->getText("");
					m_Strings[stringId] = s;
					stringId++;
					continue;
				}

				// パターンB（テキストの途中でスタイル変更がある場合にこうなる？）
				// <si>
				//   <rPr>スタイル情報いろいろ</rPr>
				//   <r><t>テキスト1</t></r>
				//   <r><t>テキスト2</t></r>
				//   ....
				// </si>
				std::string s;
				for (int r=0; r<si_elm->getChildCount(); r++) {
					const KXmlElement *r_elm = si_elm->getChild(r);
					if (r_elm->hasTag("r")) {
						const KXmlElement *t_xml = r_elm->findNode("t");
						if (t_xml) {
							s += t_xml->getText("");
						}
					}
				}
				if (!s.empty()) {
					m_Strings[stringId] = s;
					stringId++;
					continue;
				}

				// その他のパターン。解析できず。
				// 文字列IDだけを進めておく
				stringId++;
			}
			strings_doc->drop();
		}

		// ワークシート情報を取得
		m_WorkBookDoc = _LoadXmlFromZip(zr, xlsx_name, "xl/workbook.xml");

		// ワークシートを取得
		for (int i=1; ; i++) {
			char s[256];
			sprintf_s(s, sizeof(s), "xl/worksheets/sheet%d.xml", i);
			if (_FindZipEntry(zr, s) < 0) break;
			KXmlElement *sheet_doc = _LoadXmlFromZip(zr, xlsx_name, s);
			if (sheet_doc == nullptr) {
				K__ERROR("E_XLSX: Failed to load document: '%s' in xlsx file.", s);
				break;
			}
			m_WorkSheets.push_back(sheet_doc);
		}

		// 必要な情報がそろっていることを確認
		if (m_WorkBookDoc == nullptr || m_WorkSheets.empty()) {
			K__ERROR("E_FAILED_TO_READ_EXCEL_ARCHIVE: %s", xlsx_name);
			clear();
			return false;
		}

		m_FileName = xlsx_name;
		return true;
	}

private:
	const char * get_cell_text(const KXmlElement *cell_xml) const {
		Type tp;
		const char *val = get_cell_raw_data(cell_xml, &tp);
		if (val == nullptr) {
			return "";
		}
		if (tp == TP_STRINGID) {
			// val は文字列ID (整数) を表している。
			// 対応する文字列を文字列テーブルから探す
			int sid = -1;
			K::strToInt(val, &sid);
			K__ASSERT(sid >= 0);
			auto it = m_Strings.find(sid);
			if (it != m_Strings.end()) {
				return it->second.c_str();
			}
			return "";
		}
		if (tp == TP_LITERAL) {
			// val は文字列そのものを表している
			return val;
		}
		// それ以外のデータの場合は val の文字列をそのまま返す
		return val;
	}

	const KXmlElement *find_cell(const KXmlElement *sheet_xml, const std::string &s) const {
		if (sheet_xml == nullptr) return nullptr;
		if (s.empty()) return nullptr;

		for (int r=0; r<sheet_xml->getChildCount(); r++) {
			const KXmlElement *xRow = sheet_xml->getChild(r);
			if (!xRow->hasTag("row")) continue;

			for (int c=0; c<xRow->getChildCount(); c++) {
				const KXmlElement *xCell = xRow->getChild(c);
				if (!xCell->hasTag("c")) continue;

				const char *str = get_cell_text(xCell);
				if (str && s.compare(str) == 0) {
					return xCell;
				}
			}
		}
		return nullptr;
	}

	// "A1" や "AB12" などのセル番号から、行列インデックスを取得する
	bool parse_cell_position(const char *ss, int *col, int *row) const {
		return KExcelFile::decodeCellName(ss, col, row);
	}
	void scan_cells(const KXmlElement *sheet_xml, KExcelScanCellsCallback *cb) const {
		if (sheet_xml == nullptr) return;

		for (int r=0; r<sheet_xml->getChildCount(); r++) {
			const KXmlElement *xRow = sheet_xml->getChild(r);
			if (!xRow->hasTag("row")) continue;

			for (int c=0; c<xRow->getChildCount(); c++) {
				const KXmlElement *xCell = xRow->getChild(c);
				if (!xCell->hasTag("c")) continue;

				// とんでもないセル番号が入っている場合がある "ZA1" とか
				// しかし実際には空文字列が入っているだけだったりするので、
				// 有効な文字列が入っているかどうかを先に調べる。
				// 空文字列のセルだった場合は存在しないものとして扱う
				const char *val = get_cell_text(xCell);

				if (val && val[0]) {
					const char *pos = xCell->getAttrString("r");
					int cidx = -1;
					int ridx = -1;
					if (parse_cell_position(pos, &cidx, &ridx)) {
						K__ASSERT(cidx >= 0 && ridx >= 0);
						cb->onCell(cidx, ridx, val);
					}
				}
			}
		}
	}
	const KXmlElement * get_sheet_xml(int sheet) const {
		if (sheet < 0) return nullptr;
		if (sheet >= (int)m_WorkSheets.size()) return nullptr;

		const KXmlElement *doc = m_WorkSheets[sheet];
		K__ASSERT(doc);

		const KXmlElement *root_elm = doc->getChild(0);
		K__ASSERT(root_elm);

		return root_elm->findNode("sheetData");
	}
	const KXmlElement * get_row_xml(const KXmlElement *sheet_xml, int row) const {
		if (sheet_xml == nullptr) return nullptr;
		if (row < 0) return nullptr;
		// キャッシュからから探す
		if (! m_RowElements.empty()) {
			// シートを得る
			Sheet_RowElms::const_iterator sheet_it = m_RowElements.find(sheet_xml);
			if (sheet_it != m_RowElements.end()) {
				// 行を得る
				Int_Elms::const_iterator row_it = sheet_it->second.find(row);
				if (row_it != sheet_it->second.end()) {
					return row_it->second;
				}
				// 指定行が存在しない
				return nullptr;
			}
			// シートがまだキャッシュ化されていない。作成する
		}

		// キャッシュから見つからないなら、キャッシュを作りつつ目的のデータを探す
		const KXmlElement *ret = nullptr;
		for (const KXmlElement *it=sheet_xml->findNode("row"); it!=nullptr; it=sheet_xml->findNode("row", it)) {
			int val = it->getAttrInt("r");
			if (val >= 1) {
				int r = val - 1;
				m_RowElements[sheet_xml][r] = it;
				if (r == row) {
					ret = it;
				}
			}
		}
		return ret;
	}
	const KXmlElement * get_cell_xml(const KXmlElement *row_xml, int col) const {
		const int NUM_ALPHABET = 26; // A-Z
		if (row_xml == nullptr) return nullptr;
		if (col < 0) return nullptr;

		for (int c=0; c<row_xml->getChildCount(); c++) {
			const KXmlElement *c_elm = row_xml->getChild(c);
			if (!c_elm->hasTag("c")) continue;

			const char *s = c_elm->getAttrString("r");
			int col_idx = -1;
			parse_cell_position(s, &col_idx, nullptr);
			if (col_idx == col) {
				return c_elm;
			}
		}
		return nullptr;
	}

	// 無変換のセルデータを得る。得られた文字列が何を表しているかは type によって異なる
	const char * get_cell_raw_data(const KXmlElement *cell_xml, Type *type) const {
		if (cell_xml == nullptr) return nullptr;
		K__ASSERT(type);
		// cell_xml の入力例:
		// <c r="B1" s="0" t="n">
		// 	<v>12</v>
		// </c>

		// データ型
		const char *s = cell_xml->getAttrString("s");
		const char *t = cell_xml->getAttrString("t");
		if (t) {
			if (strcmp(t, "n") == 0) {
				*type = TP_NUMBER; // <v> には数値が指定されている
			} else if (strcmp(t, "s") == 0) {
				*type = TP_STRINGID; // <v> には文字列ＩＤが指定されている
			} else if (strcmp(t, "str") == 0) {
				*type = TP_LITERAL; // <v> には文字列が直接指定されている
			} else { 
				*type = TP_OTHER; // それ以外の値が入っている
			}
		} else {
			// t 属性がない
			*type = TP_LITERAL;
		}
		// データ文字列
		const KXmlElement *v_elm = cell_xml->findNode("v");
		if (v_elm == nullptr) return nullptr;
		return v_elm->getText();
	}

	class CScanDim: public KExcelScanCellsCallback {
	public:
		int col0_, col1_, row0_, row1_;

		CScanDim() {
			col0_ = -1;
			col1_ = -1;
			row0_ = -1;
			row1_ = -1;
		}
		virtual void onCell(int col, int row, const char *s) override {
			if (col0_ < 0) {
				col0_ = col;
				col1_ = col;
				row0_ = row;
				row1_ = row;
			} else {
				col0_ = K::min(col0_, col);
				col1_ = K::max(col1_, col);
				row0_ = K::min(row0_, row);
				row1_ = K::max(row1_, row);
			}
		}
	};
};


#pragma region KExcelFile
std::string KExcelFile::encodeCellName(int col, int row) {
	if (col < 0) return "";
	if (row < 0) return "";
	if (col < EXCEL_ALPHABET_NUM) { // A～Z
		char c = (char)('A' + col);
		char s[256];
		sprintf_s(s, sizeof(s), "%c%d", c, 1+row);
		return s;
	}
	if (col < EXCEL_ALPHABET_NUM*EXCEL_ALPHABET_NUM) { // AA～ZZ
		int off = col - EXCEL_ALPHABET_NUM; // AA=0 としたときのカラム番号
		char c1 = (char)('A' + (off / EXCEL_ALPHABET_NUM));
		char c2 = (char)('A' + (off % EXCEL_ALPHABET_NUM));
		char s[256];
		K__ASSERT(isalpha(c1));
		K__ASSERT(isalpha(c2));
		sprintf_s(s, sizeof(s), "%c%c%d", c1, c2, 1+row);
		return s;
	}
	return "";
}
/// "A2" や "AM244" などのセル番号を int の組にデコードする
bool KExcelFile::decodeCellName(const std::string &s, int *col, int *row) {
	if (s.empty()) return false;
	int c = -1;
	int r = -1;

	if (isalpha(s[0]) && isdigit(s[1])) {
		// セル番号が [A-Z][0-9].* にマッチしている。
		// 例えば "A1" や "Z42" など。
		c = toupper(s[0]) - 'A';
		K__ASSERT(0 <= c && c < EXCEL_ALPHABET_NUM);
		r = strtol(s.c_str() + 1, nullptr, 0);
		r--; // １起算 --> 0起算

	} else if (isalpha(s[0]) && isalpha(s[1]) && isdigit(s[2])) {
		// セル番号が [A-Z][A-Z][0-9].* にマッチしている。
		// 例えば "AA42" や "KZ1217" など
		int idx1 = toupper(s[0]) - 'A';
		int idx2 = toupper(s[1]) - 'A';
		K__ASSERT(0 <= idx1 && idx1 < EXCEL_ALPHABET_NUM);
		K__ASSERT(0 <= idx2 && idx2 < EXCEL_ALPHABET_NUM);
		// AA は A～Z の次の要素(26番目)なので、26からカウントする
		c = EXCEL_ALPHABET_NUM + idx1 * EXCEL_ALPHABET_NUM + idx2;
		r = strtol(s.c_str() + 2, nullptr, 0);
		r--; // １起算 --> 0起算
	}
	if (c >= 0 && r >= 0) {
		// ここで、セル範囲として 0xFFFFF が入る可能性に注意。(LibreOffice Calc で現象を確認）
		// 0xFFFFF 以上の値が入っていたら範囲取得失敗とみなす
		if (r >= 0xFFFFF) {
			return false;
		}
		K__ASSERT(0 <= c && c < EXCEL_COL_LIMIT);
		K__ASSERT(0 <= r && r < EXCEL_ROW_LIMIT);
		if (col) *col = c;
		if (row) *row = r;
		return true;
	}

	return false;
}


KExcelFile::KExcelFile() {
	m_Impl = std::make_shared<CCoreExcelReader>();
}
bool KExcelFile::empty() const {
	return m_Impl->empty();
}
void KExcelFile::clear() {
	m_Impl->clear();
}
std::string KExcelFile::getFileName() const {
	return m_Impl->getFileName();
}
bool KExcelFile::loadFromStream(KInputStream &file, const std::string &xlsx_name) {
	return m_Impl->loadFromFile(file, xlsx_name);
}
bool KExcelFile::loadFromFileName(const std::string &name) {
	bool ok = false;
	KInputStream file;
	if (file.openFileName(name)) {
		ok = m_Impl->loadFromFile(file, name);
	}
	if (!ok) {
		m_Impl->clear();
	}
	return ok;
}
bool KExcelFile::loadFromMemory(const void *bin, size_t size, const std::string &name) {
	bool ok = false;
	KInputStream file;
	if (file.openMemory(bin, size)) {
		ok = m_Impl->loadFromFile(file, name);
	}
	if (!ok) {
		m_Impl->clear();
	}
	return ok;
}
int KExcelFile::getSheetCount() const {
	return m_Impl->getSheetCount();
}
int KExcelFile::getSheetByName(const std::string &name) const {
	return m_Impl->getSheetByName(name);
}
std::string KExcelFile::getSheetName(int sheet) const {
	return m_Impl->getSheetName(sheet);
}
bool KExcelFile::getSheetDimension(int sheet, int *col, int *row, int *colcount, int *rowcount) const {
	return m_Impl->getSheetDimension(sheet, col, row, colcount, rowcount);
}
const char * KExcelFile::getDataString(int sheet, int col, int row) const {
	return m_Impl->getDataString(sheet, col, row);
}
bool KExcelFile::getCellByText(int sheet, const std::string &s, int *col, int *row) const {
	return m_Impl->getCellByText(sheet, s, col, row);
}
void KExcelFile::scanCells(int sheet, KExcelScanCellsCallback *cb) const {
	m_Impl->scanCells(sheet, cb);
}
std::string KExcelFile::exportXmlString(bool with_header, bool with_comment) {
	class CB: public KExcelScanCellsCallback {
	public:
		std::string &dest_;
		int last_row_;
		int last_col_;
		
		CB(std::string &s): dest_(s) {
			last_row_ = -1;
			last_col_ = -1;
		}
		virtual void onCell(int col, int row, const char *s) override {
			if (s==nullptr || s[0]=='\0') return;
			K__ASSERT(last_row_ <= row); // 行番号は必ず前回と等しいか、大きくなる
			if (last_row_ != row) {
				if (last_row_ >= 0) { // 行タグを閉じる
					dest_ += "</row>\n";
				}
				if (last_row_ < 0 || last_row_ + 1 < row) {
					// 行番号が飛んでいる場合のみ列番号を付加する
					dest_ += K::str_sprintf("\t<row r='%d'>", row);
				} else {
					// インクリメントで済む場合は行番号を省略
					dest_ += "\t<row>";
				}
				last_row_ = row;
				last_col_ = -1;
			}
			if (last_col_ < 0 || last_col_ + 1 < col) {
				// 列番号が飛んでいる場合のみ列番号を付加する
				dest_ += K::str_sprintf("<c i='%d'>", col);
			} else {
				// インクリメントで済む場合は列番号を省略
				dest_ += "<c>";
			}
			if (_ShouldEscapeString(s)) { // xml禁止文字が含まれているなら CDATA 使う
				dest_ += K::str_sprintf("<![CDATA[%s]]>", s);
			} else {
				dest_ += s;
			}
			dest_ += "</c>";
			last_col_ = col;
		}
	};
	if (empty()) return "";
	
	std::string s;
	if (with_header) {
		s += "<?xml version='1.0' encoding='utf-8'>\n";
	}
	if (with_comment) {
		s += u8"<!-- <sheet> タグは「シート」に対応する。 left, top, cols, rows 属性にはそれぞれ、シート内で値が入っているセル範囲の左、上、行数、列数が入る -->\n";
		s += u8"<!-- <row> タグは各シートの「行」に対応する。 <row> の r 属性には 0 起算での行番号が入る。ただし直前の <row> の次の行だった場合 r 属性は省略される -->\n";
		s += u8"<!-- <c> タグは、それぞれの行 <row> 内にある「セル」に対応する。 i 属性には 0 起算での列番号が入る。ただし、直前の <c> の次の列だった場合 i 属性は省略される -->\n";
	}
	s += K::str_sprintf("<excel numsheets='%d'>\n", getSheetCount());
	for (int iSheet=0; iSheet<getSheetCount(); iSheet++) {
		int col=0, row=0, nCol=0, nRow=0;
		std::string sheet_name = getSheetName(iSheet);
		getSheetDimension(iSheet, &col, &row, &nCol, &nRow);
		s += K::str_sprintf("<sheet name='%s' left='%d' top='%d' cols='%d' rows='%d'>\n", sheet_name.c_str(), col, row, nCol, nRow);
		{
			CB cb(s);
			scanCells(iSheet, &cb);
			if (cb.last_row_ >= 0) {
				s += "</row>\n"; // 最終行を閉じる
			} else {
				// セルを一つも出力していないので <row> を閉じる必要もない
			}
		}
		s += "</sheet>";
		if (with_comment) {
			s += K::str_sprintf("<!-- %s -->", sheet_name.c_str());
		}
		s += "\n\n";
	}
	s += "</excel>\n";
	return s;
}
std::string KExcelFile::exportText() {
	if (empty()) return "";
	std::string s;
	for (int iSheet=0; iSheet<getSheetCount(); iSheet++) {
		int col=0, row=0, nCol=0, nRow=0;
		std::string sheet_name = getSheetName(iSheet);
		getSheetDimension(iSheet, &col, &row, &nCol, &nRow);
		s += "\n";
		s += "============================================================================\n";
		s += sheet_name + "\n";
		s += "============================================================================\n";
		int blank_lines = 0;
		for (int r=row; r<row+nRow; r++) {
			bool has_cell = false;
			for (int c=col; c<col+nCol; c++) {
				const char *str = getDataString(iSheet, c, r);
				if (str && str[0]) {
					if (blank_lines >= 1) {
						s += "\n";
						blank_lines = 0;
					}
					if (has_cell) s += ", ";
					std::string ss = str;
					_EscapeString(ss);
					s += ss;
					has_cell = true;
				}
			}
			if (has_cell) {
				s += "\n";
			} else {
				blank_lines++;
			}
		}
	}
	return s;
}

#pragma endregion // KExcelFile




namespace Test {

void Test_excel(const std::string &filename) {
	KExcelFile ef;
	ef.loadFromFileName(filename);
	std::string xml = ef.exportXmlString();
	KOutputStream file;
	file.openFileName(filename + ".txt");
	file.writeString(xml);
}

} // Test


} // namespace