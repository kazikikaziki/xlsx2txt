/// @file
/// Copyright (c) 2020 Heliodor
/// This software is released under the MIT License.
/// http://opensource.org/licenses/mit-license.php

#pragma once
#include <inttypes.h>


namespace Kamilo {

/// XorShift 法による乱数生成を行う
/// http://ja.wikipedia.org/wiki/Xorshift
class KXorShift {
public:
	static const uint32_t MAXVAL = 0xFFFFFFFF; ///< 得られる乱数の最大値 (32bit)
	KXorShift();
	void init(uint32_t seed=(uint32_t)(-1)); ///< 乱数系列を初期化
	uint32_t random();                       ///< 一様な32ビット整数乱数
	int randInt(int n);                      ///< 0 以上 n 未満の整数
	int randIntRange(int a, int b);          ///< a 以上 b 以下（または b 以上 a 以下）の整数
	int randIntGauss(int a, int b);          ///< a 以上 b 以下（または b 以上 a 以下）で平均値が (a+b)/2 になるような整数（疑似正規分布）
	float randFloat01();                     ///< 0 以上 1 以下の実数
	float randFloatExcl01();                 ///< 0 以上 1 未満の実数
	float randFloat(float n);                ///< 0 以上 n 以下の実数
	float randFloatExcl(float n);            ///< 0 以上 n 未満の実数
	float randFloatRange(float a, float b);  ///< a 以上 b 以下（または b 以上 a 以下）の実数
	float randFloatGauss01();                ///< 0 以上 1 以下かつ平均値が 0.5 になるようなで実数（疑似正規分布）
	float randFloatGauss(float a, float b);  ///< a 以上 b 以下（または b 以上 a 以下）で平均値が (a+b)/2 になるような実数（疑似正規分布）
private:
	uint32_t mX, mY, mZ, mW;
};


class KRand {
public:
	static void init(uint32_t seed=(uint32_t)(-1)); ///< 乱数系列を初期化
	static void initByTime();
	static uint32_t random();                      ///< 一様な32ビット整数乱数
	static int randInt(int n);                     ///< 0 以上 n 未満の整数
	static int randIntRange(int a, int b);         ///< a 以上 b 以下（または b 以上 a 以下）の整数
	static int randIntGauss(int a, int b);         ///< a 以上 b 以下（または b 以上 a 以下）で平均値が (a+b)/2 になるような整数（疑似正規分布）
	static float randFloat01();                    ///< 0 以上 1 以下の実数
	static float randFloatExcl01();                ///< 0 以上 1 未満の実数
	static float randFloat(float n);               ///< 0 以上 n 以下の実数
	static float randFloatExcl(float n);           ///< 0 以上 n 未満の実数
	static float randFloatRange(float a, float b); ///< 平均値が 0.5, 範囲が 0.0 以上 1.0 以下である実数（疑似正規分布）
	static float randFloatGauss(float a, float b); ///< a 以上 b 以下（または b 以上 a 以下）で平均値が (a+b)/2 になるような実数（疑似正規分布）
};


} // namespace
