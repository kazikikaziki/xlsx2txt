#include "KJoystick.h"

#include <Windows.h>
#include "KInternal.h"

namespace Kamilo {

// https://docs.microsoft.com/ja-jp/windows/win32/api/joystickapi/nf-joystickapi-joygetdevcaps
// Identifier of the joystick to be queried.
// Valid values for uJoyID range from -1 to 15.
// A value of -1 enables retrieval of the szRegKey member of the JOYCAPS structure
// whether a device is present or not.
#define MAXJOYSTICKS 4 // 本当は 16 (0..15) まで確認にできる

static int _JoyCheck() {
	int found = -1;
	JOYCAPSW caps;
	for (int i=0; i<MAXJOYSTICKS; i++) {
		ZeroMemory(&caps, sizeof(caps));
		bool ok = (joyGetDevCapsW(i, &caps, sizeof(caps)) == 0);
		K::print("Joystick check: ID(%d): %s", i, ok ? "Yes" : "No");
		if (ok && found < 0) {
			found = i;
		}
	}
	return found;
}

class CWin32CoreJoy: public KCoreJoystick {
	JOYCAPSW m_Caps;
	JOYINFOEX m_Info;
	DWORD m_LastCheckTime;
	bool m_IsConnected;
	int m_Id;
public:
	CWin32CoreJoy(int index) {
		// ジョイスティック番号の選択
		if (index < 0) {
			index = _JoyCheck();
		}

		ZeroMemory(&m_Caps, sizeof(m_Caps));
		ZeroMemory(&m_Info, sizeof(m_Info));
		m_LastCheckTime = 0;
		m_IsConnected = false;
		m_Id = index;
		if (m_Id >= 0) {
			K::print("Joystick: Select %d", m_Id);
		} else {
			K::print("Joystick: No joystick connected");
		}
		if (m_Id >= 0) {
			if (joyGetDevCapsW(m_Id, &m_Caps, sizeof(m_Caps)) == 0) {
				m_IsConnected = true;
			}
			K::print("Joystick: ID(%d): %s", m_Id, m_IsConnected ? "Connected" : "N/A");
		}
	}
	virtual bool isConnected() override {
		return m_IsConnected;
	}
	virtual bool hasPov() override {
		return (m_Caps.wCaps & JOYCAPS_HASPOV) != 0;
	}
	virtual int getAxisCount() override {
		return m_Caps.wNumAxes;
	}
	virtual int getButtonCount() override {
		return m_Caps.wNumButtons;
	}
	virtual float getAxis(int axis, float threshold) override {
		if (axis < 0) return 0.0f;
		if (axis >= (int)m_Caps.wNumAxes) return 0.0f;
		DWORD table[] = {
			m_Info.dwXpos, // axis0
			m_Info.dwYpos, // axis1
			m_Info.dwZpos, // axis2
			m_Info.dwRpos, // axis3
			m_Info.dwUpos, // axis4
			m_Info.dwVpos  // axis5
		};
		DWORD dwAxis = table[axis];
		// dwAxis は 0x0000 .. 0xFFFF の値を取る。
		// dwXpos なら 0 が一番左に倒したとき、0xFFFF が一番右に倒したときに該当する
		// ニュートラル状態はその中間だが、きっちり真ん中の値 0x7FFF になっている保証はない。
		float value = ((float)dwAxis / 0xFFFF - 0.5f) * 2.0f;
		if (fabsf(value) < threshold) return 0.0f; // 絶対値が一定未満であればニュートラル状態であるとみなす
		return value;
	}
	virtual bool getButton(int btn) override {
		if (btn < 0) return false;
		if (btn >= (int)m_Caps.wNumButtons) return false;
		DWORD mask = 1 << btn;
		return (m_Info.dwButtons & mask) != 0; // ボタンが押されているなら true
	}
	virtual bool getPov(int *x, int *y, int *deg) override {
		// ハットスイッチの有無をチェック
		if ((m_Caps.wCaps & JOYCAPS_HASPOV) == 0) {
			return false;
		}

		// 入力の有無をチェック
		// ハットスイッチの入力がない場合、dwPOV は JOY_POVCENTERED(=0xFFFF) なのだが、
		// JOY_POVCENTERED は WORD 値であることに注意。DWORD の場合でも 0xFFFF がセットされているとは限らない。
		// 環境によっては 0xFFFFFFFF になるとの情報もあるので、どちらでも対処できるようにしておく
		if ((WORD)m_Info.dwPOV == JOY_POVCENTERED) {
			return false;
		}

		// ハットスイッチの入力がある。その向きを調べる。
		if (x || y) {
			// x, y 成分での値が要求されている。
			// dwPOV は真上入力を角度 0 とし、0.01 度単位で時計回りに定義される。最大 35900。
			// ただ、ハットスイッチが無い場合は dwPOV は 0 になり、上入力と区別がつかないため
			// joyGetDevCapsW でハットスイッチの有無を事前にチェックしておくこと
			const int xmap[] = { 0, 1, 1, 1,  1, 1, 1, 0,  0,-1,-1,-1,  -1,-1,-1, 0};
			const int ymap[] = {-1,-1,-1, 0,  0, 1, 1, 1,  1, 1, 1, 0,   0,-1,-1,-1};
			int i = m_Info.dwPOV / (36000 / 16);
			if (x) *x = xmap[i];
			if (y) *y = ymap[i];
		}
		if (deg) {
			*deg = (int)(m_Info.dwPOV / 100.0f); // 時計回りに 0.01 度単位
		}
		return true;
	}
	virtual void poll() override {
		// Joystickが接続されていないと分かっているとき、なるべく Joystick の API を呼ばないようにする。
		// ゲーム起動中にスリープモードに入り、そこからレジュームしたときに API内部でメモリ違反が起きることがある。
		// スリープ中にゲームパッドを外してレジュームした場合もヤバいかもしれない。
		// この現象を再現するには、ゲーム起動中にノートＰＣをスリープさせればよい

		// 接続されているかどうかの確認には joyGetDevCapsW を使わないようにする。
		// 一度接続された状態になって joyGetDevCapsW が成功すると、たとえその後
		// パッドを外しても joyGetDevCapsW は成功し続ける（最初に接続成功したときの結果を返し続ける）
		// そのため、接続状態かどうかは joyGetPosEx がエラーになるかどうかで判断する

		if (m_IsConnected) {
			m_Info.dwSize = sizeof(m_Info);
			m_Info.dwFlags = JOY_RETURNALL;
			if (joyGetPosEx(m_Id, &m_Info) != 0) {
				// 取得に失敗した。パッドが切断されたと判断する
				K::print("Joystick[%d]: Disconnected", m_Id);
				m_IsConnected = false;
			}
		} else {
			// パッドが非接続状態。一定時間ごとに接続を確認する
			DWORD time = GetTickCount();
			if (m_LastCheckTime + 2000 <= time) { // 前回の確認から一定時間経過した？
				m_LastCheckTime = time;
				// 接続されているかどうかを joyGetPosEx の戻り値で確認する。
				// なお、joyGetDevCapsW は使わないこと。
				// この関数は一度でも成功すると以後キャッシュされた値を返し続け、失敗しなくなる
				m_Info.dwSize = sizeof(m_Info);
				m_Info.dwFlags = JOY_RETURNALL;
				if (joyGetPosEx(m_Id, &m_Info) == 0) {
					// 接続できた。
					// caps を取得する
					joyGetDevCapsW(m_Id, &m_Caps, sizeof(m_Caps));
					m_IsConnected = true;
					K::print("Joystick[%d]: Connected", m_Id);
				}
			}
		}
	}
}; // CWin32CoreJoy

KCoreJoystick * createJoystickWin32(int index) {
	// Win32API のジョイスティックは２個まで。
	// index < 0 の場合は自動で選ぶ
	if (index < 2) {
		return new CWin32CoreJoy(index);
	} else {
		return nullptr;
	}
}










class CWin32Joy {
	static constexpr int MAX_JOYS = 2;
	struct _JOY {
		JOYCAPSW caps;
		JOYINFOEX info;
		DWORD last_check;
		bool connected;
	};
	_JOY m_Joy[MAX_JOYS];

