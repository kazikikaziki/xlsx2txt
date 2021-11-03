#include "KInputMap.h"
//
#include "KSig.h"
#include "KInternal.h"
#include "KJoystick.h"
#include "KKeyboard.h"
#include "KThread.h"
#include "KImGui.h"
#include "keng_game.h"

namespace Kamilo {

static int _sign(float f) {
	if (f < 0.0f) return -1;
	if (f > 0.0f) return  1;
	return 0;
}


class CActionButtonKeyElm;



#pragma region Buttons
static const int MAX_SEQUENCE_SIZE = 12;
static const int MAX_HISTORY_SIZE = 16;
static const int TIMEOUT_MSEC = 500; // シーケンスキーのキー要素同士の最大受け入れ時間

struct SKeyHistoryItem {
	std::vector<const IKeyElm *> m_PressedButtons;
	int m_TimestampMsec;

	SKeyHistoryItem() {
		m_TimestampMsec = 0;
	}

	bool contains(const IKeyElm *elm) const {
		for (size_t i=0; i<m_PressedButtons.size(); i++) {
			if (m_PressedButtons[i] == elm) {
				return true;
			}
		}
		return false;
	}
};

class IKeyHistory {
public:
	virtual const SKeyHistoryItem * get_history_item(int index) const = 0;
	virtual int get_history_size() const = 0;
};


// keyboard key
class CKbKeyElm: public IKeyboardKeyElm {
	// このボタンに割り当てられたキーボードキー。
	// 割り当てなしの場合は KKey_NONE
	KKey m_Key;
	KKeyModifiers m_Modifiers;

public:
	CKbKeyElm() {
		m_Key = KKey_NONE;
		m_Modifiers = KKeyModifier_NONE;
	}
	CKbKeyElm(KKey key, KKeyModifiers mod) {
		K__ASSERT(key != KKey_NONE);
		m_Key = key;
		m_Modifiers = mod;
	}

	virtual KKey get_key() const override {
		return m_Key;
	}
	virtual void set_key(KKey key) override {
		m_Key = key;
	}
	virtual KKeyModifiers get_modifiers() const override {
		return m_Modifiers;
	}
	virtual const char * get_key_name() const override {
		return KKeyboard::getKeyName(m_Key);
	}

	// IKeyElm
	virtual bool isConflictWith(const IKeyElm *k) const override {
		const CKbKeyElm *obj = dynamic_cast<const CKbKeyElm *>(k);
		return obj && obj->m_Key == m_Key;
	}
	virtual bool isPressed(float *val, KPollFlags flags) const override {
		if (flags & POLLFLAG_NO_KEYBOARD) return false;
		if (m_Key == KKey_NONE) return false;
		if (KKeyboard::isKeyDown(m_Key)) {
			if (KKeyboard::matchModifiers(m_Modifiers)) {
				if (val) *val = 1;
				return true;
			}
		}
		return false;
	}
};

// joystick key
class CJoyKeyElm: public IJoystickKeyElm {
	KCoreJoystick *m_Joy;

	// このボタンに割り当てられたジョイスティックボタン。
	// 割り当てなしの場合は KJoyKey_NONE
	KJoystickButton m_Btn;

public:
	CJoyKeyElm(KCoreJoystick *joy) {
		m_Joy = joy;
		m_Joy->grab();
		m_Btn = KJoystickButton_NONE;
	}
	CJoyKeyElm(KCoreJoystick *joy, KJoystickButton btn) {
		m_Joy = joy;
		m_Joy->grab();
		m_Btn = btn;
	}
	virtual ~CJoyKeyElm() {
		m_Joy->drop();
	}
	virtual KJoystickButton get_button() const override {
		return m_Btn;
	}
	virtual void set_button(KJoystickButton btn) override {
		m_Btn = btn;
	}

	// IKeyElm
	virtual bool isConflictWith(const IKeyElm *k) const override {
		const CJoyKeyElm *obj= dynamic_cast<const CJoyKeyElm *>(k);
		return obj && obj->m_Btn == m_Btn;
	}
	virtual bool isPressed(float *val, KPollFlags flags) const override {
		if (flags & POLLFLAG_NO_JOYSTICK) return false;
		if (m_Btn < 0) return false;
		if (m_Joy && m_Joy->isConnected()) {
			if (m_Joy->getButton(m_Btn)) {
				if (val) *val = 1;
				return true;
			}
		}
		return false;
	}
};

// mouse key
class CMouseKeyElm: public IKeyElm {
	// このボタンに割り当てられたマウスボタン。
	// 割り当てなしの場合は KMouseKey_NONE
	KCoreMouse *m_Mouse;
	KMouseButton m_Btn;

public:
	CMouseKeyElm(KCoreMouse *mouse) {
		m_Mouse = mouse;
		m_Mouse->grab();
		m_Btn = KMouseButton_NONE;
	}
	CMouseKeyElm(KCoreMouse *mouse, KMouseButton btn) {
		m_Mouse = mouse;
		m_Mouse->grab();
		m_Btn = btn;
	}
	virtual ~CMouseKeyElm() {
		m_Mouse->drop();
	}

	// IKeyElm
	virtual bool isConflictWith(const IKeyElm *k) const override {
		const CMouseKeyElm *obj= dynamic_cast<const CMouseKeyElm *>(k);
		return obj && obj->m_Btn == m_Btn;
	}
	virtual bool isPressed(float *val, KPollFlags flags) const override {
		if (flags & POLLFLAG_NO_MOUSE) return false;
		if (m_Btn < 0) return false;
		if (m_Mouse->isButtonDown(m_Btn)) {
			if (val) *val = 1;
			return true;
		}
		return false;
	}
};

// joystick axis
class CJoyAxisKeyElm: public IKeyElm {
	KCoreJoystick *m_Joy;

	// このボタンに割り当てられたジョイスティック軸。
	// 割り当てなしの場合は KJoyAxis_NONE
	KJoystickAxis m_Axis;

	// このクラスは正負どちらかの入力を使う。
	// 軸入力は [-1 .. 1] のうち、
	// 負の入力 [-1..0] に反応するなら -1 を指定する
	// 正の入力 [0..1] に反応するなら 1 を指定する
	// 仕様上、[-1..1] 全体には反応しない。かならずどちらか一方である
	int m_HalfRange;

	float m_Threshold;
public:
	CJoyAxisKeyElm(KCoreJoystick *joy) {
		m_Joy = joy;
		m_Joy->grab();
		m_Axis = KJoystickAxis_NONE;
		m_HalfRange = 0;
		m_Threshold = 0;
	}
	CJoyAxisKeyElm(KCoreJoystick *joy, KJoystickAxis axis, int halfrange, float threshold) {
		m_Joy = joy;
		m_Joy->grab();
		m_Axis = axis;
		m_HalfRange = halfrange;
		m_Threshold = threshold;
	}
	virtual ~CJoyAxisKeyElm() {
		m_Joy->drop();
	}
	// IKeyElm
	virtual bool isConflictWith(const IKeyElm *k) const override {
		const CJoyAxisKeyElm *obj= dynamic_cast<const CJoyAxisKeyElm *>(k);
		return obj && obj->m_Axis == m_Axis;
	}
	virtual bool isPressed(float *val, KPollFlags flags) const override {
		if (flags & POLLFLAG_NO_JOYSTICK) return false;
		if (m_Axis == KJoystickAxis_NONE) return false;

		// 傾きの絶対値が最も大きい軸の値を得る
		float axisval = 0.0f;
		if (m_Joy && m_Joy->isConnected()) {
			float val = m_Joy->getAxis(m_Axis, m_Threshold);
			if (fabsf(val) > fabsf(axisval)) {
				axisval = val;
			}
		}
		if (m_HalfRange < 0) {
			// in[-1..0] ==> out[0..1]
			if (axisval < 0) {
				if (val) *val = fabsf(axisval);
				return true;
			}
		}
		if (m_HalfRange > 0) {
			// in[0..1] ==> out[0..1]
			if (axisval > 0) {
				if (val) *val = fabsf(axisval);
				return true;
			}
		}
		return false;
	}
};

// joystick pov
class CJoyPovKeyElm: public IKeyElm {
	KCoreJoystick *m_Joy;
	int m_PovX;
	int m_PovY;
public:
	CJoyPovKeyElm(KCoreJoystick *joy) {
		m_Joy = joy;
		m_Joy->grab();
		m_PovX = 0;
		m_PovY = 0;
	}
	CJoyPovKeyElm(KCoreJoystick *joy, int povX, int povY) {
		m_Joy = joy;
		m_Joy->grab();
		m_PovX = povX;
		m_PovY = povY;
	}
	virtual ~CJoyPovKeyElm() {
		m_Joy->drop();
	}
	// IKeyElm
	virtual bool isConflictWith(const IKeyElm *k) const override {
		const CJoyPovKeyElm *obj = dynamic_cast<const CJoyPovKeyElm *>(k);
		return obj && obj->m_PovX==m_PovX && obj->m_PovY==m_PovY;
	}
	virtual bool isPressed(float *val, KPollFlags flags) const override {
		if (flags & POLLFLAG_NO_JOYSTICK) return false;
		if (m_PovX == 0 && m_PovY == 0) return false;
		if (m_Joy->isConnected()) {
			int x=0, y=0;
			if (m_Joy->getPov(&x, &y, nullptr)) {
			#if 1
				// 設定値と入力値の符号の一致を確認する.
				// ただし設定値が 0 の場合は常に一致するとみなす
				bool xmatch = m_PovX ? (x*m_PovX > 0) : true; 
				bool ymatch = m_PovY ? (y*m_PovY > 0) : true;
			#else
				// 設定値と入力値の符号の一致を確認する.
				// 設定値が 0 の場合、入力値も 0 でないといけない
				bool xmatch = KMath::signf(x)==KMath::signf(m_PovX);
				bool ymatch = KMath::signf(y)==KMath::signf(m_PovY);
			#endif
				if (xmatch && ymatch) {
					if (val) *val = 1;
					return true;
				}
			}
		}
		return false;
	}
};

// command button sequence
class CSquenceKeyElm: public IKeyElm {
	std::vector<const IKeyElm *> m_Keys;
	const IKeyHistory *m_History; // 入力履歴
	mutable bool m_OK;
public:
	explicit CSquenceKeyElm(const IKeyHistory *hist) {
		m_History = hist;
		m_OK = false;
	}
	bool empty() const {
		return m_Keys.empty();
	}
	void clear() {
		m_Keys.clear();
		m_OK = false;
	}
	void add_seq(const IKeyElm *btn) {
		K__ASSERT(btn);
		m_Keys.push_back(btn);
		m_OK = false;
	}

