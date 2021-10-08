#include "KJoystick.h"

#include <Windows.h>
#include "KInternal.h"

namespace Kamilo {



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

static CWin32Joy g_Joy;

bool KJoystick::init() {
	return g_Joy.init();
}
void KJoystick::shutdown() {
	g_Joy.shutdown();
}
bool KJoystick::isInit() {
	return g_Joy.m_IsInit;
}
bool KJoystick::isConnected(int index) {
	return g_Joy.isConnected(index);
}
bool KJoystick::hasPov(int index) {
	return g_Joy.hasPov(index);
}
int KJoystick::getAxisCount(int index) {
	return g_Joy.getAxisCount(index);
}
int KJoystick::getButtonCount(int index) {
	return g_Joy.getButtonCount(index);
}
float KJoystick::getAxis(int index, int axis, float threshold) {
	return g_Joy.getAxis(index, axis, threshold);
}
bool KJoystick::getButton(int index, int btn) {
	return g_Joy.getButton(index, btn);
}
bool KJoystick::getPov(int index, int *x, int *y, int *deg) {
	return g_Joy.getPov(index, x, y, deg);
}
void KJoystick::poll(int index) {
	g_Joy.poll(index);
}

} // namespace
