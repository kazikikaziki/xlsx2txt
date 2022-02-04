#include "KRand.h"
#include "KInternal.h"
#include <time.h>

namespace Kamilo {


#pragma region KXorShift
static const int GAUSS_RAND_COUNT = 4; // 疑似正規分布を得るときの試行回数

KXorShift::KXorShift() {
	init();
}
void KXorShift::init(uint32_t seed) {
	mX = 123456789;
	mY = 362436069;
	mZ = 521288629;
	mW = (seed != 0) ? seed : 88675123;
#if 0
	for (int i=0; i<256; i++) {
		random();
	}
#endif
}
uint32_t KXorShift::random() {
	// http://ja.wikipedia.org/wiki/Xorshift
	uint32_t t = mX ^ (mX << 11);
	mX = mY;
	mY = mZ;
	mZ = mW;
	mW = (mW ^ (mW >> 19)) ^ (t ^ (t >> 8));
	return mW;
}
int KXorShift::randInt(int range) {
	if (range <= 0) return 0;
	uint32_t _range = (uint32_t)range;
	uint32_t mod = MAXVAL % _range;
	uint32_t lim = MAXVAL - mod;
	// 元の乱数範囲が range の倍数でない場合、そのまま計算すると確率が均等にならない。
	// lim を超えた場合は再生成する必要がある
	uint32_t n;
	do {
		n = random();
	} while (n >= lim);
	int val = (int)n % _range;
	K__ASSERT(0 <= val && val < range);
	return val;
}
int KXorShift::randIntRange(int a, int b) {
	if (a < b) {
		return a + randInt(b - a + 1);
	} else {
		return b + randInt(a - b + 1);
	}
}

int KXorShift::randIntGauss(int a, int b) {
	// 乱数の加算平均を求めて正規分布っぽくする
	int sum = 0;
	for (int i=0; i<GAUSS_RAND_COUNT; i++) {
		sum += randIntRange(a, b);
	}
	return sum / GAUSS_RAND_COUNT;
}
float KXorShift::randFloat01() {
	// random value between 0.0 and 1.0 (include 1.0)
	uint32_t a = random();
	uint32_t b = MAXVAL;
	return (float)a / b;
}
float KXorShift::randFloatExcl01() {
	// random value between 0.0 and 1.0 (exclude 1.0)
	while (1) {
		float ret = randFloat01();
		if (ret < 1.0f) return ret;
	}
}
float KXorShift::randFloat(float n) {
	return randFloat01() * n;
}
float KXorShift::randFloatRange(float a, float b) {
	if (a < b) {
		return a + randFloat(b - a);
	} else {
		return b + randFloat(a - b);
	}
}
float KXorShift::randFloatExcl(float n) {
	return randFloatExcl01() * n;
}
float KXorShift::randFloatGauss01() {
	// 乱数の加算平均を求めて正規分布っぽくする
	float gauss01 = 0;
	for (int i=0; i<GAUSS_RAND_COUNT; i++) {
		gauss01 += randFloat01();
	}
	return gauss01 /= GAUSS_RAND_COUNT;
}
float KXorShift::randFloatGauss(float a, float b) {
	return a + randFloatGauss01() * (b - a);
}

#pragma endregion // KXorShift


static KXorShift g_Random;


#pragma region KRand
void KRand::init(uint32_t seed) {
	g_Random.init(seed);
}
void KRand::initByTime() {
	int tm = (int)::time(NULL);
	g_Random.init(tm);
}
uint32_t KRand::random() {
	return g_Random.random();
}
int KRand::randInt(int n) {
	return g_Random.randInt(n);
}
int KRand::randIntRange(int a, int b) {
	return g_Random.randIntRange(a, b);
}
int KRand::randIntGauss(int a, int b) {
	return g_Random.randIntGauss(a, b);
}
float KRand::randFloat01() {
	return g_Random.randFloat01();
}
float KRand::randFloatExcl01() {
	return g_Random.randFloatExcl01();
}
float KRand::randFloat(float n) {
	return g_Random.randFloat(n);
}
float KRand::randFloatRange(float a, float b) {
	return g_Random.randFloatRange(a, b);
}
float KRand::randFloatExcl(float n) {
	return g_Random.randFloatExcl(n);
}
float KRand::randFloatGauss(float a, float b) {
	return g_Random.randFloatGauss(a, b);
}

#pragma endregion // KRand


} // namespace
