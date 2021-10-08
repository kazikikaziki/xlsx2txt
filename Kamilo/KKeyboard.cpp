#include "KKeyboard.h"

#include <Windows.h>
#include "KInternal.h"

namespace Kamilo {


static const char * g_KeyNames[KKeyboard::KEY_ENUM_MAX+1] = {
	nullptr,        // KEY_NONE
	"A",            // KEY_A
	"B",            // KEY_B
	"C",            // KEY_C
	"D",            // KEY_D
	"E",            // KEY_E
	"F",            // KEY_F
	"G",            // KEY_G
	"H",            // KEY_H
	"I",            // KEY_I
	"J",            // KEY_J
	"K",            // KEY_K
	"L",            // KEY_L
	"M",            // KEY_M
	"N",            // KEY_N
	"O",            // KEY_O
	"P",            // KEY_P
	"Q",            // KEY_Q
	"R",            // KEY_R
	"S",            // KEY_S
	"T",            // KEY_T
	"U",            // KEY_U
	"V",            // KEY_V
	"W",            // KEY_W
	"X",            // KEY_X
	"Y",            // KEY_Y
	"Z",            // KEY_Z
	"0",            // KEY_0
	"1",            // KEY_1
	"2",            // KEY_2
	"3",            // KEY_3
	"4",            // KEY_4
	"5",            // KEY_5
	"6",            // KEY_6
	"7",            // KEY_7
	"8",            // KEY_8
	"9",            // KEY_9
	"Space",        // KEY_SPACE
	"At",           // KEY_AT
	"Hat",          // KEY_HAT
	"Comma",        // KEY_COMMA
	"Minus",        // KEY_MINUS
	"Period",       // KEY_PERIOD
	"Slash",        // KEY_SLASH
	"Colon",        // KEY_COLON
	"Semicolon",    // KEY_SEMICOLON
	"LeftBracket",  // KEY_BRACKET_L
	"RightBracket", // KEY_BRACKET_R
	"Backslash",    // KEY_BACKSLASH
	"Numpad0",      // KEY_NUMPAD_0
	"Numpad1",      // KEY_NUMPAD_1
	"Numpad2",      // KEY_NUMPAD_2
	"Numpad3",      // KEY_NUMPAD_3
	"Numpad4",      // KEY_NUMPAD_4
	"Numpad5",      // KEY_NUMPAD_5
	"Numpad6",      // KEY_NUMPAD_6
	"Numpad7",      // KEY_NUMPAD_7
	"Numpad8",      // KEY_NUMPAD_8
	"Numpad9",      // KEY_NUMPAD_9
	"NumpadDot",    // KEY_NUMPAD_DOT
	"NumpadAdd",    // KEY_NUMPAD_ADD
	"NumpadSub",    // KEY_NUMPAD_SUB
	"NumpadMul",    // KEY_NUMPAD_MUL
	"NumpadDiv",    // KEY_NUMPAD_DIV
	"F1",           // KEY_F1
	"F2",           // KEY_F2
	"F3",           // KEY_F3
	"F4",           // KEY_F4
	"F5",           // KEY_F5
	"F6",           // KEY_F6
	"F7",           // KEY_F7
	"F8",           // KEY_F8
	"F9",           // KEY_F9
	"F10",          // KEY_F10
	"F11",          // KEY_F11
	"F12",          // KEY_F12
	"Left",         // KEY_LEFT
	"Right",        // KEY_RIGHT
	"Up",           // KEY_UP
	"Down",         // KEY_DOWN
	"PageUp",       // KEY_PAGE_UP
	"PageDown",     // KEY_PAGE_DOWN
	"Home",         // KEY_HOME
	"End",          // KEY_END
	"Back",         // KEY_BACKSPACE
	"Tab",          // KEY_TAB
	"Return",       // KEY_ENTER
	"Insert",       // KEY_INSERT
	"Delete",       // KEY_DEL
	"Escape",       // KEY_ESCAPE
	"PrintScreen",  // KEY_SNAPSHOT
	"Shift",        // KEY_SHIFT
	"LeftShift",    // KEY_SHIFT_L
	"RightShift",   // KEY_SHIFT_R
	"Ctrl",         // KEY_CTRL
	"LeftCtrl",     // KEY_CTRL_L
	"RightCtrl",    // KEY_CTRL_R
	"Alt",          // KEY_ALT
	"LeftAlt",      // KEY_ALT_L
	"RightAlt",     // KEY_ALT_R
	nullptr         // KEY_ENUM_MAX (Sentinel)
};
static int g_Win32KeyMap[KKeyboard::KEY_ENUM_MAX+1] = {
	0,             // KEY_NONE
	'A',           // KEY_A
	'B',           // KEY_B
	'C',           // KEY_C
	'D',           // KEY_D
	'E',           // KEY_E
	'F',           // KEY_F
	'G',           // KEY_G
	'H',           // KEY_H
	'I',           // KEY_I
	'J',           // KEY_J
	'K',           // KEY_K
	'L',           // KEY_L
	'M',           // KEY_M
	'N',           // KEY_N
	'O',           // KEY_O
	'P',           // KEY_P
	'Q',           // KEY_Q
	'R',           // KEY_R
	'S',           // KEY_S
	'T',           // KEY_T
	'U',           // KEY_U
	'V',           // KEY_V
	'W',           // KEY_W
	'X',           // KEY_X
	'Y',           // KEY_Y
	'Z',           // KEY_Z
	'0',           // KEY_0
	'1',           // KEY_1
	'2',           // KEY_2
	'3',           // KEY_3
	'4',           // KEY_4
	'5',           // KEY_5
	'6',           // KEY_6
	'7',           // KEY_7
	'8',           // KEY_8
	'9',           // KEY_9
	' ',           // KEY_SPACE
	VK_OEM_3,      // KEY_AT        [@]
	VK_OEM_7,      // KEY_HAT       [^]
	VK_OEM_COMMA,  // KEY_COMMA     [.]
	VK_OEM_MINUS,  // KEY_MINUS     [-]
	VK_OEM_PERIOD, // KEY_PERIOD    [.]
	VK_OEM_2,      // KEY_SLASH     [/]
	VK_OEM_1,      // KEY_COLON     [:]
	VK_OEM_PLUS,   // KEY_SEMICOLON [;] ※名前に注意（日本語だと ; と + は同じキーにある） 
	VK_OEM_4,      // KEY_BRACKET_L [[]
	VK_OEM_6,      // KEY_BRACKET_R []]
	VK_OEM_5,      // KEY_BACKSLASH [\]
	VK_NUMPAD0,    // KEY_NUMPAD_0
	VK_NUMPAD1,    // KEY_NUMPAD_1
	VK_NUMPAD2,    // KEY_NUMPAD_2
	VK_NUMPAD3,    // KEY_NUMPAD_3
	VK_NUMPAD4,    // KEY_NUMPAD_4
	VK_NUMPAD5,    // KEY_NUMPAD_5
	VK_NUMPAD6,    // KEY_NUMPAD_6
	VK_NUMPAD7,    // KEY_NUMPAD_7
	VK_NUMPAD8,    // KEY_NUMPAD_8
	VK_NUMPAD9,    // KEY_NUMPAD_9
	VK_DECIMAL,    // KEY_NUMPAD_DOT
	VK_ADD,        // KEY_NUMPAD_ADD
	VK_SUBTRACT,   // KEY_NUMPAD_SUB
	VK_MULTIPLY,   // KEY_NUMPAD_MUL
	VK_DIVIDE,     // KEY_NUMPAD_DIV
	VK_F1,         // KEY_F1
	VK_F2,         // KEY_F2
	VK_F3,         // KEY_F3
	VK_F4,         // KEY_F4
	VK_F5,         // KEY_F5
	VK_F6,         // KEY_F6
	VK_F7,         // KEY_F7
	VK_F8,         // KEY_F8
	VK_F9,         // KEY_F9
	VK_F10,        // KEY_F10
	VK_F11,        // KEY_F11
	VK_F12,        // KEY_F12
	VK_LEFT,       // KEY_LEFT
	VK_RIGHT,      // KEY_RIGHT
	VK_UP,         // KEY_UP
	VK_DOWN,       // KEY_DOWN
	VK_PRIOR,      // KEY_PAGE_UP
	VK_NEXT,       // KEY_PAGE_DOWN
	VK_HOME,       // KEY_HOME
	VK_END,        // KEY_END
	VK_BACK,       // KEY_BACKSPACE
	VK_TAB,        // KEY_TAB
	VK_RETURN,     // KEY_ENTER
	VK_INSERT,     // KEY_INSERT
	VK_DELETE,     // KEY_DEL
	VK_ESCAPE,     // KEY_ESCAPE
	VK_SNAPSHOT,   // KEY_SNAPSHOT
	VK_SHIFT,      // KEY_SHIFT
	VK_LSHIFT,     // KEY_SHIFT_L
	VK_RSHIFT,     // KEY_SHIFT_R
	VK_CONTROL,    // KEY_CTRL
	VK_LCONTROL,   // KEY_CTRL_L
	VK_RCONTROL,   // KEY_CTRL_R
	VK_MENU,       // KEY_ALT
	VK_LMENU,      // KEY_ALT_L
	VK_RMENU,      // KEY_ALT_R
	0 // KEY_ENUM_MAX (Sentinel)
};

bool KKeyboard::isKeyDown(Key key) {
	int vKey = getVirtualKey(key);
	SHORT stat = GetAsyncKeyState(vKey);
	return (stat & 0x8000) != 0;
}
const char * KKeyboard::getKeyName(Key key) {
	if (0 <= key && key < KEY_ENUM_MAX) {
		return g_KeyNames[key];
	}
	return nullptr;
}
int KKeyboard::getVirtualKey(Key key) {
	if (0 <= key && key < KEY_ENUM_MAX) {
		return g_Win32KeyMap[key];
	}
	return 0;
}
KKeyboard::Key KKeyboard::findKeyByName(const char *name) {
	K__ASSERT_RETURN_VAL(name, KEY_NONE);
	for (size_t i=0; i<KEY_ENUM_MAX; i++) {
		const char *n = g_KeyNames[i];
		if (n && _stricmp(n, name) == 0) {
			return (Key)i;
		}
	}
	return KEY_NONE;
}
KKeyboard::Key KKeyboard::findKeyByVirtualKey(int vKey) {
	for (size_t i=0; i<KEY_ENUM_MAX; i++) {
		if (g_Win32KeyMap[i] == vKey) {
			return (Key)i;
		}
	}
	return KEY_NONE;
}
KKeyboard::Modifiers KKeyboard::getModifiers() {
	Modifiers flags = 0;
	if (GetAsyncKeyState(VK_SHIFT  ) & 0x8000) flags |= MODIF_SHIFT;
	if (GetAsyncKeyState(VK_CONTROL) & 0x8000) flags |= MODIF_CTRL;
	if (GetAsyncKeyState(VK_MENU   ) & 0x8000) flags |= MODIF_ALT;
	return flags;
}
bool KKeyboard::matchModifiers(Modifiers mods) {
	if (mods & MODIF_DONTCARE) {
		return true; // 常にマッチする
	}
	if (getModifiers() == mods) {
		return true; // 完全一致の場合のみOK
	}
	return false;
}


} // namespace
