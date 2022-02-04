#pragma once
#include "KRef.h"
#include "KVec.h"

namespace Kamilo {
class KNode;
class KSig;
class KMatrix4;

class KAction: public virtual KRef {
	KNode *m_node;
public:
	KAction() {
		m_node = nullptr;
	}
	virtual ~KAction() {}
	virtual void onEnterAction() {}
	virtual void onExitAction() {}
	virtual void onStepAction() {}
	virtual void onStepAction2() {} // KNode::tick や他の KAction::onStepAction() が全て呼ばれた後に実行する必要のある処理
	virtual void onCommand(KSig &cmd) {}
	virtual void onSignal(KSig &sig) {}
	virtual void onAabbCulling(KVec3 *aabb_min, KVec3 *aabb_max) {}
	virtual void onWillRender() {} ///< 描画直前。カリングなどの前処理が終わり、実際にノードを描画する直前に呼ばれる
	virtual void onInspector() {}
	virtual void onDebugDraw(const KMatrix4 &tr) {}
	virtual void onDebugGUI() {}

	// システムノードとして登録されている場合に呼ばれる。
	// デバッグ用の強制ポーズの影響を受けず（ゲームが停止していても）常に呼ばれる
	// KNode の KNode::FLAG_SYSTEM フラグが有効になっているノードでのみ呼ばれる
	virtual void onStepSystemAction() {}

	void _setNode(KNode *node) {
		 m_node = node;
		 if (node) {
			 m_last_node_path = node->getNameInTree();
		 }
	}
	KNode * getSelf() {
		return m_node;
	}
	std::string m_last_node_path;
};

} // namespace
