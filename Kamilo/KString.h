/// @file
/// Copyright (c) 2020 Heliodor
/// This software is released under the MIT License.
/// http://opensource.org/licenses/mit-license.php

#pragma once
#include <string>
#include <vector>
#include <stdarg.h> // va_list

#ifndef __nodiscard__
#	define __nodiscard__  [[nodiscard]]
#endif


namespace Kamilo {

static const char *K_TOKEN_DELIMITERS = "\n\r\t ";
static const char *K_UTF8BOM_STR = "\xEF\xBB\xBF";
static const int   K_UTF8BOM_LEN = 3;

class KStringUtils {
public:
	/// strtol を簡略化したもの
	static int toInt(const char *s, int def=0);
	static int toInt(const std::string &s, int def=0);

	/// strtol を簡略化したもの。
	///
	/// 整数への変換が成功すれば val に値をセットして true を返す。
	/// 失敗した場合は何もせずに false を返す
	static bool toIntTry(const char *s, int *val);
	static bool toIntTry(const std::string &s, int *val);

	/// strtof を簡略化したもの
	static float toFloat(const char *s, float def=0.0f);
	static float toFloat(const std::string &s, float def=0.0f);

	/// strtof を簡略化したもの。
	///
	/// 数値への変換が成功すれば val に値をセットして true を返す。
	/// 失敗した場合は何もせずに false を返す
	static bool toFloatTry(const char *s, float *val);
	static bool toFloatTry(const std::string &s, float *val);

	/// strtoul を簡略化したもの
	static uint32_t toUint(const char *s);
	static uint32_t toUint(const std::string &s);

	/// strtoul を簡略化したもの
	///
	/// 数値への変換が成功すれば val に値をセットして true を返す。
	/// 失敗した場合は何もせずに false を返す
	static bool toUintTry(const char *s, uint32_t *val);
	static bool toUintTry(const std::string &s, uint32_t *val);

	static bool hexToUintTry(const char *s, uint32_t *val);

	/// _strtoui64 を簡略化したもの
	///
	/// 数値への変換が成功すれば val に値をセットして true を返す。
	/// 失敗した場合は何もせずに false を返す
	static bool toUint64Try(const char *s, uint64_t *val);
	static bool toUint64Try(const std::string &s, uint64_t *val);


	static bool isEmpty(const char *s);
	static bool trim(char *s);
	static bool trim(std::string &s);
	static bool equals(const char *s1, const char *s2);

	static uint32_t gethash(const char *s, int len=0);
};




#pragma region Path
#if 0
class KPathUtils {
public:
	static const int MAX_SIZE = 300;

	// 末尾の区切り文字を取り除き、適切な区切り文字に変換した文字列を得る
	static void K_PathNormalize(char *path_u8);
	static void K_PathNormalize(char *out_path, int out_size, const char *path_u8);

	// 末尾の区切り文字を取り除き、指定した区切り文字に変換した文字列を得る
	static void K_PathNormalizeEx(char *path_u8, char old_delim, char new_delim);
	static void K_PathNormalizeEx(char *out_path, int out_size, const char *path_u8, char old_delim, char new_delim);

	/// パスの末尾にサブパスを追加する
	static void K_PathPushLast(char *path_u8, int size, const char *more);
	static void K_PathPushLast(char *out_path, int out_size, const char *path, const char *more);

	/// パスの末尾に拡張子を追加する
	static void K_PathPushExt(char *path_u8, int size, const char *ext);
	static void K_PathPushExt(char *out_path, int out_size, const char *path, const char *ext);

	/// パス末尾にあるサブパスを取り除く
	static void K_PathPopLast(char *path_u8);
	static void K_PathPopLast(char *out_path, int out_size, const char *path);

	/// パス末尾にある拡張子を取り除く
	static void K_PathPopExt(char *path_u8);
	static void K_PathPopExt(char *out_path, int out_size, const char *path);

	/// パス末尾にあるサブパスの先頭部分を返す
	static const char * K_PathGetLast(const char *path_u8);
	static char * K_PathGetLast(char *path_u8);

