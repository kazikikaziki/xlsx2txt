#include "KScene.h"
#include "keng_game.h"
#include "KImGui.h"
#include "KInternal.h"

namespace Kamilo {

static const int SCENE_ID_LIMIT = 100;

#define TRAC(obj)  KLog::printInfo("%s: '%s'", __FUNCTION__, obj ? typeid(*obj).name() : "(nullptr)");


const KNamedValues * KScene::getParams() const {
	return &m_params;
}
KNamedValues * KScene::getParamsEditable() {
	return &m_params;
}
void KScene::setParams(const KNamedValues *new_params) {
	KNamedValues old_params = m_params;
	m_params.clear();
	if (new_params) {
		m_params.append(*new_params);
	}
	onSetParams(&m_params, new_params, &old_params);
}




struct SSceneDef {
	KScene *scene;
	KNamedValues params;
	KSCENEID id;

	SSceneDef() {
		scene = nullptr;
		id = -1;
		params.clear();
	}
	void clear() {
		scene = nullptr;
		id = -1;
		params.clear();
	}
};

class CSceneMgr: public KManager, public KInspectorCallback {
	std::vector<KScene *> m_scenelist;
	SSceneDef m_curr_scene;
	SSceneDef m_next_scene;
	int m_clock;
	KGameSceneSystemCallback *m_cb;
public:
	CSceneMgr() {
		m_curr_scene.clear();
		m_next_scene.clear();
		m_cb = nullptr;
		m_clock = 0;
		KEngine::addManager(this);
		KEngine::addInspectorCallback(this, u8"シーン管理");
	}
	virtual ~CSceneMgr() {
		on_manager_end();
	}
	void setCallback(KGameSceneSystemCallback *cb) {
		m_cb = cb;
	}
	virtual void on_manager_end() override {
		if (m_curr_scene.scene) {
			call_scene_exit();
		}
		for (auto it=m_scenelist.begin(); it!=m_scenelist.end(); ++it) {
			if (*it) delete *it;
		}
		m_scenelist.clear();
		m_curr_scene.clear();
		m_next_scene.clear();
	}
	virtual void on_manager_appframe() override {
		// シーン切り替えが指定されていればそれを処理する
		// （これはポーズ中でも関係なく行う）
		process_switching();
	}
	virtual void onInspectorGui() override {
		const char *nullname = "(nullptr)";
		const char *s = m_curr_scene.scene ? typeid(*m_curr_scene.scene).name() : nullname;
		ImGui::Text("Clock: %d", m_clock);
		ImGui::Text("Current scene: %s", s);
		if (ImGui::TreeNode("Current scene params")) {
			if (m_curr_scene.scene) {
				const KNamedValues *nv = m_curr_scene.scene->getParams();
				for (int i=0; i<nv->size(); i++) {
					const std::string &key = nv->getName(i);
					const std::string &val = nv->getString(i);
					ImGui::Text("%s: %s", key.c_str(), val.c_str());
				}
			}
			ImGui::TreePop();
		}
		for (size_t i=0; i<m_scenelist.size(); i++) {
			KScene *scene = m_scenelist[i];
			const char *name = scene ? typeid(*scene).name() : nullname;
			ImGui::PushID(KImGui::ID(i));

			char s[256];
			sprintf_s(s, sizeof(s), "%d", i);
			if (ImGui::Button(s)) {
				setNextScene(i, nullptr);
			}
			ImGui::SameLine();
			ImGui::Text("%s", name);
			ImGui::PopID();
		}
		ImGui::Separator();
		if (m_curr_scene.scene) {
			m_curr_scene.scene->onSceneInspectorGui();
		}
	}
	int getClock() const {
		return m_clock;
	}
	KScene * getScene(KSCENEID id) const {
		if (id < 0) return nullptr;
		if (id >= (int)m_scenelist.size()) return nullptr;
		return m_scenelist[id];
	}
	KScene * getCurrentScene() const {
		return m_curr_scene.scene;
	}
	KSCENEID getCurrentSceneId() const {
		for (size_t i=0; i<m_scenelist.size(); i++) {
			if (m_scenelist[i] == m_curr_scene.scene) {
				return i;
			}
		}
		return -1;
	}
	void addScene(KSCENEID id, KScene *scene) {
		K__ASSERT(id >= 0);
		K__ASSERT(id < SCENE_ID_LIMIT);
		if (getScene(id)) {
			KLog::printWarning("W_SCENE_OVERWRITE: KScene with the same id '%d' already exists. The scene will be overwritten by new one.", id);
			if (m_curr_scene.scene == getScene(id)) {
				KLog::printWarning("W_DESTROY_RUNNING_SCENE: Running scene with id '%d' will immediatly destroy.", id);
			}
			if (m_scenelist[id]) delete m_scenelist[id];
		}
		if ((int)m_scenelist.size() <= id) {
			m_scenelist.resize(id + 1);
		}
		m_scenelist[id] = scene;
	}
	void call_scene_enter() {
		TRAC(m_curr_scene.scene);
		if (m_curr_scene.scene) {
			m_curr_scene.scene->setParams(&m_curr_scene.params);

			// onSceneEnter でさらにシーンが変更される可能性に注意
			m_curr_scene.scene->onSceneEnter();
		}
	}
	void call_scene_exit() {
		TRAC(m_curr_scene.scene);
		if (m_curr_scene.scene) {
			m_curr_scene.scene->onSceneExit();
		}
	}
	void setNextScene(KSCENEID id, const KNamedValues *params) {
		KScene *scene = getScene(id);
		if (m_next_scene.scene) {
			KLog::printWarning("W_SCENE_OVERWRITE: Queued KScene '%s' will be overwritten by new posted scene '%s'",
				typeid(*m_next_scene.scene).name(),
				(scene ? typeid(*scene).name() : "(nullptr)")
			);
			m_next_scene.scene = nullptr;
		}
		if (scene) {
			KLog::printInfo("KSceneManager::setNextScene: %s", typeid(*scene).name());
		}
		if (id && scene==nullptr) {
			KLog::printInfo("E_NO_SCENE_ID: (KSCENEID)%s", id);
		}
		m_next_scene.scene = scene;
		m_next_scene.id = id;
		m_next_scene.params.clear();
		if (params) {
			m_next_scene.params.append(*params);
		}
	}
	void restart() {
		KLog::printInfo("Restart!");
		setNextScene(m_curr_scene.id, &m_curr_scene.params);
	}
	void process_switching() {
		// シーン切り替えが指定されていればそれを処理する
		if (m_next_scene.scene == nullptr) {
			if (m_curr_scene.scene) {
				KSCENEID id = m_curr_scene.scene->onQueryNextScene();
				setNextScene(id, nullptr);
			}
		}
		if (m_next_scene.scene) {
			if (m_cb) {
				// シーン切り替え通知
				// ここで next.params が書き換えられる可能性に注意
				KSceneManagerSignalArgs args;
				args.curr_scene = m_curr_scene.scene;
				args.curr_id = m_curr_scene.id;
				args.next_id = m_next_scene.id;
				args.next_params = &m_next_scene.params;
				m_cb->on_scenemgr_scene_changing(&args);
			}
			if (m_curr_scene.scene) {
				call_scene_exit();
				m_curr_scene.scene = nullptr;
				m_curr_scene.id = -1;
			}
			m_clock = 0;
			m_curr_scene = m_next_scene;
			m_next_scene.clear();
			call_scene_enter();

		} else {
			m_clock++;
		}
	}
};

#pragma region KSceneManager
static CSceneMgr * g_SceneMgr = nullptr;

void KSceneManager::install() {
	K__ASSERT(g_SceneMgr == nullptr);
	g_SceneMgr = new CSceneMgr();
}
void KSceneManager::uninstall() {
	if (g_SceneMgr) {
		g_SceneMgr->drop();
		g_SceneMgr = nullptr;
	}
}
bool KSceneManager::isInstalled() {
	return g_SceneMgr != nullptr;
}
void KSceneManager::setCallback(KGameSceneSystemCallback *cb) {
	K__ASSERT(g_SceneMgr);
	g_SceneMgr->setCallback(cb);
}
void KSceneManager::restart() {
	K__ASSERT(g_SceneMgr);
	g_SceneMgr->restart();
}
int KSceneManager::getClock() {
	K__ASSERT(g_SceneMgr);
	return g_SceneMgr->getClock();
}
KScene * KSceneManager::getScene(KSCENEID id) {
	K__ASSERT(g_SceneMgr);
	return g_SceneMgr->getScene(id);
}
KScene * KSceneManager::getCurrentScene() {
	K__ASSERT(g_SceneMgr);
	return g_SceneMgr->getCurrentScene();
}
KSCENEID KSceneManager::getCurrentSceneId() {
	K__ASSERT(g_SceneMgr);
	return g_SceneMgr->getCurrentSceneId();
}
void KSceneManager::addScene(KSCENEID id, KScene *scene) {
	K__ASSERT(g_SceneMgr);
	g_SceneMgr->addScene(id, scene);
}
/// シーンを切り替える
/// @param id      次のシーンの識別子（addScene で登録したもの）
/// @param params  次のシーンに渡すパラメータ。ここで渡したパラメータは KScene::getParams で取得できる
void KSceneManager::setNextScene(KSCENEID id, const KNamedValues *params) {
	K__ASSERT(g_SceneMgr);
	g_SceneMgr->setNextScene(id, params);
}
void KSceneManager::setSceneParamInt(const char *key, int val) {
	KScene *scene = getCurrentScene();
	if (scene) {
		KNamedValues *nv = scene->getParamsEditable();
		nv->setInteger(key, val);
	}
}
int KSceneManager::getSceneParamInt(const char *key) {
	KScene *scene = getCurrentScene();
	if (scene) {
		return scene->getParams()->getInteger(key);
	}
	return 0;
}
#pragma endregion // KSceneManager




} // namespace
