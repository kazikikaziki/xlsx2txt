#pragma once
#include <inttypes.h>
#include <string>
#include <memory>

namespace Kamilo {

class KInputStream;
class KOutputStream;
class CZipWriterImpl; // internal
class CZipReaderImpl; // internal


/// Zip ファイルを作成する
/// @see KUnzipper
class KZipper {
public:
	KZipper();
	KZipper(KOutputStream &output);

	void open(KOutputStream &output);
	bool isOpen();

	/// 圧縮レベルを設定する。一度設定したら再設定するまで有効になる
	/// 0 から 9 までの値を指定する. 0=無圧縮 1=最速 5=標準 9=最大圧縮
	void setCompressLevel(int level);

	/// パスワードを設定する。一度設定したら再設定するまで有効。
	/// 暗号化を解除するには nullptr または空文字列を指定する
	void setPassword(const char *password);

	/// ファイルを追加する
	/// @param name_u8   ファイル名を UTF8 で指定する. 絶対パスや相対パスを含まないよう注意する事
	/// @param data      入力データ
	/// @param size      入力データのバイト数
	/// @param time_cma  ファイルのタイムスタンプ. float[3] を Create, Modify, Access の順番で指定した配列。nullptr でも良い
	/// @param file_attr ファイル属性を指定する。 0 でもよい
	bool addEntry(const char *name_u8, const void *data, int size, const time_t *time_cma, int file_attr);

	/// フッターとZIPファイルのコメントを追加する。
	/// これを追加したら、それ以降は add_entry しても意味がない
	/// コメント不要な場合は comment に nullptr または commentsize に 0 を指定する
	void finalize(const char *comment, int commentsize);

private:
	std::shared_ptr<CZipWriterImpl> m_Impl;
};


/// Zip ファイルを展開する
/// @see KZipper
class KUnzipper {
public:
	enum INFO {
		WITH_PASSWORD, // パスワードがある
		WITH_UTF8,     // ファイル名が UTF8 でエンコードされている
		UNZIP_SIZE,    // 展開後のデータサイズ
		FILE_ATTR,     // ファイル属性
		EXTRA_COUNT,   // 拡張情報の個数
	};

	KUnzipper();
	KUnzipper(KInputStream &input);
	void open(KInputStream &input);
	bool isOpen();

	int getEntryCount() const;

	/// ファイル名を得る。
	/// ※文字コードは考慮しない。ZIPに格納されているバイナリをそのまま返す
	/// out_bin を nullptr にした場合はサイズだけ返す
	/// @see getEntryParamInt(), WITH_UTF8
	int getEntryName(int file_index, std::string *out_bin);

	/// ファイルを展開する
	/// out_bin を nullptr にした場合はサイズだけ返す
	/// @see getEntryParamInt(), UNZIP_SIZE
	bool getEntryData(int file_index, const char *password, std::string *out_bin);

	/// ファイルのコメントを得る。
	/// ※文字コードは考慮しない。ZIPに格納されているバイナリをそのまま返す
	/// out_bin を nullptr にした場合はサイズだけ返す
	int getEntryComment(int file_index, std::string *out_bin);

	/// ファイルの拡張情報を得る
	/// out_bin を nullptr にした場合はサイズだけ返す
	/// @see getEntryParamInt(), EXTRA_COUNT
	int getEntryExtra(int file_index, int extra_index, uint16_t *out_sign, std::string *out_bin);

	/// ファイルのタイムスタンプを得る。time_cma は time_t 3個から成る配列で、
	/// Create, Modify, Access の順で時刻が格納される。
	bool getEntryTimeStamp(int file_index, time_t *time_cma);

	/// ファイル情報を得る
	int getEntryParamInt(int file_index, INFO flag);

	/// このZIPファイルのコメントを得る。
	/// @note コメントは単なるバイト列であることに注意
	/// out_bin を nullptr にした場合はサイズだけ返す
	int getComment(std::string *out_bin);

private:
	std::shared_ptr<CZipReaderImpl> m_Impl;
};


namespace Test {
void Test_zip(const char *output_dir);
}

} // namespace
