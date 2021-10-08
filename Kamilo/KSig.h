#pragma once
#include <unordered_map>
#include "KAny.h"
#include "KColor.h"
#include "KString.h"
#include "KVec.h"

namespace Kamilo {

class KNode;

#define K_DECL_STRING(X)  static const char *X = #X;
#define K_DECL_SIGNAL(X)  K_DECL_STRING(X)

class KSig {
public:
	KSig();
	KSig(const KName &name);
	bool check(const KName &name) const;
	void setAny(const char *key, const KAny &val);
	void setBool(const char *key, bool val)           { setAny(key, val); }
	void setInt(const char *key, int val)             { setAny(key, val); }
	void setFloat(const char *key, float val)         { setAny(key, val); }
	void setColor(const char *key, const KColor &val) { setAny(key, val); }
	void setString(const char *key, const char *val)  { setAny(key, val); }
	void setString(const char *key, const std::string &val) { setAny(key, val.c_str()); }
	void setNode(const char *key, KNode *val)         { setAny(key, val); }
	void setPointer(const char *key, void *val)       { setAny(key, val); }
	const KAny & get(const char *key);
	bool getBool(const char *key);
	int getInt(const char *key, int def=0);
	float getFloat(const char *key, float def=0.0f);
	KColor getColor(const char *key, const KColor &def=KColor::ZERO);
	const char * getString(const char *key, const char *def=nullptr);
	std::string getStringStd(const char *key, const std::string &def="");
	KNode * getNode(const char *key);
	void * getPointer(const char *key);
	bool empty() const;
	void clear();

	KName mName;
	std::unordered_map<KName, KAny> mArgs;
	int mResult;
	int mResult_AyCharacterAttrs; // AyCharacterAttrs
	int mItemType_AyItemType; // AyItemType
	int mFaceEmo_AyFaceViewEmo; // AyFaceViewEmo
	int mFacePri_AyFaceViewPriority; // AyFaceViewPriority
	KNode *mBoss1_Node;
	KNode *mBoss2_Node;
	KColor mResult_Color;
	int mArg_EInputFlags;
	int mArg_ValueX;
	int mArg_ValueY;
	KPath mArg_InputButton;

	int value;
	int valueX;
	int valueY;
	int valueZ;
	int eroEnemyPosDeterminatedByPlayer;
	int eroAcceptInAir;
	int eroEnemyMuki;
	int eroTargetMuki;
	KVec3 eroIniEnemyPos;
	KVec3 eroIniTargetPos;
	KNode *eroEnemyNode;
	KNode *eroTargetNode;
	void *mArg_AyEroCommandArg;
	int eroTargetIsBuse;
};

} // namespace
