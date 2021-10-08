#include "KSound.h"

#include <unordered_set>
#include <mutex>
#include <windows.h> // HMMIO
#include <mmsystem.h> // WAVEFORMATEX, MMCKINFO
#include <dsound.h>
#include "KInternal.h"

// stb_vorbis
// https://github.com/nothings/stb
// https://github.com/nothings/stb/blob/master/stb_vorbis.c
// license: public domain
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"


#define SND_ERROR()   K__ERROR("Sound operation failed")

namespace Kamilo {


static int _ReadLoop(KSoundFile::Impl *snd, void *_samples, int max_count, int loop_start, int loop_end) {
	K__ASSERT(snd);
	K__ASSERT(_samples);
	K__ASSERT(max_count > 0);
	K__ASSERT(0 <= loop_start);
	K__ASSERT(0 <= loop_end);

	int rate=0, ch=0, total=0;
	snd->getinfo(&rate, &ch, &total);

	// loop_end が 0 の場合は曲の終端が指定されているものとする
	if (loop_end == 0) {
		loop_end = total;
	}
	// loop_end が曲の終端を超えている場合は修正する
	if (total <= loop_end) {
		loop_end = total;
	}
	int count = 0;
	uint16_t *samples = (uint16_t*)_samples;
	while (count < max_count) {
		int pos = snd->tell();
		if (pos < loop_end) {
			int avail_r = loop_end - pos; // 読み取り可能なサンプル数（ループ終端までの残りサンプル数）
			int avail_w = max_count - count; // 書き込み可能なサンプル数（バッファ終端までの残りサンプル数）
			int got_samples;
			if (avail_r < avail_w) {
				got_samples = snd->read(samples, avail_r);
			} else {
				got_samples = snd->read(samples, avail_w);
			}
			if (got_samples == 0) {
				break; // error or eof
			}
			count += got_samples;
			samples += got_samples;
		
		} else {
			// ループ終端に達した。先頭に戻る
			snd->seek(loop_start);
		}
	}
	return count;
}


class COggImpl: public KSoundFile::Impl {
	std::string m_bin;
	stb_vorbis *m_vorbis;
	stb_vorbis_info m_info;
	int m_total_samples;
public:
	COggImpl() {
		m_vorbis = nullptr;
		memset(&m_info, 0, sizeof(m_info));
		m_total_samples = 0;
	}
	COggImpl(const void *data, size_t size, int *err) {
		K__ASSERT(data && size > 0);
		K__ASSERT(err);
		memset(&m_info, 0, sizeof(m_info));

		m_bin.resize(size);
		memcpy(&m_bin[0], data, size);
		m_vorbis = stb_vorbis_open_memory((const unsigned char*)m_bin.data(), m_bin.size(), err, nullptr);
		if (m_vorbis == nullptr) {
			// 作成失敗したのにエラーコードが入っていなかったら、とりあえず-1を入れておく。
			// stb_vorbis_open_memory でのエラーコードは必ず正の値になる。
			// のエラーコード詳細については enum STBVorbisError を参照する。
			if (*err == 0) {
				*err = -1;
			}
			return;
		}
		m_info = stb_vorbis_get_info(m_vorbis);
		m_total_samples = stb_vorbis_stream_length_in_samples(m_vorbis) * m_info.channels;
		*err = 0;
	}
	virtual ~COggImpl() {
		stb_vorbis_close(m_vorbis);
		m_bin.clear();
	}
	virtual void getinfo(int *rate, int *channels, int *samples) override {
		if (rate) *rate = m_info.sample_rate;
		if (channels) *channels = m_info.channels;
		if (samples) *samples = m_total_samples;
	}
	virtual void seek(int sample_offset) override {
		int block = sample_offset / m_info.channels;
		stb_vorbis_seek(m_vorbis, block);
	}
	virtual int read(void *buffer, int num_samples) override {
		// 与える数はトータルのサンプル数だが、戻り値は１チャンネルあたりの読み取りサンプル数であることに注意。
		// 正しく読み込まれていれば、戻り値にチャンネル数をかけたものが、引数で与えた読み取りサンプル数と一致する
		int got_samples_per_channel = stb_vorbis_get_samples_short_interleaved(m_vorbis, m_info.channels, (short*)buffer, num_samples);
		return got_samples_per_channel * m_info.channels;
	}
	virtual int tell() override {
		int block = (int)stb_vorbis_get_sample_offset(m_vorbis);
		return block * m_info.channels;
	}
};


class CWavImplWinMM: public KSoundFile::Impl {
	HMMIO m_mmio;
	WAVEFORMATEX m_fmt;
	MMCKINFO m_chunk;
	std::string m_buf;
	int m_total_samples;
	int m_bytes_per_sample;
public:
	/// file のヘッダが .WAV フォーマットかどうか調べる。
	/// @attention この関数は読み取り位置を元に戻さない
	static bool check(const void *data, size_t size) {
		bool ok = false;
		if (size >= 12) {
			const char *hdr = (const char *)data;
			if (strncmp(hdr + 0, "RIFF", 4) == 0) {
				if (strncmp(hdr + 8, "WAVE", 4) == 0) {
					ok = true;
				}
			}
		}
		return ok;
	}
public:
	CWavImplWinMM() {
		memset(&m_fmt, 0, sizeof(m_fmt));
		memset(&m_chunk, 0, sizeof(m_chunk));
		m_mmio = nullptr;
		m_total_samples = 0;
		m_bytes_per_sample = 0;
	}
	CWavImplWinMM(const void *data, size_t size, int *err) {
		memset(&m_fmt, 0, sizeof(m_fmt));
		memset(&m_chunk, 0, sizeof(m_chunk));
		m_mmio = nullptr;
		m_total_samples = 0;
		m_bytes_per_sample = 0;
		//
		K__ASSERT(err);
		m_buf.resize(size);
		memcpy(&m_buf[0], data, size);
		// http://hiroshi0945.blog75.fc2.com/?mode=m&no=33
		MMIOINFO info;
		memset(&info, 0, sizeof(MMIOINFO));
		info.pchBuffer = reinterpret_cast<HPSTR>(&m_buf[0]);
		info.fccIOProc = FOURCC_MEM; // read from memory
		info.cchBuffer = m_buf.size();
		HMMIO hMmio = mmioOpen(nullptr, &info, MMIO_ALLOCBUF|MMIO_READ);
		if (hMmio == nullptr) {
			// 失敗
			*err = 1;
			return;
		}
		if (!open_mmio(hMmio)) {
			// 失敗
			*err = 1;
			return;
		}
		m_bytes_per_sample = m_fmt.wBitsPerSample / 8;
		m_total_samples = (int)m_buf.size() / m_bytes_per_sample;
		*err = 0;
	}
	virtual ~CWavImplWinMM() {
		mmioClose(m_mmio, 0);
	}
	bool open_mmio(HMMIO hMmio) {
		// RIFFチャンク検索
		MMRESULT mmRes;
		MMCKINFO riffChunk;
		riffChunk.fccType = mmioStringToFOURCCA("WAVE", 0);
		mmRes = mmioDescend(hMmio, &riffChunk, nullptr, MMIO_FINDRIFF);
		if (mmRes != MMSYSERR_NOERROR) {
			mmioClose(hMmio, 0);
			return false;
		}
		// フォーマットチャンク検索
		MMCKINFO formatChunk;
		formatChunk.ckid = mmioStringToFOURCCA("fmt ", 0);
		mmRes = mmioDescend(hMmio, &formatChunk, &riffChunk, MMIO_FINDCHUNK);
		if (mmRes != MMSYSERR_NOERROR) {
			mmioClose(hMmio, 0);
			return false;
		}
		// WAVEFORMATEX構造体格納
		DWORD fmsize = formatChunk.cksize;
		DWORD size = mmioRead(hMmio, (HPSTR)&m_fmt, fmsize);
		if (size != fmsize) {
			mmioClose(hMmio, 0);
			return false;
		}
		// RIFFチャンクに戻る
		mmioAscend(hMmio, &formatChunk, 0);
		// データチャンク検索
		m_chunk.ckid = mmioStringToFOURCCA("data ", 0);
		mmRes = mmioDescend(hMmio, &m_chunk, &riffChunk, MMIO_FINDCHUNK);
		if (mmRes != MMSYSERR_NOERROR) {
			mmioClose(hMmio, 0);
			return false;
		}
		// いまのところ16ビットサウンドにのみ対応
		if (m_fmt.wBitsPerSample != 16) {
			SND_ERROR();
		//	mmioClose(hMmio, 0);
		//	return false;
		}
		m_mmio = hMmio;
		return true;
	}
	virtual void getinfo(int *rate, int *channels, int *samples) override {
		if (rate) *rate = m_fmt.nSamplesPerSec;
		if (channels) *channels = m_fmt.nChannels;
		if (samples) *samples = m_total_samples;
	}
	virtual void seek(int sample_offset) override {
		int num_blocks = sample_offset / m_fmt.nChannels;
		int block_bytes = m_fmt.nBlockAlign;
		int seekto = m_chunk.dwDataOffset + num_blocks * block_bytes;
		mmioSeek(m_mmio, seekto, SEEK_SET);
	}
	virtual int read(void *buffer, int num_samples) override {
		int num_blocks = num_samples / m_fmt.nChannels;
		int block_bytes = m_fmt.nBlockAlign;
		int gotBytes = mmioRead(m_mmio, (HPSTR)buffer, num_blocks * block_bytes);

		if (m_bytes_per_sample == 1) {
			// readメソッドは16ビットサウンドにのみ対応している
			//（呼び出し側も16ビットサウンドのつもりでreadを呼んでいる）ので、
			// ソースが 8-BIT サウンドだった場合は 16-BIT サウンドに変換しておく
			//
			// 呼び出し側は 16-BIT のつもりで呼んでいるので buffer には num_samples * channels * 2 バイトを割り当てているはず。
			// ところが実際には 8ビットサウンドを読みこんでいるため、 num_samples * channels * 1 バイト しか使っていない。
			//
			// buffer の前半部分に 1サンプル=1バイト で書き込まれているデータを引き延ばして 1サンプル=2バイトにしておく。
			// このとき、変換元と変換先は同じ buffer であるので変換の順序に注意。
			// 末尾から処理していかないと変換前の値が先に上書きされてしまう
			for (int i=gotBytes-1; i>=0; i--) {
				uint16_t *dst = ((uint16_t*)buffer) + i; // 変換後 (16-BIT)
				uint8_t *src = ((uint8_t*)buffer) + i; // 変換前 (8-BIT)

				// 単純に1バイトだった値を2バイトにするには8ビット左シフトすればよいが、
				// 試してみたところそれだと音量が大きくなりすぎていた。
				// 他のツールで変換したファイルと比べてみた感じだと 6 ビットシフトで丁度よくなる
			//	*dst = (*src) << 8;
				*dst = (*src) << 6;
			}
			return gotBytes / 2; // 16-BIT sound = 2bytes per sample
		}
		return gotBytes / m_bytes_per_sample;
	}
	virtual int tell() override {
		int posBytes = mmioSeek(m_mmio, 0, SEEK_CUR);
		return posBytes / m_bytes_per_sample; // サンプル数を返すので 8-BIT でも 16-BIT でも式は変わらない
	}
};


#pragma region KSoundFile
KSoundFile KSoundFile::createFromOgg(const void *data, int size) {
	int err = 0;
	COggImpl *impl = new COggImpl(data, size, &err);
	if (err) {
		delete impl;
		impl = nullptr;
	}
	return KSoundFile(impl);
}
KSoundFile KSoundFile::createFromOgg(const std::string &bin) {
	return createFromOgg(bin.data(), bin.size());
}

KSoundFile KSoundFile::createFromWav(const void *data, int size) {
	int err = 0;
	CWavImplWinMM *impl = new CWavImplWinMM(data, size, &err);
	if (err) {
		delete impl;
		impl = nullptr;
	}
	return KSoundFile(impl);
}
KSoundFile KSoundFile::createFromWav(const std::string &bin) {
	return createFromWav(bin.data(), bin.size());
}

KSoundFile::KSoundFile() {
	m_Impl = nullptr;
}
KSoundFile::KSoundFile(Impl *impl) {
	if (impl) {
		m_Impl = std::shared_ptr<Impl>(impl);
	} else {
		m_Impl = nullptr;
	}
}
bool KSoundFile::isOpen() {
	return m_Impl != nullptr;
}
void KSoundFile::getinfo(int *rate, int *channels, int *samples) {
	if (m_Impl) {
		m_Impl->getinfo(rate, channels, samples);
	}
}
void KSoundFile::seek(int sample_offset) {
	if (m_Impl) {
		m_Impl->seek(sample_offset);
	}
}
int KSoundFile::read(void *buffer, int max_count) {
	if (m_Impl) {
		return m_Impl->read(buffer, max_count);
	}
	return 0;
}
int KSoundFile::tell() {
	if (m_Impl) {
		return m_Impl->tell();
	}
	return 0;
}
int KSoundFile::readLoop(void *_samples, int max_count, int loop_start, int loop_end) {
	return _ReadLoop(m_Impl.get(), _samples, max_count, loop_start, loop_end);
}


#pragma endregion // KSoundFile


#pragma region DirectSound8
class CDS8ScopedLock {
public:
	std::recursive_mutex &m_mutex;

