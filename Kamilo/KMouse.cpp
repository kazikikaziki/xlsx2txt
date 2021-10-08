#include "KMouse.h"

#include <Windows.h>

namespace Kamilo {

int KMouse::getGlobalX() {
	POINT p = {0, 0};
	GetCursorPos(&p);
	return p.x;
}
int KMouse::getGlobalY() {
	POINT p = {0, 0};
	GetCursorPos(&p);
	return p.y;
}
bool KMouse::isButtonDown(int btn) {
	switch (btn) {
	case 0: return (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
	case 1: return (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
	case 2: return (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
	}
	return false;
}

} // namespace