	// IKeyElm
	virtual bool isPressed(float *val, KPollFlags flags) const override {
		int histsize = m_History->get_history_size();
		int seqsize = (int)m_Keys.size();

		// 既に成立している場合、最後に押したキーが押されている間は
		// 成立したままであるとみなす。たとえば↓→というコマンドの場合、
		// ↓→と押して、→を押しっぱなしにしている限りはコマンド入力が続いているものとする
		if (m_OK) {
			// キーコマンドの最終入力
			const IKeyElm *last = m_Keys[seqsize - 1];

			// そのキーをまだ押し続けているならOK
			if (last->isPressed(nullptr, flags)) {
				// 入力が継続している
				if (val) *val = 1;
				return true;
			}

			// ボタンを離した
			m_OK = false;
			return false;
		}

		// キーシーケンスが、キー入力履歴の末尾の並びと一致するか調べる
		if (histsize < seqsize) {
			m_OK = false;
			return false;
		}

		for (int i=0; i<seqsize; i++) {

			// 期待している入力
			const IKeyElm *refkey = m_Keys[i];

			// 履歴のボタン
			const SKeyHistoryItem *histitem = m_History->get_history_item(histsize - seqsize + i);

			// 期待している入力が履歴に含まれていればOK
			if (!histitem->contains(refkey)) {
				m_OK = false;
				return false; // ボタンが違う
			}

			// 時刻チェック
			// 直前のキー入力から一定時間経過している場合は連続した入力として受け付けない
			if (i > 0) {
				const SKeyHistoryItem *hist_prev = m_History->get_history_item(histsize - seqsize + i - 1);
				int time0 = hist_prev->m_TimestampMsec;
				int time1 = histitem->m_TimestampMsec;
				if (time0 + TIMEOUT_MSEC <= time1) {
					m_OK = false;
					return false; // timeout
				}
			}
		}

		// 入力が成立している
		m_OK = true;
		if (val) *val = 1;
		return true;
	}
	virtual bool isConflictWith(const IKeyElm *k) const override {
		const CSquenceKeyElm *obj= dynamic_cast<const CSquenceKeyElm *>(k);
		if (obj) {
			// コマンド（自分）をコマンド（他）と比べる

			// 長さ比較
			size_t sz1 = m_Keys.size();
			size_t sz2 = obj->m_Keys.size();
			if (sz1 != sz2) {
				return false;
			}

			// 要素比較
			for (size_t i=0; i<m_Keys.size(); i++) {
				const IKeyElm *elm1 = m_Keys[i];
				const IKeyElm *elm2 = obj->m_Keys[i];
				if (elm1 != elm2) { // ポインタを比較するだけで良く、ボタンの設定内容まで見る必要はない
					return false;
				}
			}

			return true;

		} else {
			// コマンド（自分）と非コマンド（相手）を比べる
			// 相手キーが自分のキーシーケンスに含まれているなら
			// 衝突しているとする
			for (size_t i=0; i<m_Keys.size(); i++) {
				if (m_Keys[i]->isConflictWith(k)) {
					return true;
				}
			}
			return false;
		}
	}
};

const int AUTOREPEAT_DELAY_MSEC = 300;
const int AUTOREPEAT_INTERVAL_MSEC = 100;

class CActionButtonKeyElm: public IKeyElm {
public:
	std::string m_Name; // このアクションの名前
	std::vector<IKeyElm *> m_Keys; // このボタンにバインドするキー
	float m_Tmp;      // 受付中の入力値。未確定値。次のフレームでの確定値になる
	float m_Curr;     // 確定した入力値。現在のフレームでの入力として扱う値
	float m_Last;     // 直前のフレームでの入力値
	float m_RawCurr; // 最新のポーリングで得られた入力値
	float m_RawLast; // 直前のポーリングで得られた入力値
	KButtonFlags m_Flags;
	int m_TimestampPress; // ボタン状態が OFF から ON に変化したときの時刻 
	int m_TimestampNextRepeat; // 次のオートリピート予定時刻
	bool m_Repeat; // キーリピートによる入力が発生した

	CActionButtonKeyElm(const std::string &name, KButtonFlags flags) {
		m_Name = name;
		m_Flags = flags;
		m_Tmp = 0.0f;
		m_Curr = 0.0f;
		m_Last = 0.0f;
		m_TimestampPress = 0;
		m_TimestampNextRepeat = 0;
		m_Repeat = false;
	}
	virtual ~CActionButtonKeyElm() {
		clear();
	}

	// IKeyElm
	virtual bool isConflictWith(const IKeyElm *other) const override {
		K__ASSERT(0); // 未使用。このメソッドは呼ばれない
		return false;
	}
	virtual bool isPressed(float *val, KPollFlags flags) const override {
		if (m_Curr != 0.0f) {
			if (val) *val = m_Curr;
			return true;
		}
		return false;
	}

	void clear() {
		m_Tmp = 0.0f;
		m_Curr = 0.0f;
		m_Last = 0.0f;
		m_RawCurr = 0.0f;
		m_RawLast = 0.0f;
		m_TimestampPress = 0;
		m_TimestampNextRepeat = 0;
		m_Repeat = false;
		for (auto it=m_Keys.begin(); it!=m_Keys.end(); ++it) {
			IKeyElm *key = *it;
			key->drop();
		}
		m_Keys.clear();
	}
	int add_key(IKeyElm *key) {
		if (key == nullptr) return -1;
		key->grab();
		m_Keys.push_back(key);
		return (int)m_Keys.size() - 1;
	}
	void del_key(int index) {
		if (0 <= index && index < (int)m_Keys.size()) {
			m_Keys[index]->drop();
			m_Keys.erase(m_Keys.begin() + index);
		}
	}
	IKeyElm * get_key(int index) const {
		if (0 <= index && index < (int)m_Keys.size()) {
			return m_Keys[index];
		}
		return nullptr;
	}
	int get_key_count() const {
		return (int)m_Keys.size();
	}
	bool has_cmd() const {
		for (auto it=m_Keys.begin(); it!=m_Keys.end(); ++it) {
			if (dynamic_cast<CSquenceKeyElm*>(*it) != nullptr) {
				return true;
			}
		}
		return false;
	}
	const std::string & get_name() const {
		return m_Name;
	}
	KButtonFlags get_flags() const {
		return m_Flags;
	}

	// 新しいフレームに入る。
	// 前回 newFrame() を呼んでから発生した入力は、
	// すべて同時に起こったものとみなす。
	// newFrame() の呼び出しの間に最低 1 度は poll を呼ぶこと。
	void newFrame() {
		m_Last = m_Curr;
		m_Curr = m_Tmp;
		m_Tmp = 0.0f;
		m_Repeat = false;
	}

	// 入力状態を更新し、入力状態を得る。
	// newFrame を呼ぶまでは poll による入力状態が蓄積され続け、
	// その間のすべての入力は同時に行われたものとして扱う。
	// 例えば 30FPS のゲームの場合 newFrame() と poll() を 1/30 秒ごとに 1 回ずつ呼べば良いが、
	// 1/30秒の間にボタンを押して離してしまうと、ボタンが押されていないと判断される場合がある。
	// このような時、例えば poll() を 1/60秒ごとに呼べば入力精度が上がる (1 回の newFrame() に対して poll() を 2 回呼ぶ）
	void poll(KPollFlags flags) {
		m_RawLast = m_RawCurr;
		m_RawCurr = 0.0f;

		// シーケンス入力の状態を更新する
		// 現在のボタンの状態を得る
		float raw = 0.0f;
		if (isRawPressed(&raw, flags)) {
			m_RawCurr = raw;

			if (m_Tmp == 0.0f) {
				// 最初の入力。無条件で値をセットする
				m_Tmp = raw;

			} else if (m_Tmp * raw > 0) {
				// 同符号での入力。絶対値が大きければ書き換える
				if (fabsf(m_Tmp) < fabsf(raw)) {
					m_Tmp = raw;
				}

			} else {
				// 異符号での入力。
				// 後から入力された方を優先する
				m_Tmp = raw;
			}
		}
	}

