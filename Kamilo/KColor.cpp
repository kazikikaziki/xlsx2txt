#include "KColor.h"
#include "KInternal.h"


namespace Kamilo {

#if 1
#define _Min(a, b)  std::min(a, b)
#define _Max(a, b)  std::max(a, b)
static float _Clamp01(float x) {
	return (x < 0.0f) ? 0.0f : (x < 1.0f) ? x : 1.0f;
}
static int _Clampi(int t, int a, int b) {
	return (t < a) ? a : (t < b) ? t : b;
}
static int _Clamp255(int x) {
	return _Clampi(x, 0, 255);
}
static float _Lerpf(float x, float y, float t) {
	return x + (y - x) * _Clamp01(t);
}
static int _Lerpi(int x, int y, float t) {
	return (int)_Lerpf((float)x, (float)y, t);
}
static bool _Equals(float x, float y, float err) {
	return fabsf(x - y) <= err; // err=0 だった場合に対処するため必ず等号を含むこと
}
#else
static float _Min(float a, float b) {
	return a < b ? a : b;
}
static int _Min(int a, int b) {
	return a < b ? a : b;
}
static float _Max(float a, float b) {
	return a > b ? a : b;
}
static int _Max(int a, int b) {
	return a > b ? a : b;
}
static int _Clamp01(float x) {
	return (x < 0.0f) ? 0.0f : (x < 1.0f) ? x : 1.0f;
}
static int _Clamp255(int x) {
	return (x < 0) ? 0 : (x < 255) ? x : 255;
}
static float _Lerpf(float x, float y, float t) {
	return x + (y - x) * _Clamp01(t);
}
static int _Lerpi(int x, int y, float t) {
	return (int)_Lerpf((float)x, (float)y, t);
}
static bool _Equals(float x, float y, float err) {
	return fabsf(x - y) <= err; // err=0 だった場合に対処するため必ず等号を含むこと
}
#endif



#pragma region Color func
#define _R(x)    K_Color32GetR(x)
#define _G(x)    K_Color32GetG(x)
#define _B(x)    K_Color32GetB(x)
#define _A(x)    K_Color32GetA(x)
#define _INV(x)  (255 - (x))

uint32_t K_Color32FromRgba(int r, int g, int b, int a) {
	return ((a & 0xFF) << 24) | ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF);
}
uint32_t K_Color32FromRgbaF(float r, float g, float b, float a) {
	return K_Color32FromRgba(
		(int)(_Clamp01(r) * 255),
		(int)(_Clamp01(g) * 255),
		(int)(_Clamp01(b) * 255),
		(int)(_Clamp01(a) * 255)
	);
}
uint32_t K_Color32FromRgbaF(const float *rgba) {
	return K_Color32FromRgbaF(rgba[0], rgba[1], rgba[2], rgba[3]);
}
int K_Color32GetR(uint32_t color) {
	return (color >> 16) & 0xFF;
}
int K_Color32GetG(uint32_t color) {
	return (color >> 8) & 0xFF;
}
int K_Color32GetB(uint32_t color) {
	return color & 0xFF;
}
int K_Color32GetA(uint32_t color) {
	return (color >> 24) & 0xFF;
}
void K_Color32ToRgbaF(uint32_t color, float *rgba) {
	rgba[0] = K_Color32GetR(color) / 255.0f;
	rgba[1] = K_Color32GetG(color) / 255.0f;
	rgba[2] = K_Color32GetB(color) / 255.0f;
	rgba[3] = K_Color32GetA(color) / 255.0f;
}
uint32_t K_Color32SetR(uint32_t color, int r) {
	return color & 0xFF00FFFF | ((r & 0xFF) << 16);
}
uint32_t K_Color32SetG(uint32_t color, int g) {
	return color & 0xFFFF00FF | ((g & 0xFF) << 8);
}
uint32_t K_Color32SetB(uint32_t color, int b) {
	return color & 0xFFFFFF00 | (b & 0xFF);
}
uint32_t K_Color32SetA(uint32_t color, int a) {
	return color & 0xFF00FFFF | ((a & 0xFF) << 24);
}
uint32_t K_Color32BlendAdd(uint32_t bg, uint32_t fg) {
	int a = K_Color32GetA(fg);
	return K_Color32FromRgba(
		_Min(K_Color32GetR(bg) + K_Color32GetR(fg) * a / 255, 255),
		_Min(K_Color32GetG(bg) + K_Color32GetG(fg) * a / 255, 255),
		_Min(K_Color32GetB(bg) + K_Color32GetB(fg) * a / 255, 255),
		_Min(K_Color32GetA(bg) + K_Color32GetA(fg) * a / 255, 255)
	);
}
uint32_t K_Color32BlendSub(uint32_t bg, uint32_t fg) {
	int a = K_Color32GetA(fg);
	return K_Color32FromRgba(
		_Max(K_Color32GetR(bg) - K_Color32GetR(fg) * a / 255, 0),
		_Max(K_Color32GetG(bg) - K_Color32GetG(fg) * a / 255, 0),
		_Max(K_Color32GetB(bg) - K_Color32GetB(fg) * a / 255, 0),
		_Max(K_Color32GetA(bg) - K_Color32GetA(fg) * a / 255, 0)
	);
}
uint32_t K_Color32BlendMul(uint32_t bg, uint32_t fg) {
	int a = K_Color32GetA(fg);
	return K_Color32FromRgba(
		_Min(K_Color32GetR(bg) * K_Color32GetR(fg) * a / 255 / 255, 255),
		_Min(K_Color32GetG(bg) * K_Color32GetG(fg) * a / 255 / 255, 255),
		_Min(K_Color32GetB(bg) * K_Color32GetB(fg) * a / 255 / 255, 255),
		_Min(K_Color32GetA(bg) * K_Color32GetA(fg) * a / 255 / 255, 255)
	);
}
uint32_t K_Color32BlendMax(uint32_t bg, uint32_t fg) {
	return K_Color32FromRgba(
		_Max(K_Color32GetR(bg), K_Color32GetR(fg)),
		_Max(K_Color32GetG(bg), K_Color32GetG(fg)),
		_Max(K_Color32GetB(bg), K_Color32GetB(fg)),
		_Max(K_Color32GetA(bg), K_Color32GetA(fg))
	);
}
uint32_t K_Color32BlendAlpha(uint32_t bg, uint32_t fg) {
	int a = K_Color32GetA(fg);
	return K_Color32FromRgba(
		_Min(K_Color32GetR(bg) * (255 - a) + (K_Color32GetR(fg) * a) / 255, 255),
		_Min(K_Color32GetG(bg) * (255 - a) + (K_Color32GetG(fg) * a) / 255, 255),
		_Min(K_Color32GetB(bg) * (255 - a) + (K_Color32GetB(fg) * a) / 255, 255),
		_Min(K_Color32GetA(bg) * (255 - a) + (K_Color32GetA(fg) * a) / 255, 255)
	);
}
uint32_t K_Color32BlendScreen(uint32_t bg, uint32_t fg) {
	int a = K_Color32GetA(fg);
	return K_Color32FromRgba(
		_Min(K_Color32GetR(bg) + K_Color32GetR(fg) * (255 - K_Color32GetR(bg)) * a / 255 / 255, 255),
		_Min(K_Color32GetG(bg) + K_Color32GetG(fg) * (255 - K_Color32GetG(bg)) * a / 255 / 255, 255),
		_Min(K_Color32GetB(bg) + K_Color32GetB(fg) * (255 - K_Color32GetB(bg)) * a / 255 / 255, 255),
		_Min(K_Color32GetA(bg) + K_Color32GetA(fg) * (255 - K_Color32GetA(bg)) * a / 255 / 255, 255)
	);
}
uint32_t K_Color32Grayscale(uint32_t color) {
	int i = (K_Color32GetR(color) * 3 + K_Color32GetG(color) * 6 + K_Color32GetB(color) * 1) / 10;
	return K_Color32FromRgba(i, i, i, K_Color32GetA(color));
}

