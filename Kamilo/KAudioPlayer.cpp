#include "KAudioPlayer.h"
//
#include <unordered_map>
#include <mutex>
#include "KImGui.h"
#include "KInternal.h"
#include "KMath.h"
#include "KNode.h"
#include "KSound.h"
#include "KStorage.h"
#include "KThread.h"
#include "keng_game.h"

namespace Kamilo {


#define K__DS_BYTES_PER_SAMPLE 2 // 16ビットサウンドのバイト数
#define K__DS_STREAMING_BUFFSER_SEC  2.0f // ストリーミングのバッファサイズ（秒単位）
#define K__DS_RELEASE(x) {if (x) (x)->Release(); x=nullptr;}


#pragma region Internal Utils
class CScopedLock {
public:
	std::recursive_mutex &m_Mutex;

	explicit CScopedLock(std::recursive_mutex &m): m_Mutex(m) {
		m_Mutex.lock();
	}
	~CScopedLock() {
		m_Mutex.unlock();
	}
};
#define K__SCOPED_LOCK  CScopedLock lock__(m_Mutex) // m_Mutex = std::recursive_mutex

#pragma endregion // Internal Utils




class CSoundImpl {
public:
	CSoundImpl();
	~CSoundImpl();
	bool is_init() const;
	void init();
	void shutdown();
	/// サウンドファイルをロードして playPooledSound() で再生可能な状態にする
	/// @param name 登録するサウンド名
	/// @param data サウンドデータ
	/// @param size サウンドデータバイト数
	void poolSound(const std::string &name, const void *data, size_t size);
	/// サウンドファイルがロード済みかどうか。
	/// このメソッドが true を返せば playPooledSound() で再生することができる
	/// @param name サウンド名
	/// @return ロード済みなら true
	bool isPooled(const std::string &name);
	/// ロード済みのサウンドファイルを削除する
	/// @param name サウンド名
	void deletePooledSound(const std::string &name);
	/// ロード済みオーディオクリップを再生し、サウンドIDを返す。
	/// @note サウンドはあらかじめ poolSound() でロードしておくこと
	/// @param name サウンド名
	/// @param volume 初期音量。playPooledSound を呼んでから setVolume すると、最初の音出しまでに音量設定が間に合わない可能性がある
	/// @return 再生に成功すれば、そのサウンド ID 。失敗したら NULL
	KSOUNDID playPooledSound(const std::string &name, float volume);
	/// オーディオクリップをストリーミング再生する
	/// @param data              サウンドデータ
	/// @param size              サウンドデータバイト数
	/// @param offsetSeconds     再生開始位置（秒）
	/// @param looping           ループ再生するかどうか
	/// @param loopStartSeconds  ループ範囲の始点（秒）
	/// @param loopEndSeconds    ループ範囲の終点（秒）
	/// @return 再生に成功すれば、そのサウンド ID 。失敗したら NULL
	KSOUNDID playStreamingSound(const void *data, size_t size, float offsetSeconds, bool loop, float loopStartSeconds=0.0f, float loopEndSeconds=0.0f);
	void pause(KSOUNDID id);
	void resume(KSOUNDID id);
	void deleteHandle(KSOUNDID id);
	float getPositionSeconds(KSOUNDID id);
	float getLengthSeconds(KSOUNDID id);
	void setPositionSeconds(KSOUNDID id, float seconds);
	bool isLooping(KSOUNDID id);
	bool isPlaying(KSOUNDID id);
	void setLooping(KSOUNDID id, bool value);
	void setPan(KSOUNDID id, float value);
	float getPan(KSOUNDID id);
	void setVolume(KSOUNDID id, float value);
	float getVolume(KSOUNDID id);
	void setPitch(KSOUNDID id, float value);
	float getPitch(KSOUNDID id);
	void setListenerPosition(const float *pos_xyz);
	void setSourcePosition(KSOUNDID id, const float *pos_xyz);
	void command(const char *cmd, int *args);
	void updateStreaming();
	void updateThreadProc();

private:
	std::unordered_map<KSOUNDID, KSoundPlayer::Buf> m_Sounds;
	std::unordered_map<std::string, KSoundPlayer::Buf> m_Pool;
	int m_NewId;
	std::recursive_mutex m_Mutex; // m_Sounds, m_Pool の操作に対するロック用
	bool m_IsInit;

