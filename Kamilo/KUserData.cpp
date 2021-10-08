#include "KUserData.h"
//
#include "KImGui.h"
#include "KInspector.h"
#include "KInternal.h"
#include "KZlib.h"
#include "KNamedValues.h"
#include "keng_game.h"

namespace Kamilo {



class CUserDataImpl: public KInspectorCallback {
	static std::string encryptString(const std::string &data, const std::string &key) {
		std::string s = data;
		xorString((char*)s.data(), s.size(), key.c_str());
		return s;
	}
	static std::string decryptString(const std::string &data, const std::string &key) {
		std::string s = data;
		xorString((char*)s.data(), s.size(), key.c_str());
		return s;
	}
	static void xorString(char *s, size_t len, const char *key) {
		K__ASSERT(s);
		K__ASSERT(key && key[0]); // 長さ１以上の文字列であること
		K__ASSERT(len > 0);
		size_t keylen = strlen(key);
		for (size_t i=0; i<len; i++) {
			s[i] = s[i] ^ key[i % keylen];
		}
	}

public:
	KNamedValues m_Values;
	std::unordered_map<std::string, int> m_Tags;

	CUserDataImpl() {
		KEngine::addInspectorCallback(this);
	}
	virtual void onInspectorGui() override {
		int ww = (int)ImGui::GetContentRegionAvail().x;
		int w = KMath::min(64, ww/3);
		int id = 0;
		ImGui::Indent();
		for (int i=0; i<m_Values.size(); i++) {
			const std::string &key = m_Values.getName(i);
			const std::string &val = m_Values.getString(i);
			char val_u8[256];
			strcpy_s(val_u8, sizeof(val_u8), val.c_str());
			ImGui::PushID(KImGui::ID(id));
			ImGui::PushItemWidth((float)w);
			if (ImGui::InputText("", val_u8, sizeof(val_u8))) {
				std::string s = val_u8;
				m_Values.setString(key, s);
			}
			ImGui::PopItemWidth();
			ImGui::SameLine();
			ImGui::Text("%s", key.c_str());
			ImGui::PopID();
			id++;
		}
		ImGui::Unindent();
	}