	static bool ID_CHECK(int n) {
		return 0 <= (n) && (n) < KJoystick::MAX_CONNECT;
	}
public:
	bool m_IsInit;

	CWin32Joy() {
		zero_clear();
	}
	void zero_clear() {
		m_IsInit = false;
		ZeroMemory(m_Joy, sizeof(m_Joy));
	}
	bool init() {
		zero_clear();
		for (int i=0; i<MAX_JOYS; i++) {
			init_stick(i);
		}
		m_IsInit = true;
		return true;
	}
	void shutdown() {
		zero_clear();
	}
	bool init_stick(int index) {
		if (!ID_CHECK(index)) {
			K__ERROR("Invalid joystick index!: %d", index);
			return false;
		}
		_JOY &joy = m_Joy[index];
		ZeroMemory(&joy, sizeof(joy));
		if (joyGetDevCapsW(JOYSTICKID1+index, &joy.caps, sizeof(joy.caps)) == 0) {
			joy.connected = true;
		}
		K::print("Joystick[%d]: %s", index, joy.connected ? "Connected" : "N/A");
		return true;
	}
	virtual bool isConnected(int index) {
		if (!ID_CHECK(index)) return false;
		const _JOY &joy = m_Joy[index];
		return joy.connected;
	}
	virtual bool hasPov(int index) {
		if (!ID_CHECK(index)) return false;
		const _JOY &joy = m_Joy[index];
		return (joy.caps.wCaps & JOYCAPS_HASPOV) != 0;
	}
	virtual int getAxisCount(int index) {
		if (!ID_CHECK(index)) return 0;
		const _JOY &joy = m_Joy[index];
		return joy.caps.wNumAxes;
	}
	virtual int getButtonCount(int index) {
		if (!ID_CHECK(index)) return 0;
		const _JOY &joy = m_Joy[index];
		return joy.caps.wNumButtons;
	}
	virtual float getAxis(int index, int axis, float threshold) {
		if (!ID_CHECK(index)) return 0;
		const _JOY &joy = m_Joy[index];
	
		if (axis < 0) return 0.0f;
		if (axis >= (int)joy.caps.wNumAxes) return 0.0f;
		DWORD table[] = {
			joy.info.dwXpos, // axis0
			joy.info.dwYpos, // axis1
			joy.info.dwZpos, // axis2
			joy.info.dwRpos, // axis3
			joy.info.dwUpos, // axis4
			joy.info.dwVpos  // axis5
		};
		DWORD dwAxis = table[axis];
		// dwAxis は 0x0000 .. 0xFFFF の値を取る。
		// dwXpos なら 0 が一番左に倒したとき、0xFFFF が一番右に倒したときに該当する
		// ニュートラル状態はその中間だが、きっちり真ん中の値 0x7FFF になっている保証はない。
		float value = ((float)dwAxis / 0xFFFF - 0.5f) * 2.0f;
		if (fabsf(value) < threshold) return 0.0f; // 絶対値が一定未満であればニュートラル状態であるとみなす
		return value;
	}
	virtual bool getButton(int index, int btn) {
		if (!ID_CHECK(index)) return false;
		const _JOY &joy = m_Joy[index];

		if (btn < 0) return false;
		if (btn >= (int)joy.caps.wNumButtons) return false;
		DWORD mask = 1 << btn;
		return (joy.info.dwButtons & mask) != 0; // ボタンが押されているなら true
	}
	virtual bool getPov(int index, int *x, int *y, int *deg) {
		if (!ID_CHECK(index)) return false;
		const _JOY &joy = m_Joy[index];

		// ハットスイッチの有無をチェック
		if ((joy.caps.wCaps & JOYCAPS_HASPOV) == 0) {
			return false;
		}

		// 入力の有無をチェック
		// ハットスイッチの入力がない場合、dwPOV は JOY_POVCENTERED(=0xFFFF) なのだが、
		// JOY_POVCENTERED は WORD 値であることに注意。DWORD の場合でも 0xFFFF がセットされているとは限らない。
		// 環境によっては 0xFFFFFFFF になるとの情報もあるので、どちらでも対処できるようにしておく
		if ((WORD)joy.info.dwPOV == JOY_POVCENTERED) {
			return false;
		}

		// ハットスイッチの入力がある。その向きを調べる。
		if (x || y) {
			// x, y 成分での値が要求されている。
			// dwPOV は真上入力を角度 0 とし、0.01 度単位で時計回りに定義される。最大 35900。
			// ただ、ハットスイッチが無い場合は dwPOV は 0 になり、上入力と区別がつかないため
			// joyGetDevCapsW でハットスイッチの有無を事前にチェックしておくこと
			const int xmap[] = { 0, 1, 1, 1,  1, 1, 1, 0,  0,-1,-1,-1,  -1,-1,-1, 0};
			const int ymap[] = {-1,-1,-1, 0,  0, 1, 1, 1,  1, 1, 1, 0,   0,-1,-1,-1};
			int i = joy.info.dwPOV / (36000 / 16);
			if (x) *x = xmap[i];
			if (y) *y = ymap[i];
		}
		if (deg) {
			*deg = (int)(joy.info.dwPOV / 100.0f); // 時計回りに 0.01 度単位
		}
		return true;
	}
	virtual void poll(int index) {
		if (!ID_CHECK(index)) return;
		// Joystickが接続されていないと分かっているとき、なるべく Joystick の API を呼ばないようにする。
		// ゲーム起動中にスリープモードに入り、そこからレジュームしたときに API内部でメモリ違反が起きることがある。
		// スリープ中にゲームパッドを外してレジュームした場合もヤバいかもしれない。
		// この現象を再現するには、ゲーム起動中にノートＰＣをスリープさせればよい

		// 接続されているかどうかの確認には joyGetDevCapsW を使わないようにする。
		// 一度接続された状態になって joyGetDevCapsW が成功すると、たとえその後
		// パッドを外しても joyGetDevCapsW は成功し続ける（最初に接続成功したときの結果を返し続ける）
		// そのため、接続状態かどうかは joyGetPosEx がエラーになるかどうかで判断する
		_JOY &joy = m_Joy[index];

		if (joy.connected) {
			joy.info.dwSize = sizeof(joy.info);
			joy.info.dwFlags = JOY_RETURNALL;
			if (joyGetPosEx(JOYSTICKID1+index, &joy.info) != 0) {
				// 取得に失敗した。パッドが切断されたと判断する
				K::print("GamePad[%d]: Disconnected", index);
				joy.connected = false;
			}
		} else {
			// パッドが非接続状態。一定時間ごとに接続を確認する
			DWORD time = GetTickCount();
			if (joy.last_check + 2000 <= time) { // 前回の確認から一定時間経過した？
				joy.last_check = time;
				// 接続されているかどうかを joyGetPosEx の戻り値で確認する。
				// なお、joyGetDevCapsW は使わないこと。
				// この関数は一度でも成功すると以後キャッシュされた値を返し続け、失敗しなくなる
				joy.info.dwSize = sizeof(joy.info);
				joy.info.dwFlags = JOY_RETURNALL;
				if (joyGetPosEx(JOYSTICKID1+index, &joy.info) == 0) {
					// 接続できた。
					// caps を取得する
					joyGetDevCapsW(JOYSTICKID1+index, &joy.caps, sizeof(joy.caps));
					joy.connected = true;
					K::print("GamePad[%d]: Connected", index);
				}
			}
		}
	}
}; // CWin32Joy

static KCoreJoystick *g_Joy = nullptr;

bool KJoystick::init() {
	if (g_Joy == nullptr) {
		g_Joy = Kamilo::createJoystickWin32(-1);
	}
	return g_Joy != nullptr;
}
void KJoystick::shutdown() {
	K__DROP(g_Joy);
}
bool KJoystick::isInit() {
	return g_Joy != nullptr;
}
bool KJoystick::isConnected(int index) {
	return g_Joy && g_Joy->isConnected();
}
bool KJoystick::hasPov(int index) {
	return g_Joy && g_Joy->hasPov();
}
int KJoystick::getAxisCount(int index) {
	return g_Joy ? g_Joy->getAxisCount() : 0;
}
int KJoystick::getButtonCount(int index) {
	return g_Joy ? g_Joy->getButtonCount() : 0;
}
float KJoystick::getAxis(int index, int axis, float threshold) {
	return g_Joy ? g_Joy->getAxis(axis, threshold) : 0.0f;
}
bool KJoystick::getButton(int index, int btn) {
	return g_Joy && g_Joy->getButton(btn);
}
bool KJoystick::getPov(int index, int *x, int *y, int *deg) {
	return g_Joy && g_Joy->getPov(x, y, deg);
}
void KJoystick::poll(int index) {
	if (g_Joy) g_Joy->poll();
}

} // namespace
