#pragma once
#include <string>

namespace Kamilo {

class KStorage;

struct K__SOUNDID {
	int dummy;
};
typedef K__SOUNDID * KSOUNDID; ///< サウンドID. @see KSoundPlayer


#pragma region KAudioPlayerAct
enum KAudioFlag {
	KAudioFlag_MUTE = 1,
	KAudioFlag_SOLO = 2,
};
typedef int KAudioFlags;

/// 音楽や効果音を管理する
class KAudioPlayer {
public:
	static void install();
	static void install(KStorage &storage);
	static void uninstall();

	/// 現在のサウンドグループ数を返す
	static int getGroupCount();

	/// サウンドグループ数が指定された個数になるように調整する。
	/// 新規追加されたグループの設定はすべてデフォルト値になる
	/// すでに指定された値と同数のグループが存在する場合は何もしない
	static void setGroupCount(int count);

	/// グループ単位でのサウンド設定を適用する
	/// 排他的フラグ（同時に１グループにしか適用できないフラグ）を含んでいる場合は、
	/// 自動的に他のグループの該当フラグが解除される
	static KAudioFlags getGroupFlags(int group_id);

	/// サウンドグループの設定を得る
	static void setGroupFlags(int group_id, KAudioFlags flags);

	/// サウンドグループのメイン音量を得る[0.0 - 1.0]
	static float getGroupMasterVolume(int group_id);

	/// サウンドグループのサブ音量を得る[0.0 - 1.0]
	static float getGroupVolume(int group_id);

	/// サウンドグループのメイン音量を設定する[0.0 - 1.0]
	static void setGroupMasterVolume(int group_id, float volume);

	/// サウンドグループのサブ音量を設定する[0.0 - 1.0]
	/// time フェード時間
	static void setGroupVolume(int group_id, float volume, int time);

	/// サウンドグループの実際の音量を返す。
	/// マスター音量、メイン音量、サブ音量、MUTE や SOLO の指定が反映される
	static float getActualGroupVolume(int group_id);

	/// サウンドグループ名を得る
	static const std::string & getGroupName(int group_id);

	/// サウンドグループ名を設定する。これはログやGUIで表示するためのもの
	static void setGroupName(int group_id, const std::string &name);

	/// マスター音量
	/// この設定は全てのグループと音に影響する
	static float getMasterVolume();
	static void  setMasterVolume(float volume);

	static int getNumberOfPlaying(); /// 再生中のサウンド数を得る
	static int getNumberOfPlayingInGroup(int group_id); /// 指定されたグループで再生中のサウンド数を得る
	static bool isMuted(); /// 全体をミュート中かどうか
	static void setMuted(bool value); /// 全体をミュートする

	/// サウンドファイルをストリーム再生する。
	/// サウンドに割り当てられた識別子を返す
	/// sound サウンドファイル名
	/// looping 連続再生
	/// group_id このサウンドに割り当てるグループ番号
	static KSOUNDID playStreaming(const std::string &sound, bool looping=true, int group_id=0);

	/// サウンドファイルをショット再生する。
	/// サウンドに割り当てられた識別子を返す
	/// sound サウンドファイル名
	/// group_id このサウンドに割り当てるグループ番号
	static KSOUNDID playOneShot(const std::string &sound, int group_id=0);
	
	static void stop(KSOUNDID id, int time=0);         ///< サウンドを停止する
	static void stopAll(int fade=0);                   ///< 再生中のサウンドをすべて止める
	static void stopByGroup(int group_id, int fade=0); ///< 指定されたグループで再生中の全てのサウンドを止める
	static void stopFade(KSOUNDID id);                 ///< 進行中のフェード処理を中止する
	static void stopFadeAll();                         ///< 進行中のフェード処理をすべて中止する
	static void stopFadeByGroup(int group_id);         ///< 指定したグループで進行中のフェード処理をすべて中止する
	static void seekSeconds(KSOUNDID id, float time);
	static void postDeleteHandle(KSOUNDID id);         ///< サウンドを削除する
	static void clearAllSounds();                      ///< すべてのサウンドを削除する
	static bool isValidSound(KSOUNDID id);             ///< サウンド識別子が利用可能かどうか判定する
	static float getPositionInSeconds(KSOUNDID id);    ///< 再生位置を秒単位で返す
	static float getLengthInSeconds(KSOUNDID id);      ///< サウンド長さを秒単位で返す
	static void setLooping(KSOUNDID id, bool value);   ///< サウンドをリピート再生するかどうかを指定する
	static void setVolume(KSOUNDID id, float value);   ///< サウンドごとの音量を指定する（実際の音量はマスター音量との乗算になる→setMasterVolume）
	static void setPitch(KSOUNDID id, float value);    ///< 再生速度を指定する
	static bool isPlaying(KSOUNDID id);                ///< 指定されたサウンドが再生中かどうか調べる
	static void updateSoundVolumes(int group_id=-1);   ///< 音量を更新する
	static void groupCommand(int group_id, const char *cmd);
};
#pragma endregion // KAudioPlayer


} // namespace

