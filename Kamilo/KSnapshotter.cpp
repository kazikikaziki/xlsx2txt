#include "KSnapshotter.h"
//
#include "keng_game.h"
#include "KImGui.h"
#include "KInternal.h"
#include "KSig.h"
#include "KKeyboard.h"
#include "KLocalTime.h"
#include "KScreen.h"
#include "KRes.h" // KBank
#include "KWindow.h"

namespace Kamilo {




class CSnapshot: public KManager, public KInspectorCallback {
	KPath m_TargetTextureName;
	KPath m_LastOutputFileName;
	bool m_DoShot;
	mutable KPath m_NextOutputFileName; // 次の出力ファイルを決めておく
	KPath m_OutputFileName; // 実際の出力ファイル名
	KPath m_BaseOutputName; // 出力ファイルの基本名
	bool m_WithTime; // 出力ファイル名に時刻を含める
	bool m_WithFrameNumber; // 出力ファイル名にフレーム番号を含める
public:
	CSnapshot() {
		m_WithTime  = false;
		m_WithFrameNumber = false;
		m_BaseOutputName = "__snapshot";
		m_DoShot = false;
		KEngine::addManager(this);
		KEngine::addInspectorCallback(this, u8"スクリーンショット"); // KInspectorCallback

		// 独自の PtrScr を使う（＝Windows標準の PrtScr を無効化する）
		KEngine::getWindow()->setAttribute(KWindowAttr_KILL_SNAPSHOT, true);
	}
	virtual void on_manager_signal(KSig &sig) override {
		if (sig.check(K_SIG_WINDOW_KEY_DOWN)) {
			// ウィンドウイベントは別スレッドから飛んでくる場合があることに注意
			if (sig.getInt("key") == KKey_SNAPSHOT) {
				m_DoShot = true;
			}
			return;
		}
	}
	virtual void on_manager_start() override {
		m_WithTime  = true;
		m_WithFrameNumber = true;
		m_DoShot = false;
	}
	virtual void on_manager_appframe() override {
		// システムノードとして登録されている場合に呼ばれる。
		// デバッグ用の強瀬ポーズの影響を受けず（ゲームが停止していても）常に呼ばれる
		// KNode の KNode::FLAG_SYSTEM フラグが有効になっているノードでのみ呼ばれる
		if (m_DoShot) {
			capture("");
			captureNow(m_OutputFileName.u8());
			m_DoShot = false;
		}
	}
	virtual void onInspectorGui() override { // KInspectorCallback
		if (m_WithTime || m_WithFrameNumber) {
			// ファイル名にフレーム番号または時刻を含んでいる。
			// 時間によってファイル名が変化するため GUI を表示しているあいだは常に更新する
			m_NextOutputFileName.clear(); // consumed this name
		}
		ImGui::Text("Next output file: ");
		ImGui::Text("%s", getNextName().u8());
		if (ImGui::Checkbox("Include local time", &m_WithTime)) {
			m_NextOutputFileName.clear(); // consumed this name
		}
		if (ImGui::Checkbox("Include frame number", &m_WithFrameNumber)) {
			m_NextOutputFileName.clear(); // consumed this name
		}
		if (ImGui::Button("Snap shot!")) {
			m_DoShot = true;
		}
		if (K::pathExists(m_LastOutputFileName.u8())) {
			char s[256];
			sprintf_s(s, sizeof(s), "Open: %s", m_LastOutputFileName.u8());
			if (ImGui::Button(s)) {
				K::fileShellOpen(m_LastOutputFileName.u8());
			}
		}
	}
	void capture(const char *_filename) {
		KPath filename = _filename;
		if (filename.empty()) {
			m_OutputFileName = getNextName();
			m_NextOutputFileName.clear(); // consumed this name
		} else {
			m_OutputFileName = filename;
		}
	}
	void captureNow(const char *_filename) {
		KPath filename = _filename;
		if (filename.empty()) {
			filename = getNextName();
			m_NextOutputFileName.clear(); // consumed this name
		}

		if (!m_TargetTextureName.empty()) {
			// キャプチャするテクスチャが指定されている
			KTEXID texid = KBank::getTextureBank()->getTexture(m_TargetTextureName);
			KTexture *tex = KVideo::findTexture(texid);
			if (tex) {
				KImage img = tex->exportTextureImage();
				std::string png = img.saveToMemory();
				
				KOutputStream file;
				file.openFileName(filename.u8());
				file.write(png.data(), png.size());

				K__PRINT("Texture image saved %s", filename.getFullPath().u8());
				m_LastOutputFileName = filename;
			}
		} else {
			// キャプチャ対象が未指定
			// 既定の画面を書き出す
			KScreen::postExportScreenTexture(filename); // 次回更新時に保存される。本当に保存されるか確定できないのでログはまだ出さない
			m_LastOutputFileName = filename;
		}
	}
	void setCaptureTargetRenderTexture(const char *texname) {
		m_TargetTextureName = texname;
	}
	const KPath & getNextName() const {
		if (m_NextOutputFileName.empty()) {
			KPath name = m_BaseOutputName;

			// 時刻情報を付加する
			if (m_WithTime) {
				std::string mb = KLocalTime::now().format("(%y%m%d-%H%M%S)");
				KPath time = KPath::fromUtf8(mb.c_str());
				name = name.joinString(time);
			}

			// フレーム番号を付加する
			if (m_WithFrameNumber) {
				KPath frame = KPath::fromFormat("[04%d]", getFrameNumber());
				name = name.joinString(frame);
			}

			// ファイル名を作成
			m_NextOutputFileName = name.joinString(".png");

			// 重複を確認する。重複していればインデックス番号を付加する
			int num = 1;
			while (K::pathExists(m_NextOutputFileName.u8())) {
				m_NextOutputFileName = KPath::fromFormat("%s_%d.png", name.u8(), num);
				num++;
			}
		}
		return m_NextOutputFileName;
	}
	int getFrameNumber() const {
		return KEngine::getStatus(KEngine::ST_FRAME_COUNT_GAME);
	}
};

#pragma region KSnapshotter
static CSnapshot *g_Snapshot = nullptr;

void KSnapshotter::install() {
	g_Snapshot = new CSnapshot();
}
void KSnapshotter::uninstall() {
	if (g_Snapshot) {
		g_Snapshot->drop();
		g_Snapshot = nullptr;
	}
}
void KSnapshotter::capture(const char *filename) {
	K__ASSERT(g_Snapshot);
	g_Snapshot->capture(filename);
}
void KSnapshotter::captureNow(const char *filename) {
	K__ASSERT(g_Snapshot);
	g_Snapshot->captureNow(filename);
}
void KSnapshotter::setCaptureTargetRenderTexture(const char *texname) {
	K__ASSERT(g_Snapshot);
	g_Snapshot->setCaptureTargetRenderTexture(texname);
}
#pragma endregion // KSnapshotter


} // namespace
