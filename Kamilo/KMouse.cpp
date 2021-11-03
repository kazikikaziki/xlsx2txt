#include "KMouse.h"

#include <Windows.h>

namespace Kamilo {


class CWin32Mouse: public KCoreMouse {
public:
	CWin32Mouse() {
	}
	virtual int getX() override {
		POINT p = {0, 0};
		GetCursorPos(&p);
		return p.x;
	}
	virtual int getY() override {
		POINT p = {0, 0};
		GetCursorPos(&p);
		return p.y;
	}
	virtual bool isButtonDown(KMouseButton btn) override {
		switch (btn) {
		case KMouseButton_LEFT:   return (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
		case KMouseButton_RIGHT:  return (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
		case KMouseButton_MIDDLE: return (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
		}
		return false;
	}
};


KCoreMouse * createMouseWin32() {
	return new CWin32Mouse();
}

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
