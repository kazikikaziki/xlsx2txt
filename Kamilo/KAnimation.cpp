#include "KAnimation.h"
//
#include "KImGui.h"
#include "KInternal.h"
#include "KNamedValues.h"
#include "KSig.h"
#include "KSpriteDrawable.h"

namespace Kamilo {


class CPlayback {
public:
	static void clipStart(KNode *node, const KClipRes *clip) {
		KSpriteDrawable *co = KSpriteDrawable::of(node);
		if (co) {
			// 必要なレイヤー数を用意する
			// そうしないと、ページごとにレイヤー枚数が異なった場合に面倒なことになるほか、
			// 前回のクリップよりもレイヤー数が少なかった場合に、今回使われないレイヤーがそのまま残ってしまう
			int m = clip->getMaxLayerCount();
			co->setLayerCount(m);
		}
	}
	static void clipAnimate(KNode *node, const KClipRes *clip, float frame) {
		KSpriteDrawable *co = KSpriteDrawable::of(node);
		if (co == nullptr) {
			KLog::printWarning("SPRITE ANIMATION BUT NO SPRITE RENDERER!");
			return;
		}
		int page_index = clip->getPageByFrame(frame, nullptr);

		const KClipRes::SPRITE_KEY *key = clip->getKey(page_index);
		if (key == nullptr) return;

		co->setLayerCount(key->num_layers);
		for (int i=0; i<key->num_layers; i++) {
			co->setLayerSprite(i, key->layers[i].sprite);
			co->setLayerLabel(i, key->layers[i].label);

			KSpriteAuto sprite = co->getLayerSprite(i);
			if (sprite != nullptr && sprite->mDefaultBlend != KBlend_INVALID) {
				co->getLayerMaterialAnimatable(i)->blend = sprite->mDefaultBlend;
			}
			#if 0
			// "@blend=add @blend=screen
			if (!key->layer_commands[i].empty()) {
				if (key->layer_commands[i].compare("blend=screen") == 0) {
					co->getLayerMaterialAnimatable(i)->blend = KBlend_SCREEN;
				} else {
					co->getLayerMaterialAnimatable(i)->blend = KBlend_ALPHA;
				}
			}
			#endif
		}
	}

public:
	enum Flag {
		PLAYBACKFLAG__NONE = 0,
		PLAYBACKFLAG__CAN_PREDEF_SEEK = 1, // クリップに埋め込まれたジャンプ命令に従って、指定されたフレームとは異なるフレームに再設定してもよい
		PLAYBACKFLAG__CLAMP = 2, // シーク時に範囲外のページまたはフレームが指定された場合、アニメ先頭または終端が指定されたものとする
	};
	typedef int Flags;

	CPlayback() {
		m_CB = nullptr;
		m_Clip = nullptr;
		m_Flags = 0;
		m_Frame = 0.0f;
		m_IsPlaying = false;
		m_SleepTime = 0;
		m_Target = nullptr;
		m_PostNextClip = "";
		m_PostNextPage = 0;
	}
	KClipRes *m_Clip;
	bool m_IsPlaying; // 再生中かどうか
	float m_Frame;
	int m_SleepTime; // 指定された時間だけ再生を停止する。-1 で無期限停止
	KClipRes::Flags m_Flags;
	KPlaybackCallback *m_CB;
	KNode *m_Target;
	std::string m_PostNextClip;
	int m_PostNextPage;


	void clearClip() {
		if (m_Clip) {
			m_Clip->drop();
			m_Clip = nullptr;
		}
		m_Flags = 0;
		m_Frame = 0.0f;
		m_IsPlaying = false;
		m_SleepTime = 0;
		m_PostNextClip = "";
		m_PostNextPage = 0;
		// m_CB は初期化しない
	}

	bool getFlag(KClipRes::Flag f) const {
		return (m_Flags & f) != 0;
	}

	void setFlag(KClipRes::Flag f, bool value) {
		if (value) {
			m_Flags |= f;
		} else {
			m_Flags &= ~f;
		}
	}

	void callRepeat() {
		if (m_CB) {
			KPlaybackSignalArgs args;
			args.node = m_Target;
			m_CB->on_playback_repeat(&args);
		}
	}

	void callTick(int page) {
		if (m_CB) {
			KPlaybackSignalArgs args;
			args.node = m_Target;
			args.page = page;
			m_CB->on_playback_tick(&args);
		}
	}