	/// パス末尾の拡張子部分を返す (ドットを含む)
	static const char * K_PathGetExt(const char *path_u8);
	static char * K_PathGetExt(char *path_u8);

	/// パス区切り文字を含んでいる場合は true
	static bool K_PathHasDelim(const char *path_u8);

	/// パスの末尾が / で終わっている場合、それを取り除く
	static void K_PathRemoveLastDelim(char *path_u8);
	static void K_PathRemoveLastDelim(char *out_path, int out_size, const char *path);

	/// パスの末尾が / で終わるようにする
	static void K_PathAppendLastDelim(char *path_u8, int size);
	static void K_PathAppendLastDelim(char *out_path, int out_size, char *path);

	/// 2つのパスの先頭部分に含まれる共通のサブパスの文字列長さを得る（区切り文字を含む）
	static int K_PathGetCommonSize(const char *pathA, const char *pathB);

	/// base から path への相対パスを得る
	static void K_PathGetRelative(char *out_path, int out_size, const char *path_u8, const char *base_u8);

	/// 完全なパスを得る
	static bool K_PathGetFullPath(char *out_path, int out_size, const char *path_u8);

	/// パスが実在し、かつディレクトリかどうか調べる
	static bool K_PathIsDir(const char *path_u8);

	/// パスが実在し、かつ非ディレクトリかどうか調べる
	static bool K_PathIsFile(const char *path_u8);

	/// パスが実在するか調べる
	static bool K_PathExists(const char *path_u8);

	/// パスで指定されたファイルのタイムスタンプを得る
	static bool K_PathGetTimeStamp(const char *path_u8, time_t *out_time_cma);

	static time_t K_PathGetTimeStamp_Creation(const char *path_u8) { time_t cma[3]; K_PathGetTimeStamp(path_u8, cma); return cma[0]; }
	static time_t K_PathGetTimeStamp_Modify(const char *path_u8)   { time_t cma[3]; K_PathGetTimeStamp(path_u8, cma); return cma[1]; }
	static time_t K_PathGetTimeStamp_Access(const char *path_u8)   { time_t cma[3]; K_PathGetTimeStamp(path_u8, cma); return cma[2]; }
};
#endif
#pragma endregion // Path



#pragma region KStringView, KString
// StringView は文字列のコピーを持たず、参照だけを持つ文字列。
// 既存の文字列に対して読み取り専用の操作だけを行いたい場合に使う
class KStringView {
public:
	KStringView();
	KStringView(const char *s);
	KStringView(const char *s, int len);
	KStringView(const char *s, const char *e);
	KStringView(const std::string &s);
	KStringView(const KStringView &s);
	virtual ~KStringView();

