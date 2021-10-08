#pragma once
#include <string>
#include <vector>

namespace Kamilo {

class KNamedValues;

class KUserData {
public:
	static void install();
	static void uninstall();
	static void clearValues(); /// すべての値を削除する
	static void clearValuesByTag(int tag); /// 指定されたタグの値をすべて削除する
	static void clearValuesByPrefix(const char *prefix); /// 指定された文字で始まる名前の値をすべて削除する
	static std::string getString(const std::string &key); /// 文字列を得る
	static std::string getString(const std::string &key, const std::string &def); /// 文字列を得る
	static void setString(const std::string &key, const std::string &val, int tag=0); /// 文字列を設定する
	static bool hasKey(const std::string &key); /// キーが存在するかどうか
	static int getKeys(std::vector<std::string> *p_keys); /// 全てのキーを得る
	static int getInt(const std::string &key, int def=0);
	static void setInt(const std::string &key, int val, int tag=0);

	/// 値を保存する。password に文字列を指定した場合はファイルを暗号化する
	static bool saveToFile(const std::string &filename, const char *password="");
	static bool saveToFileCompress(const std::string &filename);

	/// 値をロードする。暗号化されている場合は password を指定する必要がある
	static bool loadFromFile(const std::string &filename, const char *password="");
	static bool loadFromFileCompress(const std::string &filename);

	/// 実際にロードせずに、保存内容だけを見る
	static bool peekFile(const std::string &filename, const char *password, KNamedValues *nv);
};


} // namespace
