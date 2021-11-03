#pragma once
#include <string>

/// Lua
/// - MIT lisence
/// - http://www.lua.org/
#include "lua/lua.hpp"


namespace Kamilo {


class KLua {
public:
	/// バイナリ化したコードを bin に吐き出す。成功すれば LUA_OK を返す
	static int dump(lua_State *ls, std::string *bin);

	/// lua_pcall と同じ。エラーが発生した場合にスタックトレース付きのメッセージを取得できる。
	/// lua_pcall の戻り値をそのまま返す
	static int call(lua_State *ls, int nargs, int ret_args, char *out_errmsg=nullptr, size_t errsize=0);

	/// lua_resume と同じ。エラーが発生した場合にエラーメッセージを取得できる。
	/// lua_resume の戻り値をそのまま返す
	static int call_yieldable(lua_State *ls, int nargs, char *out_errmsg=nullptr, size_t errsize=0);

	/// スタックの idx 番目にテーブルがあると仮定して、その要素を巡回する。
	/// コールバック iter を呼び出す。コールバックが呼ばれた時、スタックトップにはキー文字列、その次には値が入っている。
	/// なお、コールバックが呼ばれた時にスタックにある2個の値は、lua_tostring しても安全である
	static int enum_table(lua_State *ls, int idx, void (*iter)(lua_State *, void *), void *user);

	/// 現在の呼び出しスタックを得る
	static int getcallstack(lua_State *ls, int level, char *file, size_t filelen, int *line);

	/// 現在の呼び出しスタックをログに出力する
	static int print_callstack(lua_State *ls);

	static int load(lua_State *ls, const char *code, int size, const char *debug_name, bool no_call=false);

	/// lua_tointeger の int 版
	static int toint(lua_State *ls, int idx, bool mute=false);

	/// lua_Number の float 版
	static float tofloat(lua_State *ls, int idx, bool mute=false);

	/// lua_toboolean と同じだが、整数表記も受け入れる。
	/// ※lua_toboolean は true/false の場合にしか働かない。
	static int toboolean(lua_State *ls, int idx, bool mute=false);

	static const char * gettypename(lua_State *ls, int idx);

	static std::string getglobal_stdstr(lua_State *ls, const char *key);
	static int getglobal_int(lua_State *ls, const char *key);
	static float getglobal_float(lua_State *ls, const char *key);
	static void getglobal_int4(lua_State *ls, const char *key, int *x, int *y, int *z, int *w);
	static void getglobal_float4(lua_State *ls, const char *key, float *x, float *y, float *z, float *w);
};


struct KLuappDef {
	const char *name;
	const char *value;
};

/// Luaを使ったテキストファイルのプリプロセッサ
///
/// http://lua-users.org/wiki/SimpleLuaPreprocessor
///
/// テキスト文書に埋め込まれた Lua スクリプトを処理する。
/// <b>\# で始まる行</b>(※)を lua のスクリプトコードとみなして実行する。
/// \# 以外で始まる行はそのまま出力するが \$(expr) と書いてある部分は
/// expr がそのまま Lua スクリプトであるとみなして実行し、その評価結果で置換する。
///
/// ※ <b>\# は行頭に無いといけない。インデントはできない</b>
///
/// 例えば次の3つの記述はすべて同じ結果になる：
/// @code
/// 記述１：
/// #for i=1, 3 do
///   <int>Num$(i)</int>
/// #end
///
/// 記述２：
/// #for i=1, 3 do
///   <int>$(string.format("Num%d", i))</int>
/// #end
///
/// 記述３：
/// #for i=1, 3 do
///   <int>$("Num" .. i)</int>
/// #end
///
/// 結果：
///   <int>Num1</int>
///   <int>Num2</int>
///   <int>Num3</int>
///
/// ※これはエラーになる。# が行頭に来ていない
/// <data>
///     #for i=1, 3 do
///       <int>$(i)</int>
///     #end
/// </data>
/// @endcode
///
/// @attention
/// 組み込み lua で boolean として処理されるためには、defines の value に <b>"true"</b> または <b>"false"</b> を指定すること。
/// "0" や "1" と書いてはいけない。
/// 条件分岐を <code>if x then ...</code> のように書いた場合、lua の仕様では <b>nil</b> と <b>false</b> だけが偽として評価され、
/// 数値の 0 や空文字列 "" は真として評価される。
///
/// @snippet KLua.cpp test
bool KLuapp_file(const std::string &srcfile_u8, const std::string &dstfile_u8, const std::string &name_u8, const KLuappDef *defines, int numdef);
bool KLuapp_text(std::string *dst, const std::string &src_u8, const std::string &name_u8, const KLuappDef *defines, int numdef);

namespace Test {
void Test_luapp();
}

} // namespace