	explicit CDS8ScopedLock(std::recursive_mutex &m): m_mutex(m) {
		m_mutex.lock();
	}
	~CDS8ScopedLock() {
		m_mutex.unlock();
	}
};
#define K__SCOPED_LOCK  CDS8ScopedLock lock__(m_mutex)

class CDS8Buffer {
protected:
	static constexpr int BYTES_FOR_16BIT = 2; // 16ビットサウンドのバイト数
	IDirectSound8 *m_ds8;
	IDirectSoundBuffer8 *m_dsbuf8;
	int m_channels;
	int m_sample_rate; // samples per second per channel
	float m_total_seconds;
public:
	explicit CDS8Buffer(IDirectSound8 *ds8) {
		m_ds8 = ds8;
		if (m_ds8) m_ds8->AddRef();
		m_dsbuf8 = nullptr;
		m_channels = 0;
		m_sample_rate = 0;
		m_total_seconds = 0;
	}
	virtual ~CDS8Buffer() {
		destroyBuffer();
		if (m_ds8) m_ds8->Release();
	}
	virtual void play() = 0;
	virtual void stop() = 0;
	virtual float getPosition() = 0;
	virtual void setPosition(float seconds) = 0;
	virtual bool loadSound(KSoundFile &strm, float buf_sec) = 0;

