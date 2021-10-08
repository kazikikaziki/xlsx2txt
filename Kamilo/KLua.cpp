#include "KLua.h"
#include "KInternal.h"

namespace Kamilo {


#pragma region KLua
/// Lua API の lua_dump() に指定するためのコールバック関数
/// @see http://milkpot.sakura.ne.jp/lua/lua52_manual_ja.html#lua_load
static int KLua__writer(lua_State *ls, const void *p, size_t sz, void *ud) {
	K__ASSERT(ls);
	K__ASSERT(p);
	K__ASSERT(ud);
	std::string *bin = reinterpret_cast<std::string *>(ud);
	size_t off = bin->size();
	bin->resize(off + sz);
	memcpy(&bin[off], p, sz);
	return LUA_OK;
}

// 実行中のトレースバックメッセージをスタックトップに置く。
// スクリプト実行中にエラーが発生したときのコールバック関数として使う
static int KLua__push_traceback_message(lua_State *ls) {
	K__ASSERT(ls);

	int top = lua_gettop(ls);

	// スタックトップにはエラーメッセージが積まれているはずなので、それを取得
	const char *errmsg_u8 = lua_tostring(ls, top);

	if (errmsg_u8) {
		// エラーメッセージがある
		// 呼び出しスタック情報とエラーメッセージを設定する
		luaL_traceback(ls, ls, errmsg_u8, 1);
		
		// traceback 情報を push したことによりスタックサイズが変化してしまっているので、
		// 最初に積んであったエラーメッセージを削除する
		// （エラーメッセージは stacktrace にも入っている）
		lua_remove(ls, top);
		
	} else {
		// スタックトップにあるオブジェクトの文字列表現を得る
		// 可能であれば文字列化のメタメソッドを使用するため、lua_tostring ではなく luaL_tolstring をつかう
		// （スタックサイズは変化しない）
		luaL_tolstring(ls, top, nullptr);
	}

	// この関数の呼び出し前後でスタックサイズが変わっていないことを確認
	K__ASSERT(lua_gettop(ls) == top);
	
	return 1;
}
const char * KLua::gettypename(lua_State *ls, int idx) {
	int tp = lua_type(ls, idx);
	return lua_typename(ls, tp);
}
int KLua::toint(lua_State *ls, int idx, bool mute) {
	// 注意。実数に対して lua_tointeger することはできない
	// 0x80000000 を超える値を lua_tonumber するとエラーになる（符号なし整数として返還しないといけない））
	if (lua_isinteger(ls ,idx)) {
		// 整数
		// 符号なし整数を考慮する
		return (int)lua_tointeger(ls, idx);
	}
	if (lua_isnumber(ls, idx)) {
		// デフォルトでは 実数に対して lua_tointeger を実行すると失敗する。
		// この動作を変えるためには LUA_FLOORN2I = 1 を定義すればよいが、
		// 面倒なのでコード側で処理する
		// return (int)lua_tointeger(ls, idx); <-- lua_tointeger の時点で失敗するのでダメ
		return (int)lua_tonumber(ls, idx); // 一度 number で受けてから整数化する
	}

	// boolean に対する lua_tointeger は失敗するため、こちらで対処する
	if (lua_isboolean(ls, idx)) {
		return (int)lua_toboolean(ls, idx);
	}

	// number でも boolean でもない
	if (!mute) {
		luaL_error(ls, "invalid integer value");
	}
	return 0;
}
float KLua::tofloat(lua_State *ls, int idx, bool mute) {
	return (float)lua_tonumber(ls, idx);
}
int KLua::toboolean(lua_State *ls, int idx, bool mute) {
	if (lua_isboolean(ls, idx)) {
		return lua_toboolean(ls, idx);
	}
	if (lua_isnumber(ls, idx)) {
		// number に対する lua_toboolean は
		// 常に true を返すので自前で処理する
		return lua_tonumber(ls, idx) != 0.0f;
	}

	// number でも boolean でもない
	if (!mute) {
		luaL_error(ls, "invalid boolean value");
	}
	return 0;
}
int KLua::dump(lua_State *ls, std::string *bin) {
	if (lua_dump(ls, KLua__writer, bin, 0) != LUA_OK) {
		const char *msg = luaL_optstring(ls, -1, __FUNCTION__);
		K__ERROR("E_LUA_DUMP: %s", msg);
	}
	return LUA_OK;
}
int KLua::call_yieldable(lua_State *ls, int nargs, char *out_errmsg, size_t errsize) {
	K__ASSERT(ls);
	K__ASSERT(nargs >= 0);
	
	// 実行（スクリプト内から自分自身が呼ばれる再入可能性に注意）
	int res = lua_resume(ls, nullptr, nargs);

	if (res == LUA_OK || res == LUA_YIELD) {
		// 成功

	} else {
		// エラー

		// lua_resume はスタックを巻き戻さないので、そのままスタックトレース関数が使える
		KLua__push_traceback_message(ls);

		// KLua__push_traceback_message がスタックトップに置いたメッセージ文字列を得る
		const char *msg = luaL_optstring(ls, -1, ""); // luaL_optstring を使い、msg が nullptr でないことを保証する

		if (out_errmsg && errsize>0) {
			// メッセージの格納先が指定されている。
			// エラーログを流さずに、メッセージを黙って格納先に渡す。
			// これは単なるエラーメッセージなので、コピー先バッファが足りなくても
			// 単純にメッセージ末尾を破棄するだけで良い。strcpy_s ではなく strncpy_s を使う
			strncpy_s(out_errmsg, errsize, msg, strlen(msg));
			out_errmsg[errsize-1] = '\0'; // 確実に nullptr 終端する
		
		} else {
			// エラーメッセージのコピー先が指定されていない。
			// そのままエラーログに流す
			K__ERROR("E_LUA_RESUME: %s", msg);
		}
		lua_settop(ls, 0); // スタックを戻す
	}
	return res;
}
int KLua::call(lua_State *ls, int nargs, int ret_args, char *out_errmsg, size_t errsize) {
	K__ASSERT(ls);
	K__ASSERT(nargs >= 0);
	K__ASSERT(ret_args >= 0);

	// http://bocchies.hatenablog.com/entry/2013/10/29/003014
	// pcall でのエラー取得用コールバック関数を登録しておく。
	// pcall はエラーが発生した場合に積まれるトレースバックをスタックから取り除いてしまう。
	// ゆえに、lua_pcall から制御が戻って来た時には既にトレースバックは失われている。
	// トレースバック情報が知りたければ、コールバック関数で情報を横取りする必要がある
	lua_pushcfunction(ls, KLua__push_traceback_message);

	// この時点でのスタックはトップから順に
	// [エラーハンドラ] [narg個の引数] [関数]
	// になっているはず。
	// ここでスタックトップにあるエラーハンドラが pcall の邪魔になるため、これを末尾に移動する
	lua_insert(ls, -2-nargs);

	// 実行（スクリプト内から自分自身が呼ばれる再入可能性に注意）
	int res = lua_pcall(ls, nargs, ret_args, -2-nargs);
	
	if (res == LUA_OK) {
		// 成功。
		// この時点でのスタックはトップから順に
		// [ret_args個の戻り値] [エラーハンドラ]
		// になっているはず。エラーハンドラはもはや不要なので取り除いておく
		lua_remove(ls, -1-ret_args);

	} else {
		// エラー。
		// この時点でのスタックはトップから順に
		// [エラーメッセージ] [エラーハンドラ]
		// になっている。
		// スタックトップにあるエラーメッセージを得る
		const char *msg = luaL_optstring(ls, -1, ""); // luaL_optstring を使い、msg が nullptr でないことを保証する

		if (out_errmsg && errsize>0) {
			// メッセージの格納先が指定されている。
			// エラーログを流さずに、メッセージを黙って格納先に渡す。
			// これは単なるエラーメッセージなので、コピー先バッファが足りなくても
			// 単純にメッセージ末尾を破棄するだけで良い。strcpy_s ではなく strncpy_s を使う
			strncpy_s(out_errmsg, errsize, msg, strlen(msg));
			out_errmsg[errsize-1] = '\0'; // 確実に nullptr 終端する
		
		} else {
			// エラーメッセージのコピー先が指定されていない。
			// そのままエラーログに流す
			K__ERROR("E_LUA_PCALL: %s", msg);
		}
		lua_pop(ls, 1); // エラーメッセージを取り除く
		lua_pop(ls, 1); // エラーハンドラを取り除く
	}

	return res;
}
int KLua::enum_table(lua_State *ls, int idx, void (*iter)(lua_State *ls, void *user), void *user) {
	if (!lua_istable(ls, idx)) return LUA_ERRRUN;
	lua_pushvalue(ls, idx);
	lua_pushnil(ls); // 最初の lua_next のための nil をセット
	while (lua_next(ls, -2)) { // この時点で、スタック状態は [tbl][key][val] になっている
		// key は次の lua_next のためにキレイな形でスタックトップに残しておく必要があるため、コピーを積んでしておく。
		// （型が勝手に文字列に代わってしまうため、キーに対して直接 lua_tostring してはいけない）
		lua_pushvalue(ls, -2);
		iter(ls, user); // この時点で、スタック状態は [tbl][key][val][key] になっている
		lua_pop(ls, 2);
	}
	lua_pop(ls, 1); // pop table
	return LUA_OK;
}
int KLua::print_callstack(lua_State *ls) {
	char file[256] = {0};
	int line = 0;
	if (getcallstack(ls, 0, file, sizeof(file), &line) == LUA_OK) {
		int level = 0;
		K::print("Lua callstack >>>");
		while (getcallstack(ls, level, file, sizeof(file), &line) == LUA_OK ){
			K::print("[%d]  %s(%d)", level, file, line);
			level++;
		}
		K::print("<<<");
		return LUA_OK;
	}
	return LUA_ERRRUN;
}
int KLua::getcallstack(lua_State *ls, int level, char *file, size_t filelen, int *line) {
	lua_Debug ar;
	memset(&ar, 0, sizeof(ar));
	if (lua_getstack(ls, level, &ar)) {
		lua_getinfo(ls, "nSl", &ar);
		if (file) strcpy_s(file, filelen, ar.source);
		if (line) *line = ar.currentline;
		return LUA_OK;
	}
	return LUA_ERRRUN;
}
int KLua::load(lua_State *ls, const char *code, int size, const char *debug_name, bool no_call) {
	if (luaL_loadbuffer(ls, code, size, debug_name) != LUA_OK) {
		const char *msg = luaL_optstring(ls, -1, __FUNCTION__);
		K__ERROR("E_LUA: %s", msg);
		return LUA_ERRRUN;
	}
	if (!no_call && lua_pcall(ls, 0, 0, 0) != LUA_OK) {
		const char *msg = luaL_optstring(ls, -1, __FUNCTION__);
		K__ERROR("E_LUA: %s", msg);
		return LUA_ERRRUN;
	}
	return LUA_OK;
}
int KLua::getglobal_int(lua_State *ls, const char *key) {
	lua_getglobal(ls, key);
	int ret = (int)lua_tointeger(ls, -1);
	lua_pop(ls, 1);
	return ret;
}
float KLua::getglobal_float(lua_State *ls, const char *key) {
	lua_getglobal(ls, key);
	float ret = (float)lua_tonumber(ls, -1);
	lua_pop(ls, 1);
	return ret;
}
void KLua::getglobal_int4(lua_State *ls, const char *key, int *x, int *y, int *z, int *w) {
	lua_getglobal(ls, key);
	if (lua_istable(ls, -1)) {
		if (x) {
			lua_geti(ls, -1, 1);
			*x = (int)lua_tointeger(ls, -1);
			lua_pop(ls, 1);
		}
		if (y) {
			lua_geti(ls, -1, 2);
			*y = (int)lua_tointeger(ls, -1);
			lua_pop(ls, 1);
		}
		if (z) {
			lua_geti(ls, -1, 3);
			*z = (int)lua_tointeger(ls, -1);
			lua_pop(ls, 1);
		}
		if (w) {
			lua_geti(ls, -1, 4);
			*w = (int)lua_tointeger(ls, -1);
			lua_pop(ls, 1);
		}
	}
	lua_pop(ls, 1); // pop global
}
void KLua::getglobal_float4(lua_State *ls, const char *key, float *x, float *y, float *z, float *w) {
	lua_getglobal(ls, key);
	if (lua_istable(ls, -1)) {
		if (x) {
			lua_geti(ls, -1, 1);
			*x = (float)lua_tonumber(ls, -1);
			lua_pop(ls, 1);
		}
		if (y) {
			lua_geti(ls, -1, 2);
			*y = (float)lua_tonumber(ls, -1);
			lua_pop(ls, 1);
		}
		if (z) {
			lua_geti(ls, -1, 3);
			*z = (float)lua_tonumber(ls, -1);
			lua_pop(ls, 1);
		}
		if (w) {
			lua_geti(ls, -1, 4);
			*w = (float)lua_tonumber(ls, -1);
			lua_pop(ls, 1);
		}
	}
	lua_pop(ls, 1); // pop global
}

#pragma endregion // KLua



#pragma region KLuapp
// 入出力ファイル名を引数に与えると、lua側でファイルを開いて置換処理を行う。
// 処理結果の文字列を返す
static const char *KLuapp_file_source = 
	"function prep(src, dst)\n"
	"	local file = assert(io.open(src))\n"
	"	local chunk = {n=0}\n"
	"	table.insert(chunk, string.format('io.output %q', dst))\n"
	"	for line in file:lines() do\n"
	"		if string.find(line, '^#') then\n"
	"			table.insert(chunk, string.sub(line, 2) .. '\\n')\n"
	"		else\n"
	"			local last = 1\n"
	"			for text, expr, index in string.gmatch(line, '(.-)$(%b())()') do \n"
	"				last = index\n"
	"				if text ~= '' then\n"
	"					table.insert(chunk, string.format('io.write %q;', text))\n"
	"				end\n"
	"				table.insert(chunk, string.format('io.write%s;', expr))\n"
	"			end\n"
	"			table.insert(chunk, string.format('io.write %q\\n', string.sub(line, last)..'\\n'))\n"
	"		end\n"
	"	end\n"
	"	return table.concat(chunk, '\\n')\n"
	"end\n";

// テキストを引数に与えると、lua側でテキストを処理する。
// 処理結果の文字列を返す
static const char *KLuapp_text_prep = 
		"function prep(src)\n"
		"	local chunk = {n=0}\n"
		"	table.insert(chunk, 'function __CLuaPrep__()')\n"
		"	table.insert(chunk, 'local S=[[]]')\n"
		"	for line in string.gmatch(src, '(.-)\\r?\\n') do\n"
		"		if string.find(line, '^#') then\n"
		"			table.insert(chunk, string.sub(line, 2) .. '\\n')\n"
		"		else\n"
		"			local last = 1\n"
		"			for text, expr, index in string.gmatch(line, '(.-)$(%b())()') do \n"
		"				last = index\n"
		"				if text ~= '' then\n"
		"					table.insert(chunk, string.format('S = S .. %q;', text))\n"
		"				end\n"
		"				table.insert(chunk, string.format('S = S .. %s;', expr))\n"
		"			end\n"
		"			table.insert(chunk, string.format('S = S .. %q\\n', string.sub(line, last)..'\\n'))\n"
		"		end\n"
		"	end\n"
		"	table.insert(chunk, 'return S')\n"
		"	table.insert(chunk, 'end')\n"
		"	return table.concat(chunk, '\\n')\n"
		"end\n";

// 文字列で表現された値を適切な型で push する
static void KLuapp__push_typed_value(lua_State *ls, const char *val) {
	K__ASSERT(ls);
	K__ASSERT(val);
	std::string str(val);

	char *end_ptr = nullptr;

	// 空文字列の場合は無条件で文字列とする
	if (val[0] == '\0') {
		lua_pushstring(ls, "");
		return;
	}

	// 整数表記ならば int として push する
	int i = strtol(val, &end_ptr, 0);
	if (end_ptr[0] == '\0') {
		lua_pushinteger(ls, i);
		return;
	}
	// 実数表記ならば float として push する
	float f = strtof(val, &end_ptr);
	if (end_ptr[0] == '\0') {
		lua_pushnumber(ls, f);
		return;
	}
	// 組み込み値表記を処理
	if (str.compare("true") == 0) {
		lua_pushboolean(ls, 1);
		return;
	}
	if (str.compare("false") == 0) {
		lua_pushboolean(ls, 0);
		return;
	}
	if (str.compare("nil") == 0) {
		lua_pushnil(ls);
		return;
	}
	// 上記どれにも該当しないなら文字列として push する
	lua_pushstring(ls, val);
}

// 注意：
// 組み込み lua で boolean として処理されるためには、defines の setString で "true" とか "false" を指定すること。
// setBool や setInteger だと単なる 0, 1 になってしまい、組み込み lua で 
// if x then
// などのように判定している場合に意図しない動作になる。（lua で偽と判定されるのは nil, false のみ。数値の 0 は真になる）
// "true", "false" は CLuaPrep で boolean として処理される。
// また "nil" は CLuaPrep で nil として処理される。
bool KLuapp_file(const char *srcfile_u8, const char *dstfile_u8, const char *name_u8, const KLuappDef *defines, int numdef) {
	std::string generated_lua;
	bool ok = true;
	if (ok) {
		// 埋め込み lua スクリプトを処理し、テキスト展開用のスクリプトを生成する
		lua_State *ls = luaL_newstate();
		luaL_openlibs(ls);
		ok &= luaL_loadbuffer(ls, KLuapp_file_source, strlen(KLuapp_file_source), "CLuaPrep_prep") == LUA_OK;
		ok &= lua_pcall(ls, 0, 0, 0) == LUA_OK; // 一番外側のスコープを実行
		lua_getglobal(ls, "prep");
		lua_pushstring(ls, srcfile_u8);
		lua_pushstring(ls, dstfile_u8);
		ok &= lua_pcall(ls, 2, 1, 0) == LUA_OK; // 実行
		generated_lua = lua_tostring(ls, -1);
		lua_close(ls);
	}
	if (ok) {
		// テキスト展開用のスクリプトを処理
		lua_State *ls = luaL_newstate();
		luaL_openlibs(ls);
		luaL_loadbuffer(ls, generated_lua.c_str(), generated_lua.size(), name_u8);
		// 組み込み定義があるなら、グローバル変数として追加しておく
		if (defines) {
			lua_getglobal(ls, "_G");
			for (int i=0; i<numdef; i++) {
				KLuapp__push_typed_value(ls, defines[i].value);
				lua_setfield(ls, -2, defines[i].name); // pop val
			}
			lua_pop(ls, 1); // _G
		}
		ok &= lua_pcall(ls, 0, 0, 0) == LUA_OK; // 実行（チャンク全体を実行する。特に関数を呼ぶ必要はない）
		lua_close(ls);
	}
	return ok;
}
bool KLuapp_text(std::string *dst_u8, const char *src_u8, const char *name_u8, const KLuappDef *defines, int numdef) {
	K__ASSERT(dst_u8);
	K__ASSERT(dst_u8->c_str() != src_u8);
	std::string generated_lua;
	bool ok = true;
	{
		// 埋め込み lua スクリプトを処理し、テキスト展開用のスクリプトを生成する
		lua_State *ls = luaL_newstate();
		luaL_openlibs(ls);

		// チャンクをロードして評価
		ok &= luaL_loadbuffer(ls, KLuapp_text_prep, strlen(KLuapp_text_prep), "CLuaPrep_prep") == LUA_OK;
		ok &= lua_pcall(ls, 0, 0, 0) == LUA_OK;

		// prep(string.gsub(src_u8, "\r\n", "\n"))
		lua_getglobal(ls, "prep");
		luaL_gsub(ls, src_u8, "\r\n", "\n");
		ok &= lua_pcall(ls, 1, 1, 0) == LUA_OK;

		// lua のスクリプト文字列が戻り値になっているので、それを取得して終了
		generated_lua = lua_tostring(ls, -1);
		lua_close(ls);
	}

	if (ok) {
		// テキスト展開用のスクリプトを処理
		lua_State *ls = luaL_newstate();
		luaL_openlibs(ls);

		luaL_loadbuffer(ls, generated_lua.c_str(), generated_lua.size(), name_u8);
		// 組み込み定義があるなら、グローバル変数として追加しておく
		if (defines) {
			lua_getglobal(ls, "_G");
			for (int i=0; i<numdef; i++) {
				KLuapp__push_typed_value(ls, defines[i].value);
				lua_setfield(ls, -2, defines[i].name); // pop val
			}
			lua_pop(ls, 1); // _G
		}
		if (ok && lua_pcall(ls, 0, 0, 0)!=LUA_OK) { // 一番外側のスコープを実行
			const char *err = luaL_optstring(ls, -1, "");
			K__ERROR("E_LUAPP: %s", err ? err : "???");
			ok = false;
		}
		lua_getglobal(ls, "__CLuaPrep__"); // <-- 関数をゲット
		if (ok && lua_pcall(ls, 0, 1, 0)!=LUA_OK) { // 実行
			const char *err = luaL_optstring(ls, -1, "");
			K__ERROR("E_LUAPP: %s", err ? err : "???");
			ok = false;
		}
		if (ok) {
			const char *str = lua_tostring(ls, -1);
			*dst_u8 = str ? str : "";
		}
		lua_close(ls);
	}
	return ok;
}



namespace Test {
void Test_luapp() {

	// プリプロセッサ入力
	const char *i =
		"#for i=1,3 do\n"
		"data$(i)\n"
		"#end\n";

	// 期待する出力
	const char *m =
		"data1\n"
		"data2\n"
		"data3\n";

	// プリプロセッサ実行
	std::string o;
	KLuapp_text(&o, i, "Test1", nullptr, 0);

	// 出力を比較
	K__ASSERT(o.compare(m) == 0);
}
} // Test
#pragma endregion // KLuapp


} // namespace
