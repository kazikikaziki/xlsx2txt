#pragma once
#include "KRef.h"

namespace Kamilo {

enum KMouseButton {
	KMouseButton_NONE = -1,
	KMouseButton_LEFT,
	KMouseButton_RIGHT,
	KMouseButton_MIDDLE,
	KMouseButton_ENUM_MAX
};

class KCoreMouse: public KRef {
public:
	virtual int getX() = 0;
	virtual int getY() = 0;
	virtual bool isButtonDown(KMouseButton btn) = 0;
};

KCoreMouse * createMouseWin32();




class KMouse {
public:
	enum Button {
		NONE = -1,
		LEFT,
		RIGHT,
		MIDDLE,
		ENUM_MAX
	};
	static int getGlobalX();
	static int getGlobalY();
	static bool isButtonDown(int btn);
};

} // namespace