	class CSoundThread: public KThread {
	public:
		CSoundImpl *m_SoundImpl;
		CSoundThread() {
			m_SoundImpl = nullptr;
		}
		virtual void run() override {
			m_SoundImpl->updateThreadProc();
		}
	};
	CSoundThread m_Thread;
};

#pragma region CSoundImpl
CSoundImpl::CSoundImpl() {
	m_NewId = 0;
	m_IsInit = false;
	m_Thread.m_SoundImpl = this;
}
CSoundImpl::~CSoundImpl() {
	m_Thread.m_SoundImpl = nullptr;
	shutdown();
}
bool CSoundImpl::is_init() const {
	return m_IsInit;
}
void CSoundImpl::init() {
	// ストリーミング再生用のスレッドを開始する
	m_Thread.start();
}
void CSoundImpl::shutdown() {
	// ストリーミング再生用のスレッドを停止する
	m_Thread.stop();

	// サウンドオブジェクトをすべて削除する
	for (auto it=m_Sounds.begin(); it!=m_Sounds.end(); ++it) {
		KSoundPlayer::remove(it->second);
	}
	for (auto it=m_Pool.begin(); it!=m_Pool.end(); ++it) {
		KSoundPlayer::remove(it->second);
	}
	// K__SCOPED_LOCK もうスレッドからは抜けているはずなので、ロック不要
	m_Sounds.clear();
	m_Pool.clear();
}
/// サウンドファイルをロードして playPooledSound() で再生可能な状態にする
/// @param name 登録するサウンド名
/// @param data サウンドデータ
/// @param size サウンドデータバイト数
void CSoundImpl::poolSound(const std::string &name, const void *data, size_t size) {
	if (isPooled(name)) {
		return;
	}
	KSoundFile oggStrm = KSoundFile::createFromOgg(data, size);
	if (oggStrm.isOpen()) {
 		KSoundPlayer::Buf buf = KSoundPlayer::makeBuffer(oggStrm);
		if (buf) {
			K__SCOPED_LOCK;
			m_Pool[name] = buf;
			return;
		}
		K__ERROR("E_FAIL_PLAY_SOUND: %s", name.c_str());
		return;
	}

	KSoundFile wavStrm = KSoundFile::createFromWav(data, size);
	if (wavStrm.isOpen()) {
		KSoundPlayer::Buf buf = KSoundPlayer::makeBuffer(wavStrm);
		if (buf) {
			K__SCOPED_LOCK;
			m_Pool[name] = buf;
			return;
		}
		K__ERROR("E_FAIL_PLAY_SOUND: %s", name.c_str());
		return;
	}
	K__ERROR("E_FAIL_LOAD_SOUND: %s", name.c_str());
	return;
}
/// サウンドファイルがロード済みかどうか。
/// このメソッドが true を返せば playPooledSound() で再生することができる
/// @param name サウンド名
/// @return ロード済みなら true
bool CSoundImpl::isPooled(const std::string &name) {
	K__SCOPED_LOCK;
	auto it = m_Pool.find(name);
	if (it != m_Pool.end()) {
		return true;
	}
	return false;
}
/// ロード済みのサウンドファイルを削除する
/// @param name サウンド名
void CSoundImpl::deletePooledSound(const std::string &name) {
	K__SCOPED_LOCK;
	m_Pool.erase(name);
}
/// ロード済みオーディオクリップを再生し、サウンドIDを返す。
/// @note サウンドはあらかじめ poolSound() でロードしておくこと
/// @param name サウンド名
/// @param volume 初期音量。playPooledSound を呼んでから setVolume すると、最初の音出しまでに音量設定が間に合わない可能性がある
/// @return 再生に成功すれば、そのサウンド ID 。失敗したら NULL
KSOUNDID CSoundImpl::playPooledSound(const std::string &name, float volume) {
	K__SCOPED_LOCK;
	K__ASSERT(!name.empty());
	auto it = m_Pool.find(name);
	if (it == m_Pool.end()) {
		return nullptr;
	}
	KSoundPlayer::Buf se = KSoundPlayer::makeClone(it->second);
	if (se == 0) {
		return nullptr;
	}
	KSoundPlayer::setVolume(se, volume);
	KSoundPlayer::play(se);

	KSOUNDID id = (KSOUNDID)(++m_NewId);
	m_Sounds[id] = se;
	return id;
}
/// オーディオクリップをストリーミング再生する
/// @param data              サウンドデータ
/// @param size              サウンドデータバイト数
/// @param offsetSeconds     再生開始位置（秒）
/// @param looping           ループ再生するかどうか
/// @param loopStartSeconds  ループ範囲の始点（秒）
/// @param loopEndSeconds    ループ範囲の終点（秒）
/// @return 再生に成功すれば、そのサウンド ID 。失敗したら NULL
KSOUNDID CSoundImpl::playStreamingSound(const void *data, size_t size, float offsetSeconds, bool loop, float loopStartSeconds, float loopEndSeconds) {
	K__SCOPED_LOCK;
	KSoundFile strm = KSoundFile::createFromOgg(data, size);
	if (!strm.isOpen()) {
		K__ERROR("E_FAIL_OPEN_SOUND");
		return nullptr;
	}

	KSoundPlayer::Buf music = KSoundPlayer::makeStreaming(strm, K__DS_STREAMING_BUFFSER_SEC);
	if (music == 0) {
		K__ERROR("E_FAIL_OPEN_SOUND");
		return nullptr;
	}

	KSoundPlayer::setStreamingLoop(music, loop);
	KSoundPlayer::setStreamingRange(music, loopStartSeconds, loopEndSeconds);
	KSoundPlayer::setPosition(music, offsetSeconds);
	KSoundPlayer::play(music);

	KSOUNDID id = (KSOUNDID)(++m_NewId);
	m_Sounds[id] = music;
	return id;
}
void CSoundImpl::pause(KSOUNDID id) {
	K__SCOPED_LOCK;
	auto it = m_Sounds.find(id);
	if (it != m_Sounds.end()) {
		KSoundPlayer::stop(it->second);
	}
}
void CSoundImpl::resume(KSOUNDID id) {
	K__SCOPED_LOCK;
	auto it = m_Sounds.find(id);
	if (it != m_Sounds.end()) {
		KSoundPlayer::play(it->second);
	}
}
void CSoundImpl::deleteHandle(KSOUNDID id) {
	K__SCOPED_LOCK;
	auto it = m_Sounds.find(id);
	if (it != m_Sounds.end()) {
		KSoundPlayer::remove(it->second);
		m_Sounds.erase(it);
	}
}
float CSoundImpl::getPositionSeconds(KSOUNDID id) {
	K__SCOPED_LOCK;
	auto it = m_Sounds.find(id);
	if (it != m_Sounds.end()) {
		return KSoundPlayer::getParameterf(it->second, KSoundPlayer::INFO_POSITION_SECONDS);
	}
	return 0.0f;
}
float CSoundImpl::getLengthSeconds(KSOUNDID id) {
	K__SCOPED_LOCK;
	auto it = m_Sounds.find(id);
	if (it != m_Sounds.end()) {
		return KSoundPlayer::getParameterf(it->second, KSoundPlayer::INFO_LENGTH_SECONDS);
	}
	return 0.0f;
}
void CSoundImpl::setPositionSeconds(KSOUNDID id, float seconds) {
	K__SCOPED_LOCK;
	auto it = m_Sounds.find(id);
	if (it != m_Sounds.end()) {
		KSoundPlayer::setPosition(it->second, seconds);
	}
}
bool CSoundImpl::isLooping(KSOUNDID id) {
	K__SCOPED_LOCK;
	auto it = m_Sounds.find(id);
	if (it != m_Sounds.end()) {
		return KSoundPlayer::getParameterf(it->second, KSoundPlayer::INFO_IS_LOOPING) != 0.0f;
	}
	return false;
}
bool CSoundImpl::isPlaying(KSOUNDID id) {
	K__SCOPED_LOCK;
	auto it = m_Sounds.find(id);
	if (it != m_Sounds.end()) {
		return KSoundPlayer::getParameterf(it->second, KSoundPlayer::INFO_IS_PLAYING) != 0.0f;
	}
	return false;
}
void CSoundImpl::setLooping(KSOUNDID id, bool value) {
	K__SCOPED_LOCK;
	auto it = m_Sounds.find(id);
	if (it != m_Sounds.end()) {
		KSoundPlayer::setStreamingLoop(it->second, value);
	}
}
void CSoundImpl::setPan(KSOUNDID id, float value) {
	K__SCOPED_LOCK;
	auto it = m_Sounds.find(id);
	if (it != m_Sounds.end()) {
		KSoundPlayer::setPan(it->second, value);
	}
}
float CSoundImpl::getPan(KSOUNDID id) {
	K__SCOPED_LOCK;
	auto it = m_Sounds.find(id);
	if (it != m_Sounds.end()) {
		return KSoundPlayer::getParameterf(it->second, KSoundPlayer::INFO_PAN);
	}
	return 0.0f;
}
void CSoundImpl::setVolume(KSOUNDID id, float value) {
	K__SCOPED_LOCK;
	auto it = m_Sounds.find(id);
	if (it != m_Sounds.end()) {
		KSoundPlayer::setVolume(it->second, value);
	}
}
float CSoundImpl::getVolume(KSOUNDID id) {
	K__SCOPED_LOCK;
	auto it = m_Sounds.find(id);
	if (it != m_Sounds.end()) {
		return KSoundPlayer::getParameterf(it->second, KSoundPlayer::INFO_VOLUME);
	}
	return 0.0f;
}
void CSoundImpl::setPitch(KSOUNDID id, float value) {
	K__SCOPED_LOCK;
	auto it = m_Sounds.find(id);
	if (it != m_Sounds.end()) {
		KSoundPlayer::setPitch(it->second, value);
	}
}
float CSoundImpl::getPitch(KSOUNDID id) {
	K__SCOPED_LOCK;
	auto it = m_Sounds.find(id);
	if (it != m_Sounds.end()) {
		return KSoundPlayer::getParameterf(it->second, KSoundPlayer::INFO_PITCH);
	}
	return 1.0f;
}
void CSoundImpl::setListenerPosition(const float *pos_xyz) {
}
void CSoundImpl::setSourcePosition(KSOUNDID id, const float *pos_xyz) {
}
void CSoundImpl::command(const char *cmd, int *args) {
}
void CSoundImpl::updateStreaming() {
	K__SCOPED_LOCK;
	for (auto it=m_Sounds.begin(); it!=m_Sounds.end(); ++it) {
		if (KSoundPlayer::getParameterf(it->second, KSoundPlayer::INFO_IS_STREAMING)) {
			KSoundPlayer::updateStreaming(it->second);
		}
	}
}
void CSoundImpl::updateThreadProc() {
	// 適当な間隔で update を繰り返す。
	// 少なくとも K__DS_STREAMING_BUFFSER_SEC 秒の半分未満の時間間隔で更新すること
	int interval_msec = (int)(K__DS_STREAMING_BUFFSER_SEC * 1000) / 4;

	while (!m_Thread.shouldExit()) {
		updateStreaming();

		// 適当な時間待つだけ。スリープの精度が低くてもよい
		K::sleep(interval_msec);
	}
}
#pragma endregion // CSoundImpl









class CAudioPlayer: public KManager, public KInspectorCallback {
public:
	struct SSndItem {
		std::string name;   // サウンド名
		float vol;    // このサウンド固有の音量（フェードなどに左右されない値）
		int group_id; // サウンドの所属グループ
		bool destroy_on_stop; // 停止したら自動的に削除する？

