#include "KStream.h"
#include "KInternal.h"
namespace Kamilo {


class CFileReadImpl: public KInputStream::Impl {
public:
	FILE *m_File;
	std::string m_Name;

	CFileReadImpl(FILE *fp, const std::string &name) {
		m_File = fp;
		m_Name = name;
	}
	virtual ~CFileReadImpl() {
		if (m_File) {
			fclose(m_File);
		}
	}
	virtual int tell() override {
		return ftell(m_File);
	}
	virtual int read(void *data, int size) override {
		if (data) {
			return fread(data, 1, size, m_File);
		} else {
			return fseek(m_File, size, SEEK_CUR);
		}
	}
	virtual void seek(int pos) override {
		fseek(m_File, pos, SEEK_SET);
	}
	virtual int size() {
		int i=ftell(m_File);
		fseek(m_File, 0, SEEK_END);
		int n=ftell(m_File);
		fseek(m_File, i, SEEK_SET);
		return n; 
	}
	virtual bool eof() override {
		return feof(m_File) != 0;
	}
	virtual void close() override {
		if (m_File) {
			fclose(m_File);
			m_File = nullptr;
		}
	}
	virtual bool isOpen() override {
		return m_File != nullptr;
	}
};


class CFileWriteImpl: public KOutputStream::Impl {
public:
	FILE *m_File;
	std::string m_Name;