	char operator[](int index) const;
	bool operator==(const KStringView &s) const;
	bool operator!=(const KStringView &s) const;
	const char *data() const;
	const char *begin() const;
	const char *end() const;
	bool empty() const;
	int size() const;
	uint32_t hash() const;
	void getString(char *out, int maxsize) const;
	std::string toStdString() const;
	std::wstring toWide() const;
	std::string toAnsi(const char *_locale="") const;
	uint32_t hexToUint() const;
	bool hexToUintTry(uint32_t *result) const;
	int toInt() const;
	bool toIntTry(int *result) const;
	uint32_t toUint() const;
	bool toUintTry(uint32_t *result) const;
	uint64_t toUint64() const;
	bool toUint64Try(uint64_t *result) const;
	float toFloat() const;
	bool toFloatTry(float *result) const;
	bool startsWith(char ch) const;
	bool startsWith(const char *sub) const;
	bool endsWith(char ch) const;
	bool endsWith(const char *sub) const;
	__nodiscard__ KStringView trimUtf8Bom() const;
	__nodiscard__ KStringView trim() const;
	__nodiscard__ KStringView substr(int start, int count) const; // start から count 文字分を得る。count<0の場合は末尾までを得る
	int findChar(char c, int start=0) const;
	 int find(const char *substr, int start=0, KStringView *result_view=nullptr) const;
	 bool findRange(const char *start_tok, const char *end_tok, KStringView *inner_range, KStringView *outer_range) const;
	 int findLastChar(char c) const;
	 int findLastDelim() const;
	__nodiscard__ KStringView pathExt() const;
	__nodiscard__ KStringView pathLast() const;
	__nodiscard__ KStringView pathPopExt() const;
	__nodiscard__ KStringView pathPopLast() const;
	__nodiscard__ KStringView pathPopLastDelim() const;
	 bool eq(const KStringView &other) const;
	 int compare(const KStringView &other, bool ignore_case=false) const;
	 int pathCommonSize(const KStringView &other) const;
	 int pathCompare(const KStringView &other, bool ignore_case, bool ignore_path) const;
	 int pathCompareLast(const KStringView &last) const;
	 int pathCompareExt(const KStringView &ext) const;
	 bool pathGlob(const KStringView &pattern) const;
	__nodiscard__ std::string pathNormalized() const;
	__nodiscard__ std::string pathPushLast(const KStringView &last) const;
	__nodiscard__ std::vector<KStringView> split(const char *delims=K_TOKEN_DELIMITERS, bool condense_delims=true, bool _trim=true, int maxcount=0, KStringView *rest=nullptr) const;
	__nodiscard__ std::vector<KStringView> splitComma(int maxcount=0, KStringView *rest=nullptr) const;
	__nodiscard__ std::vector<KStringView> splitPaths(int maxcount=0, KStringView *rest=nullptr) const;
	__nodiscard__ std::vector<KStringView> splitLines(bool skip_empty_lines=true, bool _trim=true) const;
protected:
	static const int NUMSTRLEN = 32; // max numeric string length
	static bool isPathDelim(char c);
	void set_view(const char *s, int len); // len = string length (excluding null terminator)
	void set_view(const char *s, const char *e);
private:
	const char *mStr;
	int mLen;
};

typedef std::vector<KStringView> KStringViewList;

class KString {
public:
	static KString format(const char *fmt, ...);
	static KString join(const KStringView &s1, const KStringView &s2, const char *sep="");
	static KString join(const KStringView *list, int size, const char *sep="");
	static KString join(const KString &s1, const KString &s2, const char *sep="");
	static KString join(const KString *list, int size, const char *sep="");
	static KString fromWide(const std::wstring &ws);
	static KString fromAnsi(const std::string &mb, const char *_locale="");
	static KString fromBin(const std::string &bin);
public:
	KString();
	KString(const char *s);
	KString(const std::string &s);
	KString(const KStringView &s);
	KString(const KString &s);
	virtual ~KString();
	const char * c_str() const;
	bool empty() const;
	bool notEmpty() const;
	int size() const;
	std::string toStdString() const;
	std::string toAnsi(const char *_locale="") const;
	std::wstring toWide() const;
	bool equals(const char *s) const;
	bool equals(const KString &s) const;
	int find(const char *substr, int start=0) const;
	int find(const KString &substr, int start=0) const;
	int findChar(char chr, int start=0) const;
	bool startsWith(const char *substr) const;
	bool startsWith(const KString &substr) const;
	bool endsWith(const char *substr) const;
	bool endsWith(const KString &substr) const;
	int toInt(int def=0) const;
	bool toIntTry(int *val) const;
	float toFloat(float def=0.0f) const;
	bool toFloatTry(float *val) const;
	__nodiscard__ KString replace(int start, int count, const char *str) const;
	__nodiscard__ KString replace(int start, int count, const KString &str) const;
	__nodiscard__ KString replace(const char *before, const char *after) const;
	__nodiscard__ KString replace(const KString &before, const KString &after) const;
	__nodiscard__ KString replaceChar(char before, char after) const;
	__nodiscard__ KString remove(int start, int count=-1) const;
	__nodiscard__ KString subString(int start, int count=-1) const;
	__nodiscard__ KString trimUtf8Bom() const;
	__nodiscard__ KString trim() const;
	__nodiscard__ KStringView view() const;
	__nodiscard__ std::vector<KString> split(const char *delims=K_TOKEN_DELIMITERS, bool condense_delims=true, bool _trim=true, int maxcount=0, KString *rest=nullptr) const;
	__nodiscard__ std::vector<KString> splitComma(int maxcount=0, KString *rest=nullptr) const;
	__nodiscard__ std::vector<KString> splitPaths(int maxcount=0, KString *rest=nullptr) const;
	__nodiscard__ std::vector<KString> splitLines(bool skip_empty_lines=true, bool _trim=true) const;
	__nodiscard__ KString pathExt() const;
	__nodiscard__ KString pathLast() const;
	__nodiscard__ KString pathPopExt() const;
	__nodiscard__ KString pathPopLast() const;
	__nodiscard__ KString pathPopLastDelim() const;
	__nodiscard__ KString pathJoin(const KString &last) const;
	__nodiscard__ KString pathNormalized() const;
	int pathCommonSize(const KString &other) const;
	int pathCompare(const KString &other, bool ignore_case, bool ignore_path) const;
	int pathCompareLast(const KString &last) const;
	int pathCompareExt(const KString &ext) const;
	bool pathHasLast(const KString &last) const { return pathCompareLast(last) == 0; }
	bool pathHasExt(const KString &ext) const { return pathCompareExt(ext) == 0; }
	bool pathGlob(const KString &pattern) const;

