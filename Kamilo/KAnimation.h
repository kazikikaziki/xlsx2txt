/// @file
/// Copyright (c) 2020 Heliodor
/// This software is released under the MIT License.
/// http://opensource.org/licenses/mit-license.php

#pragma once
#include <unordered_map>
#include "KRef.h"

namespace Kamilo {

class KNode;
class KClipRes;
class KNamedValues;
class KXmlElement;

// アニメ。アニメはアクションとは異なり、基本的には同じ結果をもたらし、再生するごとに効果が変わったりはしない。
// またアクションとは異なり、自分自身を改変しない。常に const で再生可能で、一つのアニメを同時に複数のターゲットに適用しても安全である。

struct KPlaybackSignalArgs {
	KNode *node;
	const KClipRes *clip;
	int page;

	KPlaybackSignalArgs() {
		node = nullptr;
		clip = nullptr;
		page = -1;
	}
};

class KPlaybackCallback {
public:
	virtual void on_playback_enter_clip(KPlaybackSignalArgs *args) {}
	virtual void on_playback_exit_clip(KPlaybackSignalArgs *args) {}
	virtual void on_playback_enter_page(KPlaybackSignalArgs *args) {}
	virtual void on_playback_exit_page(KPlaybackSignalArgs *args) {}
	virtual void on_playback_repeat(KPlaybackSignalArgs *args) {}
	virtual void on_playback_tick(KPlaybackSignalArgs *args) {}
};


class CPlayback; // internal


class KAnimation: public KRef {
public:
	static void install();
	static void uninstall();
	static void attach(KNode *node);
	static bool isAttached(KNode *node);
	static KAnimation * of(KNode *node);
	static KAnimation * getAnimationOf(KNode *node) { return of(node); }

public:
	KAnimation();
	virtual ~KAnimation();
	void _setNode(KNode *node);
	KNode * getNode();
	void clearMainClip();
	const KClipRes * getMainClip() const; ///< アニメクリップを返す
	std::string getMainClipName() const; ///< アニメクリップ名を返す
	void setMainClipSleep(int duration); ///< アニメを指定フレームの間だけ停止する
	bool isMainClipSleep() const; ///< 一時停止中かどうか
	void seekMainClipBegin(); ///< 先頭に移動
	void seekMainClipEnd(); ///< 終端に移動
	bool seekMainClipFrame(int frame); /// <フレーム番号を指定して移動
	bool seekMainClipKey(int key); ///< キー番号を指定して移動
	bool seekMainClipToMark(int mark); ///< マーカーを指定して移動
	int findPageByMark(int mark) const; ///< 指定したマーカーがついているページを返す
	int getMainClipPage(int *out_pageframe=nullptr) const; ///< アニメクリップを再生中の場合、そのページ番号を返す
	bool isMainClipPlaying(const std::string &name_or_alias, std::string *p_post_next_clip=nullptr, int *p_post_next_page=nullptr) const; ///< アニメクリップを再生中かどうか
	bool setMainClipName(const std::string &name, bool keep=false); ///< クリップ名を指定してアニメをセットする
	bool setMainClipAlias(const std::string &alias, bool keep=false); ///< クリップのエイリアスを指定してアニメをセットする
	bool setMainClip(KClipRes *clip, bool keep=false); ///< クリップオブジェクトを指定してアニメをセットする
	void setMainClipCallback(KPlaybackCallback *cb); ///< アニメの再生状態に応じてコールバックが呼ばれるようにする
	void tickTracks();
	void setAlias(const std::string &alias, const std::string &name);
	std::string getClipNameByAlias(const std::string &alias) const;
	void setSpeedScale(float speed, bool current_clip_only=false); ///< アニメ速度の倍率を設定する
	float getSpeedScale() const;
	bool getCurrentParameterBool(const std::string &name) const; ///< アニメクリップの Parameters タグで指定された値を得る
	int getCurrentParameterInt(const std::string &name, int def=0) const;
	float getCurrentParameterFloat(const std::string &name, float def=0.0f) const;
	const std::string & getCurrentParameter(const std::string &name) const;
	bool queryCurrentParameterInt(const std::string &name, int *value) const;
	void setCurrentParameter(const std::string &key, const std::string &value);
	const KNamedValues * getCurrentUserParameters() const;
	KNamedValues * getCurrentUserParametersEdit();
	const KXmlElement * getCurrentDataXml() const;
	KXmlElement * getCurrentDataXmlEdit();
	bool updateClipGui();
	void updateInspector();

private:
	bool seekMainclip(float frame, int key, int mark); ///< フレーム、キー番号、マーカーのどれかを指定して移動

	KNode *m_Node;
	CPlayback *m_MainPlayback; // クリップアニメの再生情報
	float m_ClipSpeed;         // クリップアニメの再生速度 (1.0 で等倍）
	bool m_AutoResetClipSpeed; // 現在のクリップアニメが外されたらアニメ速度を戻す
	std::unordered_map<std::string, std::string> m_AliasMap; ///< [Alias, Clip]
};


} // namespace
