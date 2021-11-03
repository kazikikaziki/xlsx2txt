#include "KKeyboard.h"

#include <Windows.h>
#include "KInternal.h"

namespace Kamilo {


static const char * g_KeyNames[KKey_ENUM_MAX+1] = {
	nullptr,        // KKey_NONE
	"A",            // KKey_A
	"B",            // KKey_B
	"C",            // KKey_C
	"D",            // KKey_D
	"E",            // KKey_E
	"F",            // KKey_F
	"G",            // KKey_G
	"H",            // KKey_H
	"I",            // KKey_I
	"J",            // KKey_J
	"K",            // KKey_K
	"L",            // KKey_L
	"M",            // KKey_M
	"N",            // KKey_N
	"O",            // KKey_O
	"P",            // KKey_P
	"Q",            // KKey_Q
	"R",            // KKey_R
	"S",            // KKey_S
	"T",            // KKey_T
	"U",            // KKey_U
	"V",            // KKey_V
	"W",            // KKey_W
	"X",            // KKey_X
	"Y",            // KKey_Y
	"Z",            // KKey_Z
	"0",            // KKey_0
	"1",            // KKey_1
	"2",            // KKey_2
	"3",            // KKey_3
	"4",            // KKey_4
	"5",            // KKey_5
	"6",            // KKey_6
	"7",            // KKey_7
	"8",            // KKey_8
	"9",            // KKey_9
	"Space",        // KKey_SPACE
	"At",           // KKey_AT
	"Hat",          // KKey_HAT
	"Comma",        // KKey_COMMA
	"Minus",        // KKey_MINUS
	"Period",       // KKey_PERIOD
	"Slash",        // KKey_SLASH
	"Colon",        // KKey_COLON
	"Semicolon",    // KKey_SEMICOLON
	"LeftBracket",  // KKey_BRACKET_L
	"RightBracket", // KKey_BRACKET_R
	"Backslash",    // KKey_BACKSLASH
	"Numpad0",      // KKey_NUMPAD_0
	"Numpad1",      // KKey_NUMPAD_1
	"Numpad2",      // KKey_NUMPAD_2
	"Numpad3",      // KKey_NUMPAD_3
	"Numpad4",      // KKey_NUMPAD_4
	"Numpad5",      // KKey_NUMPAD_5
	"Numpad6",      // KKey_NUMPAD_6
	"Numpad7",      // KKey_NUMPAD_7
	"Numpad8",      // KKey_NUMPAD_8
	"Numpad9",      // KKey_NUMPAD_9
	"NumpadDot",    // KKey_NUMPAD_DOT
	"NumpadAdd",    // KKey_NUMPAD_ADD
	"NumpadSub",    // KKey_NUMPAD_SUB
	"NumpadMul",    // KKey_NUMPAD_MUL
	"NumpadDiv",    // KKey_NUMPAD_DIV
	"F1",           // KKey_F1
	"F2",           // KKey_F2
	"F3",           // KKey_F3
	"F4",           // KKey_F4
	"F5",           // KKey_F5
	"F6",           // KKey_F6
	"F7",           // KKey_F7
	"F8",           // KKey_F8
	"F9",           // KKey_F9
	"F10",          // KKey_F10
	"F11",          // KKey_F11
	"F12",          // KKey_F12
	"Left",         // KKey_LEFT
	"Right",        // KKey_RIGHT
	"Up",           // KKey_UP
	"Down",         // KKey_DOWN
	"PageUp",       // KKey_PAGE_UP
	"PageDown",     // KKey_PAGE_DOWN
	"Home",         // KKey_HOME
	"End",          // KKey_END
	"Back",         // KKey_BACKSPACE
	"Tab",          // KKey_TAB
	"Return",       // KKey_ENTER
	"Insert",       // KKey_INSERT
	"Delete",       // KKey_DEL
	"Escape",       // KKey_ESCAPE
	"PrintScreen",  // KKey_SNAPSHOT
	"Shift",        // KKey_SHIFT
	"LeftShift",    // KKey_SHIFT_L
	"RightShift",   // KKey_SHIFT_R
	"Ctrl",         // KKey_CTRL
	"LeftCtrl",     // KKey_CTRL_L
	"RightCtrl",    // KKey_CTRL_R
	"Alt",          // KKey_ALT
	"LeftAlt",      // KKey_ALT_L
	"RightAlt",     // KKey_ALT_R
	nullptr         // KKey_ENUM_MAX (Sentinel)
};
static int g_Win32KeyMap[KKey_ENUM_MAX+1] = {
	0,             // KKey_NONE
	'A',           // KKey_A
	'B',           // KKey_B
	'C',           // KKey_C
	'D',           // KKey_D
	'E',           // KKey_E
	'F',           // KKey_F
	'G',           // KKey_G
	'H',           // KKey_H
	'I',           // KKey_I
	'J',           // KKey_J
	'K',           // KKey_K
	'L',           // KKey_L
	'M',           // KKey_M
	'N',           // KKey_N
	'O',           // KKey_O
	'P',           // KKey_P
	'Q',           // KKey_Q
	'R',           // KKey_R
	'S',           // KKey_S
	'T',           // KKey_T
	'U',           // KKey_U
	'V',           // KKey_V
	'W',           // KKey_W
	'X',           // KKey_X
	'Y',           // KKey_Y
	'Z',           // KKey_Z
	'0',           // KKey_0
	'1',           // KKey_1
	'2',           // KKey_2
	'3',           // KKey_3
	'4',           // KKey_4
	'5',           // KKey_5
	'6',           // KKey_6
	'7',           // KKey_7
	'8',           // KKey_8
	'9',           // KKey_9
	' ',           // KKey_SPACE
	VK_OEM_3,      // KKey_AT        [@]
	VK_OEM_7,      // KKey_HAT       [^]
	VK_OEM_COMMA,  // KKey_COMMA     [.]
	VK_OEM_MINUS,  // KKey_MINUS     [-]
	VK_OEM_PERIOD, // KKey_PERIOD    [.]
	VK_OEM_2,      // KKey_SLASH     [/]
	VK_OEM_1,      // KKey_COLON     [:]
	VK_OEM_PLUS,   // KKey_SEMICOLON [;] ※名前に注意（日本語だと ; と + は同じキーにある） 
	VK_OEM_4,      // KKey_BRACKET_L [[]
	VK_OEM_6,      // KKey_BRACKET_R []]
	VK_OEM_5,      // KKey_BACKSLASH [\]
	VK_NUMPAD0,    // KKey_NUMPAD_0
	VK_NUMPAD1,    // KKey_NUMPAD_1
	VK_NUMPAD2,    // KKey_NUMPAD_2
	VK_NUMPAD3,    // KKey_NUMPAD_3
	VK_NUMPAD4,    // KKey_NUMPAD_4
	VK_NUMPAD5,    // KKey_NUMPAD_5
	VK_NUMPAD6,    // KKey_NUMPAD_6
	VK_NUMPAD7,    // KKey_NUMPAD_7
	VK_NUMPAD8,    // KKey_NUMPAD_8
	VK_NUMPAD9,    // KKey_NUMPAD_9
	VK_DECIMAL,    // KKey_NUMPAD_DOT
	VK_ADD,        // KKey_NUMPAD_ADD
	VK_SUBTRACT,   // KKey_NUMPAD_SUB
	VK_MULTIPLY,   // KKey_NUMPAD_MUL
	VK_DIVIDE,     // KKey_NUMPAD_DIV
	VK_F1,         // KKey_F1
	VK_F2,         // KKey_F2
	VK_F3,         // KKey_F3
	VK_F4,         // KKey_F4
	VK_F5,         // KKey_F5
	VK_F6,         // KKey_F6
	VK_F7,         // KKey_F7
	VK_F8,         // KKey_F8
	VK_F9,         // KKey_F9
	VK_F10,        // KKey_F10
	VK_F11,        // KKey_F11
	VK_F12,        // KKey_F12
	VK_LEFT,       // KKey_LEFT
	VK_RIGHT,      // KKey_RIGHT
	VK_UP,         // KKey_UP
	VK_DOWN,       // KKey_DOWN
	VK_PRIOR,      // KKey_PAGE_UP
	VK_NEXT,       // KKey_PAGE_DOWN
	VK_HOME,       // KKey_HOME
	VK_END,        // KKey_END
	VK_BACK,       // KKey_BACKSPACE
	VK_TAB,        // KKey_TAB
	VK_RETURN,     // KKey_ENTER
	VK_INSERT,     // KKey_INSERT
	VK_DELETE,     // KKey_DEL
	VK_ESCAPE,     // KKey_ESCAPE
	VK_SNAPSHOT,   // KKey_SNAPSHOT
	VK_SHIFT,      // KKey_SHIFT
	VK_LSHIFT,     // KKey_SHIFT_L
	VK_RSHIFT,     // KKey_SHIFT_R
	VK_CONTROL,    // KKey_CTRL
	VK_LCONTROL,   // KKey_CTRL_L
	VK_RCONTROL,   // KKey_CTRL_R
	VK_MENU,       // KKey_ALT
	VK_LMENU,      // KKey_ALT_L
	VK_RMENU,      // KKey_ALT_R
	0 // KKey_ENUM_MAX (Sentinel)
};




class CWin32CoreKB: public KCoreKeyboard {
public:
	CWin32CoreKB() {
	}
	virtual bool isKeyDown(KKey key) override {
		int vKey = getVirtualKey(key);
		SHORT stat = GetAsyncKeyState(vKey);
		return (stat & 0x8000) != 0;
	}
	virtual KKeyModifiers getModifiers() override {
		KKeyModifiers flags = 0;
		if (GetAsyncKeyState(VK_SHIFT  ) & 0x8000) flags |= KKeyModifier_SHIFT;
		if (GetAsyncKeyState(VK_CONTROL) & 0x8000) flags |= KKeyModifier_CTRL;
		if (GetAsyncKeyState(VK_MENU   ) & 0x8000) flags |= KKeyModifier_ALT;
		return flags;
	}
	virtual bool matchModifiers(KKeyModifiers mods) override {
		if (mods & KKeyModifier_DONTCARE) {
			return true; // 常にマッチする
		}
		if (getModifiers() == mods) {
			return true; // 完全一致の場合のみOK
		}
		return false;
	}
	virtual const char * getKeyName(KKey key) override {
		if (0 <= key && key < KKey_ENUM_MAX) {
			return g_KeyNames[key];
		}
		return nullptr;
	}
	virtual int getVirtualKey(KKey key) override {
		if (0 <= key && key < KKey_ENUM_MAX) {
			return g_Win32KeyMap[key];
		}
		return 0;
	}
	virtual KKey findKeyByName(const char *name) override {
		K__ASSERT_RETURN_VAL(name, KKey_NONE);
		for (size_t i=0; i<KKey_ENUM_MAX; i++) {
			const char *n = g_KeyNames[i];
			if (n && _stricmp(n, name) == 0) {
				return (KKey)i;
			}
		}
		return KKey_NONE;
	}
	virtual KKey findKeyByVirtualKey(int vKey) override {
		for (size_t i=0; i<KKey_ENUM_MAX; i++) {
			if (g_Win32KeyMap[i] == vKey) {
				return (KKey)i;
			}
		}
		return KKey_NONE;
	}
};

KCoreKeyboard * createKeyboardWin32() {
	return new CWin32CoreKB();
}




bool KKeyboard::isKeyDown(KKey key) {
	int vKey = getVirtualKey(key);
	SHORT stat = GetAsyncKeyState(vKey);
	return (stat & 0x8000) != 0;
}
const char * KKeyboard::getKeyName(KKey key) {
	if (0 <= key && key < KKey_ENUM_MAX) {
		return g_KeyNames[key];
	}
	return nullptr;
}
int KKeyboard::getVirtualKey(KKey key) {
	if (0 <= key && key < KKey_ENUM_MAX) {
		return g_Win32KeyMap[key];
	}
	return 0;
}
KKey KKeyboard::findKeyByName(const char *name) {
	K__ASSERT_RETURN_VAL(name, KKey_NONE);
	for (size_t i=0; i<KKey_ENUM_MAX; i++) {
		const char *n = g_KeyNames[i];
		if (n && _stricmp(n, name) == 0) {
			return (KKey)i;
		}
	}
	return KKey_NONE;
}
KKey KKeyboard::findKeyByVirtualKey(int vKey) {
	for (size_t i=0; i<KKey_ENUM_MAX; i++) {
		if (g_Win32KeyMap[i] == vKey) {
			return (KKey)i;
		}
	}
	return KKey_NONE;
}
KKeyModifiers KKeyboard::getModifiers() {
	KKeyModifiers flags = 0;
	if (GetAsyncKeyState(VK_SHIFT  ) & 0x8000) flags |= KKeyModifier_SHIFT;
	if (GetAsyncKeyState(VK_CONTROL) & 0x8000) flags |= KKeyModifier_CTRL;
	if (GetAsyncKeyState(VK_MENU   ) & 0x8000) flags |= KKeyModifier_ALT;
	return flags;
}
bool KKeyboard::matchModifiers(KKeyModifiers mods) {
	if (mods & KKeyModifier_DONTCARE) {
		return true; // 常にマッチする
	}
	if (getModifiers() == mods) {
		return true; // 完全一致の場合のみOK
	}
	return false;
}


} // namespace
