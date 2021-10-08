#include "KEasing.h"

#define _USE_MATH_DEFINES // M_PI
#include <math.h>
#include <algorithm>
#include "KInternal.h"


namespace Kamilo {

static const float _pi = (float)M_PI;

inline float _clamp01(float x) {
	return (x < 0.0f) ? 0.0f : (x < 1.0f) ? x : 1.0f;
}
inline float _lerp_unclamped(float a, float b, float t) {
	return (a + (b - a) * t);
}

#pragma region KEasing static
KEasing::Func01 KEasing::getfunc01(KEasing::Expr expr) {
	static const Func01 s_table[EXPR_ENUM_MAX] {
		KEasing::one01,        // EXPR_ONE
		KEasing::keep01,       // EXPR_KEEP
		KEasing::step01,       // EXPR_STEP
		KEasing::linear01,     // EXPR_LINEAR
	
		KEasing::inSine01,     // EXPR_INSINE
		KEasing::outSine01,    // EXPR_OUTSINE
		KEasing::inOutSine01,  // EXPR_INOUTSINE
	
		KEasing::inQuad01,     // EXPR_INQUAD
		KEasing::outQuad01,    // EXPR_OUTQUAD
		KEasing::inOutQuad01,  // EXPR_INOUTQUAD
	
		KEasing::inExp01,      // EXPR_INEXP
		KEasing::outExp01,     // EXPR_OUTEXP
		KEasing::inOutExp01,   // EXPR_INOUTEXP
	
		KEasing::inCubic01,    // EXPR_INCUBIC
		KEasing::outCubic01,   // EXPR_OUTCUBIC
		KEasing::inOutCubic01, // EXPR_INOUTCUBIC
	
		KEasing::inQuart01,    // EXPR_INQUART
		KEasing::outQuart01,   // EXPR_OUTQUART
		KEasing::inOutQuart01, // EXPR_INOUTQUART
	
		KEasing::inQuint01,    // EXPR_INQUINT
		KEasing::outQuint01,   // EXPR_OUTQUINT
		KEasing::inOutQuint01, // EXPR_INOUTQUINT
	
		KEasing::inCirc01,     // EXPR_INCIRC
		KEasing::outCirc01,    // EXPR_OUTCIRC
		KEasing::inOutCirc01,  // EXPR_INOUTCIRC
	
		KEasing::inBack01,     // EXPR_INBACK
		KEasing::outBack01,    // EXPR_OUTBACK
		KEasing::inOutBack01,  // EXPR_INOUTBACK
	};
	return s_table[expr];
}

float KEasing::one01(float t) {
	return 1.0f;
}
float KEasing::keep01(float t) {
	return (t < 1.0f) ? 0.0f : 1.0f;
}
float KEasing::step01(float t) {
	return (t <= 0.0f) ? 0.0f : 1.0f;
}
float KEasing::linear01(float t) {
	t = _clamp01(t);
	return t; // 初期値 0 終了値 1 の線形補間。つまり t をそのまま返す
}
float KEasing::inSine01(float t) {
	t = _clamp01(t);
	return 1 - cosf(t * _pi / 2.0f);
}
float KEasing::outSine01(float t) {
	t = _clamp01(t);
	return sinf(t * _pi / 2.0f);
}
float KEasing::inOutSine01(float t) {
	t = _clamp01(t);
	return 0.5f * (1.0f - cosf(t * _pi));
}
float KEasing::inQuad01(float t) {
	t = _clamp01(t);
	return t * t;
}
float KEasing::outQuad01(float t) {
	t = _clamp01(t);
	float T = 1.0f - t;
	return 1.0f - T * T;
}
float KEasing::inOutQuad01(float t) {
	return (t < 0.5f) ?
		0.5f * inQuad01(t*2) :
		0.5f + 0.5f * outQuad01(t*2-1);
}
float KEasing::inExp01(float t) {
	t = _clamp01(t);
	return powf(2.0f, 10.0f * (t - 1.0f));
}
float KEasing::outExp01(float t) {
	t = _clamp01(t);
	return (1.0f - powf(2.0f, -10.0f * t));
}
float KEasing::inOutExp01(float t) {
	return (t < 0.5f) ?
		0.5f * inExp01(t*2) :
		0.5f + 0.5f * outExp01(t*2-1);
}
float KEasing::inCubic01(float t) {
	t = _clamp01(t);
	return t * t * t;
}
float KEasing::outCubic01(float t) {
	t = _clamp01(t);
	float T = 1.0f - t;
	return 1.0f - T * T * T;
}
float KEasing::inOutCubic01(float t) {
	return (t < 0.5f) ?
		0.5f * inCubic01(t*2):
		0.5f + 0.5f * outCubic01(t*2-1);
}
float KEasing::inQuart01(float t) {
	t = _clamp01(t);
	return t * t * t * t;
}
float KEasing::outQuart01(float t) {
	t = _clamp01(t);
	float T = 1.0f - t;
	return 1.0f - T * T * T * T;
}
float KEasing::inOutQuart01(float t) {
	return (t < 0.5f) ?
		0.5f * inQuart01(t*2) :
		0.5f + 0.5f * outQuart01(t*2-1);
}
float KEasing::inQuint01(float t) {
	t = _clamp01(t);
	return t * t * t * t * t;
}
float KEasing::outQuint01(float t) {
	t = _clamp01(t);
	float T = 1.0f - t;
	return 1.0f - T * T * T * T * T;
}
float KEasing::inOutQuint01(float t) {
	return (t < 0.5f) ?
		0.5f * inQuint01(t*2) :
		0.5f + 0.5f * outQuint01(t*2-1);
}
float KEasing::inCirc01(float t) {
	t = _clamp01(t);
	return 0 - (sqrtf(1.0f - t * t) - 1.0f);
}
float KEasing::outCirc01(float t) {
	t = _clamp01(t);
	float T = t - 1.0f;
	return sqrtf(1.0f - T * T);
}
float KEasing::inOutCirc01(float t) {
	return (t < 0.5f) ?
		0.5f * inCirc01(t*2) :
		0.5f + 0.5f * outCirc01(t*2-1);
}
float KEasing::inBack01(float t) {
	return inBackEx01(t, KEasing::BACK_10);
}
float KEasing::outBack01(float t) {
	return outBackEx01(t, KEasing::BACK_10);
}
float KEasing::inOutBack01(float t) {
	return inOutBackEx01(t, KEasing::BACK_10);
}
float KEasing::inBackEx01(float t, float s) {
	// https://github.com/kikito/tween.lua/blob/master/tween.lua
	t = _clamp01(t);
	return t * t * ((s + 1.0f) * t - s);
}
float KEasing::outBackEx01(float t, float s) {
	t = _clamp01(t);
	float T = t - 1.0f;
	return T * T * ((s + 1.0f) * T + s) + 1.0f;
}
float KEasing::inOutBackEx01(float t, float s) {
	float S = s * 1.525f;
	return (t < 0.5f) ? 
		0.5f * inBackEx01(t*2, S) :
		0.5f + 0.5f * outBackEx01(t*2-1, S);
}
float KEasing::hermite(float t, float v0, float v1, float slope0, float slope1) {
	t = _clamp01(t);
	float a =  2 * (v0 - v1) + slope0 + slope1;
	float b = -3 * (v0 - v1) - 2 * slope0 + slope1;
	float c = slope0;
	float d = v0;
	float t2 = t * t;
	float t3 = t * t2;
	return a * t3  + b * t2 + c * t + d; 
}
float KEasing::one(float t, float a, float b) {
	return linear(one01(t), a, b); // return b; と同じ
}
float KEasing::keep(float t, float a, float b) {
	return linear(keep01(t), a, b); // return (t < 1.0f) ? a : b; と同じ
}
float KEasing::step(float t, float a, float b) {
	return linear(step01(t), a, b); // return (t <= 0.0f) ? a : b; と同じ
}
float KEasing::linear(float t, float a, float b) {
	t = _clamp01(t);
	return a + (b - a) * t;
}
float KEasing::inSine(float t, float a, float b) {
	return linear(inSine01(t), a, b);
}
float KEasing::outSine(float t, float a, float b) {
	return linear(outSine01(t), a, b);
}
float KEasing::inOutSine(float t, float a, float b) {
	return linear(inOutSine01(t), a, b);
}
float KEasing::inQuad(float t, float a, float b) {
	return linear(inQuad01(t), a, b);
}
float KEasing::outQuad(float t, float a, float b) {
	return linear(outQuad01(t), a, b);
}
float KEasing::inOutQuad(float t, float a, float b) {
	return linear(inOutQuad01(t), a, b);
}
float KEasing::inExp(float t, float a, float b) {
	return linear(inExp01(t), a, b);
}
float KEasing::outExp(float t, float a, float b) {
	return linear(outExp01(t), a, b);
}
float KEasing::inOutExp(float t, float a, float b) {
	return linear(inOutExp01(t), a, b);
}
float KEasing::inCubic(float t, float a, float b) {
	return linear(inCubic01(t), a, b);
}
float KEasing::outCubic(float t, float a, float b) {
	return linear(outCubic01(t), a, b);
}
float KEasing::inOutCubic(float t, float a, float b) {
	return linear(inOutCubic01(t), a, b);
}
float KEasing::inQuart(float t, float a, float b) {
	return linear(inQuart01(t), a, b);
}
float KEasing::outQuart(float t, float a, float b) {
	return linear(outQuart01(t), a, b);
}
float KEasing::inOutQuart(float t, float a, float b) {
	return linear(inOutQuart01(t), a, b);
}
float KEasing::inQuint(float t, float a, float b) {
	return linear(inQuint01(t), a, b);
}
float KEasing::outQuint(float t, float a, float b) {
	return linear(outQuint01(t), a, b);
}
float KEasing::inOutQuint(float t, float a, float b) {
	return linear(inOutQuint01(t), a, b);
}
float KEasing::inCirc(float t, float a, float b) {
	return linear(inCirc01(t), a, b);
}
float KEasing::outCirc(float t, float a, float b) {
	return linear(outCirc01(t), a, b);
}
float KEasing::inOutCirc(float t, float a, float b) {
	return linear(inOutCirc01(t), a, b);
}
float KEasing::inBack(float t, float a, float b, float s) {
	return linear(inBack01(t), a, b);
}
float KEasing::outBack(float t, float a, float b, float s) {
	return linear(outBack01(t), a, b);
}
float KEasing::inOutBack(float t, float a, float b, float s) {
	return linear(inOutBack01(t), a, b);
}
float KEasing::wave(float t, float a, float b) {
	float k = 0.5f + 0.5f * -cosf(2 * _pi * t);
	return linear(k, a, b);
}
float KEasing::easing01(float t, Expr expr) {
	Func01 func = getfunc01(expr);
	return func(t);
}
float KEasing::easing(float t, float a, float b, Expr expr) {
	Func01 func = getfunc01(expr);
	return _lerp_unclamped(a, b, func(t));
}
#pragma endregion // KEasing static



#pragma region KEasing
KEasing::KEasing() {
	clear();
}
void KEasing::clear() {
	m_Keys.clear();
}
KEasing & KEasing::addKey(int time, float value, Expr expr) {
	// 指定時刻にすでにキーがあるなら消す
	{
		int i = getIndexByTime(time);
		if (i >= 0) {
			m_Keys.erase(m_Keys.begin() + i);
		}
	}
	// キーを追加
	m_Keys.push_back(TimeKey(time, value, expr));

	// 時間順でソート
	std::sort(m_Keys.begin(), m_Keys.end(), SortByTime());
	return *this;
}
int KEasing::getDuration() const {
	return m_Keys.size() ? m_Keys.back().time : 0;
}
int KEasing::getKeyCount() const {
	return (int)m_Keys.size();
}
void KEasing::setKey(int index, int *time, float *value, Expr *expr) {
	if (0 <= index && index < (int)m_Keys.size()) {
		if (time)  m_Keys[index].time  = *time;
		if (value) m_Keys[index].value = *value;
		if (expr)  m_Keys[index].expr  = *expr;
		std::sort(m_Keys.begin(), m_Keys.end(), SortByTime());
	}
}
bool KEasing::getKey(int index, int *time, float *value, Expr *expr) const {
	if (0 <= index && index < (int)m_Keys.size()) {
		if (time ) *time  = m_Keys[index].time;
		if (value) *value = m_Keys[index].value;
		if (expr ) *expr  = m_Keys[index].expr;
		return true;
	}
	return false;
}
float KEasing::getValue(int time) const {
	if (m_Keys.size() == 0) {
		return 0.0f;
	}
	if (time < m_Keys.front().time) {
		return m_Keys.front().value;
	}
	if (time >= m_Keys.back().time) {
		return m_Keys.back().value;
	}
	int i0 = 0;
	for (size_t i=0; i<m_Keys.size(); i++) {
		const TimeKey &key1 = m_Keys[i];
		if (key1.time <= time) {
			i0 = i;
		}
		if (time < key1.time) {
			const TimeKey &key0 = m_Keys[i0];
			int dur = key1.time - key0.time;
			K__ASSERT(dur > 0);
			float t = (float)(time - key0.time) / dur;
			return easing(t, key0.value, key1.value, key1.expr); // key1 のイージング設定を使う事に注意
		}
	}
	K__ASSERT(0);
	return 0.0f;
}
int KEasing::getIndexByTime(int time) const {
	if (time < 0) return -1;
	for (size_t i=0; i<m_Keys.size(); i++) {
		if (m_Keys[i].time == time) {
			return i;
		}
	}
	return -1;
}
#pragma endregion // KEasing


} // namespace
