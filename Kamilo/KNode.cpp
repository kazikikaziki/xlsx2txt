#include "KNode.h"
#include "KInternal.h"
#include "KCamera.h"
#include "KDrawable.h"
#include "KAction.h"
#include "KSig.h"
#include "KAny.h"
#include <algorithm> // std::sort


#define IGNORE_REMOVING_NODES 1
#define TEST_WORLDMATRIX 1

namespace Kamilo {

#define PATH_DELIM "/"

static void KNodeTree_on_node_enter(CNodeTreeImpl *tree, KNode *node);
static void KNodeTree_on_node_exit(CNodeTreeImpl *tree, KNode *node);
static void KNodeTree_on_node_removing(CNodeTreeImpl *tree, KNode *node);
static void KNodeTree_add_tag(CNodeTreeImpl *tree, KNode *node, const KName &tag);
static void KNodeTree_del_tag(CNodeTreeImpl *tree, KNode *node, const KName &tag);
static void KNodeTree_on_node_marked_as_remove(CNodeTreeImpl *tree, KNode *node);


// スケーリング * 並行移動
static KMatrix4 _MakeMatrix_Sc_Tr(const KVec3 &sc, const KVec3 &tr) {
//	KMatrix4 T = KMatrix4::fromTranslation(tr);
//	KMatrix4 S = KMatrix4::fromScale(sc);
//	KMatrix4 ST = S * T; で得られる行列 ST と同じ
	float tmp[] = {
		sc.x, 0.0f, 0.0f, 0.0f,
		0.0f, sc.y, 0.0f, 0.0f,
		0.0f, 0.0f, sc.z, 0.0f,
		tr.x, tr.y, tr.z, 1.0f,
	};
	return KMatrix4(tmp);
}

// 並行移動 * スケーリング の逆行列。
// 逆平行移動 * 逆スケーリングで求める
static KMatrix4 _MakeMatrix_Inv_Tr_Sc(const KVec3 &tr, const KVec3 &sc) {
	// KMatrix4 inv_T = KMatrix4::fromTranslation(-tr);
	// KMatrix4 inv_S = KMatrix4::fromScale(sc);
	// KMatrix4 inv_TS = inv_T * inv_S; で得られる行列 inv_TS と同じ
	float tmp[] = {
		 1.0f/sc.x,        0.0f,        0.0f, 0.0f,
		      0.0f,   1.0f/sc.y,        0.0f, 0.0f,
		      0.0f,        0.0f,   1.0f/sc.z, 0.0f,
		-tr.x/sc.x,  -tr.y/sc.y,  -tr.z/sc.z, 1.0f,
	};
	return KMatrix4(tmp);
}







#pragma region STransformData
STransformData::STransformData() {
	m_Scale = KVec3(1.0f, 1.0f, 1.0f);
	m_DirtyLocalMatrix = true;
	m_DirtyWorldMatrix = true;
	m_UsingEuler = true;
	m_UsingCustom = false;
	m_InheritTransform = true;
	m_Node = nullptr;
}
const KVec3 & STransformData::getPosition() const {
	return m_Position;
}
const KVec3 & STransformData::getScale() const {
	return m_Scale;
}
const KQuat & STransformData::getRotation() const {
	return m_Rotation;
}
void STransformData::setRotationDelta(const KQuat &delta) {
	KQuat rot = getRotation();
	rot *= delta;
	setRotation(rot);
}
bool STransformData::getRotationEuler(KVec3 *out_euler) const {
	if (m_UsingEuler) {
		if (out_euler) *out_euler = m_RotationEuler;
		return true;
	}
	return false;
}
/// 座標を設定する。
/// デフォルトの設定では、これは親ノードからの相対的な位置を表す。
/// @note 座標、スケール、回転を親から継承したくない場合は setTransformInherit を使って継承フラグを外す
void STransformData::setPosition(const KVec3 &value) {
	m_Position = value;
	_updateTree();
}
void STransformData::setPositionDelta(const KVec3 &delta) {
	KVec3 pos = getPosition();
	pos += delta;
	setPosition(pos);
}
/// スケールを設定する。
/// デフォルトの設定では、これは親ノードからの相対的なスケールを表す。
/// @note 座標、スケール、回転を親から継承したくない場合は setTransformInherit を使って継承フラグを外す
void STransformData::setScale(const KVec3 &value) {
	m_Scale = value;
	_updateTree();
}
/// 回転をクォータニオンで設定する。
/// クォータニオンからオイラー角への変換は一意に定まらないため、
/// クォータニオンを設定すると getRotationEuler によるオイラー角取得ができなくなる
void STransformData::setRotation(const KQuat &value) {
	m_Rotation = value;
	m_RotationEuler = KVec3();
	m_UsingEuler = false;
	_updateTree();
}
/// 回転をオイラー角で設定する。
/// 角度の適用順序は X --> Y --> Z である。
/// オイラー角を適用するとクォータニオンも連動して変化する（その逆はできない）
void STransformData::setRotationEuler(const KVec3 &euler_degrees) {
#if 1
	KQuat q;
	q = KQuat::mul_xrot(q, KMath::degToRad(euler_degrees.x));
	q = KQuat::mul_yrot(q, KMath::degToRad(euler_degrees.y));
	q = KQuat::mul_zrot(q, KMath::degToRad(euler_degrees.z));
	m_Rotation = q;
#else
	m_Rotation = KQuat::fromXDeg(euler_degrees.x) * KQuat::fromYDeg(euler_degrees.y) * KQuat::fromZDeg(euler_degrees.z);
#endif
	m_RotationEuler = euler_degrees;
	m_UsingEuler = true;
	_updateTree();
}
const KMatrix4 & STransformData::getLocalMatrix() const {
	if (m_DirtyLocalMatrix) {
		_updateLocalMatrix();
	}
	return m_LocalMatrix;
}
const KMatrix4 & STransformData::getLocalMatrixInversed() const {
	if (m_DirtyLocalMatrix) {
		_updateLocalMatrix();
	}
	return m_LocalMatrixInv;
}
/// 移動、拡大、回転では設定できないようなカスタム変形行列を指定する（Skewなど）
/// この行列は 移動、拡大、回転 に追加で適用される
/// あらかじめ逆行列が分かっている場合は matrix_inv を指定する。これが nullptr の場合は自動的に逆行列を計算する
void STransformData::setCustomTransform(const KMatrix4 &matrix, const KMatrix4 *p_matrix_inv) {
	if (matrix.equals(KMatrix4(), FLT_MIN)) { // is identity?
		// 単位行列を指定した（カスタム行列使用しない設定にした）
		m_CustomMatrix = KMatrix4();
		m_CustomMatrixInv = KMatrix4();
		m_UsingCustom = false;
		m_DirtyLocalMatrix = true;
	} else {
		m_CustomMatrix = matrix;
		if (p_matrix_inv) {
			m_CustomMatrixInv = *p_matrix_inv;
		} else {
			m_CustomMatrixInv = matrix.inverse();
		}
		m_UsingCustom = true;
		m_DirtyLocalMatrix = true;
	}
	_updateTree();
}
void STransformData::getCustomTransform(KMatrix4 *out_matrix, KMatrix4 *out_matrix_inv) const {
	if (out_matrix) *out_matrix = m_CustomMatrix;
	if (out_matrix_inv) *out_matrix_inv = m_CustomMatrixInv;
}
void STransformData::getWorld2LocalMatrix(KMatrix4 *out) const {
	KMatrix4 m;
	const KNode *node = m_Node;
	while (node) {
		const STransformData &node_tr = node->_getTransformData();
		const KMatrix4 &inv_tm = node_tr.getLocalMatrixInversed();
		m = inv_tm * m;
		if (node_tr.getTransformInherit() == false) {
			break; // これ以上親をたどらない
		}
		node = node->getParent();
	}
	if (out) *out = m;
}
KMatrix4 STransformData::getWorld2LocalMatrix() const {
	KMatrix4 mat;
	getWorld2LocalMatrix(&mat);
	return mat;
}
void STransformData::getLocal2WorldMatrix(KMatrix4 *out) const {
	if (m_DirtyWorldMatrix) {
		m_WorldMatrix = getLocalMatrix();
		if (m_InheritTransform == false) {
			// 親の変形を継承しない
		} else {
			KMatrix4 parent_matrix;
			if (m_Node->getParent()) {
				m_Node->getParent()->_getTransformData().getLocal2WorldMatrix(&parent_matrix);
			}
			m_WorldMatrix = m_WorldMatrix * parent_matrix;
		}
		m_DirtyWorldMatrix = false;
	}
	if (out) *out = m_WorldMatrix;
}
KMatrix4 STransformData::getLocal2WorldMatrix() const {
	KMatrix4 mat;
	getLocal2WorldMatrix(&mat);
	return mat;
}
void STransformData::getLocal2WorldRotation(KQuat *p_rot) const {
	KNode *parent = m_Node->getParent();
	if (parent) {
		const STransformData &parent_tr = parent->_getTransformData();
		KQuat parent_rot;
		parent_tr.getLocal2WorldRotation(&parent_rot);
		*p_rot = parent_rot * getRotation();
	} else {
		*p_rot = getRotation();
	}
}
KQuat STransformData::getLocal2WorldRotation() const {
	KQuat rot;
	getLocal2WorldRotation(&rot);
	return rot;
}
void STransformData::setTransformInherit(bool value) {
	m_InheritTransform = value;
	_updateTree();
}
bool STransformData::getTransformInherit() const {
	return m_InheritTransform;
}
KVec3 STransformData::getWorldPosition() const {
	KMatrix4 m;
	getLocal2WorldMatrix(&m);
	return m.transformZero(); // m.transform(KVec3(0, 0, 0));
}
KVec3 STransformData::localToWorldPoint(const KVec3 &local) const {
	KMatrix4 m;
	getLocal2WorldMatrix(&m);
	return m.transform(local);
}
KVec3 STransformData::worldToLocalPoint(const KVec3 &world) const {
	KMatrix4 m;
	getWorld2LocalMatrix(&m);
	return m.transform(world);
}
void STransformData::copyTransform(const KNode *other, bool copy_independent_flag) {
	if (other == nullptr) return;
	const STransformData &other_tr = other->_getTransformData();
	setPosition(other_tr.getPosition());
	setScale(other_tr.getScale());
	setRotation(other_tr.getRotation());
	KMatrix4 tm, tm_inv;
	other_tr.getCustomTransform(&tm, &tm_inv);
	setCustomTransform(tm, &tm_inv);
	if (copy_independent_flag) {
		setTransformInherit(other_tr.getTransformInherit());
	}
}
void STransformData::_updateLocalMatrix() const {
	m_DirtyLocalMatrix = false;

	// 基本変形行列
	KMatrix4 sc_ro_tr;
	KMatrix4 inv_tr_ro_sc;
	if (m_Rotation.equals(KQuat(), FLT_MIN)) {
		// 回転なし
		sc_ro_tr = _MakeMatrix_Sc_Tr(m_Scale, m_Position);
		inv_tr_ro_sc = _MakeMatrix_Inv_Tr_Sc(m_Position, m_Scale);
	} else {
		// 回転あり
		KMatrix4 Tr = KMatrix4::fromTranslation(m_Position);
		KMatrix4 Ro = KMatrix4::fromRotation(m_Rotation);
		KMatrix4 Sc = KMatrix4::fromScale(m_Scale);
		sc_ro_tr = Sc * Ro * Tr;

		KMatrix4 inv_Tr = KMatrix4::fromTranslation(-m_Position);
		KMatrix4 inv_Ro = KMatrix4::fromRotation(m_Rotation.inverse());
		KMatrix4 inv_Sc = KMatrix4::fromScale(1.0f / m_Scale.x, 1.0f / m_Scale.y, 1.0f / m_Scale.z);
		inv_tr_ro_sc = inv_Tr * inv_Ro * inv_Sc;
	}
	
	// カスタム変形行列
	if (m_UsingCustom) {
		m_LocalMatrix = m_CustomMatrix * sc_ro_tr;
		m_LocalMatrixInv = inv_tr_ro_sc * m_CustomMatrixInv;
	} else {
		m_LocalMatrix = sc_ro_tr;
		m_LocalMatrixInv = inv_tr_ro_sc;
	}
}
void STransformData::_updateWorldMatrix(const KMatrix4 &parent) const {
	if (m_InheritTransform) {
		m_WorldMatrix = m_LocalMatrix * parent;
	} else {
		m_WorldMatrix = m_LocalMatrix;
	}
	for (size_t i=0; i<m_Node->getChildCount(); i++) {
		KNode *child = m_Node->getChildFast(i);
		STransformData &data = child->_getTransformData();
		data._updateWorldMatrix(m_WorldMatrix);
	}
}
void STransformData::_setDirtyWorldMatrix() {
	m_DirtyWorldMatrix = true;
	for (size_t i=0; i<m_Node->getChildCount(); i++) {
		KNode *child = m_Node->getChildFast(i);
		STransformData &data = child->_getTransformData();
		data._setDirtyWorldMatrix();
	}
}
void STransformData::_updateTree() {
	#if TEST_WORLDMATRIX
	_updateLocalMatrix();
	if (m_Node->getParent()) {
		STransformData &parent_tr = m_Node->getParent()->_getTransformData();
		_updateWorldMatrix(parent_tr.m_WorldMatrix);
	} else {
		_updateWorldMatrix(m_LocalMatrix);
	}
	#else
	m_DirtyLocalMatrix = true;
	_setDirtyWorldMatrix();
	#endif
}

#pragma endregion // STransformData






#pragma region STagData
STagData::STagData() {
	m_Node = nullptr;
}
bool STagData::hasTag(const KTag &tag) const {
	return KNameList_contains(m_SelfTags, tag);
}
bool STagData::hasTagInTree(const KTag &tag) const {
	return KNameList_contains(getTagListInTree(), tag);
}
const KNameList & STagData::getTagList() const {
	return m_SelfTags;
}
const KNameList & STagData::getTagListInherited() const {
	return m_ParentTags;
}
const KNameList & STagData::getTagListInTree() const {
	return m_TagsInTree;
}
void STagData::setTag(const KTag &tag) {
	_setTagEx(tag, false);
}
void STagData::_setTagEx(const KTag &tag, bool is_inherited) {
	K__ASSERT(!tag.empty());
	if (is_inherited) {
		// 継承タグリストに追加
		if (KNameList_pushback_unique(m_ParentTags, tag)) {
			//	if (get_tree()) {
			//		KNodeTree_add_tag(get_tree(), this, tag);
			//	}
		}
	} else {
		// 自身のタグリストに追加
		if (KNameList_pushback_unique(m_SelfTags, tag)) {
			if (m_Node->get_tree()) {
				KNodeTree_add_tag(m_Node->get_tree(), m_Node, tag);
			}
		}
	}

	// ツリー内タグリストを更新
	{
		m_TagsInTree = m_SelfTags;
		KNameList_merge_unique(m_TagsInTree, m_ParentTags);
	}

	// 子に伝番
	if (is_inherited && KNameList_contains(m_SelfTags, tag)) {
		// 親から継承したタグを追加しているが、自分自身も同じタグを持っている。
		// 子ツリーは自分のタグを継承していることになるので、
		// これ以上子をたどって追加する必要はない
	} else {
		for (int i=0; i<m_Node->getChildCount(); i++) {
			KNode *child = m_Node->getChildFast(i);
			STagData &data = child->_getTagData();
			data._setTagEx(tag, true);
		}
	}
}

void STagData::removeTag(const KTag &tag) {
	_removeTagEx(tag, false);
}
void STagData::_removeTagEx(const KTag &tag, bool is_inherited) {
	K__ASSERT(!tag.empty());
	if (is_inherited) {
		// 継承タグリストから削除
		if (KNameList_erase(m_ParentTags, tag)) {
		//	if (get_tree()) {
		//		KNodeTree_del_tag(get_tree(), this, tag);
		//	}
		}
	} else {
		// 自身のタグリストから削除
		if (KNameList_erase(m_SelfTags, tag)) {
			if (m_Node->get_tree()) {
				KNodeTree_del_tag(m_Node->get_tree(), m_Node, tag);
			}
		}
	}

	// ツリー内タグリストを更新
	{
		m_TagsInTree = m_SelfTags;
		KNameList_merge_unique(m_TagsInTree, m_ParentTags);
	}

	// 子に伝番
	if (is_inherited && KNameList_contains(m_SelfTags, tag)) {
		// 親から継承したタグを削除しているが、自分自身も同じタグを持っている。
		// 子ツリーは自分のタグを継承していることになるので、
		// これ以上子をたどって削除する必要はない
	} else {
		for (int i=0; i<m_Node->getChildCount(); i++) {
			KNode *child = m_Node->getChildFast(i);
			STagData &data = child->_getTagData();
			data._removeTagEx(tag, true);
		}
	}
}
void STagData::_updateNodeTreeTags(bool add) {
	K__ASSERT(m_Node->get_tree());
	if (add) {
		// ノードツリー側に自分のタグを追加する
		for (auto it=m_SelfTags.begin(); it!=m_SelfTags.end(); ++it) {
			KNodeTree_add_tag(m_Node->get_tree(), m_Node, *it);
		}

	} else {
		// ノードツリー側から自分のタグを削除する
		for (auto it=m_SelfTags.begin(); it!=m_SelfTags.end(); ++it) {
			KNodeTree_del_tag(m_Node->get_tree(), m_Node, *it);
		}
	}

	// 子に伝番
	for (int i=0; i<m_Node->getChildCount(); i++) {
		KNode *child = m_Node->getChildFast(i);
		STagData &data = child->_getTagData();
		data._updateNodeTreeTags(add);
	}
}

void STagData::_beginParentChange() {
	if (m_Node->get_tree()) {
		// 自分の（および子の）タグをノードツリーから外す
		_updateNodeTreeTags(false);

		// 古い親から継承したタグを外す
		if (m_Node->getParent()) {
			const STagData &parent_data = m_Node->getParent()->_getTagData();
			for (auto it=parent_data.m_TagsInTree.begin(); it!=parent_data.m_TagsInTree.end(); ++it) {
				_removeTagEx(*it, true);
			}
		}
	}
}

void STagData::_endParentChange() {
	if (m_Node->get_tree()) {
		// 自分の（および子の）タグをノードツリーに追加する
		_updateNodeTreeTags(true);

		// 新しい親から継承したタグを追加する
		if (m_Node->getParent()) {
			const STagData &parent_data = m_Node->getParent()->_getTagData();
			for (auto it=parent_data.m_TagsInTree.begin(); it!=parent_data.m_TagsInTree.end(); ++it) {
				_setTagEx(*it, true);
			}
		}
	}
}
#pragma endregion // STagData




#pragma region SFlagData
SFlagData::SFlagData() {
	m_Bits = 0;
	m_BitsInTreeAll = 0;
	m_BitsInTreeAny = 0;
	m_Node = nullptr;
}
bool SFlagData::hasFlag(uint32_t flag) const {
	return m_Bits & flag;
}
bool SFlagData::hasFlagInTreeAll(uint32_t flag) const {
	return m_BitsInTreeAll & flag;
}
bool SFlagData::hasFlagInTreeAny(uint32_t flag) const {
	return m_BitsInTreeAny & flag;
}
uint32_t SFlagData::getFlagBits() const {
	return m_Bits;
}
uint32_t SFlagData::getFlagBitsInTreeAll() const {
	return m_BitsInTreeAll;
}
uint32_t SFlagData::getFlagBitsInTreeAny() const {
	return m_BitsInTreeAny;
}
void SFlagData::setFlag(uint32_t flag, bool value) {
	bool b = m_Bits & flag;
	if (b != value) {
		if (value) {
			m_Bits |= flag;
		} else {
			m_Bits &= ~flag;
		}
		_updateTree();
	}
}
void SFlagData::_updateTree() {
	KNode *parent = m_Node->getParent();
	if (parent) {
		const SFlagData &data = parent->_getFlagData();
		_updateTree(&data);
	} else {
		_updateTree(nullptr);
	}
}
void SFlagData::_updateTree(const SFlagData *parent) {
	if (parent) {
		m_BitsInTreeAll = m_Bits & parent->m_BitsInTreeAll;
		m_BitsInTreeAny = m_Bits | parent->m_BitsInTreeAny;
	} else {
		m_BitsInTreeAll = m_Bits;
		m_BitsInTreeAny = m_Bits;
	}

	// 子に伝搬
	for (int i=0; i<m_Node->getChildCount(); i++) {
		KNode *child = m_Node->getChild(i);
		SFlagData &data = child->_getFlagData();
		data._updateTree(this);
	}
}
#pragma endregion // SFlagData




#pragma region SRenderData
SRenderData::SRenderData() {
	m_Diffuse = KColor::WHITE;
	m_Specular = KColor::ZERO;
	m_DiffuseInTree = KColor::WHITE;
	m_SpecularInTree = KColor::ZERO;
	m_InheritDiffuse = true;
	m_InheritSpecular = true;
	m_RenderAtomic = false;
	m_RenderAfterChildren = false;
	m_ViewCulling = true;
	m_LocalRenderOrder = KLocalRenderOrder_DEFAULT;
	m_Layer = 0;
	m_LayerInTree = 0;
	m_Priority = 0;
	m_PriorityInTree = 0;
	m_Node = nullptr;
}
const KColor & SRenderData::getColor() const {
	return m_Diffuse;
}
const KColor & SRenderData::getSpecular() const {
	return m_Specular;
}
const KColor & SRenderData::getColorInTree() const {
	return m_DiffuseInTree;
}
const KColor & SRenderData::getSpecularInTree() const {
	return m_SpecularInTree;
}
void SRenderData::setColorInherit(bool value) {
	m_InheritDiffuse = value;
	_updateTree();
}
bool SRenderData::getColorInherit() const {
	return m_InheritDiffuse;
}
void SRenderData::setSpecularInherit(bool value) {
	m_InheritSpecular = value;
	_updateTree();
}
bool SRenderData::getSpecularInherit() const {
	return m_InheritSpecular;
}
void SRenderData::setColor(const KColor &value) {
	m_Diffuse = value;
	_updateTree();
}
void SRenderData::setSpecular(const KColor &value) {
	m_Specular = value;
	_updateTree();
}
void SRenderData::setAlpha(float alpha) {
	m_Diffuse.a = KMath::clampf(alpha, 0.0f, 1.0f);
	_updateTree();
}
float SRenderData::getAlpha() const {
	return m_Diffuse.a;
}
void SRenderData::_updateTree() {
	KNode *parent = m_Node->getParent();
	if (parent) {
		const SRenderData &data = parent->_getRenderData();
		_updateTree(&data);
	} else {
		_updateTree(nullptr);
	}
}
void SRenderData::_updateTree(const SRenderData *parent) {
	// Diffuse
	if (parent && m_InheritDiffuse) {
		m_DiffuseInTree = m_Diffuse * parent->m_DiffuseInTree; // 乗算合成
	} else {
		m_DiffuseInTree = m_Diffuse;
	}

	// Specular
	if (parent && m_InheritSpecular) {
		m_SpecularInTree = m_Specular + parent->m_SpecularInTree; // 加算合成
	} else {
		m_SpecularInTree = m_Specular;
	}

	// Layer
	if (parent && m_Layer == 0) {
		m_LayerInTree = parent->m_LayerInTree;
	} else {
		m_LayerInTree = m_Layer;
	}

	// Priority
	if (parent && m_Priority == 0) {
		m_PriorityInTree = parent->m_PriorityInTree;
	} else {
		m_PriorityInTree = m_Priority;
	}

	// 子に伝搬
	for (int i=0; i<m_Node->getChildCount(); i++) {
		KNode *child = m_Node->getChildFast(i);
		SRenderData &data = child->_getRenderData();
		data._updateTree(this);
	}
}
void SRenderData::setRenderAtomic(bool value) {
	m_RenderAtomic = value;
}
bool SRenderData::getRenderAtomic() const {
	return m_RenderAtomic;
}
void SRenderData::setRenderAfterChildren(bool value) {
	m_RenderAfterChildren = value;
}
bool SRenderData::getRenderAfterChildren() const {
	return m_RenderAfterChildren;
}
void SRenderData::setViewCulling(bool value) {
	m_ViewCulling = value;
}
bool SRenderData::getViewCulling() const {
	return m_ViewCulling;
}
bool SRenderData::getViewCullingInTree() const {
	bool result = true;
	const KNode *node = m_Node;
	while (node && result) {
		const SRenderData &node_ren = node->_getRenderData();
		result = result && node_ren.getViewCulling();
		node = node->getParent();
	}
	return result;
}
KLocalRenderOrder SRenderData::getLocalRenderOrder() const {
	return m_LocalRenderOrder;
}
void SRenderData::setLocalRenderOrder(KLocalRenderOrder lro) {
	m_LocalRenderOrder = lro;
}
void SRenderData::setLayer(int value) {
	m_Layer = value;
	_updateTree();
}
int SRenderData::getLayer() const {
	return m_Layer;
}
int SRenderData::getLayerInTree() const {
	return m_LayerInTree;
}
void SRenderData::setPriority(int value) {
	m_Priority = value;
	_updateTree();
}
int SRenderData::getPriority() const {
	return m_Priority;
}
int SRenderData::getPriorityInTree() const {
	return m_PriorityInTree;
}
#pragma endregion // SRenderData








#pragma region KNode
static int g_KNode_unique_id = 0;

__nodiscard__ KNode * KNode::create() {
	return new KNode();
}

KNode::KNode() {
#ifdef _DEBUG
	m_pName = &m_NodeData.name;
	m_pParent = &m_NodeData.parent;
#endif
	m_NodeData = NodeData();
	m_TransformData = STransformData();
	m_TransformData.m_Node = this;
	m_TagData = STagData();
	m_TagData.m_Node = this;
	m_FlagData = SFlagData();
	m_FlagData.m_Node = this;
	m_RenderData = SRenderData();
	m_RenderData.m_Node = this;
	m_ActionNext = nullptr;
	m_ActionCurr = nullptr;
	m_NodeData.uuid = (EID)(++g_KNode_unique_id);
}
KNode::~KNode() {
	// 既にノードツリーからは切り離されているはず
	K__ASSERT(m_NodeData.tree == nullptr);
	m_NodeData.name; // ↑の assert でひっかかったら、この名前を確認

	on_node_end();
	if (m_ActionCurr || m_ActionNext) {
		_ExitAction();
		_DeleteAction();
	}
	_invalidate_child_tree();
}
void KNode::lock() const {
#if K_THREAD_SAFE
	m_mutex.lock();
#endif
}
void KNode::unlock() const {
#if K_THREAD_SAFE
	m_mutex.unlock();
#endif
}
const char * KNode::getReferenceDebugString() const {
	return m_NodeData.name.c_str();
}
std::string KNode::getWarningString() {
	std::string msg;
	if (KCamera::of(this)) {
		if (!msg.empty()) msg += "\n";
		msg += KCamera::of(this)->getWarningString();
	}
	if (KDrawable::of(this)) {
		if (!msg.empty()) msg += "\n";
		msg += KDrawable::of(this)->getWarningString();
	}
	return msg;
}




#pragma region Tree
CNodeTreeImpl * KNode::get_tree() const {
	return m_NodeData.tree;
}
void KNode::_set_tree(CNodeTreeImpl *tree) {
	// 現在のツリーから抜ける
	if (m_NodeData.tree) {
		_propagate_exit_tree();
	}

	// ツリーを設定する
	// 参照カウンタのデッドロック防止のために m_tree は grab しない
	m_NodeData.tree = tree;

	// 新しいツリーに入る
	if (m_NodeData.tree) {
		_propagate_enter_tree();

		// 準備完了した親に子ノードを追加した場合は、その子ツリーも準備完了状態になる
		if (m_NodeData.parent==nullptr || m_NodeData.parent->m_NodeData.is_ready) {
			_propagate_ready();
		}
	}
}
void KNode::_propagate_enter_tree() {
	// 親のツリーに自分も入ることになる
	// ※参照カウンタのデッドロック防止のために m_tree は grab しない
	if (m_NodeData.parent) {
		m_NodeData.tree = m_NodeData.parent->m_NodeData.tree;
	} else {
		// ルートノードの場合 m_parent が nullptr の状態でツリーに入ることになる。
		// 親がいないからと言ってここで m_tree = nullptr してはいけない。
	}

	// ツリーへ通知
	if (m_NodeData.tree) {
		KNodeTree_on_node_enter(m_NodeData.tree, this);
	}

	// 子へ伝搬
	for (int i=0; i<(int)m_NodeData.children.size(); i++) {
		m_NodeData.children[i]->_propagate_enter_tree();
	}
}
void KNode::_propagate_exit_tree() {
	// 子へ伝搬
	// ※enter_tree とは逆順で処理する
	for (int i=(int)m_NodeData.children.size()-1; i>=0; i--) {
		m_NodeData.children[i]->_propagate_exit_tree();
	}

	// ツリーに対して通知
	if (m_NodeData.tree) {
		KNodeTree_on_node_exit(m_NodeData.tree, this);
	}

	// ツリー設定をクリア
	m_NodeData.tree = nullptr;
}
void KNode::_propagate_ready() {
	// 子ツリーに対する準備完了通知
	for (auto it=m_NodeData.children.begin(); it!=m_NodeData.children.end(); ++it) {
		KNode *sub = *it;
		sub->_propagate_ready();
	}

	// 自分に対する準備完了通知（まだ通知していなかった場合のみ）
	if (!m_NodeData.is_ready) {
		m_NodeData.is_ready = true;
		on_node_ready();
		tick(KNodeTick_ENTERONLY);
	}
}
#pragma endregion // Tree




#pragma region Parent/Children/Sibling
KNode * KNode::getRoot() {
	KNode *nd = this;
	while (nd->getParent()) {
		nd = nd->getParent();
	}
	return nd;
}
KNode * KNode::getParent() const {
	return m_NodeData.parent;
}
void KNode::setParent(KNode *new_parent) {
	if (this == new_parent) {
		K__ERROR("Parent node can not have itself as a child");
		return;
	}
	m_TagData._beginParentChange();

	grab();
	{
		// 古い親から切り離す
		if (m_NodeData.parent) {
			m_NodeData.parent->_remove_child_nocheck(this);
			m_NodeData.parent = nullptr;
		}

		// 新しい親に接続する
		// m_parent は _add_child_nocheck(this) によって変更される
		if (new_parent) {
			new_parent->_add_child_nocheck(this);
		}
	}
	drop();

	// 親が変化した。親に影響を受けるパラメータ類も更新する
	_updateNames();
	m_TransformData._updateTree();
	m_FlagData._updateTree();
	m_RenderData._updateTree();
	m_TagData._endParentChange();

	K__ASSERT(m_NodeData.parent == new_parent);
}
KNode * KNode::getChild(int index) const {
	if (0 <= index && index < (int)m_NodeData.children.size()) {
		return m_NodeData.children[index];
	}
	return nullptr;
}
KNode * KNode::getChildFast(int index) const {
	return m_NodeData.children[index];
}
int KNode::getChildCount() const {
	return (int)m_NodeData.children.size();
}
void KNode::setChildIndex(KNode *child, int new_index) {
	if (child == nullptr) {
		K__ASSERT(child);
		return;
	}

	lock();
	{
		// childを親から切り離す
		{
			int i=0;
			for (auto it=m_NodeData.children.begin(); it!=m_NodeData.children.end(); ++it) {
				if (*it == child) {
					m_NodeData.children.erase(it);
					break;
				}
				i++;
			}
		}

		// 新しいインデックスで親に追加する
		if (new_index < 0) {
			K__ASSERT(child);
			m_NodeData.children.insert(m_NodeData.children.begin(), child);
		} else if (new_index < (int)m_NodeData.children.size()) {
			K__ASSERT(child);
			m_NodeData.children.insert(m_NodeData.children.begin() + new_index, child);

		} else {
			K__ASSERT(child);
			m_NodeData.children.push_back(child);
		}
	}
	unlock();
}
int KNode::getChildIndex(const KNode *child) const {
	int ret = -1;
	lock();
	if (child) {
		for (int i=0; i<(int)m_NodeData.children.size(); i++) {
			if (m_NodeData.children[i] == child) {
				ret = i;
				break;
			}
		}
	}
	unlock();
	return ret;
}
bool KNode::isParentOf(const KNode *child) const {
	bool ret = false;
	lock();
	if (child) {
		const KNode *node = child;
		while (node->getParent()) {
			if (node->getParent() == this) {
				ret = true;
				break;
			}
			node = node->getParent();
		}
	}
	unlock();
	return ret;
}
void KNode::_add_child_nocheck(KNode *child) {
	K__ASSERT(child);
	K__ASSERT(child != this); // 自分自身はダメ
	K__ASSERT(child->m_NodeData.tree == nullptr); // child は既にツリーから外れている
	K__ASSERT(child->m_NodeData.parent == nullptr); // child の親子関係は既に解消されている
	K__ASSERT(m_NodeData.blocked == 0);

	// node を子リストに追加する
	m_NodeData.children.push_back(child);
	child->grab();

	// node の親を自分にする
	child->m_NodeData.parent = this;

	// 自分がすでにツリーに入っているなら、新しい子も同じツリーに入れる
	if (m_NodeData.tree) {
		child->_set_tree(m_NodeData.tree);
	}
}
void KNode::_remove_child_nocheck(KNode *child) {
	K__ASSERT(child);
	K__ASSERT(m_NodeData.blocked == 0);

	// child が自分の子ならばインデックスを得る
	int index = -1;
	for (int i=0; i<(int)m_NodeData.children.size(); i++) {
		if (m_NodeData.children[i] == child) {
			index = i;
			break;
		}
	}
	if (index < 0) {
		return; // child は自分の子ではない
	}

	// child がノードツリーから出る
	child->_set_tree(nullptr);

	// 親子関係を解消する
	child->m_NodeData.parent = nullptr;
	m_NodeData.children.erase(m_NodeData.children.begin() + index);

	// 参照カウンタを減らす
	child->drop();
}
#pragma endregion // Parent/Children/Sibling




#pragma region Removing
void KNode::remove() {
	markAsRemove();
}
void KNode::remove_children() {
	int num = getChildCount();
	for (int i=0; i<num; i++) {
		KNode *node = getChildFast(i);
		node->markAsRemove();
	}
}
void KNode::markAsRemove() {
	setFlag(KNode::FLAG__MARK_REMOVE, true);
	if (m_NodeData.tree) {
		KNodeTree_on_node_marked_as_remove(m_NodeData.tree, this);
	}
}
void KNode::_remove_all() {
	grab();
	m_NodeData.blocked++;
	for (int i=0; i<(int)m_NodeData.children.size(); i++) {
		m_NodeData.children[i]->_remove_all();
	}
	
	// アクションを終了
	on_node_end();
	_ExitAction();

	// 削除通知
	if (m_NodeData.tree) {
		KNodeTree_on_node_removing(m_NodeData.tree, this);
	}

	// アクションやレンダラーを削除
	_DeleteAction();

	// ツリーから取り除く
	if (m_NodeData.parent) {
		m_NodeData.parent->_remove_child_nocheck(this);
	}
	_Invalidated();
	m_NodeData.blocked--;
	drop();
}
void KNode::_Invalidated() {
	setFlag(KNode::FLAG__INVALD, true);
	m_NodeData.parent = nullptr;
	K__ASSERT(m_NodeData.children.empty());
	K__ASSERT(m_ActionNext == nullptr);
	K__ASSERT(m_ActionCurr == nullptr);
	m_ActionNext = nullptr;
	m_ActionCurr = nullptr;
}
void KNode::_invalidate_child_tree() {
	// 子を再帰的に無効化する

	// 無効化フラグを付けたうえで参照カウンタを減らす。
	// もしほかの場所で参照カウンタを保持していても、
	// 無効化フラグの有無を見れば破棄すべきかどうかが分かる

	// 子ツリーを再帰的に無効化
	for (auto it=m_NodeData.children.begin(); it!=m_NodeData.children.end(); ++it) {
		KNode *node = *it;
		node->setFlag(KNode::FLAG__INVALD, true);
		node->_invalidate_child_tree();
		node->m_NodeData.parent = nullptr;
		node->drop();
	}
	m_NodeData.children.clear();
}
#pragma endregion // Removing












#pragma region Find
KNode * KNode::findChild(const std::string &name, const KTag &tag) const {
	KNode *ret = nullptr;
	lock();
	for (auto it=m_NodeData.children.begin(); it!=m_NodeData.children.end(); ++it) {
		KNode *sub = *it;
		#if IGNORE_REMOVING_NODES
		if (sub->hasFlag(KNode::FLAG__MARK_REMOVE)) {
			continue; // 削除マークがついている場合は、もう削除済みとして扱う
		}
		#endif
		bool nameOK = false;
		bool tagOK = false;
		if (name.empty() || sub->hasName(name)) {
			nameOK = true;
		}
		if (tag.empty() || sub->hasTag(tag)) {
			tagOK = true;
		}
		if (nameOK && tagOK) {
			ret = sub;
			break;
		}
	}
	unlock();
	return ret;
}
KNode * KNode::findChildInTree(const std::string &name, const KTag &tag) const {
	KNode *ret = nullptr;
	lock();
	{
		ret = findChildInTree_unsafe(name, tag);
	}
	unlock();
	return ret;
}
KNode * KNode::findChildInTree_unsafe(const std::string &name, const KTag &tag) const {
	for (auto it=m_NodeData.children.begin(); it!=m_NodeData.children.end(); ++it) {
		KNode *sub = *it;
		#if IGNORE_REMOVING_NODES
		if (sub->hasFlag(KNode::FLAG__MARK_REMOVE)) {
			continue; // 削除マークがついている場合は、もう削除済みとして扱う
		}
		#endif
		bool name_ok = name.empty() || sub->hasName(name);
		bool tag_ok = tag.empty() || sub->hasTag(tag);
		if (name_ok && tag_ok) {
			return sub;
		}
		KNode *subsub = sub->findChildInTree_unsafe(name, tag);
		if (subsub) {
			return subsub;
		}
	}
	return nullptr;
}

// node の子から、node からの相対パスが child_path に一致する子を探す
// 例えば "aaa/bbb" を指定した場合、node の子である "aaa", その子である "bbb" を探す
KNode * KNode::findChildPath(const std::string &subpath) const {
	std::string path = subpath;
	std::string name;
	KNode *child = const_cast<KNode*>(this);
	while (child && !path.empty()) {
		name = K::pathPopLeft(path);
		child = child->findChild(name);
	}
	return child;
}

#pragma endregion // Find





#pragma region Name and ID
const std::string & KNode::getName() const {
	return m_NodeData.name;
}
void KNode::setName(const std::string &name) {
	if (name.empty()) {
		// 空文字列が指定された場合は自動的に名前をつける
		char s[256] = {0};
		sprintf_s(s, sizeof(s), "__%p", m_NodeData.uuid);
		m_NodeData.name = s;
	} else {
		// 名前を適用
		m_NodeData.name = name;
	}
	_updateNames();
}
bool KNode::hasName(const std::string &name) const {
	return m_NodeData.name.compare(name) == 0;
}
const std::string & KNode::getNameInTree() const {
	return m_NodeData.nameInTree;
}
EID KNode::getId() const {
	return m_NodeData.uuid;
}
void KNode::_updateNames() {
	if (m_NodeData.parent) {
		m_NodeData.nameInTree = m_NodeData.parent->m_NodeData.nameInTree + PATH_DELIM + m_NodeData.name;
	} else {
		m_NodeData.nameInTree = m_NodeData.name;
	}

	for (auto it=m_NodeData.children.begin(); it!=m_NodeData.children.end(); ++it) {
		KNode *child = *it;
		child->_updateNames();
	}
}
#pragma endregion // Name and ID






#pragma region Transform
const KVec3 & KNode::getPosition() const {
	return m_TransformData.getPosition();
}
const KVec3 & KNode::getScale() const {
	return m_TransformData.getScale();
}
const KQuat & KNode::getRotation() const {
	return m_TransformData.getRotation();
}
void KNode::setRotationDelta(const KQuat &delta) {
	m_TransformData.setRotationDelta(delta);
}
bool KNode::getRotationEuler(KVec3 *out_euler) const {
	return m_TransformData.getRotationEuler(out_euler);
}
/// 座標を設定する。
/// デフォルトの設定では、これは親ノードからの相対的な位置を表す。
/// @note 座標、スケール、回転を親から継承したくない場合は setTransformInherit を使って継承フラグを外す
void KNode::setPosition(const KVec3 &value) {
	m_TransformData.setPosition(value);
}
void KNode::setPositionDelta(const KVec3 &delta) {
	m_TransformData.setPositionDelta(delta);
}
/// スケールを設定する。
/// デフォルトの設定では、これは親ノードからの相対的なスケールを表す。
/// @note 座標、スケール、回転を親から継承したくない場合は setTransformInherit を使って継承フラグを外す
void KNode::setScale(const KVec3 &value) {
	m_TransformData.setScale(value);
}
/// 回転をクォータニオンで設定する。
/// クォータニオンからオイラー角への変換は一意に定まらないため、
/// クォータニオンを設定すると getRotationEuler によるオイラー角取得ができなくなる
void KNode::setRotation(const KQuat &value) {
	m_TransformData.setRotation(value);
}
/// 回転をオイラー角で設定する。
/// 角度の適用順序は X --> Y --> Z である。
/// オイラー角を適用するとクォータニオンも連動して変化する（その逆はできない）
void KNode::setRotationEuler(const KVec3 &euler_degrees) {
	m_TransformData.setRotationEuler(euler_degrees);
}
const KMatrix4 & KNode::getLocalMatrix() const {
	return m_TransformData.getLocalMatrix();
}
const KMatrix4 & KNode::getLocalMatrixInversed() const {
	return m_TransformData.getLocalMatrixInversed();
}
/// 移動、拡大、回転では設定できないようなカスタム変形行列を指定する（Skewなど）
/// この行列は 移動、拡大、回転 に追加で適用される
/// あらかじめ逆行列が分かっている場合は matrix_inv を指定する。これが nullptr の場合は自動的に逆行列を計算する
void KNode::setCustomTransform(const KMatrix4 &matrix, const KMatrix4 *p_matrix_inv) {
	m_TransformData.setCustomTransform(matrix, p_matrix_inv);
}
void KNode::getCustomTransform(KMatrix4 *out_matrix, KMatrix4 *out_matrix_inv) const {
	m_TransformData.getCustomTransform(out_matrix, out_matrix_inv);
}
void KNode::getWorld2LocalMatrix(KMatrix4 *out) const {
	m_TransformData.getWorld2LocalMatrix(out);
}
KMatrix4 KNode::getWorld2LocalMatrix() const {
	return m_TransformData.getWorld2LocalMatrix();
}
void KNode::getLocal2WorldMatrix(KMatrix4 *out) const {
	m_TransformData.getLocal2WorldMatrix(out);
}
KMatrix4 KNode::getLocal2WorldMatrix() const {
	return m_TransformData.getLocal2WorldMatrix();
}
void KNode::getLocal2WorldRotation(KQuat *p_rot) const {
	m_TransformData.getLocal2WorldRotation(p_rot);
}
KQuat KNode::getLocal2WorldRotation() const {
	return m_TransformData.getLocal2WorldRotation();
}
void KNode::setTransformInherit(bool value) {
	m_TransformData.setTransformInherit(value);
}
bool KNode::getTransformInherit() const {
	return m_TransformData.getTransformInherit();
}
KVec3 KNode::getWorldPosition() const {
	return m_TransformData.getWorldPosition();
}
KVec3 KNode::localToWorldPoint(const KVec3 &local) const {
	return m_TransformData.localToWorldPoint(local);
}
KVec3 KNode::worldToLocalPoint(const KVec3 &world) const {
	return m_TransformData.worldToLocalPoint(world);
}
void KNode::copyTransform(const KNode *other, bool copy_independent_flag) {
	m_TransformData.copyTransform(other, copy_independent_flag);
}
#pragma endregion // Transform





#pragma region Color
const KColor & KNode::getColor() const {
	return m_RenderData.getColor();
}
const KColor & KNode::getSpecular() const {
	return m_RenderData.getSpecular();
}
const KColor & KNode::getColorInTree() const {
	return m_RenderData.getColorInTree();
}
const KColor & KNode::getSpecularInTree() const {
	return m_RenderData.getSpecularInTree();
}
void KNode::setColorInherit(bool value) {
	m_RenderData.setColorInherit(value);
}
bool KNode::getColorInherit() const {
	return m_RenderData.getColorInherit();
}
void KNode::setSpecularInherit(bool value) {
	m_RenderData.setSpecularInherit(value);
}
bool KNode::getSpecularInherit() const {
	return m_RenderData.getSpecularInherit();
}
void KNode::setColor(const KColor &value) {
	m_RenderData.setColor(value);
}
void KNode::setSpecular(const KColor &value) {
	m_RenderData.setSpecular(value);
}
void KNode::setAlpha(float alpha) {
	m_RenderData.setAlpha(alpha);
}
float KNode::getAlpha() const {
	return m_RenderData.getAlpha();
}
void KNode::setRenderAtomic(bool value) {
	m_RenderData.setRenderAtomic(value);
}
bool KNode::getRenderAtomic() const {
	return m_RenderData.getRenderAtomic();
}
void KNode::setRenderAfterChildren(bool value) {
	m_RenderData.setRenderAfterChildren(value);
}
bool KNode::getRenderAfterChildren() const {
	return m_RenderData.getRenderAfterChildren();
}
void KNode::setViewCulling(bool value) {
	m_RenderData.setViewCulling(value);
}
bool KNode::getViewCulling() const {
	return m_RenderData.getViewCulling();
}
bool KNode::getViewCullingInTree() const {
	return m_RenderData.getViewCullingInTree();
}
KLocalRenderOrder KNode::getLocalRenderOrder() const {
	return m_RenderData.getLocalRenderOrder();
}
void KNode::setLocalRenderOrder(KLocalRenderOrder lro) {
	m_RenderData.setLocalRenderOrder(lro);
}
#pragma endregion // Color




#pragma region Flags
bool KNode::hasFlag(Flag flag) const {
	return m_FlagData.hasFlag(flag);
}
bool KNode::hasFlagInTreeAll(Flag flag) const {
	return m_FlagData.hasFlagInTreeAll(flag);
}
bool KNode::hasFlagInTreeAny(Flag flag) const {
	return m_FlagData.hasFlagInTreeAny(flag);
}
KNode::Flags KNode::getFlagBits() const {
	return m_FlagData.getFlagBits();
}
KNode::Flags KNode::getFlagBitsInTreeAll() const {
	return m_FlagData.getFlagBitsInTreeAll();
}
KNode::Flags KNode::getFlagBitsInTreeAny() const {
	return m_FlagData.getFlagBitsInTreeAny();
}
void KNode::setFlag(Flag flag, bool value) {
	m_FlagData.setFlag(flag, value);
}
#pragma endregion // Flags




#pragma region Tags
bool KNode::hasTag(const KTag &tag) const {
	return m_TagData.hasTag(tag);
}
bool KNode::hasTagInTree(const KTag &tag) const {
	return m_TagData.hasTagInTree(tag);
}
const KNameList & KNode::getTagList() const {
	return m_TagData.getTagList();
}
const KNameList & KNode::getTagListInherited() const {
	return m_TagData.getTagListInherited();
}
const KNameList & KNode::getTagListInTree() const {
	return m_TagData.getTagListInTree();
}
void KNode::setTag(const KTag &tag) {
	return m_TagData.setTag(tag);
}
void KNode::removeTag(const KTag &tag) {
	m_TagData.removeTag(tag);
}
void KNode::_updateNodeTreeTags(bool add) {
	m_TagData._updateNodeTreeTags(add);
}
void KNode::copyTags(KNode *src) {
	const KNameList &tags = src->getTagList();
	for (auto it=tags.begin(); it!=tags.end(); ++it) {
		setTag(*it);
	}
}
#pragma endregion // Tags




#pragma region Traverse
void KNode::traverse_parents(KTraverseCallback *cb) {
	lock();
	for (KNode *it=m_NodeData.parent; it!=nullptr; it=it->m_NodeData.parent) {
		cb->onVisit(it);
	}
	unlock();
}
void KNode::traverse_children(KTraverseCallback *cb, bool recurse) {
	lock();
	for (size_t i=0; i<m_NodeData.children.size(); i++) {
		KNode *child = m_NodeData.children[i];
		K__ASSERT(child);
		cb->onVisit(child);
		if (recurse) {
			child->traverse_children(cb, true);
		}
	}
	unlock();
}
#pragma endregion // Traverse



#pragma region Action
void KNode::setAction(KAction *act, bool update_now) {
	lock();
	{
		// 新しいアクションを「次アクション」として設定しておく。
		// 既に「次アクション」が指定されている場合は、それを上書きする。
		// このとき、上書きされる「次アクション」はまだ実行されていないので
		// アクション終了の通知関数を呼ぶことなく黙って削除して良い
		if (m_ActionNext) {
			m_ActionNext->drop();
			m_ActionNext = nullptr;
		}

		// nullptr が指定された場合はアクションを削除する
		if (act == nullptr) {
			if (m_ActionCurr) {
				_ExitAction();
				m_ActionCurr = nullptr;
			}
		}

		// 新しいアクションを予約しておく
		m_ActionNext = act;

		// ノードがすでに待機状態にあり、かつ「今すぐ更新」が指定されている場合はすぐに更新関数を呼ぶ
		if (m_NodeData.is_ready && update_now) {
			tick(0);
		}
	}
	unlock();
}
KAction * KNode::getAction() const {
	return m_ActionCurr;
}
void KNode::sys_tick() {
	// システムアクションを更新
	on_node_systemstep();
	if (m_ActionCurr) {
		m_ActionCurr->onStepSystemAction();
	}

	// 子ノードのシステムアクションを更新する
	// 更新中に子ノードが変更される可能性があるため（新ノードの追加など）
	// あらかじめノード列をコピーしておく
	for (auto it=m_NodeData.children.begin(); it!=m_NodeData.children.end(); ++it) {
		KNode *sub = *it;
		K__ASSERT(sub);
		// システムアクションは enabled や pause の影響を受けない
		//if (sub->getEnable() && !sub->getPause()) {
			sub->sys_tick();
		//}
	}
}
void KNode::tick(KNodeTickFlags flags) {
	{
		bool nostep = false;
		if (m_NodeData.should_call_onstart) {
			m_NodeData.should_call_onstart = false;
			on_node_start();
			on_node_step();
			nostep = true;
		}
		if (!nostep && (flags & KNodeTick_ENTERONLY) == 0) {
			on_node_step();
		}
	}

	// アクションの切り替えと更新
	{
		// 次アクションが指定されていればアクションの切り替え処理を行う
		bool stepped = false;
		if (m_ActionNext) {
			// 現在のアクションを削除する
			if (m_ActionCurr) {
				_ExitAction();
				m_ActionCurr = nullptr;
			}
			// 新しいアクションを設定
			{
				m_ActionCurr = m_ActionNext;
				m_ActionNext = nullptr;
				m_ActionCurr->_setNode(this);
				m_ActionCurr->onEnterAction(); // 開始通知（この中で自分自身に setAction しないこと。動作保証しない）
				m_ActionCurr->onStepAction(); // enter 直後に step しておく
				stepped = true; // 既に step しているのでこの後の step を無視する (KNodeTick_ENTERONLY がある場合でも１度は step したいので）
			}
		}

		// アクションを更新
		if (m_ActionCurr && !stepped) {
			if ((flags & KNodeTick_ENTERONLY) == 0) {
				m_ActionCurr->onStepAction();
			}
		}
	}

	// 子ノードのアクションを更新する
	// 更新中に子ノードが変更される可能性があるため（新ノードの追加など）
	// あらかじめノード列をコピーしておく
	m_TmpNodes.assign(m_NodeData.children.begin(), m_NodeData.children.end());
	for (auto it=m_TmpNodes.begin(); it!=m_TmpNodes.end(); ++it) {
		KNode *sub = *it;

		// enabled なノードだけ更新する。ただし Enabled 無視フラグがある場合は disabled でも OK
		bool enabled = sub->getEnable() || (flags & KNodeTick_DONTCARE_ENABLE);

		// not paused なノードだけ更新する。ただし Paused 無視フラグがある場合は paused でも OK
		bool unpaused = !sub->getPause() || (flags & KNodeTick_DONTCARE_PAUSED);

		if (enabled && unpaused) {
			sub->tick(flags);
		}
	}
}

// 二回目の更新
void KNode::tick2(KNodeTickFlags flags) {
	if (m_ActionCurr) {
		m_ActionCurr->onStepAction2();
	}
	for (auto it=m_NodeData.children.begin(); it!=m_NodeData.children.end(); ++it) {
		KNode *sub = *it;

		// enabled なノードだけ更新する。ただし Enabled 無視フラグがある場合は disabled でも OK
		bool enabled = sub->getEnable() || (flags & KNodeTick_DONTCARE_ENABLE);

		// not paused なノードだけ更新する。ただし Paused 無視フラグがある場合は paused でも OK
		bool unpaused = !sub->getPause() || (flags & KNodeTick_DONTCARE_PAUSED);

		if (enabled && unpaused) {
			sub->tick2(flags);
		}
	}
}
void KNode::sendActionCommand(KSig &cmd) {
	if (m_ActionCurr) {
		m_ActionCurr->onCommand(cmd);
	}
}
void KNode::_ExitAction() {
	// 自分を頂点とするノードツリーの全アクションに対して削除通知をする。
	// あくまでも通知するだけで、まだ削除はしない
	if (m_ActionCurr) {
		m_ActionCurr->onExitAction(); // 終了通知（この中で自分自身に setAction しないこと。動作保証しない）
		m_ActionCurr->_setNode(nullptr);
	}
}
void KNode::_DeleteAction() {
	if (m_ActionCurr) {
		K__DROP(m_ActionCurr);
		m_ActionCurr = nullptr;
	}
	if (m_ActionNext) {
		K__DROP(m_ActionNext);
		m_ActionNext = nullptr;
	}
}
#pragma endregion // Action



#pragma region Signal
void KNode::broadcastSignalToChildren(KSig &sig) {
	// 子ツリーに対してシグナルを送信する
	for (int i=0; i<(int)m_NodeData.children.size(); i++) {
		KNode *nd = m_NodeData.children[i];
		nd->processSignal(sig);
		nd->broadcastSignalToChildren(sig);
	}
}
void KNode::broadcastSignalToParents(KSig &sig) {
	// 親に対してシグナルを送信する
	if (m_NodeData.parent) {
		m_NodeData.parent->processSignal(sig);
		m_NodeData.parent->broadcastSignalToParents(sig);
	}
}
void KNode::processSignal(KSig &sig) {
	on_node_signal(sig);
	if (m_ActionCurr) {
		m_ActionCurr->onSignal(sig);
	}
}
#pragma endregion // Signal


void KNode::_RegisterForDelete(std::vector<KNode*> &list, bool all) {
	if (all || hasFlag(KNode::FLAG__MARK_REMOVE)) {
		list.push_back(this);
		for (auto it=m_NodeData.children.begin(); it!=m_NodeData.children.end(); ++it) {
			KNode *child = *it;
			K__ASSERT(child);
			child->_RegisterForDelete(list, true);
		}
	} else {
		for (auto it=m_NodeData.children.begin(); it!=m_NodeData.children.end(); ++it) {
			KNode *child = *it;
			K__ASSERT(child);
			child->_RegisterForDelete(list, false);
		}
	}
}





#pragma region Helper
bool KNode::isInvalid() const {
	return hasFlag(FLAG__INVALD);
}

void KNode::setPause(bool value) {
	setFlag(FLAG_NO_UPDATE, value);
}
bool KNode::getPause() const {
	return hasFlag(FLAG_NO_UPDATE);
}
bool KNode::getPauseInTree() const {
	return hasFlagInTreeAny(FLAG_NO_UPDATE);
}

void KNode::setEnable(bool value) {
	setFlag(FLAG_NO_ENABLE, !value);
}
bool KNode::getEnable() const {
	return !hasFlag(FLAG_NO_ENABLE);
}
bool KNode::getEnableInTree() const {
	return !hasFlagInTreeAny(FLAG_NO_ENABLE); // FLAG_NO_ENABLE が一つでもあったらダメ
}

void KNode::setVisible(bool value) {
	setFlag(FLAG_NO_RENDER, !value);
}
bool KNode::getVisible() const {
	return !hasFlag(FLAG_NO_RENDER);
}
bool KNode::getVisibleInTree() const {
	return !hasFlagInTreeAny(FLAG_NO_RENDER); // FLAG_NO_RENDER が一つでもあったらダメ
}

void KNode::setLayer(int value) {
	m_RenderData.setLayer(value);
}
int KNode::getLayer() const {
	return m_RenderData.getLayer();
}
int KNode::getLayerInTree() const {
	return m_RenderData.getLayerInTree();
}

void KNode::setPriority(int value) {
	m_RenderData.setPriority(value);
}
int KNode::getPriority() const {
	return m_RenderData.getPriority();
}
int KNode::getPriorityInTree() const {
	return m_RenderData.getPriorityInTree();
}
#pragma endregion // Helper





#pragma endregion // KNode



#pragma region CNodeTreeImpl
// EID ==> KNode 
class CIdNodeMap {
	std::unordered_map<EID, KNode*> m_map;
public:
	CIdNodeMap() {
	}
	~CIdNodeMap() {
		clear();
	}
	void clear() {
		for (auto it=m_map.begin(); it!=m_map.end(); ++it) {
			K__DROP(it->second);
		}
		m_map.clear();
	}
	int size() const {
		return (int)m_map.size();
	}
	KNode * get(EID uuid) const {
		auto it = m_map.find(uuid);
		if (it != m_map.end()) {
			return it->second;
		}
		return nullptr;
	}
	bool contains(KNode *node) const {
		if (node == nullptr) return false;
		EID uuid = node->getId();
		auto it = m_map.find(uuid);
		return it != m_map.end();
	}
	// node が未登録ならば追加除する。参照カウンタを増やす
	void insert(KNode *node) {
		if (node == nullptr) return;
		if (contains(node)) return;
		EID uuid = node->getId();
		m_map[uuid] = node;
		K__GRAB(node);
	}
	// node があればその要素を削除する。参照カウンタを減らす
	void remove(KNode *node) {
		if (node == nullptr) return;
		EID uuid = node->getId();
		auto it = m_map.find(uuid);
		if (it != m_map.end()) {
			K__DROP(node);
			m_map.erase(it);
		}
	}
	// 終了済みのノード要素を削除する
	void remove_all_invalidated_nodes() {
		for (auto it=m_map.begin(); it!=m_map.end(); /*NO EXPR*/) {
			KNode *node = it->second;
			if (node->isInvalid()) {
				K__DROP(node);
				it = m_map.erase(it);
			} else {
				it++;
			}
		}
	}
	int get_vector(std::vector<KNode*> *out_list) const {
		if (out_list) {
			for (auto it=m_map.begin(); it!=m_map.end(); ++it) {
				out_list->push_back(it->second);
			}
		}
		return (int)m_map.size();
	}
};




class CNodeTreeImpl {
	typedef std::unordered_map<int, std::string> NAMES;
	std::unordered_map<int, NAMES> m_group_names;
	CIdNodeMap m_id2node_map; // EID から Node へのマッピング
	std::vector<KNode *> m_tmp_nodes; // 作業用のノード
	KNodeManagerCallback *m_cb;
	KNode *m_root; // ルートノード
	bool m_node_will_be_removed; // 削除すべきノードが少なくともひとつ存在する？
	mutable std::recursive_mutex m_mutex;
	std::unordered_map<KName, std::unordered_set<KNode*>> m_tag_nodes;
	std::vector<KNode*> m_tmp_node_starts;  // on_node_start を呼ぶべきノード
	std::vector<KNode*> m_tmp_node_steps;   // on_node_step を呼ぶべきノード
	std::vector<KNode*> m_tmp_node_actions; // アクションを更新するべきノード

