#pragma once
#include <inttypes.h>
#include <string>
#include <memory> // std::shared_ptr

namespace Kamilo {

class KInputStream {
public:
	static KInputStream fromFileName(const std::string &filename);
	static KInputStream fromMemory(const void *data, int size);
	static KInputStream fromMemoryCopy(const void *data, int size);

	class Impl {
	public:
		virtual ~Impl() {}
		virtual int read(void *buf, int size) = 0;
		virtual int tell() = 0;
		virtual int size() = 0;
		virtual void seek(int pos) = 0;
		virtual bool eof() = 0;
		virtual void close() = 0;
		virtual bool isOpen() = 0;
	};

	KInputStream();
	explicit KInputStream(Impl *impl);
	~KInputStream();

	/// 読み取り位置。先頭からのオフセットバイト数
	int tell();
	
	/// 読み取り位置を設定する。先頭からのオフセットバイト数で指定する
	/// 0 <= pos <= size()
	void seek(int pos);

	/// 先頭から末尾までのバイト数
	int size();

	/// sizeバイトを読み取って data にコピーする。
	/// data が NULLの場合は単なるシークになる
	int read(void *data, int size);

	uint16_t readUint16();
	uint32_t readUint32();
	std::string readBin(int readsize=-1);

	/// アクセス可能な範囲の終端に達しているか
	bool eof();

	void close();
	bool isOpen();

private:
	std::shared_ptr<Impl> m_Impl;
};

class KOutputStream {
public:
	static KOutputStream fromFileName(const std::string &filename, const char *mode="wb");
	static KOutputStream fromMemory(std::string *dest);

	class Impl {
	public:
		virtual ~Impl() {}
		virtual int write(const void *buf, int size) = 0;
		virtual int tell() = 0;
		virtual void seek(int pos) = 0;
		virtual void close() = 0;
		virtual bool isOpen() = 0;
	};

	KOutputStream();
	explicit KOutputStream(Impl *impl);
	~KOutputStream();

	/// 読み取り位置。先頭からのオフセットバイト数
	int tell();

	/// 読み取り位置を設定する。先頭からのオフセットバイト数で指定する
	/// 0 <= pos <= size()
	void seek(int pos);

	/// 現在の書き込み位置にデータを書き込む
	int write(const void *data, int size);

	/// 16ビット符号なし整数値を書き込む
	int writeUint16(uint16_t value);

	/// 32ビット符号なし整数値を書き込む
	int writeUint32(uint32_t value);

	/// ヌル終端文字列を書き込む。
	/// 文字コードなどは一切考慮せず s で指定されたままのバイナリを書き込む
	int writeString(const std::string &s);

	void close();
	bool isOpen();

private:
	std::shared_ptr<Impl> m_Impl;
};

namespace Test {
void Test_stream();
}

} // namespace