	// poll() 前後で入力が発生したかどうか
	bool isJustPressedAtPoll(float *val) const {
		// ゼロから非ゼロに変化したか、符号が変わっていたら新たな入力が発生したものとする
		int s0 = _sign(m_RawLast);
		int s1 = _sign(m_RawCurr);
		if ((s0 == 0 && s1 != 0) || (s0 * s1 < 0)) {
			if (val) *val = m_RawCurr;
			return true;
		}
		return false;
	}

	// newFrame() 前後で入力が発生したかどうか
	bool isJustPressedAtFrame(float *val, bool check_autorepeat) const {
		// ゼロから非ゼロに変化したか、符号が変わっていたら新たな入力が発生したものとする
		int s0 = _sign(m_Last);
		int s1 = _sign(m_Curr);
		if ((s0 == 0 && s1 != 0) || (s0 * s1 < 0)) {
			if (val) *val = m_Curr;
			return true; // OFF から ON へ変化している
		}
		if (check_autorepeat && s1 && m_Repeat) {
			return true; // ボタンは押しっぱなしの状態だがオートリピート機能により入力が発生した
		}
		return false;
	}

	// 現在のボタンの状態を得る
	bool isRawPressed(float *val, KPollFlags flags) const {
		if (flags & POLLFLAG_FORCE_PUSH) {
			if (val) *val = 1.0f;
			return true;
		}

		for (auto it=m_Keys.begin(); it!=m_Keys.end(); ++it) {
			if ((*it)->isPressed(val, flags)) {
				return true;
			}
		}
		return false;
	}
};



/// ゲーム用のボタン入力を統合するためのクラス。
///
/// 1つのボタンに対して複数のキーボードのキーやジョイスティックボタン等を割り当てることができる
/// @snippet K_button.cpp test
class CButtonMgrImpl: public virtual KRef, public IKeyHistory {
	mutable std::mutex m_Mutex;
	std::vector<CActionButtonKeyElm*> m_Buttons;

	// フレーム単位での履歴。
	// newFrame するたびに更新される
	// フレーム更新の間に押されたボタンがマージされる
	std::vector<SKeyHistoryItem> m_HistoryPerFrame;

	// ポーリング単位での入力履歴。
	// poll するたびに更新される
	std::vector<SKeyHistoryItem> m_HistoryPerPoll;

	std::unordered_set<std::string> m_PostButtons;

	KPollFlags m_PollFlags;

	KCoreKeyboard *m_KB;
	KCoreMouse *m_Mouse;
	KCoreJoystick *m_Joy;

public:
	CButtonMgrImpl(KCoreKeyboard *kb, KCoreMouse *mouse, KCoreJoystick *joy) {
		m_HistoryPerFrame.clear();
		m_HistoryPerPoll.clear();
		m_Buttons.clear();
		m_PollFlags = 0;
		m_KB = kb; K__GRAB(m_KB);
		m_Mouse = mouse; K__GRAB(m_Mouse);
		m_Joy = joy; K__GRAB(m_Joy);
	}
	virtual ~CButtonMgrImpl() {
		for (size_t i=0; i<m_Buttons.size(); i++) {
			m_Buttons[i]->drop();
		}
		m_Buttons.clear();
		K__DROP(m_Mouse);
		K__DROP(m_Joy);
		K__DROP(m_KB);
	}

	#pragma region IKeyHistory inherit
	virtual const SKeyHistoryItem * get_history_item(int index) const override {
		return &m_HistoryPerPoll[index];
	}
	virtual int get_history_size() const override {
		return (int)m_HistoryPerPoll.size();
	}
	#pragma endregion // IKeyHistory inherit

	void setPollFlags(KPollFlags flags) {
		m_PollFlags = flags;
	}

	// 仮想ボタンを登録する
	void addButton(const std::string &button, KButtonFlags flags) {
		m_Mutex.lock();
		if (get_button(button) == nullptr) {
			m_Buttons.push_back(new CActionButtonKeyElm(button, flags));
		} else {
			get_button(button)->clear(); // リセット
		}
		m_Mutex.unlock();
	}

	// 仮想ボタンを削除
	void removeButton(const std::string &button) {
		m_Mutex.lock();
		for (size_t i=0; i<m_Buttons.size(); i++) {
			if (m_Buttons[i]->m_Name.compare(button) == 0) {
				m_Buttons[i]->drop();
				m_Buttons.erase(m_Buttons.begin() + i);
				break;
			}
		}
		m_Mutex.unlock();
	}

	// ボタン数を返す
	int getButtonCount() const {
		return (int)m_Buttons.size();
	}
	
	// ボタンが押されたことにする（次回のポーリング時に反映される）
	void postButtonDown(const std::string &button) {
		m_PostButtons.insert(button);
	}

	// ボタンを得る
	CActionButtonKeyElm * findButtonItem(const std::string &button) const {
		for (size_t i=0; i<m_Buttons.size(); i++) {
			if (m_Buttons[i]->m_Name.compare(button) == 0) {
				return m_Buttons[i];
			}
		}
		return nullptr;
	}

	// ボタンを得る
	CActionButtonKeyElm * getButtonItem(int index) const {
		if (0 <= index && index < (int)m_Buttons.size()) {
			return m_Buttons[index];
		}
		return nullptr;
	}

	// アクションボタンが押されているかどうか
	// poll() した段階で入力が成立しているなら true を返す
	bool isButtonDownByPoll(const std::string &button, float *out_value) const {
		// ボタンが押されているかどうか。
		// 入力を受付済みならば true を返す。これは次回 newFrame 呼び出し時に正式な入力として確定されるもの。
		// 押されているなら value に入力値をセットして true を返す
		bool retval = false;
		m_Mutex.lock();
		{
			const CActionButtonKeyElm *btn = get_button(button);
			if (btn && btn->isRawPressed(out_value, m_PollFlags)) {
				retval = true;
			}
		}
		m_Mutex.unlock();
		return retval;
	}

	// アクションボタンが押されているかどうか
	// ボタンが押されている状態ならば、ボタンの入力値を value にセットして true を返す
	// newFrame によって確定された入力だけを扱う。入力途中 (poll) の状態を調べたい場合は isButtonDownByPoll を使う
	bool isButtonDown(const std::string &button, float *out_value) const {
		// ボタンが押されているかどうか。
		// 押されているなら value に入力値をセットして true を返す
		bool retval = false;
		m_Mutex.lock();
		{
			const CActionButtonKeyElm *btn = get_button(button);
			if (btn && btn->isPressed(out_value, m_PollFlags)) {
				retval = true;
			}
		}
		m_Mutex.unlock();
		return retval;
	}

	// アクションボタンを押した瞬間かどうか
	// ボタンが押した瞬間（直後）ならば、ボタンの入力値を value にセットして true を返す
	// 押された瞬間かどうかは、直前に newFrame() を呼び出した時点でのボタンの状態で決まる。
	bool getButtonDown(const std::string &button, float *out_value) const {
		// ボタンが今押されたばかりかどうか。
		// 押された瞬間ならば value に入力値をセットして true を返す
		bool retval = false;
		m_Mutex.lock();
		{
			const CActionButtonKeyElm *btn = get_button(button);
			if (btn) {
				if (btn->isJustPressedAtFrame(out_value, true)) {
					retval = true;
				}
			}
		}
		m_Mutex.unlock();
		return retval;
	}

	int bindKey(const std::string &button, IKeyElm *key) {
		if (key == nullptr) {
			K__ERROR("invalid key");
			return -1;
		}
		CActionButtonKeyElm *btn = get_button(button);
		if (btn == nullptr) {
			K__ERROR(u8"E_BIND_KEY: 未登録のボタン(%s)にキーを割り当てようとしました", button.c_str());
			return -1;
		}
		m_Mutex.lock();
		btn->add_key(key);
		m_Mutex.unlock();
		return (int)btn->m_Keys.size() - 1;
	}

	void unbindKey(const std::string &button, int index) {
		CActionButtonKeyElm *btn = get_button(button);
		if (btn == nullptr) {
			btn->del_key(index);
		}
	}

	int getKeyCount(const std::string &button) {
		CActionButtonKeyElm *btn = get_button(button);
		return btn ? (int)btn->m_Keys.size() : 0;
	}

	IKeyElm * getKeyBinding(const std::string &button, int index) {
		CActionButtonKeyElm *btn = get_button(button);
		if (btn && 0<=index && index<(int)btn->m_Keys.size()) {
			return btn->m_Keys[index];
		}
		return nullptr;
	}

