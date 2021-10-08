#include "KZip.h"
//
#include <time.h>
#include <inttypes.h>
#include <vector>
#include "KStream.h"
#include "KInternal.h"
#include "KCrc32.h"
#include "KZlib.h"

#define ZIP_ERROR(msg)  K__ERROR("%s", msg)


namespace Kamilo {

#pragma region ZIP


// ZIP書庫ファイルフォーマット
// http://www.tvg.ne.jp/menyukko/cauldron/dtzipformat.html

// 圧縮zipを作ってみる
// http://www.tnksoft.com/reading/zipfile/

#pragma region ZipTypes
// ローカルファイルヘッダ
//
// ※この構造体はそのままファイルに書き込むため、
// 各メンバーのサイズ、オフセットは ZIP の仕様と完全に一致している。
// アラインメントに注意すること。
#pragma pack (push, 1)
struct SZipLocalFileHeader {
	uint32_t signature;
	uint16_t version_needed_to_extract;
	uint16_t general_purpose_bit_flag;
	uint16_t compression_method;
	uint16_t last_mod_file_time;
	uint16_t last_mod_file_date;
	uint32_t data_crc32;
	uint32_t compressed_size;
	uint32_t uncompressed_size;
	uint16_t file_name_length;
	uint16_t extra_field_length;
};
#pragma pack (pop)

// 中央ディレクトリヘッダ
//
// ※この構造体はそのままファイルに書き込むため、
// 各メンバーのサイズ、オフセットは ZIP の仕様と完全に一致している。
// アラインメントに注意すること。
#pragma pack (push, 1)
struct SZipCentralDirectoryHeader {
	uint32_t signature;
	uint16_t version_made_by;
	uint16_t version_needed_to_extract;
	uint16_t general_purpose_bit_flag;
	uint16_t compression_method;
	uint16_t last_mod_file_time;
	uint16_t last_mod_file_date;
	uint32_t data_crc32;
	uint32_t compressed_size;
	uint32_t uncompressed_size;
	uint16_t file_name_length;
	uint16_t extra_field_length;
	uint16_t file_comment_length;
	uint16_t disk_number_start;
	uint16_t internal_file_attributes;
	uint32_t external_file_attributes;
	uint32_t relative_offset_of_local_header;
};
#pragma pack (pop)

// 終端レコード
//
// ※この構造体はそのままファイルに書き込むため、
// 各メンバーのサイズ、オフセットは ZIP の仕様と完全に一致している。
// アラインメントに注意すること。
#pragma pack (push, 1)
struct SZipEndOfCentralDirectoryRecord {
	uint32_t signature;
	uint16_t disknum;
	uint16_t startdisknum;
	uint16_t diskdirentry;
	uint16_t direntry;
	uint32_t dirsize;
	uint32_t startpos;
	uint16_t comment_length;
};
#pragma pack (pop)


#if 0
// データデスクリプタ
//
// ※この構造体はそのままファイルに書き込むため、
// 各メンバーのサイズ、オフセットは ZIP の仕様と完全に一致している。
// アラインメントに注意すること。
//
//
// ところで、このプログラムでは、これは使わない。
//
// データデスクリプタ―の有無は ZIP_OPT_DATADESC フラグで調べられるが、
// でスクリプターが存在する場合はローカルファイルヘッダにファイルサイズが載っていないため
// ファイル末尾の終端ヘッダから辿らないといけない。
// 終端ヘッダには中央ディレクトリヘッダのオフセットが載っており、それを元にしてチュウオウディレクトリヘッダまで辿ることができる。
// ところで、チュウオウディレクトリヘッダにもデータサイズが載っている。
// 故に、データデスクリプタ―を参照してファイルサイズを得ることはない。
#pragma pack (push, 1)
struct SZipDataDescriptor {
	uint32_t data_crc32;
	uint32_t compressed_size;
	uint32_t uncompressed_size;
};
#pragma pack (pop)
#endif


#pragma endregion // ZipTypes


#pragma region ZipDef
#define ZIP_CRYPT_HEADER_SIZE           12         // 暗号ヘッダのバイト数
#define ZIP_VERSION_UNCOMPRESS          10         // ver1.0 無圧縮
#define ZIP_VERSION_DEFLATE             20         // ver2.0 Deflate 圧縮
#define ZIP_COMPRESS_METHOD_UNCOMPRESS  0          // 圧縮方法: 無圧縮
#define ZIP_COMPRESS_METHOD_DEFLATE     8          // 圧縮方法: Deflate
#define ZIP_OPT_ENCRYPTED               0x0001     // bit  0 -- 暗号化されている
#define ZIP_OPT_DATADESC                0x0008     // bit  3 -- Data Descriptor (PK0708) が存在する
#define ZIP_OPT_UTF8                    0x0800     // bit 11 -- ファイル名やコメントが UTF-8文字
#define ZIP_SIGN_PK0102                 0x02014B50 // 中央ディレクトリヘッダ
#define ZIP_SIGN_PK0304                 0x04034B50 // ローカルファイルヘッダ
#define ZIP_SIGN_PK0506                 0x06054B50 // 終端レコード
#define ZIP_SIGN_PK0708                 0x08074B50 // データデスクリプタ http://www.tnksoft.com/reading/zipfile/pk0708.php
#define ZIPEX_MAX_NAME                  256        // ZIP格納するエントリー名の最大数（自主的に科した制限で、ZIPの仕様とは無関係）
#define ZIPEX_MAX_EXTRA                 16         // ZIPに格納する拡張データの最大数（自主的に科した制限で、ZIPの仕様とは無関係）
#pragma endregion // ZipDef


#pragma region ZipClasses
class CZipCrypt {
public:
	// 圧縮済みデータを暗号化する
	// data: バイナリデータ
	// size: バイナリデータのバイト数
	// password: パスワード。他ツールでの復元を考慮しないなら、ASCII 文字でなくても良い。
	// crypt_header: ZIPファイルに書き込むための暗号化ヘッダへのポインタ。長さ ZIP_CRYPT_HEADER_SIZE バイトの領域を指定する
	// crc32_hi_8bit: 圧縮前のデータに対するCRC32値の上位8ビット
	static void encode(void *data, int size, const char *password, uint8_t *crypt_header, uint8_t crc32_hi_8bit) {
		CZipCrypt z;
		z.encodeInit(password, crypt_header, crc32_hi_8bit);
		z.encodeData(data, size);
	}

