#include "KChunkedFile.h"
#include "KInternal.h"

namespace Kamilo {

const int CHUNK_SIGN_SIZE = sizeof(chunk_id_t);
const int CHUNK_SIZE_SIZE = sizeof(chunk_size_t);

#pragma region KChunkedFileReader
KChunkedFileReader::KChunkedFileReader() {
	m_ptr = nullptr;
	m_end = nullptr;
	m_chunk_data = nullptr;
	m_chunk_id = 0;
	m_chunk_size = 0;
	m_nest = std::stack<const uint8_t *>(); // clear
}
void KChunkedFileReader::init(const void *buf, size_t len) {
	m_ptr = (uint8_t *)buf;
	m_end = (uint8_t *)buf + len;
	m_chunk_id = 0;
	m_chunk_data = nullptr;
	m_chunk_size = 0;
	m_nest = std::stack<const uint8_t *>(); // clear
}
bool KChunkedFileReader::eof() const {
	return m_end <= m_ptr;
}
bool KChunkedFileReader::checkChunkId(chunk_id_t id) const {
	chunk_id_t s;
	if (getChunkHeader(&s, nullptr)) {
		if (s == id) {
			return true;
		}
	}
	return false;
}
void KChunkedFileReader::openChunk(chunk_id_t id) {
	// 子チャンクの先頭に移動する
	chunk_size_t size = 0;
	getChunkHeader(nullptr, &size);
	readChunkHeader(id, 0);

	// この時点で m_ptr は子チャンク先頭を指している。
	// そこからさらに size 進めた部分が子チャンク終端になる
	m_nest.push(m_ptr + size);
}
void KChunkedFileReader::closeChunk() {
	// 入れ子の終端に一致しているか確認する
	K__ASSERT(!m_nest.empty());
	K__ASSERT(m_nest.top() == m_ptr);
	m_nest.pop();
}
bool KChunkedFileReader::getChunkHeader(chunk_id_t *out_id, chunk_size_t *out_size) const {
	if (m_ptr+CHUNK_SIGN_SIZE+CHUNK_SIZE_SIZE < m_end) {
		if (out_id) {
			memcpy(out_id, m_ptr, CHUNK_SIGN_SIZE);
		}
		if (out_size) {
			memcpy(out_size, m_ptr+CHUNK_SIGN_SIZE, CHUNK_SIZE_SIZE);
		}
		return true;
	}
	return false;
}
void KChunkedFileReader::readChunkHeader(chunk_id_t id, chunk_size_t size) {
	K__ASSERT(m_ptr);

	memcpy(&m_chunk_id, m_ptr, CHUNK_SIGN_SIZE);
	m_ptr += CHUNK_SIGN_SIZE;

	memcpy(&m_chunk_size, m_ptr, CHUNK_SIZE_SIZE);
	m_ptr += CHUNK_SIZE_SIZE;

	K__ASSERT(id==0 || id==m_chunk_id);
	K__ASSERT(size==0 || size==m_chunk_size);
	m_chunk_data = m_ptr;
}
void KChunkedFileReader::readChunk(chunk_id_t id, chunk_size_t size, void *data) {
	readChunkHeader(id, size);

	K__ASSERT(m_chunk_data);
	if (m_chunk_size > 0) {
		if (data) memcpy(data, m_chunk_data, m_chunk_size);
	}
	m_ptr = m_chunk_data + m_chunk_size;
}

void KChunkedFileReader::readChunk(chunk_id_t id, chunk_size_t size, std::string *data) {
	readChunkHeader(id, size);

	K__ASSERT(m_chunk_data);
	if (data) {
		data->resize(m_chunk_size);
		if (m_chunk_size > 0) {
			memcpy(&(*data)[0], m_chunk_data, m_chunk_size);
		}
	}
	m_ptr = m_chunk_data + m_chunk_size;
}
std::string KChunkedFileReader::readChunk(chunk_id_t id) {
	std::string val;
	readChunk(id, 0, &val);
	return val;
}
uint8_t KChunkedFileReader::readChunk1(chunk_id_t id) {
	uint8_t val = 0;
	readChunk(id, 1, &val);
	return val;
}
uint16_t KChunkedFileReader::readChunk2(chunk_id_t id) {
	uint16_t val = 0;
	readChunk(id, 2, &val);
	return val;
}
uint32_t KChunkedFileReader::readChunk4(chunk_id_t id) {
	uint32_t val = 0;
	readChunk(id, 4, &val);
	return val;
}
void KChunkedFileReader::readChunk(chunk_id_t id, std::string *data) {
	readChunkHeader(id, 0);

	K__ASSERT(m_chunk_data);
	if (data) {
		data->resize(m_chunk_size);
		if (m_chunk_size > 0) {
			memcpy(&(*data)[0], m_chunk_data, m_chunk_size);
		}
	}
	m_ptr = m_chunk_data + m_chunk_size;
}
#pragma endregion // KChunkedFileReader


#pragma region KChunkedFileWriter
KChunkedFileWriter::KChunkedFileWriter() {
	m_pos = 0;
}
void KChunkedFileWriter::clear() {
	m_pos = 0;
	m_buf.clear();
	while (! m_stack.empty()) m_stack.pop();
}
const void * KChunkedFileWriter::data() const {
	return m_buf.data();
}
size_t KChunkedFileWriter::size() const {
	return m_buf.size();
}
void KChunkedFileWriter::openChunk(chunk_id_t id) {
	// チャンクを開く。チャンクヘッダだけ書き込んでおく
	// ただし、チャンクデータサイズは子チャンクを追加してからでないと確定できないので、
	// いま書き込むのは識別子のみ
	m_buf.resize(m_pos + CHUNK_SIGN_SIZE + CHUNK_SIZE_SIZE);

	memcpy(&m_buf[m_pos], &id, CHUNK_SIGN_SIZE);
	m_pos += CHUNK_SIGN_SIZE;

	m_stack.push(m_pos);
	m_pos += CHUNK_SIZE_SIZE; // データサイズは未確定（チャンクが閉じたときにはじめて確定する）なので、まだ書き込まない
}
void KChunkedFileWriter::closeChunk() {
	// 現在のチャンクのヘッダー位置（サイズ書き込み位置）
	chunk_size_t sizepos = m_stack.top();

	// 現在のチャンクのデータ開始位置
	chunk_size_t datapos = sizepos + CHUNK_SIZE_SIZE;

	// 現在のチャンクのデータサイズ
	chunk_size_t size = m_pos - datapos;

	// 現在のチャンクヘッダ位置まで戻り、確定したチャンクデータサイズを書き込む
	memcpy(&m_buf[sizepos], &size, CHUNK_SIZE_SIZE);
	m_stack.pop();
}
void KChunkedFileWriter::writeChunkN(chunk_id_t id, chunk_size_t size, const void *data) {
	// チャンクを書き込むのに必要なサイズだけ拡張する
	// 新たに必要になるサイズ = チャンクヘッダ＋データ
	m_buf.resize(m_pos + (CHUNK_SIGN_SIZE + CHUNK_SIZE_SIZE) + size);

	memcpy(&m_buf[m_pos], &id, CHUNK_SIGN_SIZE);
	m_pos += CHUNK_SIGN_SIZE;

	memcpy(&m_buf[m_pos], &size, 4);
	m_pos += CHUNK_SIZE_SIZE;

	if (size > 0) {
		if (data) {
			memcpy(&m_buf[m_pos], data, size);
		} else {
			memset(&m_buf[m_pos], 0, size);
		}
		m_pos += size;
	}
}
void KChunkedFileWriter::writeChunkN(chunk_id_t id, const std::string &data) {
	if (data.empty()) {
		// サイズゼロのデータを持つチャンク
		writeChunkN(id, 0, nullptr);
	} else {
		// std::string::size() の戻り値 size_t は 32ビットとは限らないので注意。
		// 例えば MacOS だと size_t は 64bit になる
		writeChunkN(id, (chunk_size_t)data.size(), &data[0]);
	}
}
void KChunkedFileWriter::writeChunkN(chunk_id_t id, const char *data) {
	// strlen の戻り値 size_t は 32ビットとは限らないので注意。
	// 例えば MacOS だと size_t は 64bit になる
	writeChunkN(id, (chunk_size_t)strlen(data), data);
}
void KChunkedFileWriter::writeChunk1(chunk_id_t id, uint8_t data) {
	writeChunkN(id, 1, &data);
}
void KChunkedFileWriter::writeChunk2(chunk_id_t id, uint16_t data) {
	writeChunkN(id, 2, &data);
}
void KChunkedFileWriter::writeChunk4(chunk_id_t id, uint32_t data) {
	writeChunkN(id, 4, &data);
}
#pragma endregion // KChunkedFileWriter


namespace Test {

/// [Test_chunk] <--- Doxgen @snippet コマンドからの参照用なので削除してはいけない
void Test_chunk() {
	// データ作成
	KChunkedFileWriter w;
	w.writeChunk2(0x1000, 0x2019);
	w.writeChunk4(0x1001, 0xDeadBeef);
	w.writeChunkN(0x1002, "HELLO WOROLD!");
	w.openChunk(0x1003);
	w.writeChunk1(0x2000, 'a');
	w.writeChunk1(0x2001, 'b');
	w.writeChunk1(0x2002, 'c');
	w.closeChunk();

	// 書き込んだバイナリデータを得る
	std::string bin((const char *)w.data(), w.size());

	// 読み取る
	KChunkedFileReader r;
	r.init(bin.data(), bin.size());
	K__ASSERT(r.readChunk2(0x1000) == 0x2019);
	K__ASSERT(r.readChunk4(0x1001) == 0xDeadBeef);
	K__ASSERT(r.readChunk(0x1002) == "HELLO WOROLD!");

	r.openChunk(0); // 入れ子チャンクに入る(識別子気にしない)
	K__ASSERT(r.readChunk1(0) == 'a');
	K__ASSERT(r.readChunk1(0) == 'b');
	K__ASSERT(r.readChunk1(0) == 'c');
	r.closeChunk();
}
/// [Test_chunk] <--- Doxgen @snippet コマンドからの参照用なので削除してはいけない

} // Test

} // namespace