#undef _R
#undef _G
#undef _B
#undef _A
#undef _INV
#pragma endregion // Color func



#pragma region Color classes

#pragma region KColor static members
const KColor KColor::WHITE = KColor(1.0f, 1.0f, 1.0f, 1.0f);
const KColor KColor::BLACK = KColor(0.0f, 0.0f, 0.0f, 1.0f);
const KColor KColor::ZERO  = KColor(0.0f, 0.0f, 0.0f, 0.0f);

KColor KColor::lerp(const KColor &c1, const KColor &c2, float t) {
	return KColor(
		_Lerpf(c1.r, c2.r, t),
		_Lerpf(c1.g, c2.g, t),
		_Lerpf(c1.b, c2.b, t),
		_Lerpf(c1.a, c2.a, t)
	);
}
KColor KColor::getmax(const KColor &dst, const KColor &src) {
	return KColor(
		_Max(dst.r, src.r),
		_Max(dst.g, src.g),
		_Max(dst.b, src.b),
		_Max(dst.a, src.a)
	);
}
KColor KColor::getmin(const KColor &dst, const KColor &src) {
	return KColor(
		_Min(dst.r, src.r),
		_Min(dst.g, src.g),
		_Min(dst.b, src.b),
		_Min(dst.a, src.a)
	);
}
KColor KColor::add(const KColor &dst, const KColor &src) {
	return KColor(
		_Min(dst.r + src.r, 1.0f),
		_Min(dst.g + src.g, 1.0f),
		_Min(dst.b + src.b, 1.0f),
		_Min(dst.a + src.a, 1.0f)
	);
}
KColor KColor::addAlpha(const KColor &dst, const KColor &src) {
	return KColor(
		_Min(dst.r + src.r * src.a, 1.0f),
		_Min(dst.g + src.g * src.a, 1.0f),
		_Min(dst.b + src.b * src.a, 1.0f),
		_Min(dst.a + src.a,         1.0f)
	);
}
KColor KColor::addAlpha(const KColor &dst, const KColor &src, const KColor &factor) {
	return KColor(
		_Min(dst.r + src.r * src.a * factor.r * factor.a, 1.0f),
		_Min(dst.g + src.g * src.a * factor.g * factor.a, 1.0f),
		_Min(dst.b + src.b * src.a * factor.b * factor.a, 1.0f),
		_Min(dst.a + src.a,                               1.0f)
	);
}
KColor KColor::sub(const KColor &dst, const KColor &src) {
	return KColor(
		_Max(dst.r - src.r, 0.0f),
		_Max(dst.g - src.g, 0.0f),
		_Max(dst.b - src.b, 0.0f),
		_Max(dst.a - src.a, 0.0f)
	);
}
KColor KColor::subAlpha(const KColor &dst, const KColor &src) {
	return KColor(
		_Max(dst.r - src.r * src.a, 0.0f),
		_Max(dst.g - src.g * src.a, 0.0f),
		_Max(dst.b - src.b * src.a, 0.0f),
		_Max(dst.a - src.a,     0.0f)
	);
}
KColor KColor::subAlpha(const KColor &dst, const KColor &src, const KColor &factor) {
	return KColor(
		_Max(dst.r - src.r * src.a * factor.r * factor.a, 0.0f),
		_Max(dst.g - src.g * src.a * factor.g * factor.a, 0.0f),
		_Max(dst.b - src.b * src.a * factor.b * factor.a, 0.0f),
		_Max(dst.a - src.a,                               0.0f)
	);
}
KColor KColor::mul(const KColor &dst, const KColor &src) {
	return KColor(
		_Min(dst.r * src.r, 1.0f),
		_Min(dst.g * src.g, 1.0f),
		_Min(dst.b * src.b, 1.0f),
		_Min(dst.a * src.a, 1.0f)
	);
}
KColor KColor::blendAlpha(const KColor &dst, const KColor &src) {
	float a = src.a;
	return KColor(
		_Min(dst.r * (1.0f - a) + src.r * a, 1.0f),
		_Min(dst.g * (1.0f - a) + src.g * a, 1.0f),
		_Min(dst.b * (1.0f - a) + src.b * a, 1.0f),
		_Min(dst.a * (1.0f - a) +         a, 1.0f)
	);
}
KColor KColor::blendAlpha(const KColor &dst, const KColor &src, const KColor &factor) {
	float a = src.a * factor.a;
	return KColor(
		_Min(dst.r * (1.0f - a) + src.r * factor.r * a, 1.0f),
		_Min(dst.g * (1.0f - a) + src.g * factor.g * a, 1.0f),
		_Min(dst.b * (1.0f - a) + src.b * factor.b * a, 1.0f),
		_Min(dst.a * (1.0f - a) +                    a, 1.0f)
	);
}
#pragma endregion KColor static members