	KString & operator = (const KString &s);
	char operator[](int index) const;
	bool operator == (const KString &s) const;
	bool operator != (const KString &s) const;

private:
	void _assign(const char *s, int n);
	char *mStr;
	int mLen;
};
typedef std::vector<KString> KStringList;
#pragma endregion // KStringView, KString



#pragma region KName
#define NAME_PTR 1

class KName {
public:
	KName();
	KName(const char *name);
	KName(const std::string &name);
	KName(const KName &name);
	~KName();
	bool operator == (const char *name) const;
	bool operator == (const std::string &name) const;
	bool operator == (const KName &name) const;
	bool operator != (const char *name) const;
	bool operator != (const std::string &name) const;
	bool operator != (const KName &name) const;
	bool operator < (const char *name) const;
	bool operator < (const std::string &name) const;
	bool operator < (const KName &name) const;
	const char * c_str() const;
	bool empty() const;
	size_t hash() const;

private:
#if NAME_PTR
	const char *m_ptr;
#else
	char *m_str;
	mutable uint32_t m_hash;
#endif
};

typedef std::vector<KName> KNameList;

// list が value を含んでいれば true を返す
inline bool KNameList_contains(const KNameList &list, const KName &value) {
	auto it = std::find(list.begin(), list.end(), value);
	return it != list.end();
}

// list が value を含んでいれば削除して true を返す
// 含んでいない場合は何もせずに false を返す
inline bool KNameList_erase(KNameList &list, const KName &value) {
	auto it = std::find(list.begin(), list.end(), value);
	if (it != list.end()) {
		list.erase(it);
		return true;
	}
	return false;
}


// list が value を含んでいない場合のみ、value を push_back する
// push_back したら true を返す。すでに value を含んでいた場合は false を返す
inline bool KNameList_pushback_unique(KNameList &list, const KName &value) {
	if (!KNameList_contains(list, value)) {
		list.push_back(value);
		return true;
	}
	return false;
}

// list に more を結合する（内容が重複しないようにする）
inline void KNameList_merge_unique(KNameList &list, const KNameList &more) {
	for (auto it=more.begin(); it!=more.end(); ++it) {
		KNameList_pushback_unique(list, *it);
	}
}
#pragma endregion // KName




#pragma region KPath
class KPath;
typedef std::vector<KPath> KPathList;

class KPath {
public:
#ifdef _WIN32
	static const char NATIVE_DELIM = '\\';
	static const char NATIVE_DELIM_W = L'\\';
#else
	static const char NATIVE_DELIM = '/';
	static const char NATIVE_DELIM_W = L'/';
#endif
	static const int SIZE = 260;
	static const KPath Empty; // 空のパス。KPath("") や KPath() と等しい
	static KPath fromAnsi(const char *mb, const char *locale);
	static KPath fromUtf8(const char *u8);
	static KPath fromFormat(const char *fmt, ...);
	
