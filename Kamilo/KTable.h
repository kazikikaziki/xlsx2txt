#pragma once
#include <memory>
#include <string>

namespace Kamilo {

class KDataGrid;

class KTableCallback {
public:
	/// 文字列を整数または実数として解釈する必要があるときに、その直前に呼ばれる。
	/// 必要に応じて s を書き換えることができる。
	/// (例)
	/// "010" ==> "10" (頭のゼロをとる。"010" だと 8 進数の 10 とみなされてしまう）
	/// ""    ==> "0" （空文字列を 0 とみなす）
	/// "n/a" ==> "0" （"n/a"という言葉が入っていたら 0 とみなす）
	virtual void onTableNumericText(std::string &s) = 0;
};

/// 行と列で定義されるデータテーブルを扱う。
/// 単純な「名前＝値」の形式のテーブルで良ければ KNamedValues を使う
class KTable {
public:
	KTable();
	bool empty() const;
	void clear();
	void setCallback(KTableCallback *cb);
	std::string getSourceLocation(int *col=nullptr, int *row=nullptr) const;

	/// Excel シートからテーブルオブジェクトを作成する
	/// 
	/// @param file             Excel オブジェクト
	/// @param sheet_name       シート名
	/// @param top_cell_text    テーブル範囲の左上にあるセルのテキスト。このテキストに一致するセルをシートから探し、テーブル範囲上端として設定する。
	///                         このパラメータを空文字列 "" にした場合、文字列 "@BEGIN" を指定したのと同じになる
	/// @param bottom_cell_text テーブル範囲の左下にあるセルのテキスト。このテキストに一致するセルをシートから探し、テーブル範囲下端として設定する。
	///                         このパラメータを空文字列 "" にした場合、文字列 "@END" を指定したのと同じになる
	///
	/// テーブルの行範囲は top_cell_text と btm_cell_text で挟まれた部分になる。
	/// テーブルの列範囲は top_cell_text の右隣から始まるカラムIDテキストが終わるまで
	///（top_cell_text の右側のセルを順番に見ていったとき、空白セルがあればそこがテーブルの右端になる）
	///
	/// 例えばデータシートを次のように記述する:
	/// @code
	///      | [A]    | [B]   | [C] | [D] |
	/// -----+--------+-------+-----+-----+--
	///    1 | @BEGIN | KEY   | VAL |     |
	/// -----+--------+-------+-----+-----+--
	///    2 |        | one   | 100 |     |
	/// -----+--------+-------+-----+-----+--
	///    3 | //     | two   | 200 |     |
	/// -----+--------+-------+-----+-----+--
	///    4 |        | three | 300 |     |
	/// -----+--------+-------+-----+-----+--
	///    5 |        | four  | 400 |     |
	/// -----+--------+-------+-----+-----+--
	///    6 |        | five  | 500 |     |
	/// -----+--------+-------+-----+-----+--
	///    7 | @END   |       |     |     |
	/// -----+--------+-------+-----+-----+--
	///    8 |        |       |     |     |
	/// @endcode
	/// この状態で loadFromDataGrid(sheet, "@BEGIN") を呼び出すと、A1 から C7 の範囲を KTable に読み込むことになる
	/// このようにして取得した KTable は以下のような値を返す
	/// @code
	///     table.findColumnByName("KEY") ==> 0
	///     table.findColumnByName("VAL") ==> 1
	///     table.getDataString(0, 0) ==> "one"
	///     table.getDataString(1, 0) ==> "100"
	///     table.getDataString(0, 2) ==> "three"
	///     table.getDataString(1, 2) ==> "300"
	///     table.getRowMarker(0) ==> nullptr  <---- セルA3の内容
	///     table.getRowMarker(1) ==> "//"     <---- セルA4の内容
	/// @endcode
	bool loadFromDataGrid(const KDataGrid &grid, const std::string &topleft="", bool mute=false);

	/// カラム名のインデックスを返す。カラム名が存在しない場合は -1 を返す
	int findColumnByName(const std::string &column_name) const;