	void make_clone_of(const CDS8Buffer *source) {
		// 既存のデータは上書きされるので破棄しておく
		destroyBuffer();

		// クローンを作成
		if (m_ds8 && source->m_dsbuf8) {
			m_ds8->DuplicateSoundBuffer(source->m_dsbuf8, (IDirectSoundBuffer **)&m_dsbuf8);
			m_channels = source->m_channels;
			m_sample_rate = source->m_sample_rate;
			m_total_seconds = source->m_total_seconds;
		}
	}
	bool isPlaying() const {
		DWORD status = 0;
		if (m_dsbuf8) m_dsbuf8->GetStatus(&status);
		return status & DSBSTATUS_PLAYING;
	}
	void setPan(float pan) {
		K__ASSERT_RETURN(m_dsbuf8);
		K__ASSERT_RETURN(DSBPAN_RIGHT - DSBPAN_CENTER == DSBPAN_CENTER - DSBPAN_LEFT);
		K__ASSERT_RETURN(DSBPAN_RIGHT > 0);
		const long RANGE = DSBPAN_RIGHT;
		m_dsbuf8->SetPan((long)(RANGE * pan));
	}
	float getPan() const {
		K__ASSERT_RETURN_ZERO(m_dsbuf8);
		K__ASSERT_RETURN_ZERO(DSBPAN_RIGHT - DSBPAN_CENTER == DSBPAN_CENTER - DSBPAN_LEFT);
		K__ASSERT_RETURN_ZERO(DSBPAN_RIGHT > 0);
		const long RANGE = DSBPAN_RIGHT;
		long dspan = 0;
		m_dsbuf8->GetPan(&dspan);
		return (float)dspan / RANGE;
	}
	void setPitch(float pitch) {
		K__ASSERT_RETURN(m_dsbuf8);
		WAVEFORMATEX wf;
		m_dsbuf8->GetFormat(&wf, sizeof(wf), nullptr);
		DWORD def_freq = wf.nSamplesPerSec;
		DWORD new_freq = (DWORD)(def_freq * pitch);
		m_dsbuf8->SetFrequency(new_freq);
	}
	float getPitch() const {
		K__ASSERT_RETURN_ZERO(m_dsbuf8);
		WAVEFORMATEX wf;
		m_dsbuf8->GetFormat(&wf, sizeof(wf), nullptr);
		DWORD def_freq = wf.nSamplesPerSec;
		DWORD cur_freq = def_freq;
		m_dsbuf8->GetFrequency(&cur_freq);
		return (float)cur_freq / def_freq;
	}
	void setVolume(float vol) {
		K__ASSERT_RETURN(m_dsbuf8);
		LONG db_cent; // 0.01[dB]単位での値
		if (vol <= 0.0f) {
			db_cent = DSBVOLUME_MIN;
		} else if (vol >= 1.0f) {
			db_cent = DSBVOLUME_MAX;
		} else {
			db_cent = (LONG)(20 * log10f(vol) * 100);
			if (db_cent < DSBVOLUME_MIN) db_cent = DSBVOLUME_MIN;
			if (db_cent > DSBVOLUME_MAX) db_cent = DSBVOLUME_MAX;
		}
		m_dsbuf8->SetVolume(db_cent);
	}
	float getVolume() const {
		K__ASSERT_RETURN_ZERO(m_dsbuf8);
		LONG db_cent = 0;
		m_dsbuf8->GetVolume(&db_cent); // 0.01 デシベル単位で取得
		float db = db_cent * 0.01f;
		float vol = powf(10, db / 20.0f); // dB = 20*log(OUT/IN)
		if (vol < 0.0f) return 0.0f;
		if (vol > 1.0f) return 1.0f;
		return vol;
	}
	float getLength() {
		return m_total_seconds;
	}