	void _clear(const std::vector<std::string> &keys) {
		for (auto it=keys.begin(); it!=keys.end(); ++it) {
			const std::string &k = *it;
			auto kit = m_Tags.find(k);
			if (kit != m_Tags.end()) {
				m_Tags.erase(kit);
			}
			m_Values.remove(k.c_str());
		}
	}
	void clearValues() {
		m_Values.clear();
		m_Tags.clear();
	}
	void clearValuesByTag(int tag) {
		std::vector<std::string> keys;
		for (auto it=m_Tags.begin(); it!=m_Tags.end(); ++it) {
			if (it->second == tag) {
				keys.push_back(it->first);
			}
		}
		_clear(keys);
	}
	void clearValuesByPrefix(const char *prefix) {
		std::vector<std::string> keys;
		for (int i=0; i<m_Values.size(); i++) {
			const std::string &key = m_Values.getName(i);
			if (K::strStartsWith(key, prefix)) {
				keys.push_back(key);
			}
		}
		_clear(keys);
	}
	int getKeys(std::vector<std::string> *p_keys) const {
		if (p_keys) {
			for (int i=0; i<m_Values.size(); i++) {
				const std::string &key = m_Values.getName(i);
				p_keys->push_back(key);
			}
		}
		return m_Values.size();
	}
	bool hasKey(const std::string &key) const {
		return m_Values.getString(key) != "";
	}
	std::string getString(const std::string &key, const std::string &def) const {
		return m_Values.getString(key, def);
	}
	void setString(const std::string &key, const std::string &val, int tag) {
		m_Values.setString(key, val);
		m_Tags[key] = tag;
	}
	bool saveToFileEx(const KNamedValues *nv, const std::string &filename, const std::string &password) {
		bool ret = false;
	
		KOutputStream file;
		if (file.openFileName(filename)) {
			std::string u8 = nv->saveToString();
			if (!password.empty()) {
				u8 = encryptString(u8, password);
			}
			if (!u8.empty()) {
				file.write(u8.data(), u8.size());
			}
			ret = true;
		}
		return ret;
	}
	bool saveToFile(const std::string &filename, const std::string &password) {
		return saveToFileEx(&m_Values, filename, password);
	}
	bool saveToFileCompress(const std::string &filename) {
		return saveToFileCompressEx(&m_Values, filename);
	}
	bool saveToFileCompressEx(const KNamedValues *nv, const std::string &filename) {
		if (nv == nullptr) return false;
		std::string u8 = nv->saveToString();
		if (u8.empty()) return false;

		std::string zbin = KZlib::compress_raw(u8, 1);
		if (zbin.empty()) return false;

		KOutputStream file;
		if (file.openFileName(filename)) {
			file.writeUint16((uint16_t)u8.size());
			file.writeUint16((uint16_t)zbin.size());
			file.write(zbin.data(), zbin.size());
			return true;
		}
		return false;
	}
	bool loadFromFile(const std::string &filename, const std::string &password) {
		return peekFile(filename, password, &m_Values);
	}
	bool loadFromFileCompress(const std::string &filename) {
		return loadFromFileCompressEx(&m_Values, filename);
	}
	bool loadFromFileCompressEx(KNamedValues *nv, const std::string &filename) const {
		KInputStream file;
		if (!file.openFileName(filename)) {
			return false;
		}
		uint16_t uzsize = file.readUint16();
		uint16_t zsize  = file.readUint16();
		std::string zbin = file.readBin(zsize);
		std::string text_u8 = KZlib::uncompress_raw(zbin, uzsize);
		text_u8.push_back(0); // 終端文字を追加
		if (nv) {
			return nv->loadFromString(text_u8.c_str(), filename.c_str());
		} else {
			return true;
		}
	}
	bool loadFromNamedValues(const KNamedValues &nv) {
		m_Values.clear();
		m_Values.append(nv);
		return true;
	}
	bool peekFile(const std::string &filename, const std::string &password, KNamedValues *nv) const {
		bool ret = false;
		KInputStream file;
		if (file.openFileName(filename)) {
			std::string u8 = file.readBin();
			if (!password.empty()) {
				u8 = decryptString(u8, password);
			}
			if (nv) {
				nv->loadFromString(u8.c_str(), filename.c_str());
			}
			ret = true;
		}
		return ret;
	}
};


static CUserDataImpl *g_UserData = nullptr;


void KUserData::install() {
	K__ASSERT(g_UserData == nullptr);
	g_UserData = new CUserDataImpl();
}
void KUserData::uninstall() {
	if (g_UserData) {
		delete g_UserData;
		g_UserData = nullptr;
	}
}
void KUserData::clearValues() {
	K__ASSERT(g_UserData);
	g_UserData->clearValues();
}
void KUserData::clearValuesByTag(int tag) {
	K__ASSERT(g_UserData);
	g_UserData->clearValuesByTag(tag);
}
void KUserData::clearValuesByPrefix(const char *prefix) {
	K__ASSERT(g_UserData);
	g_UserData->clearValuesByPrefix(prefix);
}
std::string KUserData::getString(const std::string &key) {
	static const std::string s_Empty;
	K__ASSERT(g_UserData);
	return g_UserData->getString(key, s_Empty);
}
std::string KUserData::getString(const std::string &key, const std::string &def) {
	K__ASSERT(g_UserData);
	return g_UserData->getString(key, def);
}
void KUserData::setString(const std::string &key, const std::string &val, int tag) {
	K__ASSERT(g_UserData);
	g_UserData->setString(key, val, tag);
}
bool KUserData::hasKey(const std::string &key) {
	K__ASSERT(g_UserData);
	return g_UserData->hasKey(key);
}
int KUserData::getKeys(std::vector<std::string> *p_keys) {
	K__ASSERT(g_UserData);
	return g_UserData->getKeys(p_keys);
}
int KUserData::getInt(const std::string &key, int def) {
	std::string s = getString(key);
	int ret = def;
	K::strToInt(s, &ret);
	return ret;
}
void KUserData::setInt(const std::string &key, int val, int tag) {
	std::string pval = std::to_string(val);
	setString(key, pval, tag);
}
bool KUserData::saveToFile(const std::string &filename, const char *password) {
	K__ASSERT(g_UserData);
	return g_UserData->saveToFile(filename, password);
}
bool KUserData::saveToFileCompress(const std::string &filename) {
	K__ASSERT(g_UserData);
	return g_UserData->saveToFileCompress(filename);
}
bool KUserData::loadFromFile(const std::string &filename, const char *password) {
	K__ASSERT(g_UserData);
	return g_UserData->loadFromFile(filename, password);
}
bool KUserData::loadFromFileCompress(const std::string &filename) {
	K__ASSERT(g_UserData);
	return g_UserData->loadFromFileCompress(filename);
}
bool KUserData::peekFile(const std::string &filename, const char *password, KNamedValues *nv) {
	K__ASSERT(g_UserData);
	return g_UserData->peekFile(filename, password, nv);
}


} // namespace