	/// 指定された列に対応するカラム名を返す
	std::string getColumnName(int col) const;
	
	/// データ列数
	int getDataColCount() const;

	/// データ行数
	int getDataRowCount() const;

	/// このテーブルがより大きなテーブルの一部から抜き出されたものである場合、元のテーブル内でのセル位置を得る
	bool getCellCoordInSource(int col, int row, int *col_in_file, int *row_in_file) const;

	/// col 列のなかから、指定された value を探して行インデックス（ゼロ起算）を返す
	int findRowInColumnByString(int col, const std::string &value) const;
	int findRowInColumnByInt(int col, int value) const;

	/// データ行にユーザー定義のマーカーが設定されているなら、それを返す。
	/// 例えば行全体がコメントアウトされている時には "#" や "//" を返すなど。
	std::string getRowMarker(int row) const;

	void setKeyColumn(int col); // キーカラムを設定する

	/// 指定セルのデータ文字列を得る(UTF8)
	/// @param col セル列（ゼロ起算）
	/// @param row セル行（ゼロ起算）
	/// @retval utf8文字列またはnullptr
	bool queryDataString(int col, int row, std::string *p_val) const;
	bool queryDataInt(int col, int row, int *p_val) const;
	bool queryDataFloat(int col, int row, float *p_val) const;

	// キー列内の値 key（文字列）に対応する col 列の値を得る
	// @see setKeyColumn
	bool queryDataStringByKey(int select_col, const std::string &value_in_keycol, std::string *p_val) const;
	bool queryDataIntByKey(int select_col, const std::string &value_in_keycol, int *p_val) const;
	bool queryDataFloatByKey(int select_col, const std::string &value_in_keycol, float *p_val) const;

	// キー列内の値 key （整数）に対応する col 列の値を得る
	// Excel シートなどでは整数のつもりでも 1.00000001 のように記録されている場合があるので、その場合は文字列で "1" を検索しても見つからない
	// @see setKeyColumn
	bool queryDataStringByKey(int select_col, int value_in_keycol, std::string *p_val) const;
	bool queryDataIntByKey(int select_col, int value_in_keycol, int *p_val) const;
	bool queryDataFloatByKey(int select_col, int value_in_keycol, float *p_val) const;

	// queryDataString の簡易版
	std::string getDataString(int col, int row) const {
		std::string val;
		queryDataString(col, row, &val);
		return val;
	}
	int getDataInt(int col, int row) const {
		int val = 0;
		queryDataInt(col, row, &val);
		return val;
	}
	float getDataFloat(int col, int row) const {
		float val = 0;
		queryDataFloat(col, row, &val);
		return val;
	}

	// queryDataStringByKey の簡易版
	std::string getDataStringByKey(int select_col, const std::string &value_in_keycol) const {
		std::string val;
		queryDataStringByKey(select_col, value_in_keycol, &val);
		return val;
	}
	int getDataIntByKey(int select_col, const std::string &value_in_keycol) const {
		int val;
		queryDataIntByKey(select_col, value_in_keycol, &val);
		return val;
	}
	float getDataFloatByKey(int select_col, const std::string &value_in_keycol) const {
		float val;
		queryDataFloatByKey(select_col, value_in_keycol, &val);
		return val;
	}
	
	// queryDataStringByIntKey の簡易版
	std::string getDataStringByKey(int select_col, int value_in_keycol) const {
		std::string val;
		queryDataStringByKey(select_col, value_in_keycol, &val);
		return val;
	}
	int getDataIntByIntKey(int select_col, int value_in_keycol) const {
		int val;
		queryDataIntByKey(select_col, value_in_keycol, &val);
		return val;
	}
	float getDataFloatByKey(int select_col, int value_in_keycol) const {
		float val;
		queryDataFloatByKey(select_col, value_in_keycol, &val);
		return val;
	}

private:
	class Impl;
	std::shared_ptr<Impl> m_impl;
};


} // namespace