	void callEnterClip(int page) {
		if (m_CB) {
			KPlaybackSignalArgs args;
			args.node = m_Target;
			args.clip = m_Clip;
			args.page = page;
			m_CB->on_playback_enter_page(&args);
		}
	}
	void callEnterPage(int new_page) {
		if (m_CB) {
			KPlaybackSignalArgs args;
			args.node = m_Target;
			args.clip = m_Clip;
			args.page = new_page;
			m_CB->on_playback_enter_page(&args);
		}
	}
	void callExitPage(int old_page) {
		if (m_CB) {
			if (m_Clip) {
				KPlaybackSignalArgs args;
				args.node = m_Target;
				args.clip = m_Clip;
				args.page = old_page;
				m_CB->on_playback_exit_page(&args);
			}
		}
	}
	void callExitClip() {
		if (m_CB) {
			KPlaybackSignalArgs args;
			args.node = m_Target;
			m_CB->on_playback_exit_clip(&args);
		}
	}

	// playback の内容を初期化する
	// keep_clip: clip を drop & 削除せずに残しておく（アニメが終わったことを通知するだけで、クリップを外さない）
	void playbackClear(bool keep_clip) {
		if (m_Clip) {

			// スプライトページの終了を通知
			callExitPage(m_Clip->getPageByFrame(m_Frame, nullptr));

			// アニメクリップの解除を通知
			callExitClip();

			// アニメクリップを解除
			if (!keep_clip) {
				m_Clip->drop();
				m_Clip = nullptr;
			}
			m_IsPlaying = false;
		}
	}
	// 現在の再生位置にあるコマンドをシグナル送信する
	// e: playback のコールバックに渡すパラメータ
	void playbackSignal() const {
		if (m_Clip && m_Target) {
			const KClipRes::SPRITE_KEY *key = m_Clip->getKeyByFrame(m_Frame);
			if (key) {
				// コマンドを通知する
				const KNamedValues &nv = key->user_parameters;
				for (int i=0; i<nv.size(); i++) {
					KSig sig(K_SIG_ANIMATION_COMMAND);
					sig.setNode("target", m_Target);
					sig.setString("cmd", nv.getName(i));
					sig.setString("val", nv.getString(i));
					sig.setString("clipname", m_Clip ? m_Clip->getName() : "");
					sig.setPointer("clipptr", m_Clip);
					m_Target->getRoot()->broadcastSignalToChildren(sig);
				}
			}
		}
	}

	// e: playback のコールバックに渡すパラメータ
	// flags: シークオプション
	bool playbackSeek(float frame, Flags flags) {
		// スプライトアニメ
		if (playbackSeekSprite(frame, flags)) {
			return true;
		}

		// アニメを完全に終了する

		// 終了処理よりも前にフラグを取得しておく
		const bool kill = getFlag(KClipRes::FLAG_KILL_SELF);

		// 終了処理
		playbackClear(true); // アニメは終了するが、クリップは設定したままにしておく

		// エンティティの自動削除
		if (kill) {
			m_Target->markAsRemove();
		}
		return false;
	}

