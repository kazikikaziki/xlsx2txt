#include "KSig.h"
#include "KInternal.h"

namespace Kamilo {

KSig::KSig() {
	clear();
}
KSig::KSig(const KName &name) {
	clear();
	mName = name;
}
bool KSig::empty() const {
	return mName.empty();
}
void KSig::clear() {
	mName = "";
	mArgs.clear();

	mResult = 0;
	mResult_AyCharacterAttrs = 0;
	mItemType_AyItemType = 0;
	mFaceEmo_AyFaceViewEmo = 0;
	mFacePri_AyFaceViewPriority = 0;
	mBoss1_Node = nullptr;
	mBoss2_Node = nullptr;
	mArg_EInputFlags = 0;
	mArg_ValueX = 0;
	mArg_ValueY = 0;

	eroEnemyMuki = 0;
	eroTargetMuki = 0;
	value = 0;
	valueX = 0;
	valueY = 0;
	valueZ = 0;
	eroEnemyPosDeterminatedByPlayer = 0;
	mArg_AyEroCommandArg = nullptr;
	eroTargetIsBuse = 0;
	eroEnemyNode = nullptr;
	eroTargetNode = nullptr;
}
bool KSig::check(const KName &name) const {
	return mName == name;
}
void KSig::setAny(const char *key, const KAny &val) {
	mArgs[key ? key : ""] = val;
}
const KAny & KSig::get(const char *key) {
	if (key && key[0]) {
		auto it = mArgs.find(key);
		if (it != mArgs.end()) {
			return it->second;
		}
	}
	K__WARNING("KSig: \"%s\" not found", key);
	static const KAny s_dummy;
	return s_dummy;
}
bool KSig::getBool(const char *key) {
	return get(key).getBool();
}
int KSig::getInt(const char *key, int def) {
	return get(key).getInt(def);
}
float KSig::getFloat(const char *key, float def) {
	return get(key).getFloat(def);
}
const char * KSig::getString(const char *key, const char *def) {
	return get(key).getString(def);
}
std::string KSig::getStringStd(const char *key, const std::string &def) {
	const char *s = get(key).getString(def.c_str());
	return s ? s : "";
}
KColor KSig::getColor(const char *key, const KColor &def) {
	return get(key).getColor(def);
}
KNode * KSig::getNode(const char *key) {
	return get(key).getNode();
}
void * KSig::getPointer(const char *key) {
	return get(key).getPointer();
}

} // namespace
