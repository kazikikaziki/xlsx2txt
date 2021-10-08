#pragma once
namespace Kamilo {

/// [PrcSc] キーでスナップショットを自動保存する
class KSnapshotter {
public:
	static void install();
	static void uninstall();
	static void capture(const char *filename);
	static void captureNow(const char *filename);
	static void setCaptureTargetRenderTexture(const char *texname);
};

}