#pragma region KColor
KColor::KColor() {
	r = 0;
	g = 0;
	b = 0;
	a = 0;
}
KColor::KColor(float _r, float _g, float _b, float _a) {
	r = _r;
	g = _g;
	b = _b;
	a = _a;
#ifdef _DEBUG
	K__ASSERT(r >= 0 && g >= 0 && b >= 0 && a >= 0);
#endif
}
KColor::KColor(const KColor &rgb, float _a) {
	r = rgb.r;
	g = rgb.g;
	b = rgb.b;
	a = _a;
#ifdef _DEBUG
	K__ASSERT(r >= 0 && g >= 0 && b >= 0 && a >= 0);
#endif
}
KColor::KColor(const KColor32 &argb32) {
	r = (float)argb32.r / 255.0f;
	g = (float)argb32.g / 255.0f;
	b = (float)argb32.b / 255.0f;
	a = (float)argb32.a / 255.0f;
#ifdef _DEBUG
	K__ASSERT(r >= 0 && g >= 0 && b >= 0 && a >= 0);
#endif
}
KColor32 KColor::toColor32() const {
	int ur = (int)(_Clamp01(r) * 255.0f);
	int ug = (int)(_Clamp01(g) * 255.0f);
	int ub = (int)(_Clamp01(b) * 255.0f);
	int ua = (int)(_Clamp01(a) * 255.0f);
	return KColor32(ur, ug, ub, ua);
}

