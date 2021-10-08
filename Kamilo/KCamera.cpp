#include "KCamera.h"
//
#include "KImGui.h"
#include "KInspector.h"
#include "KInternal.h"
#include "KScreen.h"
#include "keng_game.h"

namespace Kamilo {


/// ファイル名として安全な文字列にする（簡易版）
static void _EscapeFilename(std::string &name) {
	for (size_t i=0; name[i]; i++) {
		if (isalnum(name[i])) continue;
		if (strchr(" ._()[]{}<>#$%&=~^@+-", name[i])) continue;
		name[i] = '_'; //replace
	}
}





class CCameraManager: public KManager {
public:
	KCompNodes<KCamera> m_Nodes;

	CCameraManager() {
		KEngine::addManager(this);
	}
	virtual bool on_manager_isattached(KNode *node) override {
		return m_Nodes.contains(node);
	}
	virtual void on_manager_detach(KNode *node) override {
		m_Nodes.detach(node);
	}
	virtual void on_manager_nodeinspector(KNode *node) override {
		KCamera::of(node)->on_inspector();
	}
	KNode * findCameraFor(const KNode *target) {
		if (target == nullptr) return nullptr;
		int target_layer = target->getLayerInTree();

		for (auto it=m_Nodes.begin(); it!=m_Nodes.end(); ++it ) {
			KNode *node = it->first;
			if (node == nullptr) {
				continue;
			}
			int camera_layer = node->getLayerInTree();
			if (camera_layer != target_layer) {
				continue;
			}
			if (node->getEnableInTree()) {
				return node;
			}
		}
		return nullptr;
	}
};



static CCameraManager *g_CameraMgr = nullptr;


#pragma region KCamera

void KCamera::install() {
	g_CameraMgr = new CCameraManager();
}
void KCamera::uninstall() {
	if (g_CameraMgr) {
		g_CameraMgr->drop();
		g_CameraMgr = nullptr;
	}
}
void KCamera::attach(KNode *node) {
	K__ASSERT(g_CameraMgr);
	if (node && !isAttached(node)) {
		KCamera *co = new KCamera();
		g_CameraMgr->m_Nodes.attach(node, co);
		co->drop();
	}
}
bool KCamera::isAttached(KNode *node) {
	return of(node) != nullptr;
}
KCamera * KCamera::of(KNode *node) {
	K__ASSERT(g_CameraMgr);
	return g_CameraMgr->m_Nodes.get(node);
}
KNode * KCamera::findCameraFor(const KNode *node) {
	K__ASSERT(g_CameraMgr);
	return g_CameraMgr->findCameraFor(node);
}
int KCamera::getCameraNodes(KNodeArray &list) {
	K__ASSERT(g_CameraMgr);
	return g_CameraMgr->m_Nodes.exportArray(list);
}




KCamera::KCamera() {
	int w, h;
	KScreen::getGameSize(&w, &h); // ゲームの基本解像度で初期化する
//	KScreen::getGuiSize(&w, &h); // GUI解像度で初期化する
//	KVideo::getViewport(0, 0, &w, &h); // 現在の描画領域サイズで初期化する
	m_Data.size_w = (float)w;
	m_Data.size_h = (float)h;
	update_projection_matrix();
}
bool KCamera::getWorld2ViewMatrix(KMatrix4 *out_matrix) {
	// カメラ姿勢の行列
	KMatrix4 trans;
	getNode()->getWorld2LocalMatrix(&trans);

	// ワールド --> ビュー(-1..0..1) 変換行列
	if (out_matrix) *out_matrix = trans * m_Data.projection_matrix;
	return true;
}
bool KCamera::getView2WorldMatrix(KMatrix4 *out_matrix) {
	// 投影の逆行列
	KMatrix4 inv_proj;
	if (!m_Data.projection_matrix.computeInverse(&inv_proj)) {
		K__WARNING("E_SCREEN: projection_matrix does not have an inverse");
		return false;
	}

	// カメラ姿勢の逆行列
	KMatrix4 inv_trans;
	// getLocal2WorldMatrix と getWorld2LocalMatrix は逆行列の関係にあるため、
	// getWorld2LocalMatrix().computeInverse を実行するよりも getLocal2WorldMatrix を使った方が速い
	getNode()->getLocal2WorldMatrix(&inv_trans);

	// ビュー(-1..0..1)  --> ワールド変換行列
	if (out_matrix) *out_matrix = inv_proj * inv_trans;
	return true;
}
bool KCamera::getView2WorldVector(const KVec3 &view_point, KVec3 *out_world_point, KVec3 *out_normalized_direction) {

	// 画面奥 (z > 0) に向かう長さ1のベクトル
	KVec3 p[] = {
		view_point,
		view_point + KVec3(0, 0, 1),
	};

	// ベクトルの始点と終点座標を変換
	if (!getView2WorldPoint(p, 2)) {
		return false;
	}

	// ゼロベクトルでないことを確認
	KVec3 delta = p[1] - p[0];
	if (delta.isZero()) {
		return false;
	}

	if (out_world_point) {
		*out_world_point = p[0];
	}
	if (out_normalized_direction) {
		*out_normalized_direction = delta.getNormalized();
	}
	return true;
}
bool KCamera::getView2WorldPoint(KVec3 *points, int num_points) {
	if (points==nullptr || num_points==0) {
		return false;
	}

	// ビュー(-1..0..1)  --> ワールド変換行列
	KMatrix4 matrix;
	getView2WorldMatrix(&matrix);

	// 変換
	for (int i=0; i<num_points; i++) {
		points[i] = matrix.transform(points[i]);
	}
	return true;
}
bool KCamera::getWorld2ViewPoint(KVec3 *points, int num_points) {
	if (points==nullptr || num_points==0) {
		return false;
	}

	// ワールド --> ビュー(-1..0..1) 変換行列
	KMatrix4 matrix;
	getWorld2ViewMatrix(&matrix);

	// 変換
	for (int i=0; i<num_points; i++) {
		points[i] = matrix.transform(points[i]);
	}
	return true;
}

// e のローカル座標系からスクリーンUVに変換するための行列。
// シェーダーの mo__ScreenTexture とともに利用する
KMatrix4 KCamera::getTargetLocal2ViewUVMatrix(KNode *target) {
	if (target == nullptr) return KMatrix4();

	// targetローカル --> ワールド
	KMatrix4 this_local2world = target->getLocal2WorldMatrix();

	// ワールド --> カメラローカル
	KMatrix4 camera_world2local = getNode()->getWorld2LocalMatrix();

	// ビュー --> スクリーンテクスチャ: 左下(0,0) 右上(1, 1) ※Y軸反転に注意
	KMatrix4 view2tex_sc = KMatrix4::fromScale(0.5f, -0.5f, 1.0f);
	KMatrix4 view2tex_tr = KMatrix4::fromTranslation(0.5f, 0.5f, 0.0f);

	// 一括変換行列
	return this_local2world * camera_world2local * getProjectionMatrix() * view2tex_sc * view2tex_tr;
}

void KCamera::update_projection_matrix()  {
	float w = m_Data.size_w / m_Data.zoom;
	float h = m_Data.size_h / m_Data.zoom;
	if (w <= 0.0001f) w = 0.0001f;
	if (h <= 0.0001f) h = 0.0001f;
	switch (m_Data.projection_type) {
	case PROJ_ORTHO:
		m_Data.projection_matrix = KMatrix4::fromOrtho(
			w,
			h,
			m_Data.znear,
			m_Data.zfar
		) * KMatrix4::fromTranslation(m_Data.projection_offset);
		break;
	case PROJ_ORTHO_CABINET:
		m_Data.projection_matrix = KMatrix4::fromCabinetOrtho(
			w,
			h,
			m_Data.znear, 
			m_Data.zfar, 
			m_Data.cabinet_dx_dz,
			m_Data.cabinet_dy_dz
		) * KMatrix4::fromTranslation(m_Data.projection_offset);
		break;
	case PROJ_FRUSTUM:
		m_Data.projection_matrix = KMatrix4::fromFrustum(
			w,
			h,
			m_Data.znear,
			m_Data.zfar
		) * KMatrix4::fromTranslation(m_Data.projection_offset);
		break;
	}
}
bool KCamera::isTarget(const KNode *object) {
	if (object == nullptr) return false;
	int this_lay = getNode()->getLayerInTree();
	int objc_lay = object->getLayerInTree();
	if (this_lay != objc_lay) return false;
	if (!getNode()->getEnableInTree()) return false;
	return true;
}
void KCamera::copyCamera(KCamera *other) {
	if (other && other != this) {
		m_Data = other->m_Data;
		update_projection_matrix();
	}
}
const KMatrix4 & KCamera::getProjectionMatrix() const {
	return m_Data.projection_matrix;
}
KVec3 KCamera::getView2WorldPoint(const KVec3 &point) {
	KVec3 p = point;
	getView2WorldPoint(&p, 1);
	return p;
}
KVec3 KCamera::getWorld2ViewPoint(const KVec3 &point) {
	KVec3 p = point;
	getWorld2ViewPoint(&p, 1);
	return p;
}
void KCamera::setRenderTarget(KTEXID tex) {
	m_Data.render_target = tex;
}
KTEXID KCamera::getRenderTarget() const {
	return m_Data.render_target;
}

void KCamera::setProjectionW(float value) {
	K__ASSERT(value > 0);
	m_Data.size_w = value;
	update_projection_matrix();
}
void KCamera::setProjectionH(float value) {
	K__ASSERT(value > 0);
	m_Data.size_h = value;
	update_projection_matrix();
}
float KCamera::getProjectionW() const {
	return m_Data.size_w;
}
float KCamera::getProjectionH() const {
	return m_Data.size_h;
}
float KCamera::getProjectionZoomW() const {
	K__ASSERT(m_Data.zoom > 0);
	return m_Data.size_w / m_Data.zoom;
}
float KCamera::getProjectionZoomH() const {
	K__ASSERT(m_Data.zoom > 0);
	return m_Data.size_h / m_Data.zoom;
}
void KCamera::setProjectionOffset(const KVec3 &offset) {
	m_Data.projection_offset = offset;
	update_projection_matrix();
}
KVec3 KCamera::getProjectionOffset() const {
	return m_Data.projection_offset;
}
void KCamera::setPass(int pass) {
	m_Data.pass = pass;
}
int KCamera::getPass() const {
	return m_Data.pass;
}
void KCamera::setZoom(float value) {
	if (value > 0) {
		m_Data.zoom = value;
	} else {
		K__ASSERT(0);
		m_Data.zoom = 1.0f;
	}
	update_projection_matrix();
}
float KCamera::getZoom() const {
	return m_Data.zoom;
}

void KCamera::setZFar(float z) {
	m_Data.zfar = z;
	update_projection_matrix();
}
float KCamera::getZFar() const {
	return m_Data.zfar;
}

void KCamera::setZNear(float z) {
	m_Data.znear = z;
	update_projection_matrix();
}
float KCamera::getZNear() const {
	return m_Data.znear;
}

void KCamera::setProjectionType(Proj type) {
	m_Data.projection_type = type;
	update_projection_matrix();
}
KCamera::Proj KCamera::getProjectionType() const {
	return m_Data.projection_type;
}

void KCamera::setCabinetDxDz(float dx_dz) {
	m_Data.cabinet_dx_dz = dx_dz;
	update_projection_matrix();
}
float KCamera::getCabinetDxDz() const {
	return m_Data.cabinet_dx_dz;
}

void KCamera::setCabinetDyDz(float dy_dz) {
	m_Data.cabinet_dy_dz = dy_dz;
	update_projection_matrix();
}
float KCamera::getCabinetDyDz() const {
	return m_Data.cabinet_dy_dz;
}

void KCamera::setRenderingOrder(KCamera::Order order) {
	m_Data.rendering_order = order;
}
KCamera::Order KCamera::getRenderingOrder() const {
	return m_Data.rendering_order;
}

void KCamera::setZEnabled(bool z) {
	m_Data.z_enabled = z;
}
bool KCamera::isZEnabled() const {
	return m_Data.z_enabled;
}
void KCamera::setSnapIntEnabled(bool b) {
	m_Data.snap_int_enabled = b;
}
bool KCamera::isSnapIntEnabled() const {
	return m_Data.snap_int_enabled;
}

void KCamera::setOffsetHalfPixelEnabled(bool b) {
	m_Data.offset_half_pixel_enabled = b;
}
bool KCamera::isOffsetHalfPixelEnabled() const {
	return m_Data.offset_half_pixel_enabled;
}

void KCamera::setMaterialEnabled(bool value) {
	m_Data.material_enabled = value;
}
bool KCamera::isMaterialEnabled() const {
	return m_Data.material_enabled;
}
void KCamera::setMaterial(const KMaterial &m) {
	m_Data.material = m;
}
const KMaterial & KCamera::getMaterial() const {
	return m_Data.material;
}
bool KCamera::isWorldAabbInFrustum(const KVec3 &aabb_min, const KVec3 &aabb_max) {
	// ここでは簡易判定のみ

	// 対象AABBの８頂点をビュー座標に変換する
	KMatrix4 m;
	getWorld2ViewMatrix(&m);
	KVec3 p[8] = {
		{aabb_min.x, aabb_min.y, aabb_min.z},
		{aabb_min.x, aabb_min.y, aabb_max.z},
		{aabb_min.x, aabb_max.y, aabb_min.z},
		{aabb_min.x, aabb_max.y, aabb_max.z},
		{aabb_max.x, aabb_min.y, aabb_min.z},
		{aabb_max.x, aabb_min.y, aabb_max.z},
		{aabb_max.x, aabb_max.y, aabb_min.z},
		{aabb_max.x, aabb_max.y, aabb_max.z}
	};
	for (int i=0; i<8; i++) {
		p[i] = m.transform(p[i]);
	}

	// ビュー空間における対象のAABBを新たに得る
	KVec3 vmin, vmax;
	vmin = vmax = p[0];
	for (int i=1; i<8; i++) {
		vmin = vmin.getmin(p[i]);
		vmax = vmax.getmax(p[i]);
	}

	// ローカル描画範囲AABB
	KVec3 fru_min(-1.0f, -1.0f, m_Data.znear);
	KVec3 fru_max( 1.0f,  1.0f, m_Data.zfar);

	// 
	if (KGeom::K_GeomIntersectAabb(vmin, vmax, fru_min, fru_max, nullptr, nullptr)) {
		return true;
	}
	return false;
}

void KCamera::on_inspector() {
	bool changed = false;
	const char *proj_names[] = {
		"Ortho",
		"OrthoCabinet",
		"Frustum",
	};
	ImGui::Text("Projection Size X: %g", getProjectionZoomW());
	ImGui::Text("Projection Size Y: %g", getProjectionZoomH());
	float step = 1.0f;
	if (m_Data.size_w < 10) step = 0.1f;
	if (m_Data.size_h < 10) step = 0.1f;
	changed |= ImGui::DragFloat("Width",  &m_Data.size_w, step, 0.1f, 10000);
	changed |= ImGui::DragFloat("Height", &m_Data.size_h, step, 0.1f, 10000);
	changed |= ImGui::DragFloat("ZNear",  &m_Data.znear);
	changed |= ImGui::DragFloat("ZFar",   &m_Data.zfar);
	changed |= ImGui::DragFloat("Zoom",   &m_Data.zoom, 0.1f, 0.1f, 100.f);
	changed |= ImGui::DragFloat("Cabinet dx/dz", &m_Data.cabinet_dx_dz, 0.01f, -2.0f, 2.0f);
	changed |= ImGui::DragFloat("Cabinet dy/dz", &m_Data.cabinet_dy_dz, 0.01f, -2.0f, 2.0f);
	changed |= ImGui::Combo("Projection", (int*)&m_Data.projection_type, proj_names, 3);
	changed |= ImGui::DragFloat3("Proj offset", (float*)&m_Data.projection_offset);
	changed |= ImGui::InputInt("Pass", &m_Data.pass);
	changed |= ImGui::Checkbox("Depth test", &m_Data.z_enabled);
	changed |= ImGui::Checkbox("Snap to integer coordinate", &m_Data.snap_int_enabled);
	changed |= ImGui::Checkbox("Offset half pixel", &m_Data.offset_half_pixel_enabled);

	if (1) {
		const char *names = "AUTO\0ITEMSTAMP\0HIERARCHY\0ZDEPTH\0\0";
		changed |= ImGui::Combo("Rendering order", (int*)&m_Data.rendering_order, names);
	}
	if (changed) {
		update_projection_matrix();
	}
	if (1) {
		KDebugGui::K_DebugGui_Image(m_Data.render_target, 256, 256);
		if (1) {
			// テクスチャ画像を png でエクスポートする
			std::string filename = K::str_sprintf("__export__%s.png", getNode()->getName().c_str());
			_EscapeFilename(filename);
			KDebugGui::K_DebugGui_ImageExportButton("Export", m_Data.render_target, filename.c_str(), false);
		}
	}
}

#pragma endregion // KCamera



} // namespace