	bool playbackSeekSprite(float new_frame, Flags flags) {
		K__ASSERT(m_Clip);

		// このクリップがキーを全く含んでいない場合は必ず失敗する
		if (m_Clip->getKeyCount() == 0) {
			return false;
		}

		// アニメ全体の長さ
		int total_length = m_Clip->getLength();

		// シーク前のページ
		int old_page = m_Clip->getPageByFrame(m_Frame, nullptr);

		// フレーム調整
		if (flags & PLAYBACKFLAG__CLAMP) {
			if (new_frame < 0) {
				new_frame = 0;
			}
			if (new_frame >= total_length) {
				new_frame = (float)(total_length - 1);
			}
		}

		// シーク後のページ
		int new_page = -1;
		if (new_frame < total_length) {
			new_page = m_Clip->getPageByFrame(new_frame, nullptr);
		}

		// ページ切り替えの有無
		if (old_page == new_page) {
			// シーク前とシーク後でページが変化していない
			m_IsPlaying = true;
			m_Frame = new_frame;

			// アニメ対象の状態を更新
			if (m_Clip) {
				clipAnimate(m_Target, m_Clip, m_Frame);
			}
			callTick(new_page);
			return true;
		}

		// アニメ終端に達していて、ループが指定されているなら先頭へシークする
		if (new_page < 0 && getFlag(KClipRes::FLAG_LOOP)) {
			new_page = 0;
		}

		// アニメクリップで定義されたページジャンプに従って、シーク先ページを再設定する
		if (flags & PLAYBACKFLAG__CAN_PREDEF_SEEK) {
			const KClipRes::SPRITE_KEY *key = m_Clip->getKey(old_page);
			std::string jump_clip;
			int jump_page = -1;
			if (m_Clip->getNextPage(old_page, key->next_mark, &jump_clip, &jump_page)) {

				if (jump_clip.empty()) {
					// 同じクリップ内でジャンプする
					new_page = jump_page; // アニメクリップにより行き先が変更された
				} else {
					// 異なるクリップにジャンプする。現在のクリップをいったん終了させる
					new_page = -1;
					m_PostNextClip = jump_clip;
					m_PostNextPage = jump_page;
				}
			}
		}

		// 新しいページに入る。
		// なお、結果として old_page == new_page になった場合（同ページ内ループ）でも
		// 必ずページ切り替えイベントを生させる
		if (new_page >= 0) {
			m_IsPlaying = true;

			// 古いページから離れることを通知
			callExitPage(old_page);

			// 新しいフレーム位置を設定
			m_Frame = (float)m_Clip->getKeyTime(new_page);

			// ページ切り替え時のシグナル送信
			if (old_page != new_page) {
				playbackSignal();
			}

			// アニメ対象の状態を更新
			if (m_Clip) {
				clipAnimate(m_Target, m_Clip, m_Frame);
			}

			// 新しいページに入ったことを通知
			callEnterPage(new_page);
			return true;

		}

		// 新しいページなし。そのままアニメ終了する。

		// ここでは on_playback_exit_page を呼ばない。
		// アニメが終了した場合は、クリップを変更するまで最終ページがそのまま有効になり続ける。
		// 最後のページに対する on_playback_exit_page が呼ばれるのは、クリップが削除された時である
		// playbackClear を参照すること

		// 完全に終了
		return false;
	}

	bool playback_seek_non_sprite(float new_frame, Flags flags) {
		const int total_length = m_Clip->getLength(); // アニメ全体の長さ

		if (new_frame<total_length) {
			// まだ終端に達していない or 無限長さ
			m_IsPlaying = true;
			m_Frame = new_frame;
			// アニメ対象の状態を更新
			if (m_Clip) {
				clipAnimate(m_Target, m_Clip, m_Frame);
			}
			return true;
		}
		// 再生位置がアニメ終端に達した。
		// ループまたは終了処理する
		if (getFlag(KClipRes::FLAG_LOOP)) {
			// 先頭に戻る
			m_IsPlaying = true;
			m_Frame = 0;
			// リピート通知
			callRepeat();
			// アニメ対象の状態を更新
			if (m_Clip) {
				clipAnimate(m_Target, m_Clip, m_Frame);
			}
			return true;
		}
		// 完全に終了
		return false;
	}
};
// CPlayback



class CAnimationMgr: public KManager {
public:
	KCompNodes<KAnimation> m_Nodes;

