#include "CController.h"
#include "KInternal.h"
#include "KMath.h"

using namespace Kamilo;

static float _Signf(float x) {
	if (x < 0.0f) return -1.0f;
	if (x > 0.0f) return  1.0f;
	return 0.0f;
}
static float _Min(float x, float y) {
	return (x < y) ? x : y;
}
static float _Max(float x, float y) {
	return (x > y) ? x : y;
}
static float _Reduce(float val, float delta) {
	if (val > 0) {
		val = _Max(val - delta, 0.0f);
	}
	if (val < 0) {
		val = _Min(val + delta, 0.0f);
	}
	return val;
}

CController::CController() {
	m_AxisX = 0;
	m_AxisY = 0;
	m_AxisZ = 0;
	m_AxisReduce = 0.2f; // 1フレーム当たりの軸入力の消失値（正の値を指定）
	m_Peeked.clear();
	m_Buttons.clear();
	m_TriggerTimeout = 10;
	m_ReadTrigger = 0;
}
void CController::setTriggerTimeout(int val) {
	m_TriggerTimeout = val;
}
void CController::clearInputs() {
	m_AxisX = 0;
	m_AxisY = 0;
	m_AxisZ = 0;
	m_Buttons.clear();
	m_Peeked.clear();
}

// トリガー入力をリセットする
// なお beginReadTrigger/endReadTrigger で読み取りモードにしている場合でも clearTrighgers は関係なく動作する
void CController::clearTriggers() {
	for (auto it=m_Buttons.begin(); it!=m_Buttons.end(); /*++it*/) {
		if (it->second >= 0) {
			it = m_Buttons.erase(it);
		} else {
			it++;
		}
	}
	m_Peeked.clear();
}
void CController::setInputAxisX(int i) {
	setInputAxisX_Analog((float)i);
}
void CController::setInputAxisY(int i) {
	setInputAxisY_Analog((float)i);
}
void CController::setInputAxisZ(int i) {
	setInputAxisZ_Analog((float)i);
}
void CController::setInputAxisX_Analog(float value) {
	K__ASSERT(fabsf(value) <= 1);
	m_AxisX = value;
}
void CController::setInputAxisY_Analog(float value) {
	K__ASSERT(fabsf(value) <= 1);
	m_AxisY = value;
}
void CController::setInputAxisZ_Analog(float value) {
	K__ASSERT(fabsf(value) <= 1);
	m_AxisZ = value;
}
int CController::getInputAxisX() {
	return (int)_Signf(m_AxisX); // returns -1, 0, 1
}
int CController::getInputAxisY() {
	return (int)_Signf(m_AxisY); // returns -1, 0, 1
}
int CController::getInputAxisZ() {
	return (int)_Signf(m_AxisZ); // returns -1, 0, 1
}
float CController::getInputAxisX_Analog() {
	return KMath::clampf(m_AxisX, -1.0f, 1.0f);
}
float CController::getInputAxisZ_Analog() {
	return KMath::clampf(m_AxisZ, -1.0f, 1.0f);
}

void CController::setInputTrigger(const std::string &button) {
	if (button.empty()) return;
	m_Buttons[button] = m_TriggerTimeout;
}
void CController::setInputBool(const std::string &button, bool pressed) {
	if (button.empty()) return;
	if (pressed) {
		m_Buttons[button] = -1;
	} else {
		m_Buttons.erase(button);
	}
}
bool CController::getInputBool(const std::string &button) {
	if (button.empty()) return false;
	return m_Buttons.find(button) != m_Buttons.end();
}

/// トリガーボタンの状態を得て、トリガー状態を false に戻す。
/// トリガーをリセットしたくない場合は peekInputTrigger を使う。
/// また、即座にリセットするのではなく特定の時点までトリガーのリセットを
/// 先延ばしにしたい場合は beginReadTrigger() / endReadTrigger() を使う
bool CController::getInputTrigger(const std::string &button) {
	if (button.empty()) return false;
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
		return true;
	}
	return false;
}
	
/// トリガーの読み取りモードを開始する
/// endReadTrigger が呼ばれるまでの間は getInputTrigger を呼んでもトリガー状態が false に戻らない
void CController::beginReadTrigger() {
	m_ReadTrigger++;
}

/// トリガーの読み取りモードを終了する。
/// beginReadTrigger が呼ばれた後に getInputTrigger されたトリガーをすべて false に戻す
void CController::endReadTrigger() { 
	m_ReadTrigger--;
	if (m_ReadTrigger == 0) {
		for (auto it=m_Peeked.begin(); it!=m_Peeked.end(); ++it) {
			const std::string &btn = *it;
			m_Buttons.erase(btn);
		}
		m_Peeked.clear();
	}
}
void CController::tickInput() {
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
}
