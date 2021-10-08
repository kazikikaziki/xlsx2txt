#pragma once
#include <memory>
#include <string>

namespace Kamilo {

class KExcelFile;
class KInputStream;

/// 行と列で定義されるデータテーブルを扱う。
/// 単純な「名前＝値」の形式のテーブルで良ければ KNamedValues を使う
class KTable {
public:
	KTable();
	bool empty() const;
	void clear();

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
	/// この状態で loadFromExcelFile(excel, sheetname, "@BEGIN", "@END") を呼び出すと、A1 から C7 の範囲を KTable に読み込むことになる
	/// このようにして取得した KTable は以下のような値を返す
	/// @code
	///     table.getDataColIndexByName("KEY") ==> 0
	///     table.getDataColIndexByName("VAL") ==> 1
	///     table.getDataString(0, 0) ==> "one"
	///     table.getDataString(1, 0) ==> "100"
	///     table.getDataString(0, 2) ==> "three"
	///     table.getDataString(1, 2) ==> "300"
	///     table.getRowMarker(0) ==> nullptr  <---- セルA3の内容
	///     table.getRowMarker(1) ==> "//"     <---- セルA4の内容
	/// @endcode
	bool loadFromExcelFile(const KExcelFile &file, const std::string &sheet_name, const std::string &top_cell_text, const std::string &bottom_cell_text);

	/// テーブルを作成する。詳細は loadFromExcelFile を参照
	/// @param xlsx .xlsx ファイルオブジェクト
	/// @param filename ファイル名（エラーメッセージの表示などで使用）
	/// @param sheetname シート名
	/// @param top_cell_text テーブル範囲の左上にあるセルのテキスト。このテキストと一致するセルを探し、それをテーブル左上とする
	/// @param btm_cell_text テーブル範囲の左下（右下ではない）にあるセルのテキスト。このテキストと一致するセルを探し、それをテーブル左下とする
	bool loadFromStream(KInputStream &xlsx, const std::string &filename, const std::string &sheetname, const std::string &top_cell_text, const std::string &btm_cell_text);

	/// テーブルを作成する。詳細は loadFromExcelFile を参照
	/// @param xlsx_bin  .xlsx ファイルのバイナリデータ
	/// @param xlsx_size .xlsx ファイルのバイナリバイト数
	/// @param filename  ファイル名（エラーメッセージの表示などで使用）
	/// @param sheetname シート名
	/// @param top_cell_text テーブル範囲の左上にあるセルのテキスト。このテキストと一致するセルを探し、それをテーブル左上とする
	/// @param btm_cell_text テーブル範囲の左下（右下ではない）にあるセルのテキスト。このテキストと一致するセルを探し、それをテーブル左下とする
	bool loadFromExcelMemory(const void *xlsx_bin, size_t xlsx_size, const std::string &filename, const std::string &sheetname, const std::string &top_cell_text, const std::string &btm_cell_text);

	/// カラム名のインデックスを返す。カラム名が存在しない場合は -1 を返す
	int getDataColIndexByName(const std::string &column_name) const;

	/// 指定された列に対応するカラム名を返す
	std::string getColumnName(int col) const;
	
	/// データ列数
	int getDataColCount() const;

	/// データ行数
	int getDataRowCount() const;

	/// データ行にユーザー定義のマーカーが設定されているなら、それを返す。
	/// 例えば行全体がコメントアウトされている時には "#" や "//" を返すなど。
	const char * getRowMarker(int row) const;

	/// このテーブルのデータ文字列を得る(UTF8)
	/// @param col データ列インデックス（ゼロ起算）
	/// @param row データ行インデックス（ゼロ起算）
	/// @retval utf8文字列またはnullptr
	const char * getDataString(int col, int row) const;
	std::string getDataStringStd(int col, int row) const;

	/// このテーブルのデータ整数を得る
	/// @param col データ列インデックス（ゼロ起算）
	/// @param row データ行インデックス（ゼロ起算）
	/// @param def セルが存在しない時の戻り値
	int getDataInt(int col, int row, int def=0) const;
	bool getDataIntTry(int col, int row, int *p_val) const;

	/// このテーブルのデータ実数を得る
	/// @param col データ列インデックス（ゼロ起算）
	/// @param row データ行インデックス（ゼロ起算）
	/// @param def セルが存在しない時の戻り値
	float getDataFloat(int col, int row, float def=0.0f) const;
	bool getDataFloatTry(int col, int row, float *p_val) const;

	/// データ列とデータ行を指定し、それが定義されている列と行を得る
	bool getDataSource(int col, int row, int *col_in_file, int *row_in_file) const;

	int findRowByIntData(int col, int value) const;

	// value に一致する文字列を探す
	// trim 先頭と末尾の空白を取り除いて比較する？
	int findRowByStringData(int col, const std::string &value, bool trim=true) const;

private:
	class Impl;
	std::shared_ptr<Impl> m_impl;
};


} // namespace