	KPath();
	KPath(const char *u8);
	KPath(const std::string &s);
	KPath(const wchar_t *s);
	KPath(const std::wstring &s);
	KPath(const KPath &s);

	void clear();
	bool empty() const;
	size_t size() const;

	bool operator ==(const KPath &p) const;
	bool operator !=(const KPath &p) const;
	bool operator < (const KPath &p) const;

	/// 内部文字列のハッシュを返す
	size_t hash() const;

	/// ファイルパスのディレクトリ名部分を得る
	KPath directory() const;
	bool hasDirectory() const;

	/// ファイルパスのファイル名部分を得る（ディレクトリを含まない）
	KPath filename() const;

	/// ファイルパスの拡張子部分を得る
	/// デフォルト状態ではピリオドを含んだ拡張子を返すが、
	/// strip_period を true にするとピリオドを取り除いた拡張子を返す
	KPath extension(bool strip_period=false) const;

	/// 区切り文字を挟んで追加する
	KPath join(const KPath &more) const;
	KPath join(const KPathList &more) const;
	
	/// 単なる文字列として s を連結する
	KPath joinString(const KPath &s) const;
	KPath joinString(const char *s) const;

	/// ファイルパスの拡張子部分を別の拡張子で置換したものを返す。
	/// ext には新しい拡張子をピリオドを含めて指定する
	KPath changeExtension(const KPath &ext) const;
	KPath changeExtension(const char *ext) const;

	bool hasExtension(const KPath &ext) const;
	bool hasExtension(const char *ext) const;

	/// 自分が相対パスであれば、カレントディレクトリを元にして得られた絶対パスを返す
	KPath getFullPath() const;

	/// 共通パス部分を返す
	KPath getCommonPath(const KPath &orther) const;
	int getCommonPath(const KPath &other, KPathList *list) const;

	/// basedir からの相対パスを返す
	/// @code
	/// KPath("q/aaa/bbb.c").getRelativePathFrom("q/aaa"    ) ===> "bbb.c"
	/// KPath("q/aaa/bbb.c").getRelativePathFrom("q/aaa/ccc") ===> "../bbb.c"
	/// KPath("q/aaa/bbb.c").getRelativePathFrom("q/bbb"    ) ===> "../aaa/bbb.c"
	/// KPath("q/aaa/bbb.c").getRelativePathFrom("q/bbb/ccc") ===> "../../aaa/bbb.c"
	/// @endcode
	KPath getRelativePathFrom(const KPath &basedir) const;

	/// 要素ごとに分解したリストを返す
	/// 要素ではなく、文字単位で分割したい場合は普通の文字列関数 KToken を使う
	size_t split(KPathList *list) const;

	/// ファイルパスが相対パス形式かどうか
	bool isRelative() const;

	/// 現在のロケールにしたがってマルチバイト文字列に変換したものを返す
	std::string toAnsiString(char sep, const char *_locale) const;

	/// utf8 文字列を返す。BOM (バイトオーダーマーク) は付かない
	std::string toUtf8(char sep='/') const;

	/// ワイド文字列に変換したものを返す
	std::wstring toWideString(char sep='/') const;

	void getNativeWideString(wchar_t *s, size_t max_wchars) const;
	void getNativeAnsiString(char *s, size_t maxsize) const;

	/// パスが sub パスで始まっているかどうかを、パス単位で調べる
	bool startsWithPath(const KPath &sub) const;

	/// パスが sub パスで終わっているかどうかを、パス単位で調べる
	bool endsWithPath(const KPath &sub) const;

	/// パス文字列が s で始まっているかどうかを調べる
	bool startsWithString(const char *sub) const;

	/// パス文字列が s で終わっているかどうかを調べる
	bool endsWithString(const char *sub) const;

	/// パス同士を比較する
	int compare(const KPath &path) const;
	int compare(const KPath &path, bool ignore_case, bool ignore_path) const;

	/// 内部文字列の表現と直接比較する。
	/// str がパス区切りを含む場合は注意。パス区切りは内部で自動変換されている可能性がある
	int compare_str(const char *str) const;
	int compare_str(const char *str, bool ignore_case, bool ignore_path) const;