	IDirectSoundBuffer8 * getDS8Buf() { return m_dsbuf8; }

protected:
	bool createBuffer(int numSamples, int channels, int sample_rate) {
		destroyBuffer(); // 既存のバッファを確実に削除しておくように

		K__ASSERT(numSamples >= 0);
		K__ASSERT(channels == 1 || channels == 2);
		K__ASSERT(sample_rate >= 0);

		WAVEFORMATEX wf;
		ZeroMemory(&wf, sizeof(WAVEFORMATEX));
		wf.wFormatTag      = WAVE_FORMAT_PCM;
		wf.nChannels       = (WORD)channels;
		wf.nSamplesPerSec  = (DWORD)sample_rate;
		wf.wBitsPerSample  = (WORD)(BYTES_FOR_16BIT * 8); // Bits
		wf.nBlockAlign     = (WORD)(BYTES_FOR_16BIT * channels);
		wf.nAvgBytesPerSec = (DWORD)(BYTES_FOR_16BIT * channels * sample_rate);

		DSBUFFERDESC desc;
		ZeroMemory(&desc, sizeof(DSBUFFERDESC));
		desc.dwSize = sizeof(DSBUFFERDESC);
		desc.dwBufferBytes = numSamples * BYTES_FOR_16BIT;
		desc.lpwfxFormat = &wf;
		desc.dwFlags =
			DSBCAPS_GETCURRENTPOSITION2 |
			DSBCAPS_CTRLVOLUME          |
			DSBCAPS_CTRLPAN             |
			DSBCAPS_CTRLFREQUENCY       |
			DSBCAPS_GLOBALFOCUS; // ウィンドウがフォーカスを失っても再生を継続
		
		IDirectSoundBuffer *dsbuf = nullptr;
		if (m_ds8) m_ds8->CreateSoundBuffer(&desc, &dsbuf, nullptr); // <-- ここで失敗した場合、desc.dwFlags の組み合わせを疑う
		if (dsbuf) {
			dsbuf->QueryInterface(IID_IDirectSoundBuffer8, (void**)&m_dsbuf8);
			dsbuf->Release();
			dsbuf = nullptr;
		}
		if (m_dsbuf8) {
			m_dsbuf8->SetVolume(0); // 0dB
			m_channels = channels;
			m_sample_rate = sample_rate;
			return true;
		}
		return false;
	}
	void destroyBuffer() {
		if (m_dsbuf8) {
			m_dsbuf8->Release();
			m_dsbuf8 = nullptr;
		}
	}
	int write(int offsetSamples, KSoundFile &strm, int numSamples) {
		int gotSamples = 0;
		if (m_dsbuf8) {
			int16_t *buf;
			DWORD locked;
			DWORD off = offsetSamples * BYTES_FOR_16BIT;
			DWORD num = numSamples * BYTES_FOR_16BIT;
			if (SUCCEEDED(m_dsbuf8->Lock(off, num, (void**)&buf, &locked, nullptr, nullptr, 0))) {
				ZeroMemory(buf, locked);
				gotSamples = strm.read(buf, numSamples);
				m_dsbuf8->Unlock(buf, locked, nullptr, 0);
			}
		}
		return gotSamples;
	}
	int writeLoop(int offsetSamples, KSoundFile &strm, int numSamples, int loopStartSamples, int loopEndSamples) {
		int gotSamples = 0;
		if (m_dsbuf8) {
			int16_t *buf;
			DWORD locked;
			DWORD off = offsetSamples * BYTES_FOR_16BIT;
			DWORD num = numSamples * BYTES_FOR_16BIT;
			if (SUCCEEDED(m_dsbuf8->Lock(off, num, (void**)&buf, &locked, nullptr, nullptr, 0))) {
				ZeroMemory(buf, locked);
				gotSamples = strm.readLoop(buf, numSamples, loopStartSamples, loopEndSamples);
				m_dsbuf8->Unlock(buf, locked, nullptr, 0);
			}
		}
		return gotSamples;
	}
	void setBufferPosition(int samples) {
		DWORD pos_bytes = (DWORD)samples * BYTES_FOR_16BIT;
		if (m_dsbuf8) m_dsbuf8->SetCurrentPosition(pos_bytes);
	}
	int getBufferPosition() const {
		DWORD pos_bytes = 0;
		if (m_dsbuf8) m_dsbuf8->GetCurrentPosition(&pos_bytes, 0);
		return (int)pos_bytes / BYTES_FOR_16BIT; // num samples
	}
	void playBuffer(bool loop) {
		K__ASSERT_RETURN(m_dsbuf8);
		m_dsbuf8->Play(0, 0, loop ? DSBPLAY_LOOPING : 0);
	}
	void stopBuffer() {
		K__ASSERT_RETURN(m_dsbuf8);
		m_dsbuf8->Stop();
	}
};

class CDS8StaticBuffer: public CDS8Buffer {
public:
	explicit CDS8StaticBuffer(IDirectSound8 *ds8): CDS8Buffer(ds8) {}
	virtual void play() override {
		playBuffer(false);
	}
	virtual void stop() override {
		stopBuffer();
	}
	virtual float getPosition() override {
		return (float)getBufferPosition() / m_channels / m_sample_rate;
	}
	virtual void setPosition(float seconds) override {
		setBufferPosition((int)(seconds * m_sample_rate) * m_channels);
	}
	virtual bool loadSound(KSoundFile &strm, float buf_sec) override {
		int rate=0, ch=0, total=0;
		strm.getinfo(&rate, &ch, &total);
		if (createBuffer(total, ch, rate)) {
			m_total_seconds = (float)total / ch / rate;
			write(0, strm, total);
			return true;
		}
		return false;
	}
};

class CDS8StreamingBuffer: public CDS8Buffer {
public:
	static const int NUM_BLOCKS = 2; // ストリーミング再生用のバッファブロック数

public:
	explicit CDS8StreamingBuffer(IDirectSound8 *ds8): CDS8Buffer(ds8) {
		clear();
	}
	virtual ~CDS8StreamingBuffer() {
		clear();
	}
	void clear() {
		m_loop_start  = 0;
		m_loop_end    = 0;
		m_loop        = false;
		m_stop_next   = false;
		m_blocksize   = 0;
		m_blockindex  = -1;
		m_inputpos[0] = 0;
		m_inputpos[1] = 0;
	}
	virtual void play() override {
		K__SCOPED_LOCK;
		m_blockindex = 0;
		m_stop_next = false;
		writeToBlock(0); // ブロック前半にデータを転送
		writeToBlock(1); // ブロック後半にデータを転送
		setBufferPosition(0);
		playBuffer(true);
	}
	virtual void stop() override {
		K__SCOPED_LOCK;
		stopBuffer();
	}
	virtual float getPosition() override {
		K__SCOPED_LOCK;
		int pos = getBufferPosition();
		if (pos < 0) return 0.0f;

		// 再生中のブロック番号
		int idx = pos / m_blocksize;
		if (idx < 0 || NUM_BLOCKS <= idx) return 0.0f;

		// ブロック先頭からのオフセット
		int off = pos % m_blocksize;

		// 入力ストリームの先頭から数えたサンプリング数
		int samples = m_inputpos[idx] + off;
		
		return (float)samples / m_channels / m_sample_rate;
	}
	virtual void setPosition(float seconds) override {
		K__SCOPED_LOCK;
		// 入力ストリームの読み取り位置を変更
		int samples = (int)(seconds * m_sample_rate) * m_channels;
		m_source.seek(samples);

		// 再生中であれば改めて play を実行し、バッファ内容を更新しておく
		if (isPlaying()) {
			play();
		}
	}
	virtual bool loadSound(KSoundFile &strm, float buf_sec) override {
		K__SCOPED_LOCK;
		clear();
		if (!strm.isOpen()) return false;
		if (buf_sec <= 0) return false;

		int rate=0, ch=0, total=0;
		strm.getinfo(&rate, &ch, &total);

		// ストリーム再生用バッファ（メモリブロック）のサイズを決める。
		// 1ブロック = buf_sec 秒分。
		// 作成するバッファは前半用と後半用で2ブロック分になる
		int block_sz = (int)(rate * buf_sec);
		
		if (createBuffer(block_sz * NUM_BLOCKS, ch, rate)) {
			m_source = strm;
			m_blocksize = block_sz;
			m_total_seconds = (float)total / ch / rate;
			return true;
		}
		return false;
	}
	void update() {
		K__SCOPED_LOCK;
		if (m_blockindex < 0) {
			return; // 停止中
		}
		// 再生中のブロック番号を得る
		int idx = getBufferPosition() / m_blocksize;
		if (idx == m_blockindex) {
			return; // 現在のブロックを再生中
		}
		// ブロックの再生が終わり、次のブロックを再生し始めている。
		// 終了フラグがあるなら再生を停止する
		if (m_stop_next) {
			stop();
			m_blockindex = -1;
			m_stop_next = false;
			return;
		}
		// たった今再生が終わったばかりのブロックに次のデータを入れておく
		if (writeToBlock(m_blockindex) == 0) {
			m_stop_next = true; // 入力ストリームからのデータが途絶えた
		}
		// ブロック番号を更新
		m_blockindex = (m_blockindex + 1) % NUM_BLOCKS;
	}
	void setLoop(bool value) {
		m_loop = value;
	}
	bool isLoop() const {
		return m_loop;
	}
	void setLoopRange(float start_seconds, float end_seconds) {
		K__SCOPED_LOCK;
		int rate=0, ch=0;
		m_source.getinfo(&rate, &ch, nullptr);
		m_loop_start = (int)(rate * start_seconds) * ch;
		m_loop_end   = (int)(rate * end_seconds)   * ch;
	}
	void getLoopRange(float *start_seconds, float *end_seconds) {
		K__SCOPED_LOCK;
		int rate=0, ch=0;
		m_source.getinfo(&rate, &ch, nullptr);
		if (start_seconds) *start_seconds = (float)(m_loop_start / ch) / rate;
		if (end_seconds) *end_seconds = (float)(m_loop_end / ch) / rate;
	}
protected:
	int writeToBlock(int index) {
		K__SCOPED_LOCK;
		m_inputpos[index] = m_source.tell();
		if (m_loop) {
			return writeLoop(m_blocksize*index, m_source, m_blocksize, m_loop_start, m_loop_end);
		} else {
			return write(m_blocksize*index, m_source, m_blocksize);
		}
	}
private:
	KSoundFile m_source;
	std::recursive_mutex m_mutex;
	int m_blocksize;
	int m_blockindex;
	int m_loop_start;
	int m_loop_end;
	int m_play_pos;
	int m_inputpos[NUM_BLOCKS];
	bool m_loop;
	bool m_stop_next;
};

class CDS8Sound {
	std::unordered_set<KSoundPlayer::Buf> m_valid_sound_ptr; // ポインタが有効かどうか
	IDirectSound8 *m_ds8;
public:
	CDS8Sound() {
		m_ds8 = nullptr;
	}
	~CDS8Sound() {
		shutdown();
	}
	bool init(HWND hWnd) {
		m_ds8 = nullptr;

		// COM を初期化
		CoInitialize(nullptr);

		// DirectSound8 を作成する
		DirectSoundCreate8(nullptr, &m_ds8, nullptr);

		// ウィンドウを関連付けて優先度を設定する
		if (m_ds8) {
			if (hWnd == nullptr) {
				hWnd = GetActiveWindow(); // 現在のスレッドのウィンドウを使う
			}
			if (hWnd == nullptr) {
				hWnd = GetDesktopWindow(); // デスクトップの HWND を使う
			}
			m_ds8->SetCooperativeLevel(hWnd, DSSCL_PRIORITY);
		}

		return true;
	}
	void shutdown() {
		// DirectSound8 を廃棄
		if (m_ds8) {
			m_ds8->Release();
			m_ds8 = nullptr;
		}
		// COM を終了
		CoUninitialize();
	}