float * KColor::floats() const {
	return (float *)this;
}
float KColor::grayscale() const {
	// R: 0.298912
	// G: 0.586611
	// B: 0.114478
	return r * 0.3f + g * 0.6f + b * 0.1f;
}
KColor KColor::clamp() const {
	return KColor(
		_Clamp01(r),
		_Clamp01(g),
		_Clamp01(b),
		_Clamp01(a));
}
KColor KColor::operator + (const KColor &op) const {
	return KColor(
		r + op.r,
		g + op.g,
		b + op.b,
		a + op.a);
}
KColor KColor::operator + (float k) const {
	return KColor(r+k, g+k, b+k, a+k);
}
KColor KColor::operator - (const KColor &op) const {
	return KColor(
		_Clamp01(r - op.r),
		_Clamp01(g - op.g),
		_Clamp01(b - op.b),
		_Clamp01(a - op.a));
}
KColor KColor::operator - (float k) const {
	return KColor(r-k, g-k, b-k, a-k);
}
KColor KColor::operator - () const {
	return KColor(-r, -g, -b, -a);
}
KColor KColor::operator * (const KColor &op) const {
	return KColor(
		r * op.r,
		g * op.g,
		b * op.b,
		a * op.a);
}
KColor KColor::operator * (float k) const {
	return KColor(r*k, g*k, b*k, a*k);
}
KColor KColor::operator / (float k) const {
	return KColor(r/k, g/k, b/k, a/k);
}
KColor KColor::operator / (const KColor &op) const {
	return KColor(
		r / op.r,
		g / op.g,
		b / op.b,
		a / op.a);
}
bool KColor::equals(const KColor &other, float tolerance) const {
	float dr = fabsf(other.r - r);
	float dg = fabsf(other.g - g);
	float db = fabsf(other.b - b);
	float da = fabsf(other.a - a);
	return (dr <= tolerance) && (dg <= tolerance) && (db <= tolerance) && (da <= tolerance);
}
bool KColor::operator == (const KColor &op) const {
	return 
		_Equals(this->r, op.r, K_MAX_COLOR_ERROR) &&
		_Equals(this->g, op.g, K_MAX_COLOR_ERROR) &&
		_Equals(this->b, op.b, K_MAX_COLOR_ERROR) &&
		_Equals(this->a, op.a, K_MAX_COLOR_ERROR);
}
bool KColor::operator != (const KColor &op) const {
	return !(*this == op);
}
KColor & KColor::operator += (const KColor &op) {
	r += op.r;
	g += op.g;
	b += op.b;
	a += op.a;
	return *this;
}
KColor & KColor::operator += (float k) {
	r += k;
	g += k;
	b += k;
	a += k;
	return *this;
}
KColor & KColor::operator -= (const KColor &op) {
	r -= op.r;
	g -= op.g;
	b -= op.b;
	a -= op.a;
	return *this;
}
KColor & KColor::operator -= (float k) {
	r -= k;
	g -= k;
	b -= k;
	a -= k;
	return *this;
}
KColor & KColor::operator *= (const KColor &op) {
	r *= op.r;
	g *= op.g;
	b *= op.b;
	a *= op.a;
	return *this;
}
KColor & KColor::operator *= (float k) {
	r *= k;
	g *= k;
	b *= k;
	a *= k;
	return *this;
}
KColor & KColor::operator /= (const KColor &op) {
	r /= op.r;
	g /= op.g;
	b /= op.b;
	a /= op.a;
	return *this;
}
KColor & KColor::operator /= (float k) {
	r /= k;
	g /= k;
	b /= k;
	a /= k;
	return *this;
}
#pragma endregion // KColor