	/// 簡易版の glob テストを行い、ファイル名がパターンと一致しているかどうかを調べる
	///
	/// ワイルドカードは * のみ利用可能。
	/// ワイルドカード * はパス区切り記号 / とは一致しない。
	/// @code
	/// KPath("abc").glob("*") ===> true
	/// KPath("abc").glob("a*") ===> true
	/// KPath("abc").glob("ab*") ===> true
	/// KPath("abc").glob("abc*") ===> true
	/// KPath("abc").glob("a*c") ===> true
	/// KPath("abc").glob("*abc") ===> true
	/// KPath("abc").glob("*bc") ===> true
	/// KPath("abc").glob("*c") ===> true
	/// KPath("abc").glob("*a*b*c*") ===> true
	/// KPath("abc").glob("*bc*") ===> true
	/// KPath("abc").glob("*c*") ===> true
	/// KPath("aaa/bbb.ext").glob("a*.ext") ===> false
	/// KPath("aaa/bbb.ext").glob("a*/*.ext") ===> true
	/// KPath("aaa/bbb.ext").glob("a*/*.*t") ===> true
	/// KPath("aaa/bbb.ext").glob("aaa/*.ext") ===> true
	/// KPath("aaa/bbb.ext").glob("aaa/bbb*ext") ===> true
	/// KPath("aaa/bbb.ext").glob("aaa*bbb*ext") ===> false
	/// KPath("aaa/bbb/ccc.ext").glob("aaa/*/ccc.ext") ===> true
	/// KPath("aaa/bbb/ccc.ext").glob("aaa/*.ext") ===> false
	/// KPath("aaa/bbb.ext").glob("*.aaa") ===> false
	/// KPath("aaa/bbb.ext").glob("aaa*bbb") ===> false
	/// @endcode
	bool glob(const KPath &pattern) const;

	const char * c_str() const;
	const char * u8() const;
	char * ptr();

private:
	char m_path[SIZE];
	mutable uint32_t m_hash;
};

// list が value を含んでいれば true を返す
inline bool KPathList_contains(const KPathList &list, const KPath &value) {
	auto it = std::find(list.begin(), list.end(), value);
	return it != list.end();
}

// list が value を含んでいない場合のみ、value を push_back する
// push_back したら true を返す。すでに value を含んでいた場合は false を返す
inline bool KPathList_pushback_unique(KPathList &list, const KPath &value) {
	if (!KPathList_contains(list, value)) {
		list.push_back(value);
		return true;
	}
	return false;
}
#pragma endregion // KPath




#pragma region KToken
class KToken {
public:
	KToken();

	/// コンストラクタ。引数は tokenize() と同じ
	KToken(const char *text, const char *delims=K_TOKEN_DELIMITERS, bool condense_delims=true, size_t maxtokens=0);

	/// 文字列 text を文字集合 delim で区切り、トークン配列を得る
	///
	/// 文字列 text から区切り文字集合 delim のいずれかの文字を探し、見つかるたびにトークンとして分割していく。
	/// 得られたトークンは [] 演算子または get() で取得できる
	///
	/// @param text            入力文字列
	///
	/// @param delims          区切り文字の集合。
	///
	/// @param condense_delims 連続する区切り文字を結合するか。
	///                        例えば区切り文字が "/" の時に入力文字列 "AA/BB//CC/" を処理した場合、
	///                        区切り文字を結合するなら分割結果は {"AA", "BB", "CC"} になる。
	///                        区切り文字を結合しない場合、分割結果は {"AA", "BB", "", "CC", ""} になる
	///
	/// @param maxtokens       最大分割数。0 だと無制限。例えば 2 を指定した場合、"AA/BB/CC" の分割結果は {"AA", "BB/CC"} になる
	///
	/// @return 得られたトークンの数。 size() と同じ
	size_t tokenize(const char *text, const char *delims="\n\r\t ", bool condense_delims=true, size_t maxtokens=0);

