#pragma once

class CController {
	static float _signf(float x) {
		if (x < 0.0f) return -1.0f;
		if (x > 0.0f) return  1.0f;
		return 0.0f;
	}
	static float _min(float x, float y) {
		return (x < y) ? x : y;
	}
	static float _max(float x, float y) {
		return (x > y) ? x : y;
	}
public:
	CController() {
		m_AxisX = 0;
		m_AxisY = 0;
		m_AxisZ = 0;
		m_AxisReduce = 0.2f; // 1フレーム当たりの軸入力の消失値（正の値を指定）
		m_Peeked.clear();
		m_Buttons.clear();
		m_TriggerTimeout = 10;
		m_ReadTrigger = 0;
	}
	virtual void setTriggerTimeout(int val) {
		m_TriggerTimeout = val;
	}
	virtual void clearInputs() {
		m_AxisX = 0;
		m_AxisY = 0;
		m_AxisZ = 0;
		m_Buttons.clear();
		m_Peeked.clear();
	}

	// トリガー入力をリセットする
	// なお beginReadTrigger/endReadTrigger で読み取りモードにしている場合でも clearTrighgers は関係なく動作する
	virtual void clearTriggers() {
		for (auto it=m_Buttons.begin(); it!=m_Buttons.end(); /*++it*/) {
			if (it->second >= 0) {
				it = m_Buttons.erase(it);
			} else {
				it++;
			}
		}
		m_Peeked.clear();
	}
	virtual void setInputAxisX(int i) {
		m_AxisX = (float)i;
	}
	virtual void setInputAxisY(int i) {
		m_AxisY = (float)i;
	}
	virtual void setInputAxisZ(int i) {
		m_AxisZ = (float)i;
	}
	virtual void setInputAxisX_Analog(float value) {
		m_AxisX = value;
	}
	virtual void setInputAxisY_Analog(float value) {
		m_AxisY = value;
	}
	virtual void setInputAxisZ_Analog(float value) {
		m_AxisZ = value;
	}
	virtual int getInputAxisX() {
		return (int)_signf(m_AxisX); // returns -1, 0, 1
	}
	virtual int getInputAxisY() {
		return (int)_signf(m_AxisY); // returns -1, 0, 1
	}
	virtual int getInputAxisZ() {
		return (int)_signf(m_AxisZ); // returns -1, 0, 1
	}
	virtual float getInputAxisX_Analog() {
		return m_AxisX; // returns -1, 0, 1
	}
	virtual float getInputAxisZ_Analog() {
		return m_AxisZ; // returns -1, 0, 1
	}

	virtual void setInputTrigger(const std::string &button) {
		if (button.empty()) return;
		m_Buttons[button] = m_TriggerTimeout;
	}
	virtual void setInputBool(const std::string &button, bool pressed) {
		if (button.empty()) return;
		if (pressed) {
			m_Buttons[button] = -1;
		} else {
			m_Buttons.erase(button);
		}
	}
	virtual bool getInputBool(const std::string &button) {
		if (button.empty()) return false;
		return m_Buttons.find(button) != m_Buttons.end();
	}

	/// トリガーボタンの状態を得て、トリガー状態を false に戻す。
	/// トリガーをリセットしたくない場合は peekInputTrigger を使う。
	/// また、即座にリセットするのではなく特定の時点までトリガーのリセットを
	/// 先延ばしにしたい場合は beginReadTrigger() / endReadTrigger() を使う
	virtual bool getInputTrigger(const std::string &button) {
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
	virtual void beginReadTrigger() {
		m_ReadTrigger++;
	}

	/// トリガーの読み取りモードを終了する。
	/// beginReadTrigger が呼ばれた後に getInputTrigger されたトリガーをすべて false に戻す
	virtual void endReadTrigger() { 
		m_ReadTrigger--;
		if (m_ReadTrigger == 0) {
			for (auto it=m_Peeked.begin(); it!=m_Peeked.end(); ++it) {
				const std::string &btn = *it;
				m_Buttons.erase(btn);
			}
			m_Peeked.clear();
		}
	}
	virtual void tickInput() {
		// 入力状態を徐々に消失させる
		m_AxisX = reduce(m_AxisX, m_AxisReduce);
		m_AxisY = reduce(m_AxisY, m_AxisReduce);
		m_AxisZ = reduce(m_AxisZ, m_AxisReduce);

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

private:
	float reduce(float val, float delta) const {
		if (val > 0) {
			val = _max(val - delta, 0.0f);
		}
		if (val < 0) {
			val = _min(val + delta, 0.0f);
		}
		return val;
	}
	std::unordered_map<std::string, int> m_Buttons;
	std::unordered_set<std::string> m_Peeked;
	float m_AxisX;
	float m_AxisY;
	float m_AxisZ;
	float m_AxisReduce;
	int m_TriggerTimeout;
	int m_ReadTrigger;
};