	// 仮想ボタンにキーボードのキーを割り当てる
	// @param btn_id アクション識別子
	// @param key 割り当てるキー (@KKey_A など)
	IKeyElm * createKeyboardKey(KKey key, KKeyModifiers mod) {
		if (key == KKey_NONE) {
			K__ERROR("invalid key");
			return nullptr;
		}
		return new CKbKeyElm(key, mod);
	}

	// 仮想ボタンにジョイスティックのキーを割り当てる
	IKeyElm * createJoystickKey(KJoystickButton btn) {
		if (m_Joy == nullptr) {
			K__ERROR("no joystick support");
			return nullptr;
		}
		return new CJoyKeyElm(m_Joy, btn);
	}

	// 仮想ボタンにジョイスティックの軸を割り当てる
	// @param axis 割り当てる軸 (@K_JOYAXIS_X など)
	// @halfrange  軸入力の正と負のどちらに割り当てるか。-1 か 1 のどちらかを指定する（正負の両方に割り当てることはできない）
	IKeyElm * createJoystickAxis(KJoystickAxis axis, int halfrange, float threshold) {
		if (m_Joy == nullptr) {
			K__ERROR("no joystick support");
			return nullptr;
		}
		return new CJoyAxisKeyElm(m_Joy, axis, halfrange, threshold);
	}

	// 仮想ボタンにジョイスティックのハットボタン(POV)を割り当てる
	// @param xsign ハットボタンの x 符号 (-1, 0, 1)
	// @param ysign ハットボタンの y 符号 (-1, 0, 1)
	IKeyElm * createJoystickPov(int xsign, int ysign) {
		if (m_Joy == nullptr) {
			K__ERROR("no joystick support");
			return nullptr;
		}
		return new CJoyPovKeyElm(m_Joy, xsign, ysign);
	}

	// 仮想ボタンにマウスのキーを割り当てる
	IKeyElm * createMouseKey(KMouseButton btn) {
		if (m_Mouse == nullptr) {
			K__ERROR("no mouse support");
			return nullptr;
		}
		return new CMouseKeyElm(m_Mouse, btn);
	}

	// 仮想ボタンにコマンド入力を割り当てる。
	//
	// @param keys コマンド配列。仮想ボタン名を並べた配列を指定する。
	// 終端記号として配列の末尾に 0 を設定しておくこと。
	IKeyElm * createCommand(const char *buttons[]) {
		// コマンド入力を割り当てる
		// keys にはボタン番号とその符号を並べた配列を指定し、最後に 0 を置く。
		if (buttons == nullptr || buttons[0] == 0) {
			K__ERROR("empty sequence");
			return nullptr;
		}
		// 事前チェック
		for (int i=0; buttons[i]; i++) {
			const std::string &name = buttons[i];
			const CActionButtonKeyElm *act_elm = get_button(name);
			if (act_elm == nullptr) {
				K__ERROR(u8"キーコマンドに含まれているボタン(%d)は未登録です", name.c_str());
				return nullptr;
			}
			if (act_elm->has_cmd()) {
				K__ERROR(u8"キーコマンドに含まれているボタン(%d)には既に別のコマンドを持っています（再帰禁止）", name.c_str());
				return nullptr;
			}
			if (i >= MAX_SEQUENCE_SIZE) {
				K__ERROR(u8"キーコマンドが長すぎます");
				return nullptr;
			}
		}

		CSquenceKeyElm *cmdkey = new CSquenceKeyElm(this /*IKeyHistory*/);
		// コマンド列
		for (int i=0; buttons[i]; i++) {
			const std::string &name = buttons[i];
			cmdkey->add_seq(get_button(name)); // ボタンを追加
		}
		return cmdkey;
	}

	// キーバインドが衝突しているか。
	// ２個の仮想ボタンに共通のキーバインドがあるか調べ、
	// あるキーを押したときに button1 と button2 の両方が ON になるなら true を返す
	bool isConflict(const std::string &button1, const std::string &button2) const {
		// action1 と btn_action2d2 に同一のキーがバインドされているなら true
		// 存在しない KActionButtonId を指定した場合は必ず false になる
		bool retval = false;
		m_Mutex.lock();
		{
			const CActionButtonKeyElm *btn1 = get_button(button1);
			const CActionButtonKeyElm *btn2 = get_button(button2);
			retval = check_conflict(btn1, btn2);
		}
		m_Mutex.unlock();
		return retval;
	}

	// 新しい入力フレームを開始する
	//
	// この呼び出しを境にして「ボタンが押された瞬間」を判定する
	void newFrame() {
		m_Mutex.lock();

		// 全ボタンのフレームを更新
		for (size_t i=0; i<m_Buttons.size(); i++) {
			CActionButtonKeyElm *btn = m_Buttons[i];
			btn->newFrame();
		}

		// 現在時刻
		int now_msec = get_clock_msec();

		// 新フレームに入った時点で押されているボタン
		SKeyHistoryItem histitem;
		histitem.m_TimestampMsec = now_msec;
		for (size_t i=0; i<m_Buttons.size(); i++) {
			CActionButtonKeyElm *btn = m_Buttons[i];
			if (btn->isJustPressedAtFrame(nullptr, false)) {
				// ボタンが今押された
				btn->m_TimestampPress = now_msec; // ボタンONの検出時刻
				btn->m_TimestampNextRepeat = now_msec + AUTOREPEAT_DELAY_MSEC;
				histitem.m_PressedButtons.push_back(btn);
			
			} else if (btn->isPressed(nullptr, m_PollFlags)) {
				// ボタンが以前から押され続けている。
				// キーリピートありなら連続入力処理する
				if (btn->m_Flags & KButtonFlag_REPEAT) {
					if (btn->m_TimestampNextRepeat <= now_msec) { // オートリピート開始時刻を過ぎている？
						int delta = now_msec - btn->m_TimestampNextRepeat;
						if (delta < AUTOREPEAT_INTERVAL_MSEC*2) {
							btn->m_Repeat = true; // オートリピート発動
							btn->m_TimestampNextRepeat += AUTOREPEAT_INTERVAL_MSEC; // 次回のオートリピート時刻
						} else {
							// あまりにも時間が経ちすぎているのでリピート無効
						}
					}
				}
			}
		}

		// 押されたボタンがあったらフレーム単位の履歴に追加する
		if (histitem.m_PressedButtons.size() > 0) {
			m_HistoryPerFrame.push_back(histitem);
		}

		// 履歴のサイズが一定数を超えないように
		while (m_HistoryPerFrame.size() > MAX_HISTORY_SIZE) {
			m_HistoryPerFrame.erase(m_HistoryPerFrame.begin());
		}

		m_Mutex.unlock();
	}

	void updateGui() {
		if (ImGui::BeginTable("##pages", 2, ImGuiTableFlags_Resizable|ImGuiTableFlags_BordersV|ImGuiTableFlags_SizingFixedFit)) {
			for (size_t i=0; i<m_Buttons.size(); i++) {
				CActionButtonKeyElm *elm = m_Buttons[i];
				ImVec4 color;
				if (elm->m_RawCurr != 0) {
				//	color = KImGui::COLOR_WARNING);
					color = KImGui::COLOR_DEFAULT();
				} else {
					color = KImGui::COLOR_DISABLED();
				}

				ImGui::BeginGroup();
				ImGui::TableNextColumn();
				ImGui::TextColored(color, "%s", elm->m_Name.c_str());

				ImGui::TableNextColumn();
				ImGui::TextColored(color, "%.2f", elm->m_RawCurr);
				ImGui::EndGroup();

				if (ImGui::IsItemHovered()) { ImGui::SetTooltip("%s", typeid(*elm).name()); }

			}
			ImGui::EndTable();
		}
	}

