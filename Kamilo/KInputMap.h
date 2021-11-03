/// @file
/// Copyright (c) 2020 Heliodor
/// This software is released under the MIT License.
/// http://opensource.org/licenses/mit-license.php

#pragma once
#include "KRef.h"
#include "KJoystick.h"
#include "KMouse.h"
#include "KKeyboard.h"

namespace Kamilo {

class KEngine;
class KNode;

enum PollFlag {
	POLLFLAG_NO_KEYBOARD = 1,
	POLLFLAG_NO_JOYSTICK = 2,
	POLLFLAG_NO_MOUSE    = 4,
	POLLFLAG_FORCE_PUSH  = 8,
};
typedef int KPollFlags;




#pragma region // Buttons
class IKeyElm: public KRef {
public:
	IKeyElm() {}
	virtual ~IKeyElm() {}
	virtual bool isPressed(float *val, KPollFlags flags) const = 0;
	virtual bool isConflictWith(const IKeyElm *k) const = 0;
	bool hasTag(const std::string &tag) const { return m_Tag == tag; }
	void setTag(const std::string &tag) { m_Tag = tag; }

private:
	std::string m_Tag;
};

class IKeyboardKeyElm: public IKeyElm {
public:
	virtual void set_key(KKey key) = 0;
	virtual KKey get_key() const = 0;
	virtual KKeyModifiers get_modifiers() const = 0;
	virtual const char * get_key_name() const = 0;
};

class IJoystickKeyElm: public IKeyElm {
public:
	virtual void set_button(KJoystickButton key) = 0;
	virtual KJoystickButton get_button() const = 0;
};

enum KButtonFlag {
	KButtonFlag_REPEAT = 1, // オートリピートあり
};
typedef int KButtonFlags;



#pragma endregion // Buttons

class KInputCallback {
public:
	virtual void on_input_tick(KNode *node) {}
	virtual void on_input_button(KNode *node, const std::string &btn, float *p_value) {}
	virtual void on_input_axisX(KNode *node, float *p_value) {}
	virtual void on_input_axisY(KNode *node, float *p_value) {}
	virtual void on_input_axisZ(KNode *node, float *p_value) {}
};


class KInputMap {
public:
	static void install(KCoreKeyboard *kb, KCoreMouse *ms, KCoreJoystick *js);
	static void uninstall();

	// システムボタン（ポーズ中でも使える）
	static void addAppButton(const std::string &button, KButtonFlags flags=0);
	static bool isAppButtonDown(const std::string &button);
	static bool getAppButtonDown(const std::string &button);

	// ゲームボタン（ポーズ中には使えない）
	static void addGameButton(const std::string &button, KButtonFlags flags=0);
	static bool isGameButtonDown(const std::string &button);
	static bool getGameButtonDown(const std::string &button);
	static float getGameButtonAnalog(const std::string &button);

	// ボタン（システム、ゲーム統合）
	static bool isButtonDown(const std::string &button);
	static bool getButtonDown(const std::string &button);
	static void postButtonDown(const std::string &button);
	static float getButtonAnalog(const std::string &button);

	// 互換性
	static void addButton(const std::string &button, KButtonFlags flags=0) {
		addGameButton(button, flags);
	}

	// キーボードやゲームパッドにボタンを関連付ける
	static void bindAppKey(const std::string &button, KKey key, KKeyModifiers mods=KKeyModifier_DONTCARE);
	static void bindKeyboardKey(const std::string &button, KKey key, KKeyModifiers mods=KKeyModifier_DONTCARE, const std::string &tag="");
	static void bindJoystickKey(const std::string &button, KJoystickButton joybtn, const std::string &tag="");
	static void bindJoystickAxis(const std::string &button, KJoystickAxis axis, int halfrange, const std::string &tag="", float threshold=0.2f);
	static void bindJoystickPov(const std::string &button, int xsign, int ysign, const std::string &tag="");
	static void bindMouseKey(const std::string &button, KMouseButton mousebtn, const std::string &tag="");
	static void bindKeySequence(const std::string &button, const char *keys[], const std::string &tag="");
	static void unbindByTag(const std::string &button, const std::string &tag);
	static int isConflict(const std::string &button1, const std::string &button2);
	static void resetAllButtonStates();
	static IKeyboardKeyElm * findKeyboardByTag(const std::string &button, const std::string &tag);
	static IJoystickKeyElm * findJoystickByTag(const std::string &button, const std::string &tag);
	static std::string getJoystickName(KJoystickButton joybtn);
	static std::string getKeyboardName(KKey key);
	static bool getKeyboardFromName(const std::string &s, KKey *key);
	static bool getJoystickFromName(const std::string &s, KJoystickButton *btn);
	static void setPollFlags(KPollFlags flags);

	////////////////////////////////////////////////////////////////////////////////

	// ノードごとに独立した入力
	static void attach(KNode *node);
	static bool isAttached(KNode *node);
	static void setNodeInputCallback(KNode *node, KInputCallback *cb);

	// トリガー入力をリセットする
	// なお beginReadTrigger/endReadTrigger で読み取りモードにしている場合でも clearTrighgers は関係なく動作する
	static void clearTriggers(KNode *node);
	static void clearInputs(KNode *node);
	static void setTriggerTimeout(KNode *node, int value);

	static void setInputAxisX(KNode *node, int value);
	static void setInputAxisY(KNode *node, int value);
	static void setInputAxisZ(KNode *node, int value);
	static void setInputAxisAnalogX(KNode *node, float value);
	static void setInputAxisAnalogY(KNode *node, float value);
	static void setInputAxisAnalogZ(KNode *node, float value);
	static int getInputAxisX(KNode *node);
	static int getInputAxisY(KNode *node);
	static int getInputAxisZ(KNode *node);
	static float getInputAxisAnalogX(KNode *node);
	static float getInputAxisAnalogY(KNode *node);
	static float getInputAxisAnalogZ(KNode *node);
	static void setInputTrigger(KNode *node, const std::string &node_button);
	static void setInputBool(KNode *node, const std::string &node_button, bool pressed);
	static bool getInputBool(KNode *node, const std::string &node_button);

	/// トリガーボタンの状態を得て、トリガー状態を false に戻す。
	/// トリガーをリセットしたくない場合は peekInputTrigger を使う。
	/// また、即座にリセットするのではなく特定の時点までトリガーのリセットを
	/// 先延ばしにしたい場合は beginReadTrigger() / endReadTrigger() を使う
	static bool getInputTrigger(KNode *node, const std::string &node_button);
	
	/// トリガーの読み取りモードを開始する
	/// endReadTrigger が呼ばれるまでの間は getInputTrigger を呼んでもトリガー状態が false に戻らない
	static void beginReadTrigger(KNode *node);

	/// トリガーの読み取りモードを終了する。
	/// beginReadTrigger が呼ばれた後に getInputTrigger されたトリガーをすべて false に戻す
	static void endReadTrigger(KNode *node);


};


} // namespace