	struct SigItem {
		EID eid;
		KSig sig;
		int delay;

		SigItem() {
			eid = nullptr;
			delay = 0;
		}
	};
	std::vector<SigItem> m_signal_queue;

	void lock() const {
	#if K_THREAD_SAFE
		m_mutex.lock();
	#endif
	}
	void unlock() const {
	#if K_THREAD_SAFE
		m_mutex.unlock();
	#endif
	}

public:
	CNodeTreeImpl() {
		m_cb = nullptr;
		m_node_will_be_removed = false;
		m_root = new KNode();
		m_root->_set_tree(this);
	}
	~CNodeTreeImpl() {
		lock();
		{
			m_id2node_map.clear();
			m_root->_set_tree(nullptr);
			K__DROP(m_root); // ルート要素を削除
		}
		unlock();
	}
	void addCallback(KNodeManagerCallback *cb) {
		m_cb = cb;
	}
	void removeCallback(KNodeManagerCallback *cb) {
		if (m_cb == cb) {
			m_cb = nullptr;
		}
	}
	void broadcastSignal(KSig &sig) {
		if (m_root) {
			m_root->broadcastSignalToChildren(sig);
		}
	}
	void sendSignal(KNode *target, KSig &sig) {
		if (target) target->processSignal(sig);
	}
	void sendSignalDelay(KNode *target, KSig &sig, int delay) {
		if (delay > 0) {
			SigItem item;
			item.eid = target->getId();
			item.sig = sig;
			item.delay = delay;
			m_signal_queue.push_back(item);
		} else {
			target->processSignal(sig);
		}
	}
	void tick_signals() {
		for (int i=0; i<(int)m_signal_queue.size(); i++) {
			SigItem &item = m_signal_queue[i];
			KNode *target = findNodeById(item.eid);
			if (item.delay > 0) {
				if (target && !target->getPauseInTree()) {
					item.delay--;
				}
			} else {
				if (target) {
					target->processSignal(item.sig);
				}
				item.delay = -1; // mark as remove
			}
		}
		for (int i=(int)m_signal_queue.size()-1; i>=0; i--) {
			SigItem &item = m_signal_queue[i];
			if (item.delay < 0) {
				m_signal_queue.erase(m_signal_queue.begin() + i);
			}
		}
	}

