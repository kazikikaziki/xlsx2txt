#pragma once
#include "KRef.h"

namespace Kamilo {

/// キーボードのキー
enum KKey {
	KKey_NONE,
	KKey_A,
	KKey_B,
	KKey_C,
	KKey_D,
	KKey_E,
	KKey_F,
	KKey_G,
	KKey_H,
	KKey_I,
	KKey_J,
	KKey_K,
	KKey_L,
	KKey_M,
	KKey_N,
	KKey_O,
	KKey_P,
	KKey_Q,
	KKey_R,
	KKey_S,
	KKey_T,
	KKey_U,
	KKey_V,
	KKey_W,
	KKey_X,
	KKey_Y,
	KKey_Z,
	KKey_0,
	KKey_1,
	KKey_2,
	KKey_3,
	KKey_4,
	KKey_5,
	KKey_6,
	KKey_7,
	KKey_8,
	KKey_9,
	KKey_SPACE,
	KKey_AT,        // [@] (109 Japanese keyboard)
	KKey_HAT,       // [^] (109 Japanese keyboard)
	KKey_COMMA,     // [,] (109 Japanese keyboard)
	KKey_MINUS,     // [-] (109 Japanese keyboard)
	KKey_PERIOD,    // [.] (109 Japanese keyboard)
	KKey_SLASH,     // [/] (109 Japanese keyboard)
	KKey_COLON,     // [:] (109 Japanese keyboard)
	KKey_SEMICOLON, // [;] (109 Japanese keyboard)
	KKey_BRACKET_L, // [[] (109 Japanese keyboard)
	KKey_BRACKET_R, // []] (109 Japanese keyboard)
	KKey_BACKSLASH, // [\] (109 Japanese keyboard)
	KKey_NUMPAD_0,
	KKey_NUMPAD_1,
	KKey_NUMPAD_2,
	KKey_NUMPAD_3,
	KKey_NUMPAD_4,
	KKey_NUMPAD_5,
	KKey_NUMPAD_6,
	KKey_NUMPAD_7,
	KKey_NUMPAD_8,
	KKey_NUMPAD_9,
	KKey_NUMPAD_DOT,
	KKey_NUMPAD_ADD,
	KKey_NUMPAD_SUB,
	KKey_NUMPAD_MUL,
	KKey_NUMPAD_DIV,
	KKey_F1,
	KKey_F2,
	KKey_F3,
	KKey_F4,
	KKey_F5,
	KKey_F6,
	KKey_F7,
	KKey_F8,
	KKey_F9,
	KKey_F10,
	KKey_F11,
	KKey_F12,
	KKey_LEFT,
	KKey_RIGHT,
	KKey_UP,
	KKey_DOWN,
	KKey_PAGE_UP,
	KKey_PAGE_DOWN,
	KKey_HOME,
	KKey_END,
	KKey_BACKSPACE,
	KKey_TAB,
	KKey_ENTER,
	KKey_INSERT,
	KKey_DEL,
	KKey_ESCAPE,
	KKey_SNAPSHOT,
	KKey_SHIFT,
	KKey_SHIFT_L,
	KKey_SHIFT_R,
	KKey_CTRL,
	KKey_CTRL_L,
	KKey_CTRL_R,
	KKey_ALT,
	KKey_ALT_L,
	KKey_ALT_R,
	KKey_ENUM_MAX,
};
/// 修飾キー
enum KKeyModifier {
	KKeyModifier_NONE     = 0x00, ///< どの修飾キーも押されていない
	KKeyModifier_SHIFT    = 0x01, ///< Shift との同時押し
	KKeyModifier_CTRL     = 0x02, ///< Ctrl との同時押し
	KKeyModifier_ALT      = 0x04, ///< Alt との同時押し
	KKeyModifier_DONTCARE = 0x10, ///< 修飾キーをチェックしない
};
typedef int KKeyModifiers; ///< #Modifier の組み合わせ

class KCoreKeyboard: public KRef {
public:
	virtual bool isKeyDown(KKey key) = 0;
	virtual KKeyModifiers getModifiers() = 0;
	virtual bool matchModifiers(KKeyModifiers mods) = 0;
	virtual const char * getKeyName(KKey key) = 0;
	virtual int getVirtualKey(KKey key) = 0;
	virtual KKey findKeyByName(const char *name) = 0;
	virtual KKey findKeyByVirtualKey(int vKey) = 0;
};

KCoreKeyboard * createKeyboardWin32();




class KKeyboard {
public:
	static bool isKeyDown(KKey key);
	static KKeyModifiers getModifiers();
	static bool matchModifiers(KKeyModifiers mods);
	static const char * getKeyName(KKey key);
	static int getVirtualKey(KKey key);
	static KKey findKeyByName(const char *name);
	static KKey findKeyByVirtualKey(int vKey);
};

} // namespace
