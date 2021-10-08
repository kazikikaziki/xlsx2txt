#include "KAny.h"
namespace Kamilo {

KAny::KAny() {
	m_Type = TP_NONE;
	m_Value.p = 0;
}
KAny::KAny(bool val) {
	m_Type = TP_INT;
	m_Value.i = val;
}
KAny::KAny(int val) {
	m_Type = TP_INT;
	m_Value.i = val;
}
KAny::KAny(float val) {
	m_Type = TP_FLT;
	m_Value.f = val;
}
KAny::KAny(const char *val) {
	m_Type = TP_STR;
	m_Value.s = val;
}
KAny::KAny(void *val) {
	m_Type = TP_PTR;
	m_Value.p = val;
}
KAny::KAny(KNode *n) {
	m_Type = TP_NOD;
	m_Value.n = n;
}
KAny::KAny(const KColor &color) {
	m_Type = TP_COL;
	m_Value.f4[0] = color.r;
	m_Value.f4[1] = color.g;
	m_Value.f4[2] = color.b;
	m_Value.f4[3] = color.a;
}
bool KAny::isInt() const {
	return m_Type == TP_INT;
}
bool KAny::isFloat() const {
	return m_Type == TP_FLT;
}
bool KAny::isNumber() const {
	return isInt() || isFloat();
}
bool KAny::isString() const {
	return m_Type == TP_STR;
}
bool KAny::isPointer() const {
	return m_Type == TP_PTR;
}
bool KAny::isNode() const {
	return m_Type == TP_NOD;
}
bool KAny::isColor() const {
	return m_Type == TP_COL;
}
bool KAny::getBool(bool def) const {
	return getInt(def) != 0; 
}
int KAny::getInt(int def) const {
	if (isInt()) {
		return m_Value.i;
	}
	if (isFloat()) {
		return (int)m_Value.f;
	}
	return def;
}
float KAny::getFloat(float def) const {
	if (isFloat()) {
		return m_Value.f;
	}
	if (isInt()) {
		return (float)m_Value.i;
	}
	return def;
}
KColor KAny::getColor(const KColor &def) const {
	if (isColor()) {
		return KColor(
			m_Value.f4[0],
			m_Value.f4[1],
			m_Value.f4[2],
			m_Value.f4[3]
		);
	}
	return def;
}
const char * KAny::getString(const char *def) const {
	if (isString()) {
		return m_Value.s;
	}
	return def;
}
void * KAny::getPointer() const {
	if (isPointer()) {
		return m_Value.p;
	}
	return nullptr;
}
KNode * KAny::getNode() const {
	if (isNode()) {
		return m_Value.n;
	}
	return nullptr;
}

} // namespace