		SSndItem() {
			vol = 0;
			group_id = 0;
			destroy_on_stop = false;
		}
	};
	struct SSndGroup {
		SSndGroup() {
			master_volume = 1.0f;
			volume = 1.0f;
			flags = 0;
			solo = false;
			mute = false;
		}
		std::string name; // グループ名
		KAudioFlags flags;
		float master_volume; // グループ音量（オプション設定などで指定する音量）
		float volume; // グループの一時的な音量（イベント中だけ音量を下げておくなど）
		bool solo;
		bool mute;
	};
	struct SFade {
		SFade() {
			sound_id = 0;
			group_id = -1;
			volume_start = 0;
			volume_end = 0;
			duration = 0;
			time = 0;
			auto_stop = false;
			finished = false;
		}
		float normalized_time() const {
			return (float)time / duration;
		}
		KSOUNDID sound_id;
		int group_id;
		float volume_start;
		float volume_end;
		int duration;
		int time;
		bool auto_stop;
		bool finished;
	};
	std::unordered_map<KSOUNDID, SSndItem> m_Sounds;
	std::unordered_set<KSOUNDID> m_PostDestroySounds;
	std::vector<SFade> m_Fades;
	std::vector<SSndGroup> m_Groups;
	CSoundImpl m_SndImpl;
	int m_SoloGroup; // KAudioFlag_SOLO が設定されてているグループ番号。未設定ならば -1
	float m_MasterVolume;
	bool m_MasterMute;
	KStorage *m_Storage;