	CAnimationMgr() {
		KEngine::addManager(this);
	}
	virtual bool on_manager_isattached(KNode *node) override {
		return m_Nodes.contains(node);
	}
	virtual void on_manager_detach(KNode *node) override {
		m_Nodes.detach(node);
	}
	virtual void on_manager_frame() override {
		for (auto it=m_Nodes.begin(); it!=m_Nodes.end(); ++it) {
			KNode *node = it->first;
			if (node->getEnableInTree() && !node->getPauseInTree()) {
				KAnimation *ani = it->second;
				ani->tickTracks();
			}
		}
	}
	virtual void on_manager_nodeinspector(KNode *node) override {
		m_Nodes.get(node)->updateInspector();
	}
};

static CAnimationMgr *g_AnimationMgr = nullptr;


#pragma region KAnimation
void KAnimation::install() {
	K__ASSERT_RETURN(g_AnimationMgr == nullptr);
	g_AnimationMgr = new CAnimationMgr();
}
void KAnimation::uninstall() {
	if (g_AnimationMgr) {
		g_AnimationMgr->drop();
		g_AnimationMgr = nullptr;
	}
}
void KAnimation::attach(KNode *node) {
	K__ASSERT(g_AnimationMgr);
	if (node && !isAttached(node)) {
		KAnimation *co = new KAnimation();
		g_AnimationMgr->m_Nodes.attach(node, co);
		co->drop();
	}
}
bool KAnimation::isAttached(KNode *node) {
	return of(node) != nullptr;
}
KAnimation * KAnimation::of(KNode *node) {
	K__ASSERT(g_AnimationMgr);
	return g_AnimationMgr->m_Nodes.get(node);
}



KAnimation::KAnimation() {
	K__ASSERT_RETURN(KBank::getAnimationBank());
	m_Node = nullptr;
	m_ClipSpeed = 1.0f;
	m_AutoResetClipSpeed = false;
	m_MainPlayback = new CPlayback();
}
KAnimation::~KAnimation() {
	clearMainClip();
	delete m_MainPlayback;
}
void KAnimation::_setNode(KNode *node) {
	m_Node = node;
	m_MainPlayback->m_Target = node;
}
KNode * KAnimation::getNode() {
	return m_Node;
}
void KAnimation::clearMainClip() {
	m_MainPlayback->playbackClear(false);
}
const KClipRes * KAnimation::getMainClip() const {
	std::string name = getMainClipName();
	return KBank::getAnimationBank()->getClipResource(name);
}
std::string KAnimation::getMainClipName() const {
	if (m_MainPlayback->m_Clip == nullptr) return "";
	return m_MainPlayback->m_Clip->getName();
}
/// アニメを指定フレームの間だけ停止する。
/// @param duration 停止フレーム数。負の値を指定すると無期限に停止する
void KAnimation::setMainClipSleep(int duration) {
	m_MainPlayback->m_SleepTime = duration;
}
bool KAnimation::isMainClipSleep() const {
	return m_MainPlayback->m_SleepTime != 0;
}
void KAnimation::seekMainClipBegin() {
	seekMainclip(0, -1, 0);
}
void KAnimation::seekMainClipEnd() {
	if (m_MainPlayback->m_Clip) {
		int len = m_MainPlayback->m_Clip->getLength();
		if (m_MainPlayback->playbackSeek((float)len, CPlayback::PLAYBACKFLAG__CLAMP)) {
		//	this->node_update_commands();
		}
	}
}
bool KAnimation::seekMainClipFrame(int frame) {
	return seekMainclip((float)frame, -1, 0);
}
bool KAnimation::seekMainClipKey(int key) {
	return seekMainclip(-1, key, 0);
}
/// マーカーを指定して移動
/// ※ mark として有効なのは 1 以上の値のみ
bool KAnimation::seekMainClipToMark(int mark) {
	return seekMainclip(-1, -1, mark);
}
/// 指定したマーカーがついているページを返す
/// マーカーが存在しなければ -1 を返す
int KAnimation::findPageByMark(int mark) const {
	if (m_MainPlayback->m_Clip == nullptr) {
		return -1;
	}
	if (m_MainPlayback->m_Clip == nullptr) {
		return -1;
	}
	return m_MainPlayback->m_Clip->findPageByMark(mark);
}
/// アニメクリップを再生中の場合、そのページ番号を返す。
/// 失敗した場合は -1 を返す
/// out_pageframe ページ先頭からの経過フレーム数を返す
int KAnimation::getMainClipPage(int *out_pageframe) const {
	const KClipRes *clip = m_MainPlayback->m_Clip;
	if (clip == nullptr) return -1;

	float frame = m_MainPlayback->m_Frame;
	if (frame < 0) return -1;

	int page = clip->getPageByFrame(frame, out_pageframe);
	return page;
}
bool KAnimation::isMainClipPlaying(const std::string &name_or_alias, std::string *p_post_next_clip, int *p_post_next_page) const {
	if (m_MainPlayback->m_IsPlaying) {
		// メインアニメ再生中
		if (name_or_alias.empty()) {
			return true;
		}

		const std::string &playing_clip_name = m_MainPlayback->m_Clip->getName();

		// 名前が指定されている場合は、再生中のクリップ名も確認する
		K__ASSERT(m_MainPlayback->m_Clip);
		auto it = m_AliasMap.find(name_or_alias);
		if (it != m_AliasMap.end()) {
			// name_or_alias はエイリアス名で指定されている
			const std::string &clipname = it->second;
			if (playing_clip_name.compare(clipname) == 0) {
				return true;
			}

		} else {
			// name_or_alias はクリップ名で指定されている
			if (playing_clip_name.compare(name_or_alias) == 0) {
				return true;
			}
		}
	} else {
		if (p_post_next_clip) *p_post_next_clip = m_MainPlayback->m_PostNextClip;
		if (p_post_next_page) *p_post_next_page = m_MainPlayback->m_PostNextPage;
	}
	return false;
}
/// スプライトアニメをアタッチする
/// @param name アタッチするアニメクリップの名前
/// @param keep 既に同じアニメが設定されている場合の挙動を決める。
///             true なら既存のアニメがそのまま進行する。false ならリスタートする
bool KAnimation::setMainClipName(const std::string &name, bool keep) {
	KClipRes *clip = KBank::getAnimationBank()->find_clip(name);
	return setMainClip(clip, keep);
}
bool KAnimation::setMainClipAlias(const std::string &alias, bool keep) {
	// 空文字列が指定された場合はクリップを外す
	if (alias.empty()) {
		return setMainClip(nullptr);
	}
	// エイリアスから元のクリップ名を得る
	const std::string &name = m_AliasMap[alias];
	if (name.empty()) {
		KLog::printWarning("Animation alias named '%s' does not exist", alias.c_str());
		return setMainClip(nullptr);
	}
	// 新しいクリップを設定する
	KClipRes *clip = KBank::getAnimationBank()->find_clip(name);
	if (clip == nullptr) {
		KLog::printWarning("No animation named '%s' (alias '%s')", name.c_str(), alias.c_str());
	}
	return setMainClip(clip, keep);
}
bool KAnimation::setMainClip(KClipRes *clip, bool keep) {
	// 同じアニメが既に再生中の場合は何もしない
	if (keep && clip) {
		if (m_MainPlayback->m_Clip == clip) {
			return false;
		}
	}

	// 再生中のクリップを終了して後始末する
	{
		m_MainPlayback->playbackClear(false);

		// 再生中のクリップにだけアニメ速度変更が適用されているなら
		// 元の速度に戻しておく
		if (m_AutoResetClipSpeed) {
			m_ClipSpeed = 1.0f;
			m_AutoResetClipSpeed = false;
		}
		m_MainPlayback->clearClip();
	}

	if (clip) {
		// アニメ適用
		m_AutoResetClipSpeed = false;
		m_MainPlayback->clearClip();
		m_MainPlayback->m_Clip = clip;
		m_MainPlayback->m_Clip->grab();
		m_MainPlayback->m_IsPlaying = true;

		// アニメで設定されている再生フラグをコピー
		m_MainPlayback->setFlag(KClipRes::FLAG_LOOP, clip->getFlag(KClipRes::FLAG_LOOP));
		m_MainPlayback->setFlag(KClipRes::FLAG_KILL_SELF, clip->getFlag(KClipRes::FLAG_KILL_SELF));

		// コマンド更新
	//	this->node_update_commands();

		if (m_MainPlayback->m_CB) {
			KPlaybackSignalArgs args;
			args.node = getNode();
			m_MainPlayback->m_CB->on_playback_enter_clip(&args);
		}

		if (m_MainPlayback->m_Clip) {
			// 開始イベントを呼ぶ
			CPlayback::clipStart(getNode(), m_MainPlayback->m_Clip);

			// アニメ更新関数を通しておく
			CPlayback::clipAnimate(getNode(), m_MainPlayback->m_Clip, m_MainPlayback->m_Frame);
		}
		// 新しいクリップが設定されたことを通知
		if (1) {
			KSig sig(K_SIG_ANIMATION_NEWCLIP);
			sig.setNode("target", getNode());
			sig.setString("clipname", clip->getName());
			KNodeTree::broadcastSignal(sig);
		}

		// ページ開始
		if (m_MainPlayback->m_Clip) {
			int page = m_MainPlayback->m_Clip->getPageByFrame(m_MainPlayback->m_Frame, nullptr);
			m_MainPlayback->callEnterClip(page);
		}

		// コマンド送信
		m_MainPlayback->playbackSignal();
	}
	return true;
}
void KAnimation::setMainClipCallback(KPlaybackCallback *cb) {
	m_MainPlayback->m_CB = cb;
}
void KAnimation::tickTracks() {
	if (m_MainPlayback->m_Clip) {
		if (m_MainPlayback->m_SleepTime > 0) {
			m_MainPlayback->m_SleepTime--;

		} else if (m_MainPlayback->m_SleepTime == 0) {

			float newframe = m_MainPlayback->m_Frame + m_ClipSpeed;
			if (m_MainPlayback->playbackSeek(newframe, CPlayback::PLAYBACKFLAG__CAN_PREDEF_SEEK)) {
			//	this->node_update_commands();
			}
		}
	}
}
void KAnimation::setAlias(const std::string &alias, const std::string &name) {
	m_AliasMap[alias] = name;
}
std::string KAnimation::getClipNameByAlias(const std::string &alias) const {
	auto it = m_AliasMap.find(alias);
	if (it != m_AliasMap.end()) {
		return it->second;
	}
	return "";
}
/// アニメ速度の倍率を設定する。
/// speed; アニメ速度倍率。標準値は 1.0
/// current_clip_only: 現在再生中のクリップにだけ適用する。
///                    true にすると、異なるクリップが設定された時に自動的に速度設定が 1.0 に戻る。
///                    false を指定した場合、再び setSpeed を呼ぶまで速度倍率が変更されたままになる
void KAnimation::setSpeedScale(float speed, bool current_clip_only) {
	m_ClipSpeed = speed;
	m_AutoResetClipSpeed = current_clip_only;
}
float KAnimation::getSpeedScale() const {
	return m_ClipSpeed;
}
bool KAnimation::getCurrentParameterBool(const std::string &name) const {
	auto nv = getCurrentUserParameters();
	return nv ? nv->getBool(name) : false;
}
int KAnimation::getCurrentParameterInt(const std::string &name, int def) const {
	auto nv = getCurrentUserParameters();
	return nv ? nv->getInteger(name, def) : def;
}
float KAnimation::getCurrentParameterFloat(const std::string &name, float def) const {
	auto nv = getCurrentUserParameters();
	return nv ? nv->getFloat(name, def) : def;
}
const std::string & KAnimation::getCurrentParameter(const std::string &name) const {
	static const std::string s_Empty;
	auto nv = getCurrentUserParameters();
	return nv ? nv->getString(name, s_Empty) : s_Empty;
}
bool KAnimation::queryCurrentParameterInt(const std::string &name, int *value) const {
	auto nv = getCurrentUserParameters();
	return nv ? nv->queryInteger(name, value) : false;
}
void KAnimation::setCurrentParameter(const std::string &key, const std::string &value) {
	auto nv = getCurrentUserParametersEdit();
	if (nv) nv->setString(key, value);
}
const KXmlElement * KAnimation::getCurrentDataXml() const {
	const KClipRes *clip = m_MainPlayback->m_Clip;
	if (clip) {
		const KClipRes::SPRITE_KEY *key = clip->getKeyByFrame(m_MainPlayback->m_Frame);
		if (key) {
			return key->xml_data;
		}
	}
	return nullptr;
}
KXmlElement * KAnimation::getCurrentDataXmlEdit() {
	KClipRes *clip = m_MainPlayback->m_Clip;
	if (clip) {
		KClipRes::SPRITE_KEY *key = clip->getKeyByFrame(m_MainPlayback->m_Frame);
		if (key) {
			return key->xml_data;
		}
	}
	return nullptr;
}
const KNamedValues * KAnimation::getCurrentUserParameters() const {
	const KClipRes *clip = m_MainPlayback->m_Clip;
	if (clip) {
		const KClipRes::SPRITE_KEY *key = clip->getKeyByFrame(m_MainPlayback->m_Frame);
		if (key) {
			return &key->user_parameters;
		}
	}
	return nullptr;
}
KNamedValues * KAnimation::getCurrentUserParametersEdit() {
	KClipRes *clip = m_MainPlayback->m_Clip;
	if (clip) {
		KClipRes::SPRITE_KEY *key = clip->getKeyByFrame(m_MainPlayback->m_Frame);
		if (key) {
			return &key->user_parameters;
		}
	}
	return nullptr;
}
bool KAnimation::updateClipGui() {
#ifndef NO_IMGUI
	const KClipRes *clip = m_MainPlayback->m_Clip;
	if (clip == nullptr) {
		ImGui::Text("(NO CLIP)");
		return false;
	}
	bool changed = false;
	if (m_MainPlayback->m_SleepTime == 0) {
		ImGui::Text("Frame %.1f/%d", m_MainPlayback->m_Frame, clip->getLength());
	} else {
		KImGui::PushTextColor(KImGui::COLOR_WARNING);
		ImGui::Text("Frame %.1f/%d (Sleep)", m_MainPlayback->m_Frame, clip->getLength());
		KImGui::PopTextColor();
	}
	{
		const auto FLAG = KClipRes::FLAG_LOOP;
		bool a = m_MainPlayback->getFlag(FLAG);
		if (ImGui::Checkbox("Loop", &a)) {
			m_MainPlayback->setFlag(FLAG, a);
			changed = true;
		}
	}
	{
		const auto FLAG = KClipRes::FLAG_KILL_SELF;
		bool a = m_MainPlayback->getFlag(FLAG);
		if (ImGui::Checkbox("Remove entity at end", &a)) {
			m_MainPlayback->setFlag(FLAG, a);
			changed = true;
		}
	}
	{
		float spd = m_ClipSpeed;
		if (ImGui::DragFloat("Speed scale", &spd, 0.01f, 0.0f, 10.0f)) {
			m_ClipSpeed = spd;
		}
	}
	if (m_MainPlayback->m_Clip) {
		if (ImGui::TreeNodeEx("%s", ImGuiTreeNodeFlags_DefaultOpen, typeid(*m_MainPlayback->m_Clip).name())) {
			m_MainPlayback->m_Clip->on_track_gui_state(m_MainPlayback->m_Frame);
			ImGui::TreePop();
		}
	}
	return changed;
#else // !NO_IMGUI
	return false;
#endif // NO_IMGUI
}
void KAnimation::updateInspector() {
	if (m_MainPlayback->m_SleepTime == 0) {
		KImGui::PushTextColor(KImGui::COLOR_DEFAULT());
		ImGui::Text("Sleep: Off");
	} else if (m_MainPlayback->m_SleepTime > 0) {
		KImGui::PushTextColor(KImGui::COLOR_WARNING);
		ImGui::Text("Sleep: On (%d)", m_MainPlayback->m_SleepTime);
	} else {
		KImGui::PushTextColor(KImGui::COLOR_WARNING);
		ImGui::Text("Sleep: On (INF)");
	}
	KImGui::PopTextColor();

	{
		std::string name = m_MainPlayback->m_Clip ? m_MainPlayback->m_Clip->getName() : "";
		if (ImGui::TreeNode("Clip: %s", name.c_str())) {
			updateClipGui();
			ImGui::TreePop();
		}
	}
	if (ImGui::TreeNode("Parameters")) {
		auto nv = getCurrentUserParameters();
		if (nv) {
			for (int i=0; i<nv->size(); i++) {
				const std::string &k = nv->getName(i);
				const std::string &v = nv->getString(i);
				ImGui::Text("%s: %s", k.c_str(), v.c_str());
			}
		}
		ImGui::TreePop();
	}
	ImGui::Separator();
	if (ImGui::TreeNode("Alias Map")) {
		for (auto it=m_AliasMap.cbegin(); it!=m_AliasMap.cend(); ++it) {
			ImGui::Text("%s: %s", it->first.c_str(), it->second.c_str());
		}
		ImGui::TreePop();
	}
}
bool KAnimation::seekMainclip(float frame, int key, int mark) {
	if (m_MainPlayback->m_Clip == nullptr) {
		return false;
	}

	// マークが指定されているならキーに変換する
	if (mark > 0) { // mark == 0 は「マーク無し」を意味するので、探さない
		key = m_MainPlayback->m_Clip->findPageByMark(mark);
		if (key < 0) return false; // マークが見つからない
	}

	// キーが指定されているならフレームに変換する
	if (key >= 0) {
		frame = (float)m_MainPlayback->m_Clip->getKeyTime(key);
	}

	if (frame >= 0) {
		// わざわざフレームを指定してシークしているので、
		// KClipPageCmd_NEXTA フラグなどで勝手に移動しないようにする
		if (m_MainPlayback->playbackSeek((float)frame, CPlayback::PLAYBACKFLAG__NONE)) {
		//	this->node_update_commands();
		}
	}
	return true;
}
#pragma endregion KAnimation


} // namespace
