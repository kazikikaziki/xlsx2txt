#pragma once
namespace Kamilo {

/// [F11] を押すごとにウィンドウサイズが x1, x2, x3 ..., 最大化, ボーダレス最大化 と切り替わるようにする
/// [Shift + F11] だと逆順でウィンドウサイズが変化する
/// [Alt + Enter] でフルスクリーンに切り替える
class KWindowResizer {
public:
	static void install();
	static void uninstall();
	static bool isFullscreenTruly();
	static bool isFullscreenBorderless();
	static bool isWindowed();
	static void setFullscreenTruly();
	static void setFullscreenBorderless();
	static void setWindowed();
	static void resizeNext();
	static void resizePrev();
};

}
