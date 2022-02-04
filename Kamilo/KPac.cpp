#include "KPac.h"
//
#include <mutex>
#include <unordered_map>
#include "KInternal.h"
#include "KStream.h"
#include "KZlib.h"

namespace Kamilo {


const int PAC_COMPRESS_LEVEL = 1;
const int PAC_MAX_LABEL_LEN  = 128; // 128以外にすると昔の pac データが読めなくなるので注意


#pragma region KPacFileWriter
class CPacWriterImpl {
	KOutputStream m_Output;
public:
	CPacWriterImpl() {
	}
	bool open(KOutputStream &output) {
		m_Output = output;
		return m_Output.isOpen();
	}
	virtual bool addEntryFromFileName(const std::string &entry_name, const std::string &filename) {
		KInputStream file;
		if (!file.openFileName(filename)) {
			K__ERROR(u8"E_PAC_WRITE: ファイル '%s' をロードできないため pac ファイルに追加しませんでした", filename.c_str());
			return false;
		}
		std::string bin = file.readBin();
		return addEntryFromMemory(entry_name, bin.data(), bin.size());
	}
	bool addEntryFromMemory(const std::string &entry_name, const void *data, size_t size) {
		
		// エントリー名を書き込む。固定長で、XORスクランブルをかけておく
		{
			if (entry_name.size() >= PAC_MAX_LABEL_LEN) {
				K__ERROR(u8"ラベル名 '%s' が長すぎます", entry_name.c_str());
				return false;
			}
			char label[PAC_MAX_LABEL_LEN];
			memset(label, 0, PAC_MAX_LABEL_LEN);
			strcpy_s(label, sizeof(label), entry_name.c_str());
			// '\0' 以降の文字は完全に無視される部分なのでダミーの乱数を入れておく
			for (int i=entry_name.size()+1; i<PAC_MAX_LABEL_LEN; i++) {
				label[i] = rand() & 0xFF;
			}
			for (uint8_t i=0; i<PAC_MAX_LABEL_LEN; i++) {
				label[i] = label[i] ^ i;
			}
			m_Output.write(label, PAC_MAX_LABEL_LEN);
		}
		if (data == nullptr || size <= 0) {
			// nullptrデータ
			// Data size in file
			m_Output.writeUint32(0); // Hash
			m_Output.writeUint32(0); // 元データサイズ
			m_Output.writeUint32(0); // pacファイル内でのデータサイズ
			m_Output.writeUint32(0); // Flags

		} else {
			// 圧縮データ
			std::string zbuf = KZlib::compress_zlib(data, size, PAC_COMPRESS_LEVEL);
			m_Output.writeUint32(0); // Hash
			m_Output.writeUint32(size); // 元データサイズ
			m_Output.writeUint32(zbuf.size()); // pacファイル内でのデータサイズ
			m_Output.writeUint32(0); // Flags
			m_Output.write(zbuf.data(), zbuf.size());
		}
		return true;
	}
};

KPacFileWriter::KPacFileWriter() {
	m_Impl = nullptr;
}
KPacFileWriter KPacFileWriter::fromFileName(const std::string &filename) {
	KOutputStream file;
	file.openFileName(filename);
	return fromStream(file);
}
KPacFileWriter KPacFileWriter::fromStream(KOutputStream &output) {
	KPacFileWriter ret;
	if (output.isOpen()) {
		CPacWriterImpl *pac = new CPacWriterImpl();
		if (pac->open(output)) {
			ret.m_Impl = std::shared_ptr<CPacWriterImpl>(pac);
		} else {
			delete pac;
		}
	}
	return ret;
}
bool KPacFileWriter::isOpen() {
	return m_Impl != nullptr;
}
bool KPacFileWriter::addEntryFromFileName(const std::string &entry_name, const std::string &filename) {
	if (m_Impl) {
		return m_Impl->addEntryFromFileName(entry_name, filename);
	}
	return false;
}
bool KPacFileWriter::addEntryFromMemory(const std::string &entry_name, const void *data, size_t size) {
	if (m_Impl) {
		return m_Impl->addEntryFromMemory(entry_name, data, size);
	}
	return false;
}
#pragma endregion//  KPacFileWriter


#pragma region KPacFileReader
class CPacReaderImpl {
	std::unordered_map<std::string, int> m_Names;
	std::mutex m_Mutex;
	KInputStream m_Input;
public:
	CPacReaderImpl() {
	}
	~CPacReaderImpl() {
		m_Mutex.lock();
		m_Input = KInputStream(); // スレッドセーフでデストラクタが実行されるように、あえて空オブジェクトを代入しておく
		m_Mutex.unlock();
	}
	int getCount() {
		int ret = 0;
		m_Mutex.lock();
		{
			ret = getCount_unsafe();
		}
		m_Mutex.unlock();
		return ret;
	}
	int getIndexByName(const std::string &entry_name, bool ignore_case, bool ignore_path) {
		int ret = 0;
		m_Mutex.lock();
		{
			ret = getIndexByName_unsafe(entry_name, ignore_case, ignore_path);
		}
		m_Mutex.unlock();
		return ret;
	}
	std::string getName(int index) {
		std::string ret;
		m_Mutex.lock();
		{
			ret = getName_unsafe(index);
		}
		m_Mutex.unlock();
		return ret;
	}
	std::string getData(int index) {
		std::string ret;
		m_Mutex.lock();
		{
			ret = getData_unsafe(index);
		}
		m_Mutex.unlock();
		return ret;
	}
	bool open(KInputStream &input) {
		m_Mutex.lock();
		m_Input = input;
		m_Mutex.unlock();
		return m_Input.isOpen();
	}
	void seekFirst_unsafe() {
		m_Input.seek(0);
	}
	bool seekNext_unsafe(std::string *p_name) {
		if (m_Input.tell() >= m_Input.size()) {
			return false;
		}
		uint32_t len = 0;
		m_Input.read(nullptr, PAC_MAX_LABEL_LEN); // Name
		m_Input.read(nullptr, 4); // Hash
		m_Input.read(nullptr, 4); // Data size
		m_Input.read(&len, 4); // Data size in pac file
		m_Input.read(nullptr, 4); // Flags
		m_Input.read(nullptr, len); // Data
		return true;
	}
	bool readFile_unsafe(std::string *p_name, std::string *p_data) {
		if (m_Input.tell() >= m_Input.size()) {
			return false;
		}
		if (p_name) {
			char s[PAC_MAX_LABEL_LEN];
			m_Input.read(s, PAC_MAX_LABEL_LEN);
			for (uint8_t i=0; i<PAC_MAX_LABEL_LEN; i++) {
				s[i] = s[i] ^ i;
			}
			*p_name = s;
		} else {
			m_Input.read(nullptr, PAC_MAX_LABEL_LEN);
		}

		// Hash (NOT USE)
		uint32_t hash = m_Input.readUint32();

		// Data size (NOT USE)
		uint32_t datasize_orig = m_Input.readUint32();
		if (datasize_orig >= 1024 * 1024 * 100) { // 100MBはこえないだろう
			K__ERROR("too big datasize_orig size");
			return false;
		}

		// Data size in pac file
		uint32_t datasize_inpac = m_Input.readUint32();
		if (datasize_inpac >= 1024 * 1024 * 100) { // 100MBはこえないだろう
			K__ERROR("too big datasize_inpac size");
			return false;
		}

		// Flags (NOT USE)
		uint32_t flags = m_Input.readUint32();

		// Read data
		if (p_data) {
			if (datasize_orig > 0) {
				std::string zdata = m_Input.readBin(datasize_inpac);
				*p_data = KZlib::uncompress_zlib(zdata, datasize_orig);
				if (p_data->size() != datasize_orig) {
					K__ERROR("E_PAC_DATA_SIZE_NOT_MATCHED");
					p_data->clear();
					return false;
				}
			} else {
				*p_data = "";
			}
		} else {
			m_Input.read(nullptr, datasize_inpac);
		}
		return true;
	}
	int getCount_unsafe() {
		int num = 0;
		seekFirst_unsafe();
		while (seekNext_unsafe(nullptr)) {
			num++;
		}
		return num;
	}
	int getIndexByName_unsafe(const std::string &entry_name, bool ignore_case, bool ignore_path) {

		if (!ignore_case && !ignore_path) {
			// find from cache
			auto it = m_Names.find(entry_name);
			if (it != m_Names.end()) {
				return it->second;
			}
		}

		seekFirst_unsafe();
		int idx = 0;
		std::string name;
		while (readFile_unsafe(&name, nullptr)) {
			if (!ignore_case && !ignore_path) {
				// add to cache
				m_Names[name] = idx;
			}
			if (K::pathCompare(name, entry_name, ignore_case, ignore_path) == 0) {
				return idx;
			}
#ifdef _DEBUG
			if (K::pathCompare(name, entry_name, true, false) == 0) { // ignore case で一致した
				if (K::pathCompare(name, entry_name, false, false) != 0) { // case sensitive で不一致になった
					K::print(
						u8"W_PAC_CASE_NAME: PACファイル内をファイル名 '%s' で検索中に、"
						u8"大小文字だけが異なるファイル '%s' を発見しました。"
						u8"これは意図した動作ですか？予期せぬ不具合の原因になるため、ファイル名の変更を強く推奨します",
						entry_name.c_str(), name.c_str()
					);
				}
			}
#endif
			idx++;
		}
		if (!ignore_case && !ignore_path) {
			// add to cache
			m_Names[entry_name] = -1;
		}
		return -1;
	}
	std::string getName_unsafe(int index) {
		seekFirst_unsafe();
		for (int i=1; i<index; i++) {
			if (!seekNext_unsafe(nullptr)) {
				return "";
			}
		}
		std::string name;
		if (readFile_unsafe(&name, nullptr)) {
			return name;
		}
		return "";
	}
	std::string getData_unsafe(int index) {
		seekFirst_unsafe();
		for (int i=0; i<index; i++) {
			if (!seekNext_unsafe(nullptr)) {
				return std::string();
			}
		}
		std::string data;
		if (readFile_unsafe(nullptr, &data)) {
			return data;
		}
		return std::string();
	}
};

KPacFileReader::KPacFileReader() {
	m_Impl = nullptr;
}
KPacFileReader KPacFileReader::fromFileName(const std::string &filename) {
	KInputStream file;
	file.openFileName(filename);
	return fromStream(file);
}
KPacFileReader KPacFileReader::fromStream(KInputStream &input) {
	KPacFileReader ret;
	if (input.isOpen()) {
		CPacReaderImpl *pac = new CPacReaderImpl();
		if (pac->open(input)) {
			ret.m_Impl = std::shared_ptr<CPacReaderImpl>(pac);
		} else {
			delete pac;
		}
	}
	return ret;
}
bool KPacFileReader::isOpen() {
	return m_Impl != nullptr;
}
int KPacFileReader::getCount() {
	if (m_Impl) {
		return m_Impl->getCount();
	}
	return 0;
}
int KPacFileReader::getIndexByName(const std::string &entry_name, bool ignore_case, bool ignore_path) {
	if (m_Impl) {
		return m_Impl->getIndexByName(entry_name, ignore_case, ignore_path);
	}
	return 0;
}
std::string KPacFileReader::getName(int index) {
	if (m_Impl) {
		return m_Impl->getName(index);
	}
	return "";
}
std::string KPacFileReader::getData(int index) {
	if (m_Impl) {
		return m_Impl->getData(index);
	}
	return "";
}
#pragma endregion // KPacFileReader

} // namespace