	// 入力状態を更新する
	// newFrame よりも多い回数呼ぶと入力判定の精度が上がる
	void poll() {
		if (m_Joy) {
			m_Joy->poll();
		}
		m_Mutex.lock();

		// 全ボタンの入力状態を更新
		for (size_t i=0; i<m_Buttons.size(); i++) {
			CActionButtonKeyElm *btn = m_Buttons[i];

			KPollFlags poll_flags = m_PollFlags;

			auto it = m_PostButtons.find(btn->m_Name);
			if (it != m_PostButtons.end()) {
				// このボタンが押されたことにする
				poll_flags |= POLLFLAG_FORCE_PUSH;
			}

			btn->poll(poll_flags);
		}

		m_PostButtons.clear();

		// 現在時刻
		int time = get_clock_msec();
		
		// ポーリングの時点で押されているボタン
		SKeyHistoryItem histitem;
		histitem.m_TimestampMsec = time;
		for (size_t i=0; i<m_Buttons.size(); i++) {
			CActionButtonKeyElm *btn = m_Buttons[i];
			if (btn->isJustPressedAtPoll(nullptr)) {
				btn->m_TimestampPress = time; // ボタンONの検出時刻
				histitem.m_PressedButtons.push_back(btn);

			} else {
				// ボタンが押され続けている or 全く押されていない。
				// ポーリングではキーリピート処理しないので、ここでは何もしない
			}
		}

		// 押されたボタンがあったらポーリング単位の履歴に追加する
		if (histitem.m_PressedButtons.size() > 0) {
			m_HistoryPerPoll.push_back(histitem);
		}

		// 履歴のサイズが一定数を超えないように
		while (m_HistoryPerPoll.size() > MAX_HISTORY_SIZE) {
			m_HistoryPerPoll.erase(m_HistoryPerPoll.begin());
		}

		m_Mutex.unlock();
	}
private:
	bool check_conflict(const CActionButtonKeyElm *b1, const CActionButtonKeyElm *b2) const {
		// b1 と b2 に同一のキーがバインドされているなら true を返す
		// (つまりキーを押したときに b1 と b2 の両方が反応するなら ture を返す）
		if (b1 == nullptr) return false;
		if (b2 == nullptr) return false;
		for (size_t i=0; i+1<b1->m_Keys.size(); i++) {
			for (size_t j=i+1; j<b2->m_Keys.size(); j++) {
				const IKeyElm *k1 = b1->m_Keys[i];
				const IKeyElm *k2 = b2->m_Keys[j];
				if (k1 && k2) {
					if (k1->isConflictWith(k2)) {
						return true;
					}
				}
			}
		}
		return false;
	}
	bool has_conflict() const {
		// バインドが重複しているボタンがあるなら true
		// vector 内のすべての組み合わせについて衝突を調べる
		for (size_t i=0; i+1<m_Buttons.size(); i++) {
			for (size_t j=i+1; j<m_Buttons.size(); j++) {
				const CActionButtonKeyElm *k1 = m_Buttons[i];
				const CActionButtonKeyElm *k2 = m_Buttons[j];
				if (check_conflict(k1, k2)) {
					return true;
				}
			}
		}
		return false;
	}
	CActionButtonKeyElm * get_button(const std::string &action) {
		// ボタンを取得
		if (action.empty()) return nullptr;
		for (size_t i=0; i<m_Buttons.size(); i++) {
			if (m_Buttons[i]->m_Name.compare(action) == 0) {
				return m_Buttons[i];
			}
		}
		return nullptr;
	}
	const CActionButtonKeyElm * get_button(const std::string &action) const {
		// ボタンを取得
		if (action.empty()) return nullptr;
		for (size_t i=0; i<m_Buttons.size(); i++) {
			if (m_Buttons[i]->m_Name.compare(action) == 0) {
				return m_Buttons[i];
			}
		}
		return nullptr;
	}

	// 連続入力のタイムアウト計測用時刻
	int get_clock_msec() const {
		return (int)K::clockMsec32();
	}
};
#pragma endregion // Buttons







#define MAX_KEY_SEQUENCE_LENGTH 8

typedef std::unordered_map<KJoystickButton, std::string> JOYNAMES;

static JOYNAMES & _GetJoyNames() {
	static JOYNAMES s_Names;
	if (s_Names.empty()) {
		s_Names[KJoystickButton_1 ] = "Button1";
		s_Names[KJoystickButton_2 ] = "Button2";
		s_Names[KJoystickButton_3 ] = "Button3";
		s_Names[KJoystickButton_4 ] = "Button4";
		s_Names[KJoystickButton_5 ] = "Button5";
		s_Names[KJoystickButton_6 ] = "Button6";
		s_Names[KJoystickButton_7 ] = "Button7";
		s_Names[KJoystickButton_8 ] = "Button8";
		s_Names[KJoystickButton_9 ] = "Button9";
		s_Names[KJoystickButton_10] = "Button10";
		s_Names[KJoystickButton_11] = "Button11";
		s_Names[KJoystickButton_12] = "Button12";
		s_Names[KJoystickButton_13] = "Button13";
		s_Names[KJoystickButton_14] = "Button14";
		s_Names[KJoystickButton_15] = "Button15";
		s_Names[KJoystickButton_16] = "Button16";
	}
	return s_Names;
}
static void _UpdateButtonGui(CButtonMgrImpl *mgr) {
	size_t num = mgr->getButtonCount();
	for (size_t i=0; i<num; i++) {

		// ボタンIDを得る
		const std::string &name = mgr->getButtonItem(i)->get_name();

		// ボタンが入力状態かどうか？
		if (mgr->isButtonDown(name, nullptr)) {
			// 入力確定
			KImGui::PushTextColor(KImGui::COLOR_DEFAULT());
			ImGui::Text("[%s]", name.c_str());
			KImGui::PopTextColor();

		} else if (mgr->isButtonDownByPoll(name, nullptr)) {
			// 受付済み
			KImGui::PushTextColor(KImGui::COLOR_WARNING);
			ImGui::Text("[%s]", name.c_str());
			KImGui::PopTextColor();

		} else {
			// 入力なし
			KImGui::PushTextColor(KImGui::COLOR_DISABLED());
			ImGui::Text("[%s]", name.c_str());
			KImGui::PopTextColor();
		}
	}
}



#pragma region CNodeController

static float _Reduce(float val, float delta) {
	if (val > 0) {
		val = KMath::max(val - delta, 0.0f);
	}
	if (val < 0) {
		val = KMath::min(val + delta, 0.0f);
	}
	return val;
}

#pragma region Decl
class CNodeController: public KComp {
public:
	CNodeController();
	void setTriggerTimeout(int val);
	void clearInputs();

	// トリガー入力をリセットする
	// なお beginReadTrigger/endReadTrigger で読み取りモードにしている場合でも clearTrighgers は関係なく動作する
	void clearTriggers();
	void setInputAxisX(int i);
	void setInputAxisY(int i);
	void setInputAxisZ(int i);
	void setInputAxisAnalogX(float value);
	void setInputAxisAnalogY(float value);
	void setInputAxisAnalogZ(float value);
	int getInputAxisX();
	int getInputAxisY();
	int getInputAxisZ();
	float getInputAxisAnalogX();
	float getInputAxisAnalogY();
	float getInputAxisAnalogZ();
	void setInputTrigger(const std::string &button);
	void setInputBool(const std::string &button, bool pressed);
	bool getInputBool(const std::string &button);

	/// トリガーボタンの状態を得て、トリガー状態を false に戻す。
	/// トリガーをリセットしたくない場合は peekInputTrigger を使う。
	/// また、即座にリセットするのではなく特定の時点までトリガーのリセットを
	/// 先延ばしにしたい場合は beginReadTrigger() / endReadTrigger() を使う
	bool getInputTrigger(const std::string &button);
	
	/// トリガーの読み取りモードを開始する
	/// endReadTrigger が呼ばれるまでの間は getInputTrigger を呼んでもトリガー状態が false に戻らない
	void beginReadTrigger();

	/// トリガーの読み取りモードを終了する。
	/// beginReadTrigger が呼ばれた後に getInputTrigger されたトリガーをすべて false に戻す
	void endReadTrigger();
	void tickInput();

