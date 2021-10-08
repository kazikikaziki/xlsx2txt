#pragma once

namespace Kamilo {

class KKeyboard {
public:
	/// キーボードのキー
	enum Key {
		KEY_NONE,
		KEY_A,
		KEY_B,
		KEY_C,
		KEY_D,
		KEY_E,
		KEY_F,
		KEY_G,
		KEY_H,
		KEY_I,
		KEY_J,
		KEY_K,
		KEY_L,
		KEY_M,
		KEY_N,
		KEY_O,
		KEY_P,
		KEY_Q,
		KEY_R,
		KEY_S,
		KEY_T,
		KEY_U,
		KEY_V,
		KEY_W,
		KEY_X,
		KEY_Y,
		KEY_Z,
		KEY_0,
		KEY_1,
		KEY_2,
		KEY_3,
		KEY_4,
		KEY_5,
		KEY_6,
		KEY_7,
		KEY_8,
		KEY_9,
		KEY_SPACE,
		KEY_AT,        // [@] (109 Japanese keyboard)
		KEY_HAT,       // [^] (109 Japanese keyboard)
		KEY_COMMA,     // [,] (109 Japanese keyboard)
		KEY_MINUS,     // [-] (109 Japanese keyboard)
		KEY_PERIOD,    // [.] (109 Japanese keyboard)
		KEY_SLASH,     // [/] (109 Japanese keyboard)
		KEY_COLON,     // [:] (109 Japanese keyboard)
		KEY_SEMICOLON, // [;] (109 Japanese keyboard)
		KEY_BRACKET_L, // [[] (109 Japanese keyboard)
		KEY_BRACKET_R, // []] (109 Japanese keyboard)
		KEY_BACKSLASH, // [\] (109 Japanese keyboard)
		KEY_NUMPAD_0,
		KEY_NUMPAD_1,
		KEY_NUMPAD_2,
		KEY_NUMPAD_3,
		KEY_NUMPAD_4,
		KEY_NUMPAD_5,
		KEY_NUMPAD_6,
		KEY_NUMPAD_7,
		KEY_NUMPAD_8,
		KEY_NUMPAD_9,
		KEY_NUMPAD_DOT,
		KEY_NUMPAD_ADD,
		KEY_NUMPAD_SUB,
		KEY_NUMPAD_MUL,
		KEY_NUMPAD_DIV,
		KEY_F1,
		KEY_F2,
		KEY_F3,
		KEY_F4,
		KEY_F5,
		KEY_F6,
		KEY_F7,
		KEY_F8,
		KEY_F9,
		KEY_F10,
		KEY_F11,
		KEY_F12,
		KEY_LEFT,
		KEY_RIGHT,
		KEY_UP,
		KEY_DOWN,
		KEY_PAGE_UP,
		KEY_PAGE_DOWN,
		KEY_HOME,
		KEY_END,
		KEY_BACKSPACE,
		KEY_TAB,
		KEY_ENTER,
		KEY_INSERT,
		KEY_DEL,
		KEY_ESCAPE,
		KEY_SNAPSHOT,
		KEY_SHIFT,
		KEY_SHIFT_L,
		KEY_SHIFT_R,
		KEY_CTRL,
		KEY_CTRL_L,
		KEY_CTRL_R,
		KEY_ALT,
		KEY_ALT_L,
		KEY_ALT_R,
		KEY_ENUM_MAX,
	};
	/// 修飾キー
	enum Modifier {
		MODIF_NONE     = 0x00, ///< どの修飾キーも押されていない
		MODIF_SHIFT    = 0x01, ///< Shift との同時押し
		MODIF_CTRL     = 0x02, ///< Ctrl との同時押し
		MODIF_ALT      = 0x04, ///< Alt との同時押し
		MODIF_DONTCARE = 0x10, ///< 修飾キーをチェックしない
	};
	typedef int Modifiers; ///< #Modifier の組み合わせ

public:
	static bool isKeyDown(Key key);
	static Modifiers getModifiers();
	static bool matchModifiers(Modifiers mods);
	static const char * getKeyName(Key key);
	static int getVirtualKey(Key key);
	static Key findKeyByName(const char *name);
	static Key findKeyByVirtualKey(int vKey);
};

} // namespace