	KSoundPlayer::Buf makeBuffer(KSoundFile &strm) {
		// 非ストリーム再生用のサウンドバッファ（サウンドデータを丸ごと持っている）を作成する
		CDS8StaticBuffer *buf = new CDS8StaticBuffer(m_ds8);
		if (!buf->loadSound(strm, 0.0f)) {
			SND_ERROR();
			delete buf;
			return 0;
		}
		// 利用中のポインタとして登録する
		m_valid_sound_ptr.insert((KSoundPlayer::Buf)buf);
		return (KSoundPlayer::Buf)buf;
	}

	KSoundPlayer::Buf makeStreaming(KSoundFile &strm, float buf_sec) {
		// ストリーム再生用のサウンドバッファ（再生中はサウンドデータを常に供給しないといけない）を作成する
		CDS8StreamingBuffer *buf = new CDS8StreamingBuffer(m_ds8);
		if (!buf->loadSound(strm, buf_sec)) {
			SND_ERROR();
			delete buf;
			return 0;
		}
		// 利用中のポインタとして登録する
		m_valid_sound_ptr.insert((KSoundPlayer::Buf)buf);
		return (KSoundPlayer::Buf)buf;
	}

	KSoundPlayer::Buf makeClone(KSoundPlayer::Buf _buf) {
		// 無効なサウンドバッファをチェック
		if (! isValid(_buf)) {
			SND_ERROR();
			return 0;
		}

		// 非ストリーム再生用のサウンドバッファのみクローン可能
		// （ストリーム再生用の再生バッファをインスタンスごとに分けられないので）
		CDS8Buffer *sb = (CDS8Buffer*)_buf;
		CDS8StaticBuffer *ssb = dynamic_cast<CDS8StaticBuffer*>(sb);
		if (ssb == nullptr) {
			SND_ERROR();
			return 0;
		}

		// クローン（参照コピー）を作成
		CDS8StaticBuffer *copy = new CDS8StaticBuffer(m_ds8);
		copy->make_clone_of(ssb);

		// 利用中のポインタとして登録する
		m_valid_sound_ptr.insert((KSoundPlayer::Buf)copy);
		return (KSoundPlayer::Buf)copy;
	}