	KNode *m_Node;
	KInputCallback *m_CB;

private:
	std::unordered_map<std::string, int> m_Buttons;
	std::unordered_set<std::string> m_Peeked;
	float m_AxisX;
	float m_AxisY;
	float m_AxisZ;
	float m_AxisReduce;
	int m_TriggerTimeout;
	int m_ReadTrigger;
};
#pragma endregion // Decl


#pragma region Impl
CNodeController::CNodeController() {
	m_AxisX = 0;
	m_AxisY = 0;
	m_AxisZ = 0;
	m_AxisReduce = 0.2f; // 1フレーム当たりの軸入力の消失値（正の値を指定）
	m_Peeked.clear();
	m_Buttons.clear();
	m_TriggerTimeout = 10;
	m_ReadTrigger = 0;
	m_Node = nullptr;
	m_CB = nullptr;
}
void CNodeController::setTriggerTimeout(int val) {
	m_TriggerTimeout = val;
}
void CNodeController::clearInputs() {
	m_AxisX = 0;
	m_AxisY = 0;
	m_AxisZ = 0;
	m_Buttons.clear();
	m_Peeked.clear();
}

// トリガー入力をリセットする
// なお beginReadTrigger/endReadTrigger で読み取りモードにしている場合でも clearTrighgers は関係なく動作する
void CNodeController::clearTriggers() {
	for (auto it=m_Buttons.begin(); it!=m_Buttons.end(); /*++it*/) {
		if (it->second >= 0) {
			it = m_Buttons.erase(it);
		} else {
			it++;
		}
	}
	m_Peeked.clear();
}
void CNodeController::setInputAxisX(int value) {
	setInputAxisAnalogX((float)value);
}
void CNodeController::setInputAxisY(int value) {
	setInputAxisAnalogY((float)value);
}
void CNodeController::setInputAxisZ(int value) {
	setInputAxisAnalogZ((float)value);
}
void CNodeController::setInputAxisAnalogX(float value) {
	K__ASSERT(fabsf(value) <= 1);
	m_AxisX = value;
}
void CNodeController::setInputAxisAnalogY(float value) {
	K__ASSERT(fabsf(value) <= 1);
	m_AxisY = value;
}
void CNodeController::setInputAxisAnalogZ(float value) {
	K__ASSERT(fabsf(value) <= 1);
	m_AxisZ = value;
}
int CNodeController::getInputAxisX() {
	float val = getInputAxisAnalogX();
	return (int)KMath::signf(val); // returns -1, 0, 1
}
int CNodeController::getInputAxisY() {
	float val = getInputAxisAnalogY();
	return (int)KMath::signf(val); // returns -1, 0, 1
}
int CNodeController::getInputAxisZ() {
	float val = getInputAxisAnalogZ();
	return (int)KMath::signf(val); // returns -1, 0, 1
}
float CNodeController::getInputAxisAnalogX() {
	float result = KMath::clampf(m_AxisX, -1.0f, 1.0f);
	if (m_CB) {
		m_CB->on_input_axisX(m_Node, &result);
	}
	return result;
}
float CNodeController::getInputAxisAnalogY() {
	float result = KMath::clampf(m_AxisY, -1.0f, 1.0f);
	if (m_CB) {
		m_CB->on_input_axisY(m_Node, &result);
	}
	return result;
}
float CNodeController::getInputAxisAnalogZ() {
	float result = KMath::clampf(m_AxisZ, -1.0f, 1.0f);
	if (m_CB) {
		m_CB->on_input_axisZ(m_Node, &result);
	}
	return result;
}

void CNodeController::setInputTrigger(const std::string &button) {
	if (button.empty()) return;
	m_Buttons[button] = m_TriggerTimeout;
}
void CNodeController::setInputBool(const std::string &button, bool pressed) {
	if (button.empty()) return;
	if (pressed) {
		m_Buttons[button] = -1;
	} else {
		m_Buttons.erase(button);
	}
}
bool CNodeController::getInputBool(const std::string &button) {
	if (button.empty()) return false;
	return m_Buttons.find(button) != m_Buttons.end();
}

/// トリガーボタンの状態を得て、トリガー状態を false に戻す。
/// トリガーをリセットしたくない場合は peekInputTrigger を使う。
/// また、即座にリセットするのではなく特定の時点までトリガーのリセットを
/// 先延ばしにしたい場合は beginReadTrigger() / endReadTrigger() を使う
bool CNodeController::getInputTrigger(const std::string &button) {
	if (button.empty()) return false;

	float result = 0;

	auto it = m_Buttons.find(button);
	if (it != m_Buttons.end()) {
		if (m_ReadTrigger > 0) {
			// PEEKモード中。
			// トリガーを false に戻さず、参照されたことだけを記録しておく
			m_Peeked.insert(button);
		} else {
			// トリガー状態を false に戻す
			m_Buttons.erase(it);
		}
		result = 1;
	}

	if (m_CB) {
		m_CB->on_input_button(m_Node, button, &result);
	}

	return result != 0;
}
	
/// トリガーの読み取りモードを開始する
/// endReadTrigger が呼ばれるまでの間は getInputTrigger を呼んでもトリガー状態が false に戻らない
void CNodeController::beginReadTrigger() {
	m_ReadTrigger++;
}

/// トリガーの読み取りモードを終了する。
/// beginReadTrigger が呼ばれた後に getInputTrigger されたトリガーをすべて false に戻す
void CNodeController::endReadTrigger() { 
	m_ReadTrigger--;
	if (m_ReadTrigger == 0) {
		for (auto it=m_Peeked.begin(); it!=m_Peeked.end(); ++it) {
			const std::string &btn = *it;
			m_Buttons.erase(btn);
		}
		m_Peeked.clear();
	}
}
void CNodeController::tickInput() {
	// 入力状態を徐々に消失させる
	m_AxisX = _Reduce(m_AxisX, m_AxisReduce);
	m_AxisY = _Reduce(m_AxisY, m_AxisReduce);
	m_AxisZ = _Reduce(m_AxisZ, m_AxisReduce);

	// トリガー入力のタイムアウトを処理する
	for (auto it=m_Buttons.begin(); it!=m_Buttons.end(); /*EMPTY*/) {
		if (it->second > 0) {
			it->second--;
			it++;
		} else if (it->second == 0) {
			it = m_Buttons.erase(it);
		} else {
			it++; // タイムアウトが負の値の場合なら何もしない
		}
	}

	// 入力への介入
	if (m_CB) {
		m_CB->on_input_tick(m_Node);
	}
}
#pragma endregion // Impl


#pragma endregion // CNodeController








class CInputMap: public KManager, public KInspectorCallback {
	CButtonMgrImpl *m_GameButtons; // ゲーム内入力用のボタンマネージャ
	CButtonMgrImpl *m_AppButtons; // アプリ操作用のボタンマネージャ
	KCoreKeyboard *m_KB;
	KCoreMouse *m_Mouse;
	KCoreJoystick *m_Joy;
public:
	KCompNodes<CNodeController> m_Nodes;

	CInputMap(KCoreKeyboard *kb, KCoreMouse *ms, KCoreJoystick *js) {
		m_KB = kb;    K__GRAB(m_KB);
		m_Mouse = ms; K__GRAB(m_Mouse);
		m_Joy = js;   K__GRAB(m_Joy);
		m_GameButtons = new CButtonMgrImpl(m_KB, m_Mouse, m_Joy);
		m_AppButtons = new CButtonMgrImpl(m_KB, m_Mouse, m_Joy);
		KEngine::addManager(this);
		KEngine::addInspectorCallback(this, u8"入力"); // KInspectorCallback
	}
	virtual ~CInputMap() {
		K__DROP(m_GameButtons);
		K__DROP(m_AppButtons);
		K__DROP(m_Joy);
		K__DROP(m_KB);
		K__DROP(m_Mouse);
	}
	virtual void onInspectorGui() override { // KInspectorCallback
		ImGui::Text("App buttons");
		ImGui::Separator();
		ImGui::Indent();
		_UpdateButtonGui(m_AppButtons);
		ImGui::Unindent();
		KImGui::VSpace();

		ImGui::Text("Game buttons");
		ImGui::Separator();
		ImGui::Indent();
		_UpdateButtonGui(m_GameButtons);
		ImGui::Unindent();
		KImGui::VSpace();

		ImGui::Text("Raw Input");
		ImGui::Separator();
		ImGui::Indent();
		m_GameButtons->updateGui();
		ImGui::Unindent();
		KImGui::VSpace();
	}
	virtual bool on_manager_isattached(KNode *node) override {
		return m_Nodes.contains(node);
	}
	virtual void on_manager_detach(KNode *node) override {
		m_Nodes.detach(node);
	}
	virtual void on_manager_appframe() override {
		{
			// ウィンドウが非アクティブだったりIMGUIにフォーカスが行っている場合に
			// キーボード入力を拾ってしまわないように調整する
			KPollFlags flags = 0;
			if (KEngine::getStatus(KEngine::ST_KEYBOARD_BLOCKED)) {
				flags |= POLLFLAG_NO_KEYBOARD;
				flags |= POLLFLAG_NO_MOUSE;
			}
			m_AppButtons->setPollFlags(flags);
			m_GameButtons->setPollFlags(flags);
		}

		m_AppButtons->newFrame();
		m_AppButtons->poll();

		for (int i=0; i<m_AppButtons->getButtonCount(); i++) {
			CActionButtonKeyElm *btn = m_AppButtons->getButtonItem(i);
			const std::string &name = btn->get_name();
			KButtonFlags flags = btn->get_flags();
			if (m_AppButtons->getButtonDown(name, nullptr)) {
				KSig sig(K_SIG_INPUT_APP_BUTTON);
				sig.setString("button", name);
				KEngine::broadcastSignal(sig);
			}
		}
		if (0) {
			// ゲームボタンのポーリングを続ける.
			// デバッグによる一時停止中にキーシーケンスを入力し、完了することができるようになる
			m_GameButtons->poll();
		}
	}
	virtual void on_manager_frame() override {
		m_GameButtons->newFrame();
		m_GameButtons->poll();

		for (auto it=m_Nodes.begin(); it!=m_Nodes.end(); ++it) {
			CNodeController *co = it->second;
			co->tickInput();
		}
	}
	virtual void on_manager_nodeinspector(KNode *node) override {
	//	CNodeController *comp = m_Nodes.get(node);
	//	comp->_Inspector();
	}
	void setPollFlags(KPollFlags flags) {
		m_AppButtons->setPollFlags(flags);
		m_GameButtons->setPollFlags(flags);
	}
	void addAppButton(const std::string &button, KButtonFlags flags) {
		m_AppButtons->addButton(button, flags);
	}
	void addGameButton(const std::string &button, KButtonFlags flags) {
		m_GameButtons->addButton(button, flags);
	}
	bool isAppButtonDown(const std::string &button) const {
		return m_AppButtons->isButtonDown(button, nullptr);
	}
	bool getAppButtonDown(const std::string &button) const {
		return m_AppButtons->getButtonDown(button, nullptr);
	};
	bool isGameButtonDown(const std::string &button) const {
		return m_GameButtons->isButtonDown(button, nullptr);
	}
	bool getGameButtonDown(const std::string &button) const {
		return m_GameButtons->getButtonDown(button, nullptr);
	};
	float getGameButtonAnalog(const std::string &button) const {
		float analog_value = 0.0f;
		m_GameButtons->isButtonDown(button, &analog_value); // getButtonDown で取得してはいけない（アナログには値は「入力の瞬間」というものがない）
		return analog_value;
	};