	CFileWriteImpl(FILE *fp, const std::string &name) {
		m_File = fp;
		m_Name = name;
	}
	virtual ~CFileWriteImpl() {
		if (m_File) {
			fclose(m_File);
		}
	}
	virtual int tell() override {
		return ftell(m_File);
	}
	virtual int write(const void *data, int size) override {
		return fwrite(data, 1, size, m_File);
	}
	virtual void seek(int pos) override {
		fseek(m_File, pos, SEEK_SET);
	}
	virtual void close() override {
		if (m_File) {
			fclose(m_File);
			m_File = nullptr;
		}
	}
	virtual bool isOpen() override {
		return m_File != nullptr;
	}
};


class CMemoryReadImpl: public KInputStream::Impl {
	void *m_Ptr;
	int m_Size;
	int m_Pos;
	bool m_IsCopy;
public:
	CMemoryReadImpl(const void *p, int size, bool copy) {
		if (copy) {
			m_Ptr = malloc(size);
			memcpy(m_Ptr, p, size);
			m_IsCopy = true;
		} else {
			m_Ptr = const_cast<void*>(p);
			m_IsCopy = false;
		}
		m_Size = size;
		m_Pos = 0;
	}
	virtual ~CMemoryReadImpl() {
		if (m_IsCopy && m_Ptr) {
			free(m_Ptr);
		}
	}
	virtual int tell() override {
		return m_Pos;
	}
	virtual int read(void *data, int size) override {
		if (m_Ptr == nullptr) return 0;
		int n = 0;
		if (m_Pos + size <= m_Size) {
			n = size;
		} else if (m_Pos < m_Size) {
			n = m_Size - m_Pos;
		}
		if (n > 0) {
			if (data) memcpy(data, (uint8_t*)m_Ptr + m_Pos, n); // data=nullptr だと単なるシークになる
			m_Pos += n;
			return n;
		}
		return 0;
	}
	virtual void seek(int pos) override {
		if (pos < 0) {
			m_Pos = 0;
		} else if (pos < m_Size) {
			m_Pos = pos;
		} else {
			m_Pos = m_Size;
		}
	}
	virtual int size() override {
		return m_Size;
	}
	virtual bool eof() override {
		return m_Pos >= m_Size;
	}
	virtual void close() override {
		m_Ptr = nullptr;
		m_Size = 0;
		m_Pos = 0;
		m_IsCopy = false;
	}
	virtual bool isOpen() override {
		return m_Ptr != nullptr;
	}
};


class CMemoryWriteImpl: public KOutputStream::Impl {
	std::string *m_Buf;
	int m_Pos;
public:
	explicit CMemoryWriteImpl(std::string *dest) {
		m_Buf = dest;
		m_Pos = 0;
	}
	virtual int tell() override {
		return m_Pos;
	}
	virtual int write(const void *data, int size) {
		if (m_Buf == nullptr) return 0;
		m_Buf->resize(m_Pos + size);
		memcpy((char*)m_Buf->data() + m_Pos, data, size);
		m_Pos += size;
		return size;
	}
	virtual void seek(int pos) override {
		if (m_Buf == nullptr) return;
		if (pos < 0) {
			m_Pos = 0;
		} else if (pos < (int)m_Buf->size()) {
			m_Pos = pos;
		} else {
			m_Pos = m_Buf->size();
		}
	}
	virtual void close() override {
		m_Buf = nullptr;
		m_Pos = 0;
	}
	virtual bool isOpen() override {
		return m_Buf != nullptr;
	}
};


#pragma region KInputStream
KInputStream KInputStream::fromFileName(const std::string &filename) {
	Impl *impl = nullptr;
	FILE *fp = K::fileOpen(filename, "rb");
	if (fp) {
		impl = new CFileReadImpl(fp, filename);
	}
	return KInputStream(impl);
}
KInputStream KInputStream::fromMemory(const void *data, int size) {
	Impl *impl = nullptr;
	if (data && size > 0) {
		impl = new CMemoryReadImpl(data, size, false); // No copy
	}
	return KInputStream(impl);
}
KInputStream KInputStream::fromMemoryCopy(const void *data, int size) {
	Impl *impl = nullptr;
	if (data && size > 0) {
		impl = new CMemoryReadImpl(data, size, true); // Copy
	}
	return KInputStream(impl);
}

KInputStream::KInputStream() {
	m_Impl = nullptr;
}
KInputStream::KInputStream(Impl *impl) {
	if (impl) {
		m_Impl = std::shared_ptr<Impl>(impl);
	} else {
		m_Impl = nullptr;
	}
}
KInputStream::~KInputStream() {
	// ここでクローズしてはいけない. 
	// std::shared_ptr によって参照が自動カウントされているので
	// ここで参照がゼロになるとは限らない
	// close();
}
int KInputStream::tell() {
	if (m_Impl) {
		return m_Impl->tell();
	}
	return 0;
}
int KInputStream::read(void *data, int size) {
	if (m_Impl) {
		return m_Impl->read(data, size);
	}
	return 0;
}
void KInputStream::seek(int pos) {
	if (m_Impl) {
		m_Impl->seek(pos);
	}
}
int KInputStream::size() {
	if (m_Impl) {
		return m_Impl->size();
	}
	return 0;
}
bool KInputStream::eof() {
	return m_Impl && m_Impl->eof();
}
void KInputStream::close() {
	if (m_Impl) {
		m_Impl->close();
	}
}
bool KInputStream::isOpen() {
	return m_Impl && m_Impl->isOpen();
}
uint16_t KInputStream::readUint16() {
	const int size = 2;
	uint16_t val = 0;
	if (read(&val, size) == size) {
		return val;
	}
	return 0;
}
uint32_t KInputStream::readUint32() {
	const int size = 4;
	uint32_t val = 0;
	if (read(&val, size) == size) {
		return val;
	}
	return 0;
}
std::string KInputStream::readBin(int readsize) {
	if (readsize < 0) {
		readsize = size(); // 現在位置から終端までのサイズ
	}
	if (readsize > 0) {
		std::string bin(readsize, '\0');
		int sz = read((void*)bin.data(), bin.size());
		bin.resize(sz);// bin.size() が実際のデータ長さを示すように
		return bin;
	}
	return std::string(); // 読み取りサイズゼロ
}
#pragma endregion // KInputStream


#pragma region KOutputStream
KOutputStream KOutputStream::fromFileName(const std::string &filename, const char *mode) {
	Impl *impl = nullptr;
	FILE *fp = K::fileOpen(filename, mode);
	if (fp) {
		impl = new CFileWriteImpl(fp, filename);
	}
	return KOutputStream(impl);
}
KOutputStream KOutputStream::fromMemory(std::string *dest) {
	Impl *impl = new CMemoryWriteImpl(dest); // No copy
	return KOutputStream(impl);
}

KOutputStream::KOutputStream() {
	m_Impl = nullptr;
}
KOutputStream::KOutputStream(Impl *impl) {
	if (impl) {
		m_Impl = std::shared_ptr<Impl>(impl);
	} else {
		m_Impl = nullptr;
	}
}
KOutputStream::~KOutputStream() {
	// ここでクローズしてはいけない. 
	// std::shared_ptr によって参照が自動カウントされているので
	// ここで参照がゼロになるとは限らない
	// close();
}
void KOutputStream::close() {
	if (m_Impl) {
		m_Impl->close();
	}
}
bool KOutputStream::isOpen() {
	return m_Impl && m_Impl->isOpen();
}
int KOutputStream::tell() {
	if (m_Impl) {
		return m_Impl->tell();
	}
	return 0;
}
void KOutputStream::seek(int pos) {
	if (m_Impl) {
		m_Impl->seek(pos);
	}
}
int KOutputStream::write(const void *data, int size) {
	if (m_Impl) {
		return m_Impl->write(data, size);
	}
	return 0;
}
int KOutputStream::writeUint16(uint16_t value) {
	return write(&value, sizeof(value));
}
int KOutputStream::writeUint32(uint32_t value) {
	return write(&value, sizeof(value));
}
int KOutputStream::writeString(const std::string &s) {
	if (s.empty()) {
		return 0;
	} else {
		return write(s.data(), s.size());
	}
}
#pragma endregion // KOutputStream




namespace Test {

void Test_stream() {
	{
		const char *text = "hello, world.";
		KInputStream r = KInputStream::fromMemory(text, strlen(text));
		char s[32] = {0};
	
		K__ASSERT(r.read(s, 5) == 5);
		K__ASSERT(strncmp(s, "hello", 5) == 0);
		K__ASSERT(r.tell() == 5);
	
		K__ASSERT(r.read(s, 7) == 7);
		K__ASSERT(strncmp(s, ", world", 7) == 0);
		K__ASSERT(r.tell() == 12);

		K__ASSERT(r.read(s, 4) == 1);
		K__ASSERT(strncmp(s, ".", 1) == 0);
		K__ASSERT(r.tell() == 13);
	}
	{
		std::string s;
		KOutputStream w = KOutputStream::fromMemory(&s);
		K__ASSERT(w.write("abc", 3) == 3);
		K__ASSERT(w.write(" ",   1) == 1);
		K__ASSERT(w.write("def", 3) == 3);
		K__ASSERT(s.compare("abc def") == 0);
	}
}

} // namespace Test


} // namespace
