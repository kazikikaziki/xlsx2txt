#pragma once

namespace Kamilo {

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