	// 暗号化された圧縮済みデータを復号する
	// data: バイナリデータ
	// size: バイナリデータのバイト数
	// password: パスワード。他ツールでの復元を考慮しないなら、ASCII 文字でなくても良い。
	// crypt_header: 暗号化ヘッダ
	static void decode(void *data, int size, const char *password, const uint8_t *crypt_header) {
		CZipCrypt z;
		z.decodeInit(password, crypt_header);
		z.decodeData(data, size);
	}
public:
	CZipCrypt() {
		m_Keys[0] = 0;
		m_Keys[1] = 0;
		m_Keys[2] = 0;
	}
	void encodeInit(const char *password, uint8_t *crypt_header, uint8_t crc32_hi_8bit) {
		K__ASSERT(password);
		K__ASSERT(crypt_header);
		initKeys();
		for (const char *p=password; *p!='\0'; p++) {
			update(*p);
		}
		for (int i=0; i<ZIP_CRYPT_HEADER_SIZE; ++i) {
			if (i == ZIP_CRYPT_HEADER_SIZE - 1){
				crypt_header[i] = crc32_hi_8bit;
			} else {
				crypt_header[i] = ::rand() % 0xFF;
			}
			crypt_header[i] = encode_byte(crypt_header[i]);
		}
	}
	void decodeInit(const char *password, const uint8_t *crypt_header) {
		K__ASSERT(password);
		K__ASSERT(crypt_header);
		initKeys();
		for (const char *p=password; *p!='\0'; p++) {
			update(*p);
		}
		for (int i=0; i<ZIP_CRYPT_HEADER_SIZE; ++i) {
			decode_byte(crypt_header[i]);
		}
	}
	void encodeData(void *data, int size) {
		uint8_t *p = (uint8_t *)data;
		for (int i=0; i<size; ++i) {
			p[i] = encode_byte(p[i]);
		}
	}
	void decodeData(void *data, int size) {
		uint8_t *p = (uint8_t *)data;
		for (int i=0; i<size; ++i) {
			p[i] = decode_byte(p[i]);
		}
	}
private:
	void initKeys() {
		m_Keys[0] = 0x12345678;
		m_Keys[1] = 0x23456789;
		m_Keys[2] = 0x34567890;
	}
	void update(uint8_t val) {
		m_Keys[0] = KCrc32::fromByte(val, m_Keys[0]);
		m_Keys[1] += (m_Keys[0] & 0xFF);
		m_Keys[1] = m_Keys[1] * 134775813 + 1;
		m_Keys[2] = KCrc32::fromByte(m_Keys[1] >> 24, m_Keys[2]);
	}
	uint8_t next() {
		uint16_t tmp = (uint16_t)((m_Keys[2] & 0xFFFF) | 2);
		return ((tmp * (tmp ^ 1)) >> 8) & 0xFF;
	}
	uint8_t encode_byte(uint8_t val) {
		uint8_t t = next();
		update(val);
		return t ^ val;
	}
	uint8_t decode_byte(uint8_t val) {
		val ^= next();
		update(val);
		return val;
	}
	uint32_t m_Keys[3];
};

struct SZipExtraBlock {
	uint16_t sign;   // 識別子
	uint16_t size;   // データバイト数
	uint32_t offset; // データの位置（ZIPファイル先頭からのオフセット）

	SZipExtraBlock() {
		sign = 0;
		size = 0;
		offset = 0;
	}
};

struct SZipEntryBlock {
	SZipLocalFileHeader lo_hdr;        // ローカルファイルヘッダ
	SZipCentralDirectoryHeader cd_hdr; // 中央ディレクトリヘッダ
	uint32_t lo_hdr_offset;            // ローカルファイルヘッダの位置（ZIPファイル先頭からのオフセット）
	uint32_t dat_offset;               // 圧縮データの位置（ZIPファイル先頭からのオフセット）

	// ファイル名。
	// 絶対パスやnullptrは指定できない。"../" や "./" などの上に登るようなパスも指定できない。
	// この namebin の値がそのままバイナリとしてZIPファイル内に保存される。
	// 文字コードが何になっているかは環境依存だが、
	// ローカルファイルヘッダまたは中央ディレクトリヘッダの general_purpose_bit_flag が ZIP_OPT_UTF8 を
	// 含んでいれば UTF8 であり、そうでなければ環境依存のマルチバイト文字列になっている。
	char namebin[ZIPEX_MAX_NAME];

	time_t mtime;        // コンテンツの最終更新日時
	time_t atime;        // コンテンツの最終アクセス日時
	time_t ctime;        // コンテンツの作成日時
	uint32_t num_extras; // 拡張データ数
	SZipExtraBlock extras[ZIPEX_MAX_EXTRA]; // 拡張データ
	uint32_t comment_offset; // コメントがあるなら、その位置（ZIPファイル先頭からのオフセット）。なければ 0

	SZipEntryBlock() {
		memset(&lo_hdr, 0, sizeof(lo_hdr));
		memset(&cd_hdr, 0, sizeof(cd_hdr));
		lo_hdr_offset = 0;
		dat_offset = 0;
		namebin[0] = 0;
		atime = 0;
		mtime = 0;
		ctime = 0;
		num_extras = 0;
		memset(extras, 0, sizeof(extras));
		comment_offset = 0;
	}
};

struct SZipEntryWritingParams {
	std::string namebin; // ファイル名。詳細は SZipEntryBlock の namebin を参照
	std::string data; // 書き込むデータ
	time_t atime; // 最終アクセス日時
	time_t mtime; // 最終更新日時
	time_t ctime; // 作成日時
	uint32_t file_attr; // ファイル属性。特に気にしない場合は 0
	std::string password; // パスワード。設定しない場合は空文字列
	int level; // 圧縮レベル。0 で最速、9 で最大圧縮。-1でデフォルト値を使う
	bool is_name_utf8; // namebin が UT8 形式かどうか

	SZipLocalFileHeader output_lo_hdr; // ローカルファイルヘッダ（Zip__WriteEntry の実行結果として設定される）
	SZipCentralDirectoryHeader output_cd_hdr; // 中央ディレクトリヘッダ（Zip__WriteEntry の実行結果として設定される）

