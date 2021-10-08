#pragma once
namespace Kamilo {

/// ウィンドウを直接ドラッグして移動できるようにする
class KWindowDragger {
public:
	static void install();
	static void uninstall();
	static void lock();
	static void unlock();
};

}
