#pragma once
#include <memory> // std::shared_ptr
#include <string>

namespace Kamilo {


class KSoundFile {
public:
	class Impl {
	public:
		virtual ~Impl() {}
		virtual void getinfo(int *rate, int *channels, int *samples) = 0;
		virtual void seek(int sample_offset) = 0;
		virtual int read(void *buffer, int max_count) = 0;
		virtual int tell() = 0;
	};

	static KSoundFile createFromOgg(const void *data, int size); // .ogg
	static KSoundFile createFromOgg(const std::string &bin);

	static KSoundFile createFromWav(const void *data, int size); // .wav
	static KSoundFile createFromWav(const std::string &bin);

	KSoundFile();
	explicit KSoundFile(Impl *impl);

	bool isOpen();

	/// rate: 秒間サンプル数
	/// channels: チャンネル数 (1=モノラル 2=ステレオ)
	/// samples: 総サンプル数 = rate * channels * 秒数
	void getinfo(int *rate, int *channels, int *samples);

	/// サンプル単位でシークする
	/// 成功したら 0 を返す
	/// @note チャンネル数によるブロックアラインメントを考慮すること。
	/// 例えば2チャンネルサウンドならばサンプルが LRLRLRLR... と並んでいる。
	/// この場合は左右1組（2サンプル=1ブロック）を1つの単位にしてシークさせる必要があるため、サンプル数には2の倍数を指定する
	void seek(int sample_offset);

	/// サンプル単位でデータを読み取り、buffer に書き込む。
	/// 実際に読み取って書き込んだサンプル数を返す
	/// @param buffer     読み取ったデータの格納先
	/// @param max_count  読み取るサンプル数の最大値
	/// @note サンプルの数え方に注意。詳細は KSoundFile::seek() を参照
	int read(void *buffer, int max_count);

	/// 現在の読み取り位置をサンプル単位で返す
	/// @note サンプルの数え方に注意。詳細は KSoundFile::seek() を参照
	int tell();

	/// サンプル単位でデータを読み取り samples に書き込む。実際に読み取って書き込んだサンプル数を返す
	/// KSoundFile::read とは異なり、ループポイントを考慮する。
	/// 
	/// @param samples     読み取ったデータの格納先
	/// @param max_count   読み取るサンプル数の最大値
	/// @param loop_start  サンプル単位でのループ開始位置
	/// @param loop_end    サンプル単位でのループ終端位置。0 を指定した場合は曲の末尾がループ終端になる
	///
	/// @note サンプルの数え方に注意。詳細は KSoundFile::seek() を参照
	int readLoop(void *_samples, int max_count, int loop_start, int loop_end);

private:
	std::shared_ptr<Impl> m_Impl;
};


class KSoundPlayer {
public:
	typedef intptr_t Buf;

	enum Info {
		INFO_POSITION_SECONDS,
		INFO_LENGTH_SECONDS,
		INFO_PAN,
		INFO_PITCH,
		INFO_VOLUME,
		INFO_IS_PLAYING,
		INFO_IS_LOOPING,
		INFO_IS_STREAMING,
		INFO_LOOP_START_SECONDS,
		INFO_LOOP_END_SECONDS,
	};

	static bool init(void *hWnd);
	static void shutdown();

	/// サウンドバッファを作成する
	/// strmで指定されたサウンドデータはすべてコピーされるので呼び出し元で安全に削除できる
	/// 使用後は remove で削除する
	static Buf makeBuffer(KSoundFile &strm);

	/// ストリーム再生用のサウンドバッファを作成する。
	/// strm は内部で grab() するので、呼び出し側でこれ以上使わないなら drop してよい
	/// bufsize_sec にはストリーム再生用のバッファ長さを秒単位で指定する。
	/// 再生開始後、この秒数が経過する前に updateStreaming を呼んで新しいデータをセットしないといけない
	/// 使用後は remove で削除する
	static Buf makeStreaming(KSoundFile &strm, float bufsize_sec);

	/// クローン（参照コピー）を作成する。
	/// 作成できるのは makeBuffer で作成した非ストリーミングバッファのみ。
	/// 成功すれば buf の新しい参照を返す。使用後は remove で削除する
	static Buf makeClone(Buf buf);

	/// サウンドバッファを削除する
	static void remove(Buf buf);

	/// サウンドバッファ buf が有効かどうか確認する。
	/// 利用可能な状態であれば true を返す。
	/// 存在しなかったり、削除済みのサウンドバッファだった場合は false を返す
	static bool isValid(Buf buf);

	/// ループ再生を設定する。buf はストリーミング用でないといけない
	static void setStreamingLoop(Buf buf, bool value);

	/// ループ範囲を設定する。buf はストリーミング用でないといけない
	static void setStreamingRange(Buf buf, float start_sec, float end_sec);

	/// ストリーム再生用のバッファに新しいデータをセットする。
	/// ストリーム再生用のバッファの状態をチェックし、
	/// バッファに残っている未再生分のデータが一定以下になっていたら新しいデータを追加する
	static void updateStreaming(Buf buf);

	/// サウンドを再生する
	/// @param buf 操作対象のサウンド
	static void play(Buf buf);

	/// サウンドを停止する
	/// @param buf 操作対象のサウンド
	static void stop(Buf buf);

	/// 音量を取得する
	/// @param buf 操作対象のサウンド
	/// @return    音量を 0.0 以上 1.0 以下で表した値
	static void setVolume(Buf buf, float value);

	/// 再生速度を設定する
	/// @param buf    操作対象のサウンド
	/// @param value  再生速度. 1.0 で通常速度
	static void setPitch(Buf buf, float value);

	/// 音像定位を取得する
	/// @param buf 操作対象のサウンド
	/// @return    定位を -1.0 以上 +1.0 以下で表した値
	static void setPan(Buf buf, float value);

	/// 再生位置を設定する（秒単位）
	/// @param buf     操作対象のサウンド
	/// @param seconds 新しい再生位置（秒）
	static void setPosition(Buf buf, float seconds);

	/// サウンド情報を得る
	static float getParameterf(Buf buf, Info id);
};

} // namespace