	CAudioPlayer() {
		m_MasterVolume = 1.0f;
		m_MasterMute = false;
		m_SoloGroup = -1;
		m_Storage = nullptr;

		SSndGroup def_group;
		def_group.name = "Default";
		m_Groups.push_back(def_group);

		KEngine::addManager(this);
		KEngine::addInspectorCallback(this, u8"オーディオ"); // KInspectorCallback
	}
	virtual void on_manager_start() override {
		m_SndImpl.init();
	}
	virtual void on_manager_end() override {
		clearAllSounds();
		m_SndImpl.shutdown();
	}
	virtual void on_manager_frame() override {
		// サウンド処理
		for (auto it=m_Fades.begin(); it!=m_Fades.end(); ++it) {
			SFade *fade = &(*it);
			if (!fade->finished) {
				if (fade->time < fade->duration) {
					float t = fade->normalized_time();
					float v = KMath::lerp(fade->volume_start, fade->volume_end, t);

					// 音量更新
					if (fade->sound_id) {
						m_SndImpl.setVolume(fade->sound_id, v);

					} else if (fade->group_id >= 0) {
						set_group_volume(fade->group_id, v);
					}
					fade->time++;

				} else {
					// フェード終了。最終音量に設定する
					if (fade->sound_id) {
						m_SndImpl.setVolume(fade->sound_id, fade->volume_end);
					} else {
						set_group_volume(fade->group_id, fade->volume_end);
					}
					// サウンドの自動削除
					if (fade->sound_id && fade->auto_stop) {
						stop(fade->sound_id, 0);
					}
					fade->finished = true;
				}
			}
		}
		if (!m_Fades.empty()) {
			for (auto it=m_Fades.begin(); it!=m_Fades.end();/* ++it*/) {
				if (it->finished) {
					it = m_Fades.erase(it);
				} else {
					++it;
				}
			}
		}

		// 再生終了しているサウンドを削除する
		for (auto it=m_Sounds.begin(); it!=m_Sounds.end(); ++it) {
			KSOUNDID id = it->first;
			const SSndItem &snd = it->second;
			if (snd.destroy_on_stop) {
				if (! m_SndImpl.isPlaying(id)) {
					m_PostDestroySounds.insert(id);
				}
			}
		}

		// 無効化されたサウンドを削除
		if (! m_PostDestroySounds.empty()) {
			for (auto it=m_PostDestroySounds.begin(); it!=m_PostDestroySounds.end(); ++it) {
				KSOUNDID id = *it;
				m_SndImpl.deleteHandle(id);
				m_Sounds.erase(id);
			}
			m_PostDestroySounds.clear();
		}
	}
	virtual void onInspectorGui() override { // KInspectorCallback
		if (ImGui::TreeNodeEx("Groups", ImGuiTreeNodeFlags_DefaultOpen)) {
			if (ImGui::TreeNodeEx(KImGui::ID(-1), ImGuiTreeNodeFlags_Leaf, "Master")) {
				if (ImGui::SliderFloat("Volume", &m_MasterVolume, 0.0f, 1.0f)) {
					updateSoundVolumes(-1);
				}
				if (ImGui::Checkbox("Mute", &m_MasterMute)) {
					updateSoundVolumes(-1);
				}
				ImGui::TreePop();
			}
			for (size_t i=0; i<m_Groups.size(); i++) {
				SSndGroup group = m_Groups[i];
				ImGui::Separator();
				if (ImGui::TreeNodeEx(KImGui::ID(i), ImGuiTreeNodeFlags_Leaf, "%s", group.name.c_str())) {
					if (ImGui::SliderFloat("Master Vol.", &group.master_volume, 0.0f, 1.0f)) {
						setGroupMasterVolume(i, group.master_volume);
					}
					if (ImGui::SliderFloat("Volume", &group.volume, 0.0f, 1.0f)) {
						setGroupVolume(i, group.volume, 0);
					}
					if (ImGui::CheckboxFlags("Mute", (unsigned int *)&group.flags, KAudioFlag_MUTE)) {
						setGroupFlags(i, group.flags);
					}
					ImGui::SameLine();
					if (ImGui::CheckboxFlags("Solo", (unsigned int *)&group.flags, KAudioFlag_SOLO)) {
						setGroupFlags(i, group.flags);
					}
					ImGui::TreePop();
				}
			}
			ImGui::TreePop();
		}
		if (ImGui::TreeNode("Sounds")) {
			ImGui::Text("Sound count: %d", (int)m_Sounds.size());
			for (auto it=m_Sounds.begin(); it!=m_Sounds.end(); ++it) {
				const KSOUNDID sid = it->first;
				float pos_sec = m_SndImpl.getPositionSeconds(sid);
				float len_sec = m_SndImpl.getLengthSeconds(sid);
				
				ImGui::Separator();
				ImGui::Text("Name: %s", it->second.name.c_str());
				ImGui::Text("Pos[sec]: %0.3f/%0.3f", pos_sec, len_sec);

				ImGui::PushID(sid);
				if (ImGui::SliderFloat("Seek", &pos_sec, 0, len_sec)) {
					m_SndImpl.setPositionSeconds(sid, pos_sec);
				}
				float p = m_SndImpl.getPitch(sid);
				if (ImGui::SliderFloat("Pitch", &p, 0.1f, 4.0f)) {
					m_SndImpl.setPitch(sid, p);
				}
				float vol = m_SndImpl.getVolume(sid);
				if (ImGui::SliderFloat("Volume", &vol, 0.0f, 1.0f)) {
					m_SndImpl.setVolume(sid, vol);
				}
				float pan = m_SndImpl.getPan(sid);
				if (ImGui::SliderFloat("Pan", &pan, -1.0f, 1.0f)) {
					m_SndImpl.setPan(sid, pan);
				}
				bool L = m_SndImpl.isLooping(sid);
				if (ImGui::Checkbox("Loop", &L)) {
					m_SndImpl.setLooping(sid, L);
				}
				ImGui::PopID();
			}
			ImGui::TreePop();
		}
	}
	// 現在のサウンドグループ数を返す
	int getGroupCount() {
		return (int)m_Groups.size();
	}
	void setGroupCount(int count) {
		if (count > 0) {
			m_Groups.resize(count);
		} else {
			m_Groups.clear();
		}
	}
	float getGroupMasterVolume(int group_id) {
		if (0 <= group_id && group_id < (int)m_Groups.size()) {
			return m_Groups[group_id].master_volume;
		}
		return 0.0f;
	}
	float getGroupVolume(int group_id) {
		if (0 <= group_id && group_id < (int)m_Groups.size()) {
			return m_Groups[group_id].volume;
		}
		return 0.0f;
	}
	void setGroupMasterVolume(int group_id, float volume) {
		if (0 <= group_id && group_id < (int)m_Groups.size()) {
			m_Groups[group_id].master_volume = volume;
			updateSoundVolumes(group_id);
		}
	}
	void stopFadeAll() {
		for (auto it=m_Fades.begin(); it!=m_Fades.end(); ++it) {
			it->finished = true;
		}
	}
	void stopFade(KSOUNDID id) {
		for (auto it=m_Fades.begin(); it!=m_Fades.end(); ++it) {
			if (it->sound_id == id) {
				it->finished = true;
			}
		}
	}
	void stopFadeByGroup(int group_id) {
		for (auto it=m_Fades.begin(); it!=m_Fades.end(); ++it) {
			if (it->group_id == group_id) {
				it->finished = true;
			}
		}
	}
	void set_group_volume(int group_id, float volume) {
		m_Groups[group_id].volume = volume;
		updateSoundVolumes(group_id);
	}
	void setGroupVolume(int group_id, float volume, int time) {
		if (0 <= group_id && group_id < (int)m_Groups.size()) {
			// 進行中のフェードを停止する
			stopFadeByGroup(group_id);

			if (time > 0) {
				SFade fade;
				fade.duration = time;
				fade.time = 0;
				fade.sound_id = nullptr;
				fade.group_id = group_id;
				fade.volume_start = getGroupVolume(group_id);
				fade.volume_end = volume;
				fade.auto_stop = false;
				fade.finished = false;
				m_Fades.push_back(fade);

			} else {
				set_group_volume(group_id, volume);
			}
		}
	}
	KAudioFlags getGroupFlags(int group_id) {
		if (0 <= group_id && group_id < (int)m_Groups.size()) {
			return m_Groups[group_id].flags;
		}
		return 0;
	}
	void setGroupFlags(int group_id, KAudioFlags flags) {
		if (0 <= group_id && group_id < (int)m_Groups.size()) {
			// 排他的フラグが設定されている場合は、他のグループの該当フラグを解除する
			if (flags & KAudioFlag_SOLO) {
				for (auto it=m_Groups.begin(); it!=m_Groups.end(); ++it) {
					it->flags &= ~KAudioFlag_SOLO;
				}
			}
			m_Groups[group_id].flags = flags;
			updateSoundVolumes(group_id);
		}
	}
	float getActualGroupVolume(int group_id) {
		// マスターボリュームがミュート状態の時は無条件でミュート
		if (m_MasterMute) return 0.0f;

		// SOLO属性がある場合、SOLO以外の音は全てミュート
		if (m_SoloGroup >= 0 && group_id != m_SoloGroup) return 0.0f;

		// グループタインでミュートが設定されていればミュート
		if (group_id < 0 || m_Groups.size() <= group_id) return 0.0f;
		const SSndGroup &group = m_Groups[group_id];
		if (group.flags & KAudioFlag_MUTE) return 0.0f;

		// 音量 = [マスターボリューム] * [グループ単位でのマスターボリューム(OptoinなどでBGM, SEごとに設定された音量] * [一時的なグループボリューム（フェードや演出のために一時的に音量を下げるなど）]
		return m_MasterVolume * group.master_volume * group.volume;
	}
	const std::string & getGroupName(int group_id) {
		static const std::string s_empty = "";
		if (0 <= group_id && group_id < (int)m_Groups.size()) {
			return m_Groups[group_id].name;
		}
		return s_empty;
	}
	void setGroupName(int group_id, const std::string &name) {
		if (0 <= group_id && group_id < (int)m_Groups.size()) {
			m_Groups[group_id].name = name;
		}
	}
	int getNumberOfPlaying() const {
		return (int)m_Sounds.size();
	}
	int getNumberOfPlayingInGroup(int group_id) const {
		int num = 0;
		for (auto it=m_Sounds.cbegin(); it!=m_Sounds.cend(); ++it) {
			if (it->second.group_id == group_id) {
				num++;
			}
		}
		return num;
	}
	void stopAll(int fade) {
		for (auto it=m_Sounds.cbegin(); it!=m_Sounds.cend(); ++it) {
			stop(it->first, fade);
		}
	}
	void stopByGroup(int group_id, int fade) {
		for (auto it=m_Sounds.cbegin(); it!=m_Sounds.cend(); ++it) {
			if (it->second.group_id == group_id) {
				stop(it->first, fade);
			}
		}
	}
	KSOUNDID playStreaming(const std::string &name, bool looping, int group_id) {
		if (m_MasterMute) return 0;

		std::string bin = m_Storage->loadBinary(name);
		if (bin.empty()) {
			K__ERROR("Failed to open asset file: %s", name.c_str());
			return 0;
		}

		KSOUNDID snd_id = m_SndImpl.playStreamingSound(bin.data(), bin.size(), 0.0f, looping);
		if (snd_id == 0) {
			K__ERROR("playStreaming: No sound named: %s", name.c_str());
			return 0;
		}
		K__VERBOSE("BGM: %s", name.c_str());
		float group_vol = getActualGroupVolume(group_id);
		SSndItem snd;
		snd.name = name;
		snd.vol = 1.0f;
		snd.group_id = group_id;
		snd.destroy_on_stop = false;
		m_SndImpl.setVolume(snd_id, group_vol);
		m_Sounds[snd_id] = snd;
		return snd_id;
	}
	KSOUNDID playOneShot(const std::string &name, int group_id) {
		if (m_MasterMute) return 0;
		if (! m_SndImpl.isPooled(name)) {
			std::string bin = m_Storage->loadBinary(name);
			if (bin.empty()) {
				K__ERROR("Failed to open file: %s", name.c_str());
				return 0;
			}
			m_SndImpl.poolSound(name, bin.data(), bin.size());
		}
		float group_vol = getActualGroupVolume(group_id);
		KSOUNDID snd_id = m_SndImpl.playPooledSound(name, group_vol);
		if (snd_id == 0) {
			K__ERROR("playOneShot: No sound named: %s", name.c_str());
			return 0;
		}
		K__VERBOSE("SE: %s", name.c_str());
		SSndItem snd;
		snd.name = name;
		snd.vol = 1.0f;
		snd.group_id = group_id;
		snd.destroy_on_stop = true; // 停止したら自動削除
		m_SndImpl.setVolume(snd_id, group_vol);
		m_Sounds[snd_id] = snd;
		return snd_id;
	}
	void postDeleteHandle(KSOUNDID id) {
		m_PostDestroySounds.insert(id);
	}
	bool isValidSound(KSOUNDID id) {
		return m_Sounds.find(id) != m_Sounds.end();
	}
	void clearAllSounds() {
		for (auto it=m_Sounds.begin(); it!=m_Sounds.end(); ++it) {
			KSOUNDID id = it->first;
			m_SndImpl.deleteHandle(id);
		}
		m_Sounds.clear();
		m_PostDestroySounds.clear();
	}
	void seekSeconds(KSOUNDID id, float time) {
		return m_SndImpl.setPositionSeconds(id, time);
	}
	float getPositionInSeconds(KSOUNDID id) {
		return m_SndImpl.getPositionSeconds(id);
	}
	float getLengthInSeconds(KSOUNDID id) {
		return m_SndImpl.getLengthSeconds(id);
	}
	void setLooping(KSOUNDID id, bool value) {
		m_SndImpl.setLooping(id, value);
	}
	void setVolume(KSOUNDID id, float value) {
		auto it = m_Sounds.find(id);
		if (it != m_Sounds.end()) {
			const KSOUNDID s = it->first;
			SSndItem &snd = it->second;
			snd.vol = value;
			float actual_volume = getActualGroupVolume(snd.group_id) * snd.vol;
			m_SndImpl.setVolume(s, actual_volume);
		}
	}
	void setPitch(KSOUNDID id, float value) {
		m_SndImpl.setPitch(id, value);
	}
	void stop(KSOUNDID id, int time) {
		if (time > 0) {
			SFade fade;
			fade.duration = time;
			fade.time = 0;
			fade.sound_id = id;
			fade.group_id = -1;
			fade.volume_start = m_SndImpl.getVolume(id);
			fade.volume_end = 0.0f;
			fade.auto_stop = true;
			fade.finished = false;
			m_Fades.push_back(fade);
		} else {
			m_SndImpl.pause(id);
			postDeleteHandle(id);
		}
	}
	bool isPlaying(KSOUNDID id) {
		return m_SndImpl.isPlaying(id);
	}
	void updateSoundVolumes(int group_id) {
		for (auto it=m_Sounds.begin(); it!=m_Sounds.end(); ++it) {
			const KSOUNDID id = it->first;
			const SSndItem &snd = it->second;
			if (group_id < 0 || snd.group_id == group_id) {
				float vol = getActualGroupVolume(snd.group_id) * snd.vol;
				m_SndImpl.setVolume(id, vol);
			}
		}
	}
	void groupCommand(int group_id, const char *cmd) {
		for (auto it=m_Sounds.begin(); it!=m_Sounds.end(); ++it) {
			const KSOUNDID id = it->first;
			const SSndItem &snd = it->second;
			if (group_id < 0 || snd.group_id == group_id) {
				m_SndImpl.command(cmd, (int*)&id);
			}
		}
	}
	void setMuted(bool value) {
		m_MasterMute = value;
	}
	bool isMuted() const {
		return m_MasterMute;
	}
	float getMasterVolume() {
		return m_MasterVolume;
	}
	void setMasterVolume(float volume) {
		m_MasterVolume = volume;
		updateSoundVolumes(-1);
	}
}; // CAudioPlayer




#pragma region KAudioPlayer
static CAudioPlayer *g_AudioPlayer = nullptr;

void KAudioPlayer::install() {
	g_AudioPlayer = new CAudioPlayer();
}
void KAudioPlayer::install(KStorage *storage) {
	g_AudioPlayer = new CAudioPlayer();
	g_AudioPlayer->m_Storage = storage;
}
void KAudioPlayer::uninstall() {
	if (g_AudioPlayer)  {
		g_AudioPlayer->drop();
		g_AudioPlayer = nullptr;
	}
}
int KAudioPlayer::getGroupCount() {
	K__ASSERT(g_AudioPlayer);
	return g_AudioPlayer->getGroupCount();
}
void KAudioPlayer::setGroupCount(int count) {
	K__ASSERT(g_AudioPlayer);
	g_AudioPlayer->setGroupCount(count);
}
KAudioFlags KAudioPlayer::getGroupFlags(int group_id) {
	K__ASSERT(g_AudioPlayer);
	return g_AudioPlayer->getGroupFlags(group_id);
}
void KAudioPlayer::setGroupFlags(int group_id, KAudioFlags flags) {
	K__ASSERT(g_AudioPlayer);
	g_AudioPlayer->setGroupFlags(group_id, flags);
}
float KAudioPlayer::getGroupMasterVolume(int group_id) {
	K__ASSERT(g_AudioPlayer);
	return g_AudioPlayer->getGroupMasterVolume(group_id);
}
float KAudioPlayer::getGroupVolume(int group_id) {
	K__ASSERT(g_AudioPlayer);
	return g_AudioPlayer->getGroupVolume(group_id);
}
void KAudioPlayer::setGroupMasterVolume(int group_id, float volume) {
	K__ASSERT(g_AudioPlayer);
	g_AudioPlayer->setGroupMasterVolume(group_id, volume);
}
void KAudioPlayer::setGroupVolume(int group_id, float volume, int time) {
	K__ASSERT(g_AudioPlayer);
	g_AudioPlayer->setGroupVolume(group_id, volume, time);
}
float KAudioPlayer::getActualGroupVolume(int group_id) {
	K__ASSERT(g_AudioPlayer);
	return g_AudioPlayer->getActualGroupVolume(group_id);
}
const std::string & KAudioPlayer::getGroupName(int group_id) {
	K__ASSERT(g_AudioPlayer);
	return g_AudioPlayer->getGroupName(group_id);
}
void KAudioPlayer::setGroupName(int group_id, const std::string &name) {
	K__ASSERT(g_AudioPlayer);
	g_AudioPlayer->setGroupName(group_id, name);
}
float KAudioPlayer::getMasterVolume() {
	K__ASSERT(g_AudioPlayer);
	return g_AudioPlayer->getMasterVolume();
}
void KAudioPlayer::setMasterVolume(float volume) {
	K__ASSERT(g_AudioPlayer);
	g_AudioPlayer->setMasterVolume(volume);
}
int KAudioPlayer::getNumberOfPlaying() {
	K__ASSERT(g_AudioPlayer);
	return g_AudioPlayer->getNumberOfPlaying();
}
int KAudioPlayer::getNumberOfPlayingInGroup(int group_id) {
	K__ASSERT(g_AudioPlayer);
	return g_AudioPlayer->getNumberOfPlayingInGroup(group_id);
}
bool KAudioPlayer::isMuted() {
	K__ASSERT(g_AudioPlayer);
	return g_AudioPlayer->isMuted();
}
void KAudioPlayer::setMuted(bool value) {
	K__ASSERT(g_AudioPlayer);
	g_AudioPlayer->setMuted(value);
}
KSOUNDID KAudioPlayer::playStreaming(const std::string &sound, bool looping, int group_id) {
	K__ASSERT(g_AudioPlayer);
	return g_AudioPlayer->playStreaming(sound, looping, group_id);
}
KSOUNDID KAudioPlayer::playOneShot(const std::string &sound, int group_id) {
	K__ASSERT(g_AudioPlayer);
	return g_AudioPlayer->playOneShot(sound, group_id);
}
void KAudioPlayer::stop(KSOUNDID id, int time) {
	K__ASSERT(g_AudioPlayer);
	g_AudioPlayer->stop(id, time);
}
void KAudioPlayer::stopAll(int fade) {
	K__ASSERT(g_AudioPlayer);
	g_AudioPlayer->stopAll(fade);
}
void KAudioPlayer::stopByGroup(int group_id, int fade) {
	K__ASSERT(g_AudioPlayer);
	g_AudioPlayer->stopByGroup(group_id, fade);
}
void KAudioPlayer::stopFade(KSOUNDID id) {
	K__ASSERT(g_AudioPlayer);
	g_AudioPlayer->stopFade(id);
}
void KAudioPlayer::stopFadeAll() {
	K__ASSERT(g_AudioPlayer);
	g_AudioPlayer->stopFadeAll();
}
void KAudioPlayer::stopFadeByGroup(int group_id) {
	K__ASSERT(g_AudioPlayer);
	g_AudioPlayer->stopFadeByGroup(group_id);
}
void KAudioPlayer::seekSeconds(KSOUNDID id, float time) {
	K__ASSERT(g_AudioPlayer);
	g_AudioPlayer->seekSeconds(id, time);
}
void KAudioPlayer::postDeleteHandle(KSOUNDID id) {
	K__ASSERT(g_AudioPlayer);
	g_AudioPlayer->postDeleteHandle(id);
}
void KAudioPlayer::clearAllSounds() {
	K__ASSERT(g_AudioPlayer);
	g_AudioPlayer->clearAllSounds();
}
bool KAudioPlayer::isValidSound(KSOUNDID id) {
	K__ASSERT(g_AudioPlayer);
	return g_AudioPlayer->isValidSound(id);
}
float KAudioPlayer::getPositionInSeconds(KSOUNDID id) {
	K__ASSERT(g_AudioPlayer);
	return g_AudioPlayer->getPositionInSeconds(id);
}
float KAudioPlayer::getLengthInSeconds(KSOUNDID id) {
	K__ASSERT(g_AudioPlayer);
	return g_AudioPlayer->getLengthInSeconds(id);
}
void KAudioPlayer::setLooping(KSOUNDID id, bool value) {
	K__ASSERT(g_AudioPlayer);
	g_AudioPlayer->setLooping(id, value);
}
void KAudioPlayer::setVolume(KSOUNDID id, float value) {
	K__ASSERT(g_AudioPlayer);
	g_AudioPlayer->setVolume(id, value);
}
void KAudioPlayer::setPitch(KSOUNDID id, float value) {
	K__ASSERT(g_AudioPlayer);
	g_AudioPlayer->setPitch(id, value);
}
bool KAudioPlayer::isPlaying(KSOUNDID id) {
	K__ASSERT(g_AudioPlayer);
	return g_AudioPlayer->isPlaying(id);
}
void KAudioPlayer::updateSoundVolumes(int group_id) {
	K__ASSERT(g_AudioPlayer);
	g_AudioPlayer->updateSoundVolumes(group_id);
}
void KAudioPlayer::groupCommand(int group_id, const char *cmd) {
	K__ASSERT(g_AudioPlayer);
	g_AudioPlayer->groupCommand(group_id, cmd);
}
#pragma endregion // KAudioPlayer

} // namespace