	void remove(KSoundPlayer::Buf _buf) {
		stop(_buf);
		m_valid_sound_ptr.erase(_buf);
		CDS8Buffer *sb = (CDS8Buffer*)_buf;
		delete sb;
	}
	bool isValid(KSoundPlayer::Buf _buf) {
		return _buf && (m_valid_sound_ptr.find(_buf) != m_valid_sound_ptr.end());
	}
	void setStreamingLoop(KSoundPlayer::Buf _buf, bool value) {
		if (!isValid(_buf)) return;
		CDS8Buffer *sb = (CDS8Buffer*)_buf;
		CDS8StreamingBuffer *ssb = dynamic_cast<CDS8StreamingBuffer*>(sb);
		if (ssb) {
			ssb->setLoop(value);
		}
	}
	void setStreamingRange(KSoundPlayer::Buf _buf, float start_sec, float end_sec) {
		if (!isValid(_buf)) return;
		CDS8Buffer *sb = (CDS8Buffer*)_buf;
		CDS8StreamingBuffer *ssb = dynamic_cast<CDS8StreamingBuffer*>(sb);
		if (ssb) {
			ssb->setLoopRange(start_sec, end_sec);
		}
	}
	void updateStreaming(KSoundPlayer::Buf _buf) {
		if (!isValid(_buf)) return;
		CDS8Buffer *sb = (CDS8Buffer*)_buf;
		CDS8StreamingBuffer *ssb = dynamic_cast<CDS8StreamingBuffer*>(sb);
		if (ssb) {
			ssb->update();
		}
	}
	void play(KSoundPlayer::Buf _buf) {
		if (!isValid(_buf)) return;
		CDS8Buffer *sb = (CDS8Buffer*)_buf;
		sb->play();
	}
	void stop(KSoundPlayer::Buf _buf) {
		if (!isValid(_buf)) return;
		CDS8Buffer *sb = (CDS8Buffer*)_buf;
		sb->stop();
	}
	void setVolume(KSoundPlayer::Buf _buf, float value) {
		if (!isValid(_buf)) return;
		CDS8Buffer *sb = (CDS8Buffer*)_buf;
		sb->setVolume(value);
	}
	void setPitch(KSoundPlayer::Buf _buf, float value) {
		if (!isValid(_buf)) return;
		CDS8Buffer *sb = (CDS8Buffer*)_buf;
		sb->setPitch(value);
	}
	void setPan(KSoundPlayer::Buf _buf, float value) {
		if (!isValid(_buf)) return;
		CDS8Buffer *sb = (CDS8Buffer*)_buf;
		sb->setPan(value);
	}
	void setPosition(KSoundPlayer::Buf _buf, float seconds) {
		if (!isValid(_buf)) return;
		CDS8Buffer *sb = (CDS8Buffer*)_buf;
		sb->setPosition(seconds);
	}
	float getParameterf(KSoundPlayer::Buf _buf, KSoundPlayer::Info id) {
		if (!isValid(_buf)) return 0;
		CDS8Buffer *sb = (CDS8Buffer*)_buf;
		CDS8StreamingBuffer *ssb = nullptr;
		float val = 0;
		switch (id) {
		case KSoundPlayer::INFO_POSITION_SECONDS:
			return sb->getPosition();
		case KSoundPlayer::INFO_LENGTH_SECONDS:
			return sb->getLength();
		case KSoundPlayer::INFO_PAN:
			return sb->getPan();
		case KSoundPlayer::INFO_PITCH:
			return sb->getPitch();
		case KSoundPlayer::INFO_VOLUME:
			return sb->getVolume();
		case KSoundPlayer::INFO_IS_PLAYING:
			return sb->isPlaying() ? 1.0f : 0.0f;
		case KSoundPlayer::INFO_IS_LOOPING:
			ssb = dynamic_cast<CDS8StreamingBuffer*>(sb);
			return (ssb && ssb->isLoop()) ? 1.0f : 0.0f;
		case KSoundPlayer::INFO_IS_STREAMING:
			ssb = dynamic_cast<CDS8StreamingBuffer*>(sb);
			return ssb ? 1.0f : 0.0f;
		case KSoundPlayer::INFO_LOOP_START_SECONDS:
			ssb = dynamic_cast<CDS8StreamingBuffer*>(sb);
			if (ssb) ssb->getLoopRange(&val, nullptr);
			return val;
		case KSoundPlayer::INFO_LOOP_END_SECONDS:
			ssb = dynamic_cast<CDS8StreamingBuffer*>(sb);
			if (ssb) ssb->getLoopRange(nullptr, &val);
			return val;
		}
		return 0;
	}
}; // KSoundPlayer

static CDS8Sound g_Sound;
#pragma endregion // DirectSound8


#pragma region KSoundPlayer
bool KSoundPlayer::init(void *hWnd) {
	return g_Sound.init((HWND)hWnd);
}
void KSoundPlayer::shutdown() {
	g_Sound.shutdown();
}
KSoundPlayer::Buf KSoundPlayer::makeBuffer(KSoundFile &strm) {
	return g_Sound.makeBuffer(strm);
}
KSoundPlayer::Buf KSoundPlayer::makeStreaming(KSoundFile &strm, float bufsize_sec) {
	return g_Sound.makeStreaming(strm, bufsize_sec);
}
KSoundPlayer::Buf KSoundPlayer::makeClone(Buf buf) {
	return g_Sound.makeClone(buf);
}
void KSoundPlayer::remove(Buf buf) {
	g_Sound.remove(buf);
}
bool KSoundPlayer::isValid(Buf buf) {
	return g_Sound.isValid(buf);
}
void KSoundPlayer::setStreamingLoop(Buf buf, bool value) {
	g_Sound.setStreamingLoop(buf, value);
}
void KSoundPlayer::setStreamingRange(Buf buf, float start_sec, float end_sec) {
	g_Sound.setStreamingRange(buf, start_sec, end_sec);
}
void KSoundPlayer::updateStreaming(Buf buf) {
	g_Sound.updateStreaming(buf);
}
void KSoundPlayer::play(Buf buf) {
	g_Sound.play(buf);
}
void KSoundPlayer::stop(Buf buf) {
	g_Sound.stop(buf);
}
void KSoundPlayer::setVolume(Buf buf, float value) {
	g_Sound.setVolume(buf, value);
}
void KSoundPlayer::setPitch(Buf buf, float value) {
	g_Sound.setPitch(buf, value);
}
void KSoundPlayer::setPan(Buf buf, float value) {
	g_Sound.setPan(buf, value);
}
void KSoundPlayer::setPosition(Buf buf, float seconds) {
	g_Sound.setPosition(buf, seconds);
}
float KSoundPlayer::getParameterf(Buf buf, Info id) {
	return g_Sound.getParameterf(buf, id);
}
#pragma endregion // KSoundPlayer


} // namespace
