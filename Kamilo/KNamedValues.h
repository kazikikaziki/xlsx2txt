#pragma once
#include "KRef.h"
#include "KLog.h"
#include "KStream.h"
#include "KXml.h"
#include "KMath.h"

namespace Kamilo {

class CNamedValuesImpl; // internal

/// 「名前＝値」の形式のデータを順序無しで扱う
///
/// - 順序を考慮する場合は KOrderedParameters を使う。
/// - 行と列から成る二次元的なテーブルを扱いたい場合は KTable を使う
class KNamedValues {
public:
	KNamedValues();
	int size() const;
	void clear();
	void remove(const std::string &name);
	bool loadFromString(const std::string &xml_u8, const std::string &filename);
	bool loadFromXml(KXmlElement *elm, bool pack_in_attr=false);
	bool loadFromFile(const std::string &filename);
	void saveToFile(const std::string &filename, bool pack_in_attr=false) const;
	void saveToXml(KXmlElement *elm, bool pack_in_attr=false) const;
	std::string saveToString(bool pack_in_attr=false) const;
	const std::string & getName(int index) const;
	const std::string & getString(int index) const;
	void setString(const std::string &name, const std::string &value);
	KNamedValues clone() const;
	void append(const KNamedValues &nv);
	int find(const std::string &name) const;
	bool contains(const std::string &name) const;
	const std::string & getString(const std::string &name) const;
	const std::string & getString(const std::string &name, const std::string &defaultValue) const;

	void setBool(const std::string &name, bool value);
	bool queryBool(const std::string &name, bool *outValue) const;
	bool getBool(const std::string &name, bool defaultValue=false) const;
	
	void setInteger(const std::string &name, int value);
	bool queryInteger(const std::string &name, int *outValue) const;
	int getInteger(const std::string &name, int defaultValue=0) const;

	void setUInt(const std::string &name, unsigned int value);
	bool queryUInt(const std::string &name, unsigned int *outValue) const;
	int getUInt(const std::string &name, unsigned int defaultValue=0) const;

	void setFloat(const std::string &name, float value);
	bool queryFloat(const std::string &name, float *outValue) const;
	float getFloat(const std::string &name, float defaultValue=0.0f) const;

	void setFloatArray(const std::string &name, const float *values, int count);
	int getFloatArray(const std::string &name, float *outValues, int maxout) const;

	void setBinary(const std::string &name, const void *data, int size);
	int getBinary(const std::string &name, void *out, int maxsize) const;

private:
	int _size() const;
	void _clear();
	void _remove(const std::string &name);
	const std::string & _getName(int index) const;
	const std::string & _getString(int index) const;
	void _setString(const std::string &name, const std::string &value);
	int _find(const std::string &name) const;
	typedef std::pair<std::string, std::string> Pair;
	std::vector<Pair> m_Items;
};


} // namespace