	void _add_tag(KNode *node, const KName &tag) {
		m_tag_nodes[tag].insert(node);
	}
	void _del_tag(KNode *node, const KName &tag) {
		m_tag_nodes[tag].erase(node);
	}
	void _on_node_enter(KNode *node) {
		// node が tree に入った
		
		// タググループを更新する
		{
			const KNameList &tags = node->getTagList();
			for (auto it=tags.begin(); it!=tags.end(); ++it) {
				const KName &T = *it;
				m_tag_nodes[T].insert(node); // grab 省略
			}
		}
		// EID ==> KNode 変換テーブルに登録する
		{
			m_id2node_map.insert(node);
		}
	}
	void _on_node_exit(KNode *node) {
		// node が tree から出た
		
		// タググループを更新する
		{
			const KNameList &tags = node->getTagList();
			for (auto it=tags.begin(); it!=tags.end(); ++it) {
				const KName &T = *it;
				m_tag_nodes[T].erase(node);
			}
		}
		// EID ==> KNode 変換テーブルから削除する
		{
			m_id2node_map.remove(node);
		}
	}
	void _on_node_marked_as_remove(KNode *node) {
		m_node_will_be_removed = true; // 削除すべきノードが少なくともひとつ存在する
		if (m_cb) m_cb->on_node_marked_as_remove();
	}
	void _on_node_removing(KNode *node) {
		// ノードツリーから削除される直前であることを通知する
		KSig sig(K_SIG_NODE_UNBINDTREE);
		sig.setNode("node", node);
		broadcastSignal(sig);
	}
	void destroyMarkedNodes(KNodeRemovingCallback *cb) {
		if (!m_node_will_be_removed) {
			return; // 削除予約関数が呼ばれていない（＝削除すべきノードが存在しない）
		}
		if (m_cb) m_cb->on_node_remove_start();

		m_node_will_be_removed = false;

		// 削除予定ノードを得る
		// _RegisterForDelete の仕様により m_tmp_nodes には削除対象ノードが深さ優先巡回順で入る
		// ※得られた m_tmp_nodes の各ノードは grab されていない
		m_tmp_nodes.clear();
		int num = m_root->getChildCount();
		for (int i=0; i<num; i++) {
			KNode *child = m_root->getChildFast(i);
			child->_RegisterForDelete(m_tmp_nodes, false); 
		}

		if (cb) {
			cb->onRemoveNodes(m_tmp_nodes.data(), m_tmp_nodes.size());
		}
		for (int i=(int)m_tmp_nodes.size()-1; i>=0; i--) {
			m_tmp_nodes[i]->_remove_all();
		}
		m_tmp_nodes.clear(); // ※m_tmp_nodes の各ノードは grab されていないので drop　不要。詳細は KNode::_RegisterForDelete を見よ
		if (m_cb) m_cb->on_node_remove_done();
	}
	KNode * findNodeById(EID uuid) const {
		if (uuid == nullptr) return nullptr;
		KNode *ret = nullptr;
		lock();
		{
			ret = m_id2node_map.get(uuid);
		}
		unlock();
		return ret;
	}
	KNode * findNodeByName(const KNode *start, const std::string &name) const {
		if (name.empty()) return nullptr;
		KNode *ret = nullptr;
		lock();
		{
			ret = find_name_in_tree_unsafe(start, name);
		}
		unlock();
		return ret;
	}
	
