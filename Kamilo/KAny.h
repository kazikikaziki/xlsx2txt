#pragma once
#include "KColor.h"

namespace Kamilo {
class KNode;


struct KAny {
	KAny();
	KAny(bool val);
	KAny(int val);
	KAny(float val);
	KAny(const char *val);
	KAny(void *val);
	KAny(KNode *n);
	KAny(const KColor &color);
	bool isInt() const;
	bool isFloat() const;
	bool isNumber() const;
	bool isString() const;
	bool isPointer() const;
	bool isNode() const;
	bool isColor() const;
	bool         getBool(bool def=false) const;
	int          getInt(int def=0) const;
	float        getFloat(float def=0.0f) const;
	KColor       getColor(const KColor &def=KColor::ZERO) const;
	const char * getString(const char *def="") const;
	void *       getPointer() const;
	KNode *      getNode() const;

private:
	enum Type {
		TP_NONE,
		TP_INT,
		TP_FLT,
		TP_STR,
		TP_PTR,
		TP_NOD,
		TP_COL,
	};
	Type m_Type;
	union {
		float f;
		int   i;
		void *p;
		const char *s;
		KNode *n;
		float f4[4];
	} m_Value;
};

} // namespace