	/// 文字列 text を行単位で分割する
	///
	/// @param text            入力文字列
	///
	/// @param with_empty_line 空白行を含めるかどうか。
	///                        true にすると空白行を無視する。
	///                        false にすると全ての行を含める。
	///                        行番号と対応させたい場合など、行を削除すると困る場合は false にする
	size_t splitLines(const char *text, bool with_empty_line=false);

	/// 文字列 text を文字集合 delim で区切り、その前後の文字列を得る。
	/// 文字列 text が delim を含んでいれば 2 個の文字列に分割されて size() は 2 になる。
	/// 文字列 text が delim を含まない場合は text 全体が一つのトークン扱いになって size() は 1 になる
	/// @param text  入力文字列
	/// @param delim 区切り文字の集合。
	/// @return 得られたトークンの数。 size() と同じ
	size_t split(const char *text, const char *delim);

	/// トークンを取得する。index が範囲外の場合は std::vector の例外が発生する
	const char * operator[](size_t index) const;

	/// トークンを取得する。index が範囲外の場合は def を返す
	const char * get(size_t index, const char *def="") const;

	/// トークンが整数表現であると仮定して、その値を得る
	/// 整数表現でなかった場合は def を返す
	int toInt(size_t index, int def=0) const;

	bool isInt(size_t index) const;

	uint32_t toUint32(size_t index, uint32_t def=0) const;

	/// トークンが実数表現であると仮定して、その値を得る
	/// 実数表現でなかった場合は def を返す
	float toFloat(size_t index, float def=0.0f) const;

	/// トークンと文字列 str を比較する
	int compare(size_t index, const char *str) const;

	/// トークン数を返す
	size_t size() const;

private:
	// 素直にトークン分けする
	static void tokenize_raw(const char *text, const char *delims, bool condense_delims, int maxdiv, std::vector<KStringView> &ranges);

	std::vector<KStringView> m_tok;
	std::string m_str;
};
#pragma endregion // KToken






/// "50%" や "10px" など単位付きの数値を解析するクラス
///
/// @snippet KString.cpp Test_numval
struct KNumval {
//	char pre[16]; ///< プレフィックス
	char num[16]; ///< 数値部 入力が "50%" だった場合、"50" になる
	char suf[16]; ///< 単位部 入力が "50%" だった場合、"%d" になる
	float numf;   ///< 数値を実数として解釈したときの値。入力が "50%" だった場合、50 になる

	KNumval();
	KNumval(const char *expr);
	void clear();
	void assign(const KNumval &source);
	bool parse(const char *expr);

	/// サフィックスがあるかどうか
	bool has_suffix(const char *suf) const;

	/// 整数値を得る
	/// percent_base: サフィックスが % だった場合、その割合に乗算する値
	/// 入力が "50" だった場合は percent_base を無視し、そのまま 50 を返す。
	/// 入力が "50%" だった場合、percent_base に 256 を指定すると 128 を返す
	int valuei(int percent_base) const;

	/// 実数を得る
	/// percent_base: サフィックスが % だった場合、その割合に乗算する値
	/// 入力が "50" だった場合は percent_base を無視し、そのまま 50.0f を返す。
	/// 入力が "50%" だった場合、percent_base に 256.0f を指定すると 128.0f を返す
	float valuef(float percent_base) const;
};

namespace Test {
void Test_str();
void Test_pathstring();
void Test_numval();
}



class KCommandLineArgs {
	KToken m_tok;
public:
	KCommandLineArgs(const char *cmdline);
	int size() const;
	const char * operator [](int index) const;
	bool contains(const char *s) const;
};


} // namespace




// std コンテナで使えるようにハッシュ関数を定義する
// namespace std 内で HASH_DECL(MyClass); }
// のように std 名前空間内で使う。MyClass にはメンバ関数 size_t hash() を定義しておく
#define K_HASH_DECL(T)                             \
	template <> struct hash<T> {                   \
		std::size_t operator()(const T &v) const { \
			return v.hash();                       \
		}                                          \
	};




namespace std {
	K_HASH_DECL(Kamilo::KName); // KName を std コンテナのキーとして使えるようにする
	K_HASH_DECL(Kamilo::KPath); // KPath を std コンテナのキーとして使えるようにする
}