	KNode * find_name_in_tree_unsafe(const KNode *start, const std::string &name) const {
		if (name.empty()) return nullptr; // 空文字列では検索できない
		if (K::pathHasDelim(name)) return nullptr; // ディレクトリ部分を含んではいけない
		if (start == nullptr) {
			start = getRoot();
		}
		#if IGNORE_REMOVING_NODES
		if (start->hasFlag(KNode::FLAG__MARK_REMOVE)) {
			return nullptr; // 削除マークがついている場合は、もう削除済みとして扱う
		}
		#endif
		KNode *node = start->findChildInTree(name);
		if (node) {
			return node;
		}
		return nullptr;
	}

	KNode * findNodeByPath(const KNode *_start, const std::string &_path) const {
		#if IGNORE_REMOVING_NODES
		// start が削除マーク付きツリー内にある場合は、もう削除済みとして扱う
		if (_start && _start->hasFlagInTreeAny(KNode::FLAG__MARK_REMOVE)) {
			return nullptr;
		}
		#endif

		// パスが / から始まっている場合は絶対パスなので、 start は必ずルート要素になる
		const KNode *start = nullptr;
		std::string path = _path;
		if (K::strStartsWith(_path, PATH_DELIM)) {
			if (_start == nullptr || _start == getRoot()) {
				start = getRoot();
			} else {
				// start が指定されているモノの、絶対パスの検索なのでルートからの検索に変更
				K__WARNING("start param is overwriten with Root node");
				start = getRoot();
			}
			path = _path.substr(1); // 先頭の PATH_DELIM を取り除く

		} else {
			if (_start == nullptr) {
				start = getRoot();
			} else {
				start = _start;
			}
		}
		if (path.empty()) {
			return nullptr;
		}

		KNode *ret = nullptr;
		lock();
		ret = find_namepath_in_tree_unsafe(start, path);
		unlock();
		return ret;
	}
	KNode * find_namepath_in_tree_unsafe(const KNode *start, const std::string &path) const {
		K__ASSERT(start);
		K__ASSERT(!path.empty());
#if 1
		KNode *ret = start->findChildPath(path);
		if (ret) {
			return ret;
		} else {
			for (int i=0; i<start->getChildCount(); i++) {
				KNode *node = start->getChildFast(i);
				KNode *ret = find_namepath_in_tree_unsafe(node, path);
				if (ret) return ret;
			}
		}
#else
		for (int i=0; i<start->getChildCount(); i++) {
			KNode *node = start->getChildFast(i);
			if (has_name_in_tree_unsafe(node, path)) {
				return node;
			}
			KNode *sub = find_namepath_in_tree_unsafe(node, path);
			if (sub) {
				return sub;
			}
		}
#endif
		return nullptr;
	}
	bool has_name_in_tree_unsafe(const KNode *node, const std::string &path) const {
		if (node == nullptr) return false;
		if (path.empty()) return false;

		#if IGNORE_REMOVING_NODES
		// 削除マークがついている場合は、もう削除済みとして扱う
		if (node->hasFlag(KNode::FLAG__MARK_REMOVE)) {
			return false;
		}
		#endif
		
		// 与えられたパスの末尾部分と自分の名前が一致しないとダメ
		std::string last = K::pathGetLast(path);
		if (!node->hasName(last)) {
			return false;
		}

		// パスの親部分を得る
		std::string tmp = K::pathGetParent(path);

		// 親パスがないならここで比較終了
		if (tmp.empty()) {
			return true;
		}

		// 親パスはあるのに親ノードがいないなら不一致
		if (!node->getParent()) {
			return false;
		}

		// 親ノードと親パスを比較する
		return has_name_in_tree_unsafe(node->getParent(), tmp);
	}
	KNode * getRoot() const {
		return m_root;
	}
	void setGroupName(KNode::Category category, int group, const std::string &name) {
		lock();
		m_group_names[category][group] = name;
		unlock();
	}
	const char * getGroupName(KNode::Category category, int group) const {
		const char *ret = nullptr;
		lock();
		{
			auto category_it = m_group_names.find(category);
			if (category_it != m_group_names.end()) {
				const NAMES &names = category_it->second;
				auto it = names.find(group);
				if (it != names.end()) {
					ret = it->second.c_str();
				}
			}
		}
		unlock();
		return ret;
	}
	int getNodeList(KNodeArray *out_nodes) const {
		return m_id2node_map.get_vector(out_nodes);
	}
	int getNodeListByTag(KNodeArray *out_nodes, const KName &tag) const {
		int ret = 0;
		if (out_nodes) out_nodes->clear();
		auto it = m_tag_nodes.find(tag);
		if (it != m_tag_nodes.end()) {
			const std::unordered_set<KNode*> &set = it->second;
			if (out_nodes) {
				for (auto it=set.begin(); it!=set.end(); ++it) {
					out_nodes->push_back(*it);
				}
			}
			ret = set.size();
		}
		return ret;
	}
	bool makeNodesByPath(const std::string &path, KNode **out_node, bool singleton) {
		// パスを分解し、親を先に作成する
		KNode *parent = nullptr;
		if (K::pathHasDelim(path)) { // 親名を含んでいる
			std::string dir = K::pathGetParent(path);
			makeNodesByPath(dir, &parent, true); // 同名の親は複数作らない
			if (parent == nullptr) {
				return false;
			}
		} else {
			parent = KNodeTree::getRoot();
			if (parent == nullptr) {
				return false;
			}
			parent->grab(); // 辻褄合わせに grab する
		}

		// 子を作成する
		std::string name = K::pathGetLast(path);
		KNode *node = nullptr;
		if (singleton) {
			// すでに同名の子がある場合は再利用する
			node = parent->findChild(name.c_str());
			if (node == nullptr) {
				node = KNode::create();
				node->setName(name.c_str());
				node->setParent(parent);
			} else {
				node->grab(); // すでに同名の子が存在する。辻褄合わせに grab する
			}
		} else {
			// 同名の子を新規追加する
			node = KNode::create();
			node->setName(name.c_str());
			node->setParent(parent);
		}

		if (out_node) {
			*out_node = node;
		} else {
			node->drop();
		}
		parent->drop();
		return true;
	}
	void tick_nodes(KNodeTickFlags flags) {
		m_root->tick(flags);
		tick_signals();
	}
	void tick_nodes2(KNodeTickFlags flags) {
		m_root->tick2(flags);
		tick_signals();
	}
	void tick_system_nodes() {
		int num = m_root->getChildCount();
		for (int i=0; i<num; i++) {
			KNode *node = m_root->getChildFast(i);
			// ルート直下で KNode::FLAG_SYSTEM が付いているノードならば、そのノードと子ノードすべての
			// sys_tick メソッドを呼ぶ
		//	if (node->hasFlag(KNode::FLAG_SYSTEM)) {
				node->sys_tick();
		//	}
		}
	}
};
static void KNodeTree_on_node_enter(CNodeTreeImpl *tree, KNode *node) {
	K__ASSERT(tree);
	tree->_on_node_enter(node);
}
static void KNodeTree_on_node_exit(CNodeTreeImpl *tree, KNode *node) {
	K__ASSERT(tree);
	tree->_on_node_exit(node);
}
static void KNodeTree_on_node_removing(CNodeTreeImpl *tree, KNode *node) {
	K__ASSERT(tree);
	tree->_on_node_removing(node);
}
static void KNodeTree_add_tag(CNodeTreeImpl *tree, KNode *node, const KName &tag) {
	K__ASSERT(tree);
	tree->_add_tag(node, tag);
}
static void KNodeTree_del_tag(CNodeTreeImpl *tree, KNode *node, const KName &tag) {
	K__ASSERT(tree);
	tree->_del_tag(node, tag);
}
static void KNodeTree_on_node_marked_as_remove(CNodeTreeImpl *tree, KNode *node) {
	K__ASSERT(tree);
	tree->_on_node_marked_as_remove(node);
}
#pragma endregion // CNodeTreeImpl



static CNodeTreeImpl *g_NodeTree = nullptr;


#pragma region KNodeTree
void KNodeTree::install() {
	K__ASSERT(g_NodeTree == nullptr);
	g_NodeTree = new CNodeTreeImpl();
}
void KNodeTree::uninstall() {
	if (g_NodeTree) {
		delete g_NodeTree;
		g_NodeTree = nullptr;
	}
}
bool KNodeTree::isInstalled() {
	return g_NodeTree != nullptr;
}
void KNodeTree::addCallback(KNodeManagerCallback *cb) {
	K__ASSERT(g_NodeTree);
	g_NodeTree->addCallback(cb);
}
void KNodeTree::removeCallback(KNodeManagerCallback *cb) {
	K__ASSERT(g_NodeTree);
	g_NodeTree->removeCallback(cb);
}
void KNodeTree::destroyMarkedNodes(KNodeRemovingCallback *cb) {
	K__ASSERT(g_NodeTree);
	g_NodeTree->destroyMarkedNodes(cb);
}
KNode * KNodeTree::findNodeById(EID id) {
	K__ASSERT(g_NodeTree);
	return g_NodeTree->findNodeById(id);
}
KNode * KNodeTree::findNodeByName(const KNode *start, const std::string &name) {
	K__ASSERT(g_NodeTree);
	return g_NodeTree->findNodeByName(start, name);
}
KNode * KNodeTree::findNodeByPath(const KNode *start, const std::string &path) {
	K__ASSERT(g_NodeTree);
	return g_NodeTree->findNodeByPath(start, path);
}
KNode * KNodeTree::getRoot() {
	K__ASSERT(g_NodeTree);
	return g_NodeTree->getRoot();
}
void KNodeTree::setGroupName(KNode::Category category, int group, const std::string &name) {
	K__ASSERT(g_NodeTree);
	g_NodeTree->setGroupName(category, group, name);
}
const char * KNodeTree::getGroupName(KNode::Category category, int group) {
	K__ASSERT(g_NodeTree);
	return g_NodeTree->getGroupName(category, group);
}
void KNodeTree::tick_nodes(KNodeTickFlags flags) {
	K__ASSERT(g_NodeTree);
	g_NodeTree->tick_nodes(flags);
}
void KNodeTree::tick_nodes2(KNodeTickFlags flags) {
	K__ASSERT(g_NodeTree);
	g_NodeTree->tick_nodes2(flags);
}
void KNodeTree::tick_system_nodes() {
	K__ASSERT(g_NodeTree);
	g_NodeTree->tick_system_nodes();
}
void KNodeTree::broadcastSignal(KSig &sig) {
	K__ASSERT(g_NodeTree);
	g_NodeTree->broadcastSignal(sig);
}
void KNodeTree::sendSignal(KNode *target, KSig &sig) {
	K__ASSERT(g_NodeTree);
	g_NodeTree->sendSignal(target, sig);
}
void KNodeTree::sendSignalDelay(KNode *target, KSig &sig, int delay) {
	K__ASSERT(g_NodeTree);
	g_NodeTree->sendSignalDelay(target, sig, delay);
}
int KNodeTree::getNodeList(KNodeArray *out_nodes) {
	K__ASSERT(g_NodeTree);
	return g_NodeTree->getNodeList(out_nodes);
}
int KNodeTree::getNodeListByTag(KNodeArray *out_nodes, const KName &tag) {
	K__ASSERT(g_NodeTree);
	return g_NodeTree->getNodeListByTag(out_nodes, tag);
}
bool KNodeTree::makeNodesByPath(const std::string &path, KNode **out_node, bool singleton) {
	K__ASSERT(g_NodeTree);
	return g_NodeTree->makeNodesByPath(path, out_node, singleton);
}
void KNodeTree::_add_tag(KNode *node, const KName &tag) {
	K__ASSERT(g_NodeTree);
	g_NodeTree->_add_tag(node, tag);
}
void KNodeTree::_del_tag(KNode *node, const KName &tag) {
	K__ASSERT(g_NodeTree);
	g_NodeTree->_del_tag(node, tag);
}
void KNodeTree::_on_node_enter(KNode *node) {
	K__ASSERT(g_NodeTree);
	g_NodeTree->_on_node_enter(node);
}
void KNodeTree::_on_node_exit(KNode *node) {
	K__ASSERT(g_NodeTree);
	g_NodeTree->_on_node_exit(node);
}
void KNodeTree::_on_node_marked_as_remove(KNode *node) {
	K__ASSERT(g_NodeTree);
	g_NodeTree->_on_node_marked_as_remove(node);
}
void KNodeTree::_on_node_removing(KNode *node) {
	K__ASSERT(g_NodeTree);
	g_NodeTree->_on_node_removing(node);
}
#pragma endregion // KNodeTree





} // namespace