#pragma region KColor32
const KColor32 KColor32::WHITE = KColor32(0xFF, 0xFF, 0xFF, 0xFF);
const KColor32 KColor32::BLACK = KColor32(0x00, 0x00, 0x00, 0xFF);
const KColor32 KColor32::ZERO  = KColor32(0x00, 0x00, 0x00, 0x00);

KColor32 KColor32::getmax(const KColor32 &dst, const KColor32 &src) {
	return KColor32(
		_Max(dst.r, src.r),
		_Max(dst.g, src.g),
		_Max(dst.b, src.b),
		_Max(dst.a, src.a)
	);
}
KColor32 KColor32::getmin(const KColor32 &dst, const KColor32 &src) {
	return KColor32(
		_Min(dst.r, src.r),
		_Min(dst.g, src.g),
		_Min(dst.b, src.b),
		_Min(dst.a, src.a)
	);
}
KColor32 KColor32::add(const KColor32 &dst, const KColor32 &src) {
	return KColor32(
		_Min((int)dst.r + src.r, 255),
		_Min((int)dst.g + src.g, 255),
		_Min((int)dst.b + src.b, 255),
		_Min((int)dst.a + src.a, 255)
	);
}
KColor32 KColor32::addAlpha(const KColor32 &dst, const KColor32 &src) {
	int a = src.a;
	return KColor32(
		(int)dst.r + src.r * a / 255,
		(int)dst.g + src.g * a / 255,
		(int)dst.b + src.b * a / 255,
		(int)dst.a + src.a
	);
}
KColor32 KColor32::addAlpha(const KColor32 &dst, const KColor32 &src, const KColor32 &factor) {
	int a = src.a * factor.a / 255;
	return KColor32(
		(int)dst.r + src.r * a * factor.r / 255 / 255,
		(int)dst.g + src.g * a * factor.g / 255 / 255,
		(int)dst.b + src.b * a * factor.b / 255 / 255,
		(int)dst.a + src.a
	);
}
KColor32 KColor32::sub(const KColor32 &dst, const KColor32 &src) {
	return KColor32(
		_Max((int)dst.r - src.r, 0),
		_Max((int)dst.g - src.g, 0),
		_Max((int)dst.b - src.b, 0),
		_Max((int)dst.a - src.a, 0)
	);
}
KColor32 KColor32::subAlpha(const KColor32 &dst, const KColor32 &src) {
	int a = src.a;
	return KColor32(
		(int)dst.r - src.r * a / 255,
		(int)dst.g - src.g * a / 255,
		(int)dst.b - src.b * a / 255,
		(int)dst.a - src.a
	);
}
KColor32 KColor32::subAlpha(const KColor32 &dst, const KColor32 &src, const KColor32 &factor) {
	int a = (int)src.a * (int)factor.a / 255;
	return KColor32(
		(int)dst.r - src.r * a * factor.r / 255 / 255,
		(int)dst.g - src.g * a * factor.g / 255 / 255,
		(int)dst.b - src.b * a * factor.b / 255 / 255,
		(int)dst.a - src.a
	);
}
KColor32 KColor32::mul(const KColor32 &dst, const KColor32 &src) {
	// src の各要素の値 0 .. 255 を 0.0 .. 1.0 にマッピングして乗算する
	int r = _Clamp255(dst.r * src.r / 255);
	int g = _Clamp255(dst.g * src.g / 255);
	int b = _Clamp255(dst.b * src.b / 255);
	int a = _Clamp255(dst.a * src.a / 255);
	return KColor32(r, g, b, a);
}
KColor32 KColor32::mul(const KColor32 &dst, float factor) {
	int r = _Clamp255((int)(dst.r * factor));
	int g = _Clamp255((int)(dst.g * factor));
	int b = _Clamp255((int)(dst.b * factor));
	int a = _Clamp255((int)(dst.a * factor));
	return KColor32(r, g, b, a);
}
KColor32 KColor32::blendAlpha(const KColor32 &dst, const KColor32 &src) {
	int a = src.a;
	return KColor32(
		((int)dst.r * (255 - a) + src.r * a) / 255,
		((int)dst.g * (255 - a) + src.g * a) / 255,
		((int)dst.b * (255 - a) + src.b * a) / 255,
		((int)dst.a * (255 - a) + 255   * a) / 255
	);
}
KColor32 KColor32::blendAlpha(const KColor32 &dst, const KColor32 &src, const KColor32 &factor) {
	return blendAlpha(dst, mul(src, factor));
}
KColor32 KColor32::lerp(const KColor32 &c1, const KColor32 &c2, float t) {
	t = _Clamp01(t);
	return KColor32(
		_Lerpi(c1.r, c2.r, t),
		_Lerpi(c1.g, c2.g, t),
		_Lerpi(c1.b, c2.b, t),
		_Lerpi(c1.a, c2.a, t)
	);
}
KColor32 KColor32::fromARGB32(uint32_t argb32) {
	int a = (argb32 >> 24) & 0xFF;
	int r = (argb32 >> 16) & 0xFF;
	int g = (argb32 >>  8) & 0xFF;
	int b = (argb32 >>  0) & 0xFF;
	return KColor32(r, g, b, a);
}
KColor32 KColor32::fromXRGB32(uint32_t argb32) {
	int r = (argb32 >> 16) & 0xFF;
	int g = (argb32 >>  8) & 0xFF;
	int b = (argb32 >>  0) & 0xFF;
	return KColor32(r, g, b, 0xFF);
}
KColor32::KColor32() {
	a = 0;
	r = 0;
	g = 0;
	b = 0;
}
KColor32::KColor32(const KColor32 &other) {
	r = other.r;
	g = other.g;
	b = other.b;
	a = other.a;
}
KColor32::KColor32(int _r, int _g, int _b, int _a) {
	a = (uint8_t)(_a & 0xFF);
	r = (uint8_t)(_r & 0xFF);
	g = (uint8_t)(_g & 0xFF);
	b = (uint8_t)(_b & 0xFF);
}
KColor32::KColor32(uint32_t argb) { 
	a = (uint8_t)((argb >> 24) & 0xFF);
	r = (uint8_t)((argb >> 16) & 0xFF);
	g = (uint8_t)((argb >>  8) & 0xFF);
	b = (uint8_t)((argb >>  0) & 0xFF);
}
KColor32::KColor32(const KColor &color) {
#if 1
	a = (uint8_t)(color.a * 255.0f);
	r = (uint8_t)(color.r * 255.0f);
	g = (uint8_t)(color.g * 255.0f);
	b = (uint8_t)(color.b * 255.0f);
#else
	a = (uint8_t)(_Clamp01(color.a) * 255.0f);
	r = (uint8_t)(_Clamp01(color.r) * 255.0f);
	g = (uint8_t)(_Clamp01(color.g) * 255.0f);
	b = (uint8_t)(_Clamp01(color.b) * 255.0f);
#endif
}
bool KColor32::operator == (const KColor32 &other) const {
	return (
		r==other.r && 
		g==other.g &&
		b==other.b &&
		a==other.a
	);
}
bool KColor32::operator != (const KColor32 &other) const {
	return !(*this==other);
}
KColor KColor32::toColor() const {
	return KColor(
		_Clamp01(r/255.0f),
		_Clamp01(g/255.0f),
		_Clamp01(b/255.0f),
		_Clamp01(a/255.0f)
	);
}
uint32_t KColor32::toUInt32() const {
	return (a << 24) | (r << 16) | (g << 8) | b;
}
uint8_t KColor32::grayscale() const {
	// NTSC 加重平均でのグレースケール値を計算する。
	// 入力値も出力値も整数なので、簡易版の係数を使う。
	// 途中式での桁あふれに注意
	//return (uint8_t)(((int)r * 3 + g * 6 + b * 1) / 10);
	return (uint8_t)((306 * r + 601 * g + 117 + b) >> 10); // <--　306とか601が既にuint8の範囲外なので、勝手にintに昇格する
}


namespace Test {
void Test_color() {
	K__VERIFY(K_Color32FromRgba(0x22, 0x44, 0x66, 0xFF) == 0xFF224466);
	K__VERIFY(K_Color32BlendAdd(0xFF223399, 0xFF224499) == 0xFF4477FF);
//	K__VERIFY(K_Color32BlendAdd(0xFF223399, 0x7F224488) == 0xFF3355DD);
}
} // Test

#pragma endregion // KColor32

#pragma endregion // Color classes




} // namespace