	void postButtonDown(const std::string &button) {
		if (m_AppButtons->findButtonItem(button)) {
			m_AppButtons->postButtonDown(button);
		}
		if (m_GameButtons->findButtonItem(button)) {
			m_GameButtons->postButtonDown(button);
		}
	}
	int isConflict(const std::string &button1, const std::string &button2) const {
		return m_GameButtons->isConflict(button1, button2);
	}
	void unbindByTag(const std::string &button, const std::string &tag) {
		CActionButtonKeyElm *btn = m_GameButtons->findButtonItem(button);
		if (btn) {
			for (int i=0; i<btn->get_key_count(); i++) {
				IKeyElm *key = btn->get_key(i);
				if (key->hasTag(tag)) {
					btn->del_key(i);
					return;
				}
			}
		}
	}
	void bindAppKey(const std::string &button, KKey key, KKeyModifiers mods) {
		IKeyElm *elm = m_AppButtons->createKeyboardKey(key, mods);
		if (elm) {
			m_AppButtons->bindKey(button, elm);
			elm->drop();
		}
	}
	void bindKeyboardKey(const std::string &button, KKey key, KKeyModifiers mods, const std::string &tag) {
		IKeyElm *elm = m_GameButtons->createKeyboardKey(key, mods);
		if (elm) {
			elm->setTag(tag);
			m_GameButtons->bindKey(button, elm);
			elm->drop();
		}
	}
	void bindJoystickKey(const std::string &button, KJoystickButton joybtn, const std::string &tag) {
		IKeyElm *elm = m_GameButtons->createJoystickKey(joybtn);
		if (elm) {
			elm->setTag(tag);
			m_GameButtons->bindKey(button, elm);
			elm->drop();
		}
	}
	void bindJoystickAxis(const std::string &button, KJoystickAxis axis, int halfrange, const std::string &tag, float threshold) {
		IKeyElm *elm = m_GameButtons->createJoystickAxis(axis, halfrange, threshold);
		if (elm) {
			elm->setTag(tag);
			m_GameButtons->bindKey(button, elm);
			elm->drop();
		}
	}
	void bindJoystickPov(const std::string &button, int xsign, int ysign, const std::string &tag) {
		IKeyElm *elm = m_GameButtons->createJoystickPov(xsign, ysign);
		if (elm) {
			elm->setTag(tag);
			m_GameButtons->bindKey(button, elm);
			elm->drop();
		}
	}
	void bindMouseKey(const std::string &button, KMouseButton mouse_btn, const std::string &tag) {
		IKeyElm *elm = m_GameButtons->createMouseKey(mouse_btn);
		if (elm) {
			elm->setTag(tag);
			m_GameButtons->bindKey(button, elm);
			elm->drop();
		}
	}
	void bindKeySequence(const std::string &button, const char *keys[], const std::string &tag) {
		IKeyElm *elm = m_GameButtons->createCommand(keys);
		if (elm) {
			elm->setTag(tag);
			m_GameButtons->bindKey(button, elm);
			elm->drop();
		}
	}
	IKeyboardKeyElm * findKeyboardByTag(const std::string &button, const std::string &tag) {
		CActionButtonKeyElm *btn = m_GameButtons->findButtonItem(button);
		if (btn == nullptr) return nullptr;

		for (int i=0; i<btn->get_key_count(); i++) {
			IKeyElm *key = btn->get_key(i);
			if (key->hasTag(tag)) {
				IKeyboardKeyElm *kbkey = dynamic_cast<IKeyboardKeyElm*>(key);
				if (kbkey) {
					return kbkey;
				}
			}
		}
		return nullptr;
	}
	IJoystickKeyElm * findJoystickByTag(const std::string &button, const std::string &tag) {
		CActionButtonKeyElm *btn = m_GameButtons->findButtonItem(button);
		if (btn == nullptr) return nullptr;

		for (int i=0; i<btn->get_key_count(); i++) {
			IKeyElm *key = btn->get_key(i);
			if (key->hasTag(tag)) {
				IJoystickKeyElm *jskey = dynamic_cast<IJoystickKeyElm*>(key);
				if (jskey) {
					return jskey;
				}
			}
		}
		return nullptr;
	}
	void resetAllButtonStates() {
	}
	std::string getJoystickName(KJoystickButton btn) const {
		return _GetJoyNames()[btn];
	}
	std::string getKeyboardName(KKey key) const {
		return KKeyboard::getKeyName(key); 
	}
	bool getJoystickFromName(const std::string &s, KJoystickButton *btn) const {
		const JOYNAMES &names = _GetJoyNames();
		for (auto it=names.begin(); it!=names.end(); ++it) {
			if (it->second.compare(s) == 0) {
				if (btn) *btn = it->first;
				return true;
			}
		}
		return false;
	}
	bool getKeyboardFromName(const std::string &s, KKey *key) const {
		KKey k = KKeyboard::findKeyByName(s.c_str());
		if (k != KKey_NONE) {
			if (key) *key = k;
			return true;
		}
		return false;
	}
}; // CInputMap

#pragma region KInputMap
static CInputMap *g_InputMap = nullptr;

void KInputMap::install(KCoreKeyboard *kb, KCoreMouse *ms, KCoreJoystick *js) {
	K__ASSERT(g_InputMap == nullptr);
	g_InputMap = new CInputMap(kb, ms, js);
}
void KInputMap::uninstall() {
	if (g_InputMap) {
		g_InputMap->drop();
		g_InputMap = nullptr;
	}
}

/// アプリケーションボタン（ポーズに影響されない）を登録する
/// @node ここで登録するのは仮想ボタンであり、まだどの物理キーにもバインドされていない。
/// この仮想ボタンを物理キーに関連付けるためには bindAppKey を使う 
/// （アプリケーションボタンにバインドできるのはキーボードのキーのみ）
void KInputMap::addAppButton(const std::string &button, KButtonFlags flags) {
	K__ASSERT(g_InputMap);
	g_InputMap->addAppButton(button, flags);
}

/// ゲームボタン（ポーズに影響されない）が押された状態になっているか？
bool KInputMap::isAppButtonDown(const std::string &button) {
	K__ASSERT(g_InputMap);
	return g_InputMap->isAppButtonDown(button);
}

/// アプリボタン（ポーズに影響されない）がたった今押されたか？
bool KInputMap::getAppButtonDown(const std::string &button) {
	K__ASSERT(g_InputMap);
	return g_InputMap->getAppButtonDown(button);
}

/// ゲームボタン（ポーズに影響される）を登録する
/// @node ここで登録するのは仮想ボタンであり、まだどの物理キーにもバインドされていない。
/// この仮想ボタンを物理キーに関連付けるためにはバインド関数を使う
/// @see bindKeyboardKey, bindJoystickKey, bindJoystickAxis, bindJoystickPov, bindMouseKey, bindKeySequence
void KInputMap::addGameButton(const std::string &button, KButtonFlags flags) {
	K__ASSERT(g_InputMap);
	g_InputMap->addGameButton(button, flags);
}

/// ゲームボタン（ポーズに影響される）が押された状態になっているか？
bool KInputMap::isGameButtonDown(const std::string &button) {
	K__ASSERT(g_InputMap);
	return g_InputMap->isGameButtonDown(button);
}

/// ゲームボタン（ポーズに影響される）がたった今押されたか？
bool KInputMap::getGameButtonDown(const std::string &button) {
	K__ASSERT(g_InputMap);
	return g_InputMap->getGameButtonDown(button);
}
float KInputMap::getGameButtonAnalog(const std::string &button) {
	K__ASSERT(g_InputMap);
	return g_InputMap->getGameButtonAnalog(button);
}

/// 指定されたボタンが押された状態になっているか？
/// （アプリボタンとゲームボタンのどちらにも対応する）
bool KInputMap::isButtonDown(const std::string &button) {
	return isAppButtonDown(button) || isGameButtonDown(button);
}

/// 指定されたボタンががたった今押されたか？
/// （アプリボタンとゲームボタンのどちらにも対応する）
bool KInputMap::getButtonDown(const std::string &button) {
	return getAppButtonDown(button) || getGameButtonDown(button);
}
float KInputMap::getButtonAnalog(const std::string &button) {
	return getGameButtonAnalog(button);
}

/// ボタンを押したことにする
void KInputMap::postButtonDown(const std::string &button) {
	K__ASSERT(g_InputMap);
	g_InputMap->postButtonDown(button);
}



/// addAppButton で登録した仮想ボタンに対してキーボードのキーをバインドする
void KInputMap::bindAppKey(const std::string &button, KKey key, KKeyModifiers mods) {
	K__ASSERT(g_InputMap);
	g_InputMap->bindAppKey(button, key, mods);
}

/// addButton で登録した仮想ボタンに対してキーボードのキーをバインドする
/// @note tag に適当な整数値を指定した場合、後でその値を検索キーとして特定のキーバインドを探すことができる。
/// /
/// @see unbindByTag, findKeyboardByTag, findJoystickByTag
void KInputMap::bindKeyboardKey(const std::string &button, KKey key, KKeyModifiers mods, const std::string &tag) {
	K__ASSERT(g_InputMap);
	g_InputMap->bindKeyboardKey(button, key, mods, tag);
}
void KInputMap::bindJoystickKey(const std::string &button, KJoystickButton joybtn, const std::string &tag) {
	K__ASSERT(g_InputMap);
	g_InputMap->bindJoystickKey(button, joybtn, tag);
}

/// addButton で登録した仮想ボタンに対してジョイスティックの軸をバインドする
/// ※軸の負方向または正方向のどちらかにしか割り当てられない。正負のどちらに割り当てるかを halfrange に -1 または 1 で指定する
void KInputMap::bindJoystickAxis(const std::string &button, KJoystickAxis axis, int halfrange, const std::string &tag, float threshold) {
	K__ASSERT(g_InputMap);
	g_InputMap->bindJoystickAxis(button, axis, halfrange, tag, threshold);
}

void KInputMap::bindJoystickPov(const std::string &button, int xsign, int ysign, const std::string &tag) {
	K__ASSERT(g_InputMap);
	g_InputMap->bindJoystickPov(button, xsign, ysign, tag);
}
void KInputMap::bindMouseKey(const std::string &button, KMouseButton mousebtn, const std::string &tag) {
	K__ASSERT(g_InputMap);
	g_InputMap->bindMouseKey(button, mousebtn, tag);
}
void KInputMap::bindKeySequence(const std::string &button, const char *keys[], const std::string &tag) {
	K__ASSERT(g_InputMap);
	g_InputMap->bindKeySequence(button, keys, tag);
}
void KInputMap::unbindByTag(const std::string &button, const std::string &tag) {
	K__ASSERT(g_InputMap);
	g_InputMap->unbindByTag(button, tag);
}
int KInputMap::isConflict(const std::string &button1, const std::string &button2) {
	K__ASSERT(g_InputMap);
	return g_InputMap->isConflict(button1, button2);
}
void KInputMap::resetAllButtonStates() {
	K__ASSERT(g_InputMap);
	g_InputMap->resetAllButtonStates();
}
IKeyboardKeyElm * KInputMap::findKeyboardByTag(const std::string &button, const std::string &tag) {
	K__ASSERT(g_InputMap);
	return g_InputMap->findKeyboardByTag(button, tag);
}
IJoystickKeyElm * KInputMap::findJoystickByTag(const std::string &button, const std::string &tag) {
	K__ASSERT(g_InputMap);
	return g_InputMap->findJoystickByTag(button, tag);
}
std::string KInputMap::getJoystickName(KJoystickButton joybtn) {
	K__ASSERT(g_InputMap);
	return g_InputMap->getJoystickName(joybtn);
}
std::string KInputMap::getKeyboardName(KKey key) {
	K__ASSERT(g_InputMap);
	return g_InputMap->getKeyboardName(key);
}
bool KInputMap::getKeyboardFromName(const std::string &s, KKey *key) {
	K__ASSERT(g_InputMap);
	return g_InputMap->getKeyboardFromName(s, key);
}
bool KInputMap::getJoystickFromName(const std::string &s, KJoystickButton *btn) {
	K__ASSERT(g_InputMap);
	return g_InputMap->getJoystickFromName(s, btn);
}
void KInputMap::setPollFlags(KPollFlags flags) {
	K__ASSERT(g_InputMap);
	g_InputMap->setPollFlags(flags);
}








#pragma region Per Node Input
void KInputMap::attach(KNode *node) {
	K__ASSERT(g_InputMap);
	if (node && !isAttached(node)) {
		CNodeController *co = new CNodeController();
		co->m_Node = node;
		g_InputMap->m_Nodes.attach(node, co);
		co->drop();
	}
}
bool KInputMap::isAttached(KNode *node) {
	K__ASSERT(g_InputMap);
	return g_InputMap->m_Nodes.get(node) != nullptr;
}

void KInputMap::setNodeInputCallback(KNode *node, KInputCallback *cb) {
	K__ASSERT(g_InputMap);
	auto co = g_InputMap->m_Nodes.get(node);
	if (co) {
		co->m_CB = cb;
	}
}
void KInputMap::clearTriggers(KNode *node) {
	K__ASSERT(g_InputMap);
	auto co = g_InputMap->m_Nodes.get(node);
	if (co) co->clearTriggers();
}
void KInputMap::clearInputs(KNode *node) {
	K__ASSERT(g_InputMap);
	auto co = g_InputMap->m_Nodes.get(node);
	if (co) co->clearInputs();
}
void KInputMap::setTriggerTimeout(KNode *node, int value) {
	K__ASSERT(g_InputMap);
	auto co = g_InputMap->m_Nodes.get(node);
	if (co) co->setTriggerTimeout(value);
}
void KInputMap::setInputAxisX(KNode *node, int value) {
	K__ASSERT(g_InputMap);
	auto co = g_InputMap->m_Nodes.get(node);
	if (co) co->setInputAxisX(value);
}
void KInputMap::setInputAxisY(KNode *node, int value) {
	K__ASSERT(g_InputMap);
	auto co = g_InputMap->m_Nodes.get(node);
	if (co) co->setInputAxisY(value);
}
void KInputMap::setInputAxisZ(KNode *node, int value) {
	K__ASSERT(g_InputMap);
	auto co = g_InputMap->m_Nodes.get(node);
	if (co) co->setInputAxisZ(value);
}
void KInputMap::setInputAxisAnalogX(KNode *node, float value) {
	K__ASSERT(g_InputMap);
	auto co = g_InputMap->m_Nodes.get(node);
	if (co) co->setInputAxisAnalogX(value);
}
void KInputMap::setInputAxisAnalogY(KNode *node, float value) {
	K__ASSERT(g_InputMap);
	auto co = g_InputMap->m_Nodes.get(node);
	if (co) co->setInputAxisAnalogY(value);
}
void KInputMap::setInputAxisAnalogZ(KNode *node, float value) {
	K__ASSERT(g_InputMap);
	auto co = g_InputMap->m_Nodes.get(node);
	if (co) co->setInputAxisAnalogZ(value);
}
int KInputMap::getInputAxisX(KNode *node) {
	K__ASSERT(g_InputMap);
	auto co = g_InputMap->m_Nodes.get(node);
	return co ? co->getInputAxisX() : 0;
}
int KInputMap::getInputAxisY(KNode *node) {
	K__ASSERT(g_InputMap);
	auto co = g_InputMap->m_Nodes.get(node);
	return co ? co->getInputAxisY() : 0;
}
int KInputMap::getInputAxisZ(KNode *node) {
	K__ASSERT(g_InputMap);
	auto co = g_InputMap->m_Nodes.get(node);
	return co ? co->getInputAxisZ() : 0;
}
float KInputMap::getInputAxisAnalogX(KNode *node) {
	K__ASSERT(g_InputMap);
	auto co = g_InputMap->m_Nodes.get(node);
	return co ? co->getInputAxisAnalogX() : 0.0f;
}
float KInputMap::getInputAxisAnalogY(KNode *node) {
	K__ASSERT(g_InputMap);
	auto co = g_InputMap->m_Nodes.get(node);
	return co ? co->getInputAxisAnalogY() : 0.0f;
}
float KInputMap::getInputAxisAnalogZ(KNode *node) {
	K__ASSERT(g_InputMap);
	auto co = g_InputMap->m_Nodes.get(node);
	return co ? co->getInputAxisAnalogZ() : 0.0f;
}
void KInputMap::setInputTrigger(KNode *node, const std::string &node_button) {
	K__ASSERT(g_InputMap);
	auto co = g_InputMap->m_Nodes.get(node);
	if (co) co->setInputTrigger(node_button);
}
void KInputMap::setInputBool(KNode *node, const std::string &node_button, bool pressed) {
	K__ASSERT(g_InputMap);
	auto co = g_InputMap->m_Nodes.get(node);
	if (co) co->setInputBool(node_button, pressed);
}
bool KInputMap::getInputBool(KNode *node, const std::string &node_button) {
	K__ASSERT(g_InputMap);
	auto co = g_InputMap->m_Nodes.get(node);
	if (co && co->getInputBool(node_button)) {
		return true;
	}
	return false;
}
bool KInputMap::getInputTrigger(KNode *node, const std::string &node_button) {
	K__ASSERT(g_InputMap);
	auto co = g_InputMap->m_Nodes.get(node);
	if (co && co->getInputTrigger(node_button)) {
		return true;
	}
	return false;
}
void KInputMap::beginReadTrigger(KNode *node) {
	K__ASSERT(g_InputMap);
	auto co = g_InputMap->m_Nodes.get(node);
	if (co) co->beginReadTrigger();
}
void KInputMap::endReadTrigger(KNode *node) {
	K__ASSERT(g_InputMap);
	auto co = g_InputMap->m_Nodes.get(node);
	if (co) co->endReadTrigger();
}
#pragma endregion // Per Node Input




#pragma endregion // KInputMapAct

} // namespace
