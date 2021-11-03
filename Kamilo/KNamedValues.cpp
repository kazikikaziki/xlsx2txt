#include "KNamedValues.h"
//
#include "KInternal.h"

namespace Kamilo {

static const std::string s_Empty;

#pragma region KNamedValues
KNamedValues::KNamedValues() {
}
int KNamedValues::_size() const {
	return (int)m_Items.size();
}
void KNamedValues::_clear() {
	m_Items.clear();
}
void KNamedValues::_remove(const std::string &name) {
	int index = find(name);
	if (index >= 0) {
		m_Items.erase(m_Items.begin() + index);
	}
}
const std::string & KNamedValues::_getName(int index) const {
	if (0 <= index && index < (int)m_Items.size()) {
		return m_Items[index].first;
	} else {
		return s_Empty;
	}
}
const std::string & KNamedValues::_getString(int index) const {
	if (0 <= index && index < (int)m_Items.size()) {
		return m_Items[index].second;
	} else {
		return s_Empty;
	}
}
void KNamedValues::_setString(const std::string &name, const std::string &value) {
	if (!name.empty()) {
		int index = find(name);
		if (index >= 0) {
			m_Items[index].second = value;
		} else {
			m_Items.push_back(Pair(name, value));
		}
	}
}
int KNamedValues::_find(const std::string &name) const {
	for (int i=0; i<(int)m_Items.size(); i++) {
		if (m_Items[i].first.compare(name) == 0) {
			return i;
		}
	}
	return -1;
}
int KNamedValues::size() const {
	return _size();
}
void KNamedValues::clear() {
	_clear();
}
void KNamedValues::remove(const std::string &name) {
	_remove(name);
}
KNamedValues KNamedValues::clone() const {
	KNamedValues nv;
	nv.append(*this);
	return nv;
}
bool KNamedValues::loadFromString(const std::string &xml_u8, const std::string &filename) {
	clear();

	bool result = false;
	KXmlElement *xml = KXmlElement::createFromString(xml_u8, filename);
	if (xml) {
		result = loadFromXml(xml->getChild(0), false);
		xml->drop();
	} else {
		K__ERROR("E_XML: Failed to load: %s", filename);
	}
	return result;
}
bool KNamedValues::loadFromXml(KXmlElement *elm, bool pack_in_attr) {
	clear();
	if (pack_in_attr) {
		// <XXX AAA="BBB" CCC="DDD" EEE="FFF" ... >
		int numAttrs = elm->getAttrCount();
		for (int i=0; i<numAttrs; i++) {
			const char *key = elm->getAttrName(i);
			const char *val = elm->getAttrValue(i);
			if (key && val) {
				setString(key, val);
			}
		}
	} else {
		// <XXX name="AAA">BBB</XXX>/>
		for (int i=0; i<elm->getChildCount(); i++) {
			KXmlElement *xItem = elm->getChild(i);
			const char *key = xItem->getAttrString("name");
			const char *val = xItem->getText();
			if (key && val) {
				setString(key, val);
			}
		}
	}
	return true;
}
void KNamedValues::saveToXml(KXmlElement *elm, bool pack_in_attr) const {
	if (pack_in_attr) {
		for (int i=0; i<size(); i++) {
			const std::string &n = getName(i);
			const std::string &v = getString(i);
			elm->setAttrString(n, v);
		}
	} else {
		for (int i=0; i<size(); i++) {
			const std::string &n = getName(i);
			const std::string &v = getString(i);
			KXmlElement *xPair = elm->addChild("Pair");
			xPair->setAttrString("name", n);
			xPair->setText(v.c_str());
		}
	}
}
std::string KNamedValues::saveToString(bool pack_in_attr) const {
	std::string s;
	if (pack_in_attr) {
		s += "<KNamedValues\n";
		for (int i=0; i<size(); i++) {
			const std::string &n = getName(i);
			const std::string &v = getString(i);
			s += K::str_sprintf("\t%s = '%s'\n", n.c_str(), v.c_str());
		}
		s += "/>\n";
	} else {
		s += "<KNamedValues>\n";
		for (int i=0; i<size(); i++) {
			const std::string &n = getName(i);
			const std::string &v = getString(i);
			s += K::str_sprintf("  <Pair name='%s'>%s</Pair>\n", n.c_str(), v.c_str());
		}
		s += "</KNamedValues>\n";
	}
	return s;
}
const std::string & KNamedValues::getName(int index) const {
	return _getName(index);
}
const std::string & KNamedValues::getString(int index) const {
	return _getString(index);
}
void KNamedValues::setString(const std::string &name, const std::string &value) {
	_setString(name, value);
}
bool KNamedValues::loadFromFile(const std::string &filename) {
	bool success = false;
	KInputStream file;
	if (file.openFileName(filename)) {
		std::string xml_bin = file.readBin();
		std::string xml_u8 = K::strBinToUtf8(xml_bin);
		if (! xml_u8.empty()) {
			success = loadFromString(xml_u8.c_str(), filename);
		}
	}
	return success;
}
void KNamedValues::saveToFile(const std::string &filename, bool pack_in_attr) const {
	KOutputStream file;
	if (file.openFileName(filename)) {
		std::string xml_u8 = saveToString(pack_in_attr);
		file.write(xml_u8.c_str(), xml_u8.size());
	}
}
void KNamedValues::append(const KNamedValues &nv) {
	for (int i=0; i<nv.size(); i++) {
		setString(nv.getName(i), nv.getString(i));
	}
}
int KNamedValues::find(const std::string &name) const {
	return _find(name);
}
bool KNamedValues::contains(const std::string &name) const {
	return find(name) >= 0;
}
const std::string & KNamedValues::getString(const std::string &name) const {
	return getString(name, s_Empty);
}
const std::string & KNamedValues::getString(const std::string &name, const std::string &defaultValue) const {
	int index = find(name);
	if (index >= 0) {
		return getString(index);
	} else {
		return defaultValue;
	}
}
void KNamedValues::setBool(const std::string &name, bool value) {
	setString(name, value ? "1" : "0");
}
bool KNamedValues::queryBool(const std::string &name, bool *outValue) const {
	const std::string &s = getString(name);
	if (s != "") {
		int val = 0;
		if (K::strToInt(s, &val)) {
			if (outValue) *outValue = val != 0;
			return true;
		}
	}
	return false;
}
bool KNamedValues::getBool(const std::string &name, bool defaultValue) const {
	bool result = defaultValue;
	queryBool(name, &result);
	return result;
}
void KNamedValues::setInteger(const std::string &name, int value) {
	char s[32] = {0};
	sprintf_s(s, sizeof(s), "%d", value);
	setString(name, s);
}
bool KNamedValues::queryInteger(const std::string &name, int *outValue) const {
	const std::string &s = getString(name);
	if (K::strToInt(s, outValue)) {
		return true;
	}
	return false;
}
int KNamedValues::getInteger(const std::string &name, int defaultValue) const {
	int result = defaultValue;
	queryInteger(name, &result);
	return result;
}
void KNamedValues::setUInt(const std::string &name, unsigned int value) {
	char s[32] = {0};
	sprintf_s(s, sizeof(s), "%u", value);
	setString(name, s);
}
bool KNamedValues::queryUInt(const std::string &name, unsigned int *outValue) const {
	const std::string &s = getString(name);
	return K::strToUInt32(s, outValue);
}
int KNamedValues::getUInt(const std::string &name, unsigned int defaultValue) const {
	unsigned int result = defaultValue;
	queryUInt(name, &result);
	return result;
}
void KNamedValues::setFloat(const std::string &name, float value) {
	char s[32] = {0};
	sprintf_s(s, sizeof(s), "%f", value);
	setString(name, s);
}
bool KNamedValues::queryFloat(const std::string &name, float *outValue) const {
	const std::string &s = getString(name);
	if (K::strToFloat(s, outValue)) {
		return true;
	}
	return false;
}
float KNamedValues::getFloat(const std::string &name, float defaultValue) const {
	float result = defaultValue;
	queryFloat(name, &result);
	return result;
}
void KNamedValues::setFloatArray(const std::string &name, const float *values, int count) {
	std::string s;
	if (count >= 1) {
		s = K::str_sprintf("%g", values[0]);
	}
	for (int i=1; i<count; i++) {
		s += K::str_sprintf(", %g", values[i]);
	}
	setString(name, s.c_str());
}
int KNamedValues::getFloatArray(const std::string &name, float *outValues, int maxout) const {
	const std::string &s = getString(name);
	auto tok = K::strSplit(s, ", ");
	if (outValues) {
		int idx = 0;
		while (idx<(int)tok.size() && idx<maxout) {
			float f = 0.0f;
			K::strToFloat(tok[idx], &f);
			outValues[idx] = f;
			idx++;
		}
		return idx;
	} else {
		return (int)tok.size();
	}
}
void KNamedValues::setBinary(const std::string &name, const void *data, int size) {
	std::string s;
	s = K::str_sprintf("%08x:", size);
	for (int i=0; i<size; i++) {
		s += K::str_sprintf("%02x", ((uint8_t*)data)[i]);
	}
	setString(name, s.c_str());
}
int KNamedValues::getBinary(const std::string &name, void *out, int maxsize) const {
	const std::string &s = getString(name);

	// コロンよりも左側の文字列を得る。データのバイト数が16進数表記されている
	int colonPos = K::strFindChar(s, ':');
	if (colonPos <= 0) {
		return 0; // NO DATA 
	}

	// 得られたバイト数HEX文字列から整数値を得る
	char szstr[16] = {0};
	strncpy(szstr, s.c_str(), colonPos);
	szstr[colonPos] = '\0';
	int sz = 0;
	K::strToInt(szstr, &sz);

	if (sz <= 0) {
		return 0; // Empty
	}
	if (out) {
		int start = colonPos + 1; // ':' の次の文字へ移動
		int copysize = KMath::min(sz, maxsize);
		uint8_t *out_u8 = (uint8_t*)out;
		for (int i=0; i<copysize; i++) {
			char hex[3] = {s[start+i*2], s[start+i*2+1], '\0'};
			int val = 0;
			K::strToInt(hex, &val);
			out_u8[i] = val;
		}
		return copysize;
	} else {
		return sz;
	}
}
#pragma endregion // KNamedValues



} // namespace
