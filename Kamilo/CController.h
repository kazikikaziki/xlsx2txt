#pragma once
#include <unordered_map>
#include <unordered_set>
#include <string>
#include "KInternal.h"

class CController {
public:
	CController();
	virtual void setTriggerTimeout(int val);
	virtual void clearInputs();

	// トリガー入力をリセットする
	// なお beginReadTrigger/endReadTrigger で読み取りモードにしている場合でも clearTrighgers は関係なく動作する
	virtual void clearTriggers();
	virtual void setInputAxisX(int i);
	virtual void setInputAxisY(int i);
	virtual void setInputAxisZ(int i);
	virtual void setInputAxisX_Analog(float value);
	virtual void setInputAxisY_Analog(float value);
	virtual void setInputAxisZ_Analog(float value);
	virtual int getInputAxisX();
	virtual int getInputAxisY();
	virtual int getInputAxisZ();
	virtual float getInputAxisX_Analog();
	virtual float getInputAxisZ_Analog();
	virtual void setInputTrigger(const std::string &button);
	virtual void setInputBool(const std::string &button, bool pressed);
	virtual bool getInputBool(const std::string &button);

	/// トリガーボタンの状態を得て、トリガー状態を false に戻す。
	/// トリガーをリセットしたくない場合は peekInputTrigger を使う。
	/// また、即座にリセットするのではなく特定の時点までトリガーのリセットを
	/// 先延ばしにしたい場合は beginReadTrigger() / endReadTrigger() を使う
	virtual bool getInputTrigger(const std::string &button);
	
	/// トリガーの読み取りモードを開始する
	/// endReadTrigger が呼ばれるまでの間は getInputTrigger を呼んでもトリガー状態が false に戻らない
	virtual void beginReadTrigger();

	/// トリガーの読み取りモードを終了する。
	/// beginReadTrigger が呼ばれた後に getInputTrigger されたトリガーをすべて false に戻す
	virtual void endReadTrigger();
	virtual void tickInput();

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