	SZipEntryWritingParams() {
		atime = 0;
		mtime = 0;
		ctime = 0;
		file_attr = 0;
		level = 0;
		is_name_utf8 = false;
		memset(&output_lo_hdr, 0, sizeof(output_lo_hdr));
		memset(&output_cd_hdr, 0, sizeof(output_cd_hdr));
	}
};


// ZIP ファイル書き込み用の低レベルAPI
#pragma region zip

// time_tによる時刻表現からZIPのヘッダで使用する時刻表現に変換する
// zdate 更新月日
// ztime 更新時刻
// mtime time_t 形式での更新日時
static void Zip__EncodeFileTime(uint16_t *zdate, uint16_t *ztime, time_t time) {
	K__ASSERT(zdate);
	K__ASSERT(ztime);
	if (time == 0) {
		*ztime = 0;
		*zdate = 0;
	} else {
		// time_t 形式の時刻（GMT) をローカル時刻 struct tm に変換する
		struct tm tm;
		localtime_s(&tm, &time); // time_t --> struct tm

		// %1111100000000000 = 0..23 (hour)
		// %0000011111100000 = 0..59 (min)
		// %0000000000011111 = 0..29 (2 seconds)
		*ztime = (
			(tm.tm_hour << 11) |
			(tm.tm_min  <<  5) |
			(tm.tm_sec  >>  1)
		);
		// %1111111000000000 = 0..XX (year from 1980)
		// %0000000111100000 = 1..12 (month)
		// %0000000000011111 = 1..31 (day)
		*zdate = (
			((1900 + tm.tm_year - 1980) << 9) |
			((1    + tm.tm_mon        ) << 5) |
			tm.tm_mday
		);
	}
}

// コンテンツを書き込む。
// 書き込みに成功した場合は params->output_lo_hdr と params->output_cd_hdr にヘッダ情報をセットして true を返す
static bool Zip__WriteEntry(KOutputStream &output, SZipEntryWritingParams &params) {
	if (params.namebin.empty()) return false;
	if (params.namebin[0] == '.') return false;
	if (K::str_ispathdelim(params.namebin[0])) return false;

	// 元データの CRC32 を計算
	uint32_t data_crc32 = KCrc32::fromData(params.data.data(), params.data.size());

	// 圧縮
	std::string encoded_data;
	if (params.level == 0) {
		encoded_data.resize(params.data.size()); // 無圧縮
		memcpy(&encoded_data[0], params.data.data(), params.data.size());
	} else {
		// あらかじめ圧縮後のサイズを知りたい場合は ::compressBound(local_file_hdr.uncompressed_size) を使う
		encoded_data = KZlib::compress_raw(params.data, params.level);
	}

	// 暗号化
	uint8_t crypt_header[ZIP_CRYPT_HEADER_SIZE];
	if (!params.password.empty()) {
		uint8_t crc32_hi_8bit = (data_crc32 >> 24) & 0xFF;
		CZipCrypt::encode(&encoded_data[0], encoded_data.size(), params.password.c_str(), crypt_header, crc32_hi_8bit);
	}

	// ローカルファイルヘッダを作成
	SZipLocalFileHeader local_file_hdr;
	local_file_hdr.signature = ZIP_SIGN_PK0304;
	local_file_hdr.version_needed_to_extract = (params.level!=0) ? ZIP_VERSION_DEFLATE : ZIP_VERSION_UNCOMPRESS;
	local_file_hdr.general_purpose_bit_flag = 0;
	if (!params.password.empty()) {
		local_file_hdr.general_purpose_bit_flag |= ZIP_OPT_ENCRYPTED;
	}
	if (params.is_name_utf8) {
		local_file_hdr.general_purpose_bit_flag |= ZIP_OPT_UTF8;
	}
	local_file_hdr.compression_method = (params.level!=0) ? ZIP_COMPRESS_METHOD_DEFLATE : ZIP_COMPRESS_METHOD_UNCOMPRESS;
	Zip__EncodeFileTime(&local_file_hdr.last_mod_file_date, &local_file_hdr.last_mod_file_time, params.mtime);
	local_file_hdr.data_crc32 = data_crc32;
	local_file_hdr.uncompressed_size = params.data.size();
	if (!params.password.empty()) {
		local_file_hdr.compressed_size = encoded_data.size() + ZIP_CRYPT_HEADER_SIZE; // 暗号化している場合、暗号化ヘッダも圧縮済みファイルサイズに含む
	} else {
		local_file_hdr.compressed_size = encoded_data.size();
	}
	local_file_hdr.file_name_length = (uint16_t)params.namebin.size();
	local_file_hdr.extra_field_length = 0;
	params.output_lo_hdr = local_file_hdr;

	// あとでローカルファイルヘッダの書き込み位置が必要になる
	int header_pos = output.tell();

	// データの書き込み
	output.write(&local_file_hdr, sizeof(local_file_hdr)); // ローカルファイルヘッダ
	output.write(params.namebin.c_str(), local_file_hdr.file_name_length); // ファイル名
	if (!params.password.empty()) {
		output.write(crypt_header, ZIP_CRYPT_HEADER_SIZE);
		output.write(&encoded_data[0], encoded_data.size());
	} else {
		output.write(&encoded_data[0], encoded_data.size());
	}
	// 中央ディレクトリ
	// http://www.tvg.ne.jp/menyukko/cauldron/dtzipformat.html#rainbow
	SZipCentralDirectoryHeader central_dir_hdr;
	central_dir_hdr.signature                 = ZIP_SIGN_PK0102;
	central_dir_hdr.version_made_by           = 10;
	central_dir_hdr.version_needed_to_extract = local_file_hdr.version_needed_to_extract;
	central_dir_hdr.general_purpose_bit_flag  = local_file_hdr.general_purpose_bit_flag;
	central_dir_hdr.compression_method        = local_file_hdr.compression_method;
	central_dir_hdr.last_mod_file_time        = local_file_hdr.last_mod_file_time;
	central_dir_hdr.last_mod_file_date        = local_file_hdr.last_mod_file_date;
	central_dir_hdr.data_crc32                = local_file_hdr.data_crc32;
	central_dir_hdr.compressed_size           = local_file_hdr.compressed_size;
	central_dir_hdr.uncompressed_size         = local_file_hdr.uncompressed_size;
	central_dir_hdr.file_name_length          = local_file_hdr.file_name_length;
	central_dir_hdr.extra_field_length        = local_file_hdr.extra_field_length;
	central_dir_hdr.file_comment_length       = 0;
	central_dir_hdr.disk_number_start         = 0;
	central_dir_hdr.internal_file_attributes  = 0;
	central_dir_hdr.external_file_attributes  = params.file_attr;
	central_dir_hdr.relative_offset_of_local_header = header_pos;
	params.output_cd_hdr = central_dir_hdr;
	
	return true;
}

// 中央ディレクトリヘッダを書き込む
// params は既に Zip__WriteEntry によって必要な値がセットされていないといけない
static bool Zip__WriteCentralDirectoryHeader(KOutputStream &output, const SZipEntryWritingParams &params) {
	K__ASSERT(!params.namebin.empty());
	K__ASSERT(params.output_cd_hdr.file_name_length == params.namebin.size());
	output.write(&params.output_cd_hdr, sizeof(SZipCentralDirectoryHeader));
	output.write(params.namebin.c_str(), params.output_cd_hdr.file_name_length);
	return true;
}

// 終端レコードを書き込む
// cd_hdr_offset 中央ディレクトリヘッダの位置（すでに zip_write_central_directory_header によってファイルに書き込まれているものとする）
// contents      コンテンツ配列（すでに Zip__WriteEntry によってファイルに書き込まれているものとする）
// num_contents  コンテンツ数
// comment       ZIPファイル全体に対するコメント文字列または nullptr
static bool Zip__WriteEndOfCentralDirectoryHeaderAndComment(KOutputStream &output, size_t cd_hdr_offset, const SZipEntryWritingParams *contents, int num_contents, const char *comment, int commentsize) {
	SZipEndOfCentralDirectoryRecord hdr;
	hdr.signature    = ZIP_SIGN_PK0506;
	hdr.disknum      = 0;
	hdr.startdisknum = 0;
	hdr.diskdirentry = (uint16_t)num_contents;
	hdr.direntry     = (uint16_t)num_contents;
	hdr.dirsize      = 0;
	// オフセットを計算
	for (int i=0; i<num_contents; ++i) {
		const SZipCentralDirectoryHeader *cd_hdr = &contents[i].output_cd_hdr;
		hdr.dirsize += sizeof(SZipCentralDirectoryHeader);
		hdr.dirsize += cd_hdr->file_name_length;
		hdr.dirsize += cd_hdr->extra_field_length;
		hdr.dirsize += cd_hdr->file_comment_length;
	}
	hdr.startpos = cd_hdr_offset; // 中央ディレクトリの開始バイト位置
	hdr.comment_length = comment ? (uint16_t)commentsize : 0; // このヘッダに続くzipコメントのサイズ
	output.write(&hdr, sizeof(hdr));

	// ZIPファイルに対するコメント
	if (comment && commentsize > 0) {
		output.write(comment, hdr.comment_length);
	}
	return true;
}
#pragma endregion // zip


// ZIP ファイル読み取り用の低レベルAPI
#pragma region unzip

// input の現在位置からの2バイトが value と等しいか調べる。
// この関数は読み取りヘッダを移動しない
static bool Unzip__CheckFileUint16(KInputStream &input, uint16_t value) {
	int pos = input.tell();
	uint16_t data = input.readUint16();
	input.seek(pos);
	return data == value;
}

// input の現在位置からの4バイトが value と等しいか調べる。
// この関数は読み取りヘッダを移動しない
static bool Unzip__CheckFileUint32(KInputStream &input, uint32_t value) {
	int pos = input.tell();
	uint32_t data = input.readUint32();
	input.seek(pos);
	return data == value;
}


// NTFSで使われている64ビットのファイル時刻形式（1601/1/1 0:00から100ナノ秒刻み）を
// time_t 形式（1970/1/1 0:00から1秒刻み）に変換する
static time_t Unzip__NtfsToTimeT(uint64_t ntfs_time64) {
	const uint64_t basetime = 116444736000000000; // FILETIME at 1970/1/1 0:00
	const uint64_t nsec  = 10000000; // 100nsec --> sec (10 * 1000 * 1000)
	return (time_t)((ntfs_time64 - basetime) / nsec);
}

// フラグの有無を調べる
// zip_opt:
//         ZIP_OPT_ENCRYPTED
//         ZIP_OPT_DATADESC
//         ZIP_OPT_UTF8
static bool Unzip__HasOption(const SZipEntryBlock *entry, uint32_t zip_opt) {
	K__ASSERT(entry);
	return (entry->cd_hdr.general_purpose_bit_flag & zip_opt) != 0;
}

// 展開後のサイズ
static size_t Unzip__GetUnzipSize(const SZipEntryBlock *entry) {
	K__ASSERT(entry);
	// サイズ情報を見る時は必ず中央ディレクトリヘッダを見るようにする。

	// ファイルサイズは他にもローカルファイルヘッダとデータデスクリプタに記録されているが、
	// そのどちらにあるか、あるいは両方に記録されているのかは ZIP_OPT_DATADESC フラグに依存する。
	//
	// ZIP_OPT_DATADESC フラグがあるならファイルデータ末尾にデータデスクリプタが存在し、そこにファイルサイズが書いてある。
	// その場合、ローカルファイルヘッダの方にはデータサイズ 0 と記載される。
	// ZIP_OPT_DATADESC フラグが無い場合、ファイルデータ末尾にデータデスクリプタは存在せず、ローカルファイルヘッダ側にデータサイズが記録されている
	return entry->cd_hdr.uncompressed_size;
}

// コメントを得る
// comment_bin コメント文字列。文字コード変換などは何も行わず、ZIPに記録されているバイト列をそのままセットする
// ※ZIPファイル全体に対するコメントを得るには Unzip__GetZipFileComment を使う
static size_t Unzip__GetComment(KInputStream &input, const SZipEntryBlock *entry, std::string *comment_bin) {
	K__ASSERT(entry);
	if (entry->comment_offset > 0) {
		if (comment_bin) {
			input.seek(entry->comment_offset);
			*comment_bin = input.readBin(entry->cd_hdr.file_comment_length);
		}
		return entry->cd_hdr.file_comment_length;
	}
	return 0;
}

// 拡張情報を得る
static size_t Unzip__GetExtra(KInputStream &input, const SZipExtraBlock *extra, std::string *bin) {
	K__ASSERT(extra);
	if (extra->offset > 0) {
		if (bin) {
			input.seek(extra->offset);
			*bin = input.readBin(extra->size);
		}
		return extra->size;
	}
	return 0;
}

// コンテンツデータを復元する
static bool Unzip__UnzipEntry(KInputStream &input, const SZipEntryBlock *entry, const char *password, std::string *output) {
	K__ASSERT(entry);
	K__ASSERT(output);

	// 圧縮データ部分に移動
	input.seek(entry->dat_offset);

	// サイズ情報を見る時は必ず中央ディレクトリヘッダを見るようにする。

	// ファイルサイズは他にもローカルファイルヘッダとデータデスクリプタに記録されているが、
	// そのどちらにあるか、あるいは両方に記録されているのかは ZIP_OPT_DATADESC フラグに依存する。
	//
	// ZIP_OPT_DATADESC フラグがあるならファイルデータ末尾にデータデスクリプタが存在し、
	// そこにファイルサイズが書いてある。その場合、ローカルファイルヘッダの方にはデータサイズ 0 と記載される。
	//
	// ZIP_OPT_DATADESC フラグが無い場合、ファイルデータ末尾にデータデスクリプタは存在せず、
	// ローカルファイルヘッダ側にデータサイズが記録されている

//	SZipLocalFileHeader hdr = entry->lo_hdr;
	SZipCentralDirectoryHeader hdr = entry->cd_hdr;

	if (hdr.compressed_size == 0) {
		ZIP_ERROR("Invalid data size");
		return false;
	}

	std::string compressed_data(hdr.compressed_size, '\0');
	input.read(&compressed_data[0], hdr.compressed_size);

	void *data_ptr = nullptr;
	int data_len = 0;
	if (hdr.general_purpose_bit_flag & ZIP_OPT_ENCRYPTED) {
		// 暗号化を解除
		const uint8_t *crypt_header = (const uint8_t *)&compressed_data[0];
		data_ptr = &compressed_data[ZIP_CRYPT_HEADER_SIZE];
		data_len = hdr.compressed_size - ZIP_CRYPT_HEADER_SIZE;
		CZipCrypt::decode(data_ptr, data_len, password, crypt_header);
	} else {
		// 暗号化なし
		data_ptr = &compressed_data[0];
		data_len = hdr.compressed_size;
	}

	if (hdr.compression_method) {
		// 圧縮を解除
		std::string input((const char*)data_ptr, data_len);
		*output = KZlib::uncompress_raw(input, hdr.uncompressed_size);
		K__ASSERT(output->size() == hdr.uncompressed_size);
		return true;
		
	} else {
		// 無圧縮
		output->resize(hdr.uncompressed_size);
		memcpy(&(*output)[0], data_ptr, hdr.uncompressed_size);
		return true;
	}
}

// ZIPのヘッダで使用されている時刻形式を time_t に変換する
// zdate: 更新月日
// ztime: 更新時刻
static time_t Unzip__DecodeFileTime(uint16_t zdate, uint16_t ztime) {
	struct tm time;
	// http://www.tvg.ne.jp/menyukko/cauldron/dtzipformat.html
	// last mod file time タイムスタンプ(時刻)
	// ビット割り当て
	// %1111100000000000 = 0..23 (hour)
	// %0000011111100000 = 0..59 (min)
	// %0000000000011111 = 0..29 (2 seconds)
	time.tm_hour = (ztime >> 11) & 0x1F;
	time.tm_min  = (ztime >>  5) & 0x3F;
	time.tm_sec  = (ztime & 0x1F) * 2;
	// last mod file date タイムスタンプ(日付)
	// ビット割り当て
	// %1111111000000000 = 0..XX (year from 1980)
	// %0000000111100000 = 1..12 (month)
	// %0000000000011111 = 1..31 (day)
	time.tm_year = ( zdate >> 9) + 1980 - 1900; // tm_year の値は 1900 からの年数
	time.tm_mon  = ((zdate >> 5) & 0xF) - 1;
	time.tm_mday = ( zdate & 0x1F);
	//
	time_t ret = ::mktime(&time); // struct tm --> time_t
	return ret;
}

// 拡張フィールドに入っているタイムスタンプを読み取る。
// ZIPの仕様では最終更新日時のみが記録されており、作成日時、アクセス日時が知りたい場合は拡張フィールドを見る必要がある。
// タイムスタンプは識別子 0x000A の拡張フィールドに記録されている
static bool Unzip__ReadNtfsExtraField(KInputStream &input, time_t *ctime, time_t *mtime, time_t *atime) {
	K__ASSERT(ctime);
	K__ASSERT(mtime);
	K__ASSERT(atime);
	if (Unzip__CheckFileUint16(input, 0x000A)) {
		// NTFS extra field for file attributes
		// http://archimedespalimpsest.net/Documents/External/ZIPFileFormatSpecification_6.3.2.txt
		uint16_t sign;
		uint16_t size;
		input.read(&sign, 2); // 2: [NTFS extra field sign]
		input.read(&size, 2); // 2: [NTFS extra field size]
		int ntfs_end = input.tell() + size;
		{
			// ここから NTFS extra field の中
			// 4: [reseved]
			// 2: [attr tag]
			// 2: [attr len]
			// n: [attr data]
			// ... これ以降、必要なだけ [attr tag], [attr len], [attr data] を繰り返す
			//
			input.read(nullptr, 4); // [reserved]
			while (input.tell() < ntfs_end) {
				uint16_t tag;
				uint16_t len;
				input.read(&tag, 2);
				input.read(&len, 2);
				// [attr data]
				if (tag == 0x0001) {
					// NTFS 属性の 0x0001 はファイルのタイムスタンプ属性を表す
					// 8: [last modification time]
					// 8: [last access time]
					// 8: [creation time]
					uint64_t ntfs_mtime;
					uint64_t ntfs_atime;
					uint64_t ntfs_ctime;
					input.read(&ntfs_mtime, 8);
					input.read(&ntfs_atime, 8);
					input.read(&ntfs_ctime, 8);
					*mtime = Unzip__NtfsToTimeT(ntfs_mtime);
					*atime = Unzip__NtfsToTimeT(ntfs_atime);
					*ctime = Unzip__NtfsToTimeT(ntfs_ctime);
				} else {
					input.read(nullptr, len);
				}
			}
		}
		K__ASSERT(input.tell() == ntfs_end); // この時点でぴったり NTFS extra field の末尾を指しているはず
		input.seek(ntfs_end);
		return true;
	}
	return false;
}

// 現在の読み取り位置が中央ディレクトリヘッダを指していると仮定し、中央ディレクトリヘッダとコンテンツ情報を読み取る
static bool Unzip__ReadCenteralDirectoryHeaderAndEntry(KInputStream &input, SZipEntryBlock *entry) {
	K__ASSERT(entry);

	if (!Unzip__CheckFileUint32(input, ZIP_SIGN_PK0102)) {
		return false;
	}

	memset(entry, 0, sizeof(SZipEntryBlock));

	// 中央ディレクトリヘッダ
	input.read(&entry->cd_hdr, sizeof(SZipCentralDirectoryHeader));

	SZipCentralDirectoryHeader *cd = &entry->cd_hdr;

	// ローカルファイルヘッダの位置
	//
	// ※ローカルファイルヘッダは ZIP ファイルの先頭にあり、
	//   本来なら真っ先に読み取っているはずだが、
	//   一部のアーカイバが作成した ZIP ファイルでは、
	//   ローカルファイルヘッダに記述されているはずのデータサイズが 0 に設定されていて、
	//   正しく逐次読み出しすることができない。
	//   そこでローカルファイルヘッダは信用せず、
	//   中央ディレクトリヘッダにあるローカルファイルヘッダ情報を使うようにする
	//	entry->lo_hdr_offset = 0; <-- 先頭のローカルファイルヘッダは使わない
	entry->lo_hdr_offset = cd->relative_offset_of_local_header;

	// ついでにローカルファイルヘッダ自体も取得しておく
	{
		int p = input.tell();
		input.seek(entry->lo_hdr_offset);
		input.read(&entry->lo_hdr, sizeof(SZipLocalFileHeader));
		input.seek(p);
	}

	// Data Descriptor の処理
	if (entry->lo_hdr.general_purpose_bit_flag & ZIP_OPT_DATADESC) {
		// Data Descriptor が存在する。
		// この場合、Local file header の compressed_size, uncompressed_size は 0 になっている。
		// （ランダムシークできないシステムのために、データを書き込んだ後に Data Descriptor という形でファイルサイズ情報を追加している）
		// ローカルファイルヘッダのサイズ情報を補完しておく
		entry->lo_hdr.compressed_size = cd->compressed_size;
		entry->lo_hdr.uncompressed_size = cd->uncompressed_size;
	}

	// 圧縮データの先頭位置（ファイル先頭からのオフセット）
	entry->dat_offset = entry->lo_hdr_offset + sizeof(SZipLocalFileHeader) + cd->file_name_length + cd->extra_field_length;

	// タイムスタンプ
	// ZIPには各コンテンツの最終更新日時だけが入っている。
	// 作成日時、アクセス日時も取得したい場合は拡張データ (識別子 0x000A) を調べる必要がある
	entry->ctime = 0;
	entry->mtime = Unzip__DecodeFileTime(cd->last_mod_file_date, cd->last_mod_file_time);
	entry->atime = 0;
		
	// ファイル名
	if (cd->file_name_length > 0) {
		if (cd->file_name_length < ZIPEX_MAX_NAME) {
			input.read(entry->namebin, cd->file_name_length);
			entry->namebin[cd->file_name_length] = '\0';
		} else {
			ZIP_ERROR("Too long file name");
			return false;
		}
	}

	// 拡張データ
	entry->num_extras = 0;
	if (cd->extra_field_length > 0) {
		int extra_end = input.tell() + cd->extra_field_length;
		while (input.tell() < extra_end) {
			SZipExtraBlock extra;
			input.read(&extra.sign, 2);
			input.read(&extra.size, 2);
			extra.offset = input.tell();
			input.read(nullptr, extra.size); // SKIP

			// 拡張データがファイルのタイムスタンプを表している場合、その時刻を取得する
			if (extra.sign == 0x000A) {
				Unzip__ReadNtfsExtraField(input, &entry->ctime, &entry->mtime, &entry->atime);
			}

			if (entry->num_extras < ZIPEX_MAX_EXTRA) {
				entry->extras[entry->num_extras] = extra;
				entry->num_extras++;
			} else {
				ZIP_ERROR("Too many extra fields");
				return false;
			}
		}
		K__ASSERT(input.tell() == extra_end);
	}

	// コメント
	entry->comment_offset = 0;
	if (cd->file_comment_length > 0) {
		entry->comment_offset = input.tell();
		input.read(nullptr, cd->file_comment_length); // SKIP
	}

	return true;
}

// 終端レコードの先頭に移動する
static bool Unzip__SeekToEndOfCentralDirectoryRecord(KInputStream &input) {
	SZipEndOfCentralDirectoryRecord eocd;

	// ファイル末尾からシークする
	int offset = (int)(input.size() - sizeof(eocd));

	// 識別子を確認
	while (offset > 0) {
		input.seek(offset);
		if (input.readUint32() == ZIP_SIGN_PK0506) {
			// 識別子が一致したら、終端レコード全体を読む。
			// このときコメントのサイズとファイルサイズの関係に矛盾が無ければ
			// 正しい終端レコードを見つけられたものとする
			input.seek(offset);
			input.read(&eocd, sizeof(eocd));
			if (offset + sizeof(eocd) + eocd.comment_length == input.size()) {
				// 辻褄が合う。OK
				// レコード先頭に戻しておく
				input.seek(offset);
				return true;
			}
		}
		offset--;
	}
	ZIP_ERROR("Failed to find an End of Centeral Directory Record");
	return false;
}

// 中央ディレクトリヘッダの先頭に移動する
static bool Unzip__SeekToCentralDirectoryHeader(KInputStream &input) {
	// 終端レコードまでシーク
	if (! Unzip__SeekToEndOfCentralDirectoryRecord(input)) {
		ZIP_ERROR("Failed to find a Centeral Directory Record");
		return false;
	}

	// 終端レコードを読む
	SZipEndOfCentralDirectoryRecord hdr;
	input.read(&hdr, sizeof(SZipEndOfCentralDirectoryRecord));

	// 終端レコードには中央ディレクトリヘッダの位置が記録されているので、その値を使ってシークする
	input.seek(0);
	input.seek(hdr.startpos);

	// 中央ディレクトリヘッダの識別子を確認
	if (!Unzip__CheckFileUint32(input, ZIP_SIGN_PK0102)) {
		ZIP_ERROR("Failed to find a Centeral Directory Record");
		return false;
	}
	return true;
}

// ZIPファイル全体に対するコメントを得る
// comment_bin コメント文字列。文字コード変換などは何も行わず、ZIPに記録されているバイト列をそのままセットする
static bool Unzip__GetZipFileComment(KInputStream &input, std::string *comment_bin) {

	// End of central directory record までシークする
	if (! Unzip__SeekToEndOfCentralDirectoryRecord(input)) {
		return false;
	}

	// End of central directory record を得る
	if (!Unzip__CheckFileUint32(input, ZIP_SIGN_PK0506)) {
		return false;
	}

	SZipEndOfCentralDirectoryRecord hdr;
	input.read(&hdr, sizeof(SZipEndOfCentralDirectoryRecord));

	// コメントの存在を確認
	if (hdr.comment_length == 0) {
		return false;
	}

	// コメント文字列を取得
	if (comment_bin) {
		*comment_bin = input.readBin(hdr.comment_length);
	}
	return true;
}
#pragma endregion // unzip




class CZipWriterImpl {
	std::vector<SZipEntryWritingParams> m_Entries;
	std::string m_Password;
	KOutputStream m_Output;
	int m_CentralDirectoryHeaderOffset;
	int m_CompressLevel;
public:
	CZipWriterImpl() {
		clear();
	}
	void clear() {
		m_Entries.clear();
		m_Password.clear();
		m_Output = KOutputStream();
		m_CentralDirectoryHeaderOffset = 0;
		m_CompressLevel = -1;
	}
	bool isOpen() {
		return m_Output.isOpen();
	}
	void setOutput(KOutputStream &output) {
		clear();
		m_Output = output;
	}
	void setCompressLevel(int level) {
		if (level < -1) {
			m_CompressLevel = -1;
		} else if (level > 9) {
			m_CompressLevel = 9;
		} else {
			m_CompressLevel = level;
		}
	}
	void setPassword(const char *password) {
		m_Password = password;
	}
	// アーカイブにファイルを追加する。
	// @param password 暗号化パスワード。暗号化しない場合は nullptr または "" を指定する
	// @param times    タイムスタンプ。3要素から成る times_t 配列を指定する。creation, modification, access の順番で格納する。タイムスタンプ不要ならば nullptr 出もよい
	// @param attr     ファイル属性。デフォルトは 0
	bool addEntry(const char *name_u8, const void *data, int size, const time_t *timestamp_cma, int file_attr) {
		K__ASSERT(name_u8);
		if (strlen(name_u8) == 0) {
			// 名前必須
			ZIP_ERROR("Invliad name");
			return false;
		}
		// 名前チェック
		if (strstr(name_u8, "..\\") || strstr(name_u8, "../")) {
			ZIP_ERROR("Do not contain relative path");
			return false;
		}
		if (strstr(name_u8, ":")) {
			ZIP_ERROR("Do not contain drive path");
			return false;
		}

		// 元データを用意
		std::string bin;
		if (size < 0) {
			bin = (const char *)data; // null terminated string
		} else if (size > 0) {
			bin.resize(size);
			memcpy(&bin[0], data, size);
		}

		SZipEntryWritingParams params;
		params.namebin = name_u8;
		params.is_name_utf8 = true;
		params.data = bin;
		params.file_attr = 0;
		if (timestamp_cma) {
			params.ctime = timestamp_cma[0];
			params.mtime = timestamp_cma[1];
			params.atime = timestamp_cma[2];
		} else {
			params.ctime = 0;
			params.mtime = 0;
			params.atime = 0;
		} 
		params.password = m_Password.c_str();
		params.level = m_CompressLevel;
		Zip__WriteEntry(m_Output, params);

		m_Entries.push_back(params);
		return true;
	}
	void finalize(const char *comment, int size) {
		add_central_directories();
		add_end_of_central_directory_record(comment, (size >= 0) ? size : strlen(comment));
	}
private:
	// Central directory header を追加
	void add_central_directories() {
		m_CentralDirectoryHeaderOffset = m_Output.tell(); // 中央ディレクトリの開始位置を記録しておく
		for (size_t i=0; i<m_Entries.size(); ++i) {
			Zip__WriteCentralDirectoryHeader(m_Output, m_Entries[i]);
		}
	}
	// End of central directory record を追加
	void add_end_of_central_directory_record(const char *comment, int commentsize) {
		Zip__WriteEndOfCentralDirectoryHeaderAndComment(
			m_Output,
			m_CentralDirectoryHeaderOffset, 
			&m_Entries[0], 
			(int)m_Entries.size(), 
			comment,
			commentsize
		);
	}
};



#pragma region KZipper
KZipper::KZipper() {
	CZipWriterImpl *impl = new CZipWriterImpl();
	m_Impl = std::shared_ptr<CZipWriterImpl>(impl);
}
KZipper::KZipper(KOutputStream &output) { 
	CZipWriterImpl *impl = new CZipWriterImpl();
	m_Impl = std::shared_ptr<CZipWriterImpl>(impl);
	m_Impl->setOutput(output);
}
void KZipper::open(KOutputStream &output) {
	m_Impl->setOutput(output);
}
bool KZipper::isOpen() {
	return m_Impl->isOpen();
}
void KZipper::setCompressLevel(int level) {
	m_Impl->setCompressLevel(level);
}
void KZipper::setPassword(const char *password) {
	m_Impl->setPassword(password);
}
bool KZipper::addEntry(const char *name_u8, const void *data, int size, const time_t *time_cma, int file_attr) {
	return m_Impl->addEntry(name_u8, data, size, time_cma, file_attr);
}
void KZipper::finalize(const char *comment, int commentsize) {
	m_Impl->finalize(comment, commentsize);
}
#pragma endregion // KZipper






class CZipReaderImpl {
	std::vector<SZipEntryBlock> m_Entries;
	KInputStream m_Input;
public:
	CZipReaderImpl() {
		clear();
	}
	~CZipReaderImpl() {
	}
	void clear() {
		m_Entries.clear();
		m_Input = KInputStream();
	}
	void setInput(KInputStream &input) {
		clear();
		m_Input = input;
		
		// 中央ディレクトリヘッダまでシーク
		Unzip__SeekToCentralDirectoryHeader(m_Input);

		// 中央ディレクトリヘッダ及びコンテンツ情報を読み取る
		while (Unzip__CheckFileUint32(m_Input, ZIP_SIGN_PK0102)) {
			SZipEntryBlock entry;
			Unzip__ReadCenteralDirectoryHeaderAndEntry(m_Input, &entry);
			m_Entries.push_back(entry);
		}
	}
	bool isOpen() {
		return m_Input.isOpen();
	}
	int getEntryCount() const {
		return (int)m_Entries.size();
	}
	int getEntryName(int file_index, std::string *bin) {
		const SZipEntryBlock *entry = get_entry(file_index);
		if (entry) {
			if (bin) *bin = entry->namebin;
			return strlen(entry->namebin);
		}
		return 0;
	}
	int getEntryComment(int file_index, std::string *bin) {
		const SZipEntryBlock *entry = get_entry(file_index);
		int size = 0;
		if (entry) {
			size = Unzip__GetComment(m_Input, entry, bin);
		}
		return size;
	}
	int getEntryExtra(int file_index, int extra_index, uint16_t *out_sign, std::string *out_bin) {
		const SZipEntryBlock *entry = get_entry(file_index);
		int size = 0;
		if (entry) {
			if (0 <= extra_index && extra_index < (int)entry->num_extras) {
				const SZipExtraBlock *extra = &entry->extras[extra_index];
				if (out_sign) {
					*out_sign = extra->sign;
				}
				size = Unzip__GetExtra(m_Input, extra, out_bin);
			}
		}
		return size;
	}
	bool getEntryTimeStamp(int file_index, time_t *time_cma) {
		const SZipEntryBlock *entry = get_entry(file_index);
		if (entry) {
			time_cma[0] = entry->ctime; // create
			time_cma[1] = entry->mtime; // modify
			time_cma[2] = entry->atime; // access
			return true;
		}
		return false;
	}
	int getEntryParamInt(int file_index, KUnzipper::INFO info) {
		const SZipEntryBlock *entry = get_entry(file_index);
		if (entry == nullptr) {
			return 0;
		}
		switch (info) {
		case KUnzipper::WITH_PASSWORD:
			return Unzip__HasOption(entry, ZIP_OPT_ENCRYPTED) ? 1 : 0;

		case KUnzipper::WITH_UTF8:
			return Unzip__HasOption(entry, ZIP_OPT_UTF8) ? 1 : 0;

		case KUnzipper::UNZIP_SIZE:
			return Unzip__GetUnzipSize(entry);

		case KUnzipper::FILE_ATTR:
			return entry->cd_hdr.external_file_attributes;

		case KUnzipper::EXTRA_COUNT:
			return entry->num_extras;
		}
		return 0;
	}
	bool getEntryData(int file_index, const char *password, std::string *bin) {
		const SZipEntryBlock *entry = get_entry(file_index);
		if (entry) {
			return Unzip__UnzipEntry(m_Input, entry, password, bin);
		}
		return false;
	}
	int getComment(std::string *bin) {
		return Unzip__GetZipFileComment(m_Input, bin);
	}
private:
	const SZipEntryBlock * get_entry(int index) const {
		if (0 <= index && index < (int)m_Entries.size()) {
			return &m_Entries[index];
		}
		return nullptr;
	}
};


#pragma region KUnzipper
KUnzipper::KUnzipper() {
	CZipReaderImpl *impl = new CZipReaderImpl();
	m_Impl = std::shared_ptr<CZipReaderImpl>(impl);
}
KUnzipper::KUnzipper(KInputStream &input) {
	CZipReaderImpl *impl = new CZipReaderImpl();
	m_Impl = std::shared_ptr<CZipReaderImpl>(impl);
	m_Impl->setInput(input);
}
void KUnzipper::open(KInputStream &input) {
	m_Impl->setInput(input);
}
bool KUnzipper::isOpen() {
	return m_Impl->isOpen();
}
int KUnzipper::getEntryCount() const {
	return m_Impl->getEntryCount();
}
int KUnzipper::getEntryName(int file_index, std::string *out_bin) {
	return m_Impl->getEntryName(file_index, out_bin);
}
bool KUnzipper::getEntryData(int file_index, const char *password, std::string *out_bin) {
	return m_Impl->getEntryData(file_index, password, out_bin);
}
int KUnzipper::getEntryComment(int file_index, std::string *out_bin) {
	return m_Impl->getEntryComment(file_index, out_bin);
}
int KUnzipper::getEntryExtra(int file_index, int extra_index, uint16_t *out_sign, std::string *out_bin) {
	return m_Impl->getEntryExtra(file_index, extra_index, out_sign, out_bin);
}
bool KUnzipper::getEntryTimeStamp(int file_index, time_t *time_cma) {
	return m_Impl->getEntryTimeStamp(file_index, time_cma);
}
int KUnzipper::getEntryParamInt(int file_index, INFO flag) {
	return m_Impl->getEntryParamInt(file_index, flag);
}
int KUnzipper::getComment(std::string *out_bin) {
	return m_Impl->getComment(out_bin);
}
#pragma endregion // KUnzipper





namespace Test {

void Test_zip(const char *output_dir) {
	const std::string name1 = K::pathJoin(output_dir, "Test_zip(plain).zip");
	const std::string name2 = K::pathJoin(output_dir, "Test_zip(password).zip");
	const std::string name3 = K::pathJoin(output_dir, "Test_zip(each_file_has_deferent_passwords).zip");

	const char *jptest1 = "日本語テキスト\n";
	const char *jptest2 = "SJISでのダメ文字を含む日本語ファイル名を付けてみる\n";

	// 通常書き込み
	{
		KOutputStream file;
		file.openFileName(name1.c_str());
		KZipper zw(file);
		zw.addEntry("file1.txt", "This is file1.\n", -1, nullptr, 0);
		zw.addEntry("file2.txt", "This is file2.\n", -1, nullptr, 0);
		zw.addEntry("file3.txt", "This is file3.\n", -1, nullptr, 0);
		zw.addEntry(u8"日本語ファイル名.txt", jptest1, -1, nullptr, 0);
		zw.addEntry(u8"日本語ソ連表\\ファイル.txt", jptest2, -1, nullptr, 0);
		zw.addEntry("sub/file4.txt", "This is file4 in a subdirectory.\n", -1, nullptr, 0);
		zw.finalize("COMMENT", -1);
	}

	// パスワード付きで保存
	{
		const char *password = "helloworld";
		KOutputStream file;
		file.openFileName(name2.c_str());
		KZipper zw(file);
		zw.setPassword(password);
		zw.addEntry("file1_(password_helloworld).txt", "This is file1.\n", -1, nullptr, 0);
		zw.addEntry("file2_(password_helloworld).txt", "This is file2.\n", -1, nullptr, 0);
		zw.addEntry("file3_(password_helloworld).txt", "This is file3.\n", -1, nullptr, 0);
		zw.finalize(nullptr, 0);
	}
	
	// 個別のパスワード付きで保存
	{
		KOutputStream file;
		file.openFileName(name3.c_str());
		KZipper zw(file);
		zw.setPassword("helloworld"); zw.addEntry("file1_(password_helloworld).txt", "This is file1.\n", -1, nullptr, 0);
		zw.setPassword("deadbeef");   zw.addEntry("file2_(password_deadbeef).txt",   "This is file2.\n", -1, nullptr, 0);
		zw.setPassword("am1242");     zw.addEntry("file3_(password_am1242).txt",     "This is file3.\n", -1, nullptr, 0);
		zw.finalize(nullptr, 0);
	}

	// 復元
	{
		KInputStream file;
		file.openFileName(name1.c_str());
		KUnzipper zr(file);
		std::string bin;
		std::string name;
		zr.getEntryName(0, &name); zr.getEntryData(0, nullptr, &bin); K__VERIFY(name.compare("file1.txt")==0); K__VERIFY(bin.compare("This is file1.\n") == 0);
		zr.getEntryName(1, &name); zr.getEntryData(1, nullptr, &bin); K__VERIFY(name.compare("file2.txt")==0); K__VERIFY(bin.compare("This is file2.\n") == 0);
		zr.getEntryName(2, &name); zr.getEntryData(2, nullptr, &bin); K__VERIFY(name.compare("file3.txt")==0); K__VERIFY(bin.compare("This is file3.\n") == 0);
		zr.getEntryName(3, &name); zr.getEntryData(3, nullptr, &bin); K__VERIFY(name.compare(u8"日本語ファイル名.txt")==0); K__VERIFY(bin.compare(jptest1) == 0); // フォルダ名まで含めて検索
		zr.getEntryName(4, &name); zr.getEntryData(4, nullptr, &bin); K__VERIFY(name.compare(u8"日本語ソ連表\\ファイル.txt")==0); K__VERIFY(bin.compare(jptest2) == 0); // フォルダ部分を無視して検索
		zr.getEntryName(5, &name); zr.getEntryData(5, nullptr, &bin); K__VERIFY(name.compare("sub/file4.txt")==0); K__VERIFY(bin.compare("This is file4 in a subdirectory.\n") == 0);
		zr.getComment(&bin); K__VERIFY(bin.compare("COMMENT") == 0);
	}

	// 復元（パスワード付き）
	{
		KInputStream file;
		file.openFileName(name2.c_str());
		KUnzipper zr(file);
		std::string bin;
		zr.getEntryData(0, "helloworld", &bin); K__VERIFY(bin.compare("This is file1.\n") == 0);
		zr.getEntryData(1, "helloworld", &bin); K__VERIFY(bin.compare("This is file2.\n") == 0);
		zr.getEntryData(2, "helloworld", &bin); K__VERIFY(bin.compare("This is file3.\n") == 0);
	}

	// 復元（個別のパスワード）
	{
		KInputStream file;
		file.openFileName(name3.c_str());
		KUnzipper zr(file);
		std::string bin;
		zr.getEntryData(0, "helloworld", &bin); K__VERIFY(bin.compare("This is file1.\n") == 0);
		zr.getEntryData(1, "deadbeef", &bin);   K__VERIFY(bin.compare("This is file2.\n") == 0);
		zr.getEntryData(2, "am1242", &bin);     K__VERIFY(bin.compare("This is file3.\n") == 0);
	}
}

} // Test

#pragma endregion // ZIP


} // namespace
