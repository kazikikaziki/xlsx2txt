/// @file
/// Copyright (c) 2020 Heliodor
/// This software is released under the MIT License.
/// http://opensource.org/licenses/mit-license.php

#pragma once
#include <vector>

namespace Kamilo {

/// ある値から別の値への遷移関数をまとめたクラス。
/// 各関数の遷移グラフは https://easings.net/ を参照せよ
class KEasing {
public:
	enum Expr {
		EXPR_ONE, // 常に終了値
		EXPR_KEEP, // 常に初期値で、終了時刻(t>=1)になった瞬間に終端値になる
		EXPR_STEP, // 開始時刻(t=0)でのみ初期値で、開始時刻を過ぎた(t>0)瞬間に終了値になる
		EXPR_LINEAR,

		EXPR_INSINE,
		EXPR_OUTSINE,
		EXPR_INOUTSINE,

		EXPR_INQUAD,
		EXPR_OUTQUAD,
		EXPR_INOUTQUAD,

		EXPR_INEXP,
		EXPR_OUTEXP,
		EXPR_INOUTEXP,

		EXPR_INCUBIC,
		EXPR_OUTCUBIC,
		EXPR_INOUTCUBIC,

		EXPR_INQUART,
		EXPR_OUTQUART,
		EXPR_INOUTQUART,

		EXPR_INQUINT,
		EXPR_OUTQUINT,
		EXPR_INOUTQUINT,

		EXPR_INCIRC,
		EXPR_OUTCIRC,
		EXPR_INOUTCIRC,

		EXPR_INBACK,
		EXPR_OUTBACK,
		EXPR_INOUTBACK,

		EXPR_ENUM_MAX
	};
	typedef float (*Func01)(float t);
	static Func01 getfunc01(Expr expr);


	/// inBack, outBack, inOutBack の係数 s の値。
	///
	/// イージングのマジックナンバーを自分で求めてみた
	/// http://void.heteml.jp/blog/archives/2014/05/easing_magicnumber.html
	static constexpr float BACK_10  = 1.70158f; // 10%行き過ぎてから戻る
	static constexpr float BACK_20  = 2.59238f; // 20%行き過ぎてから戻る
	static constexpr float BACK_30  = 3.39405f; // 30%行き過ぎてから戻る
	static constexpr float BACK_40  = 4.15574f; // 40%行き過ぎてから戻る
	static constexpr float BACK_50  = 4.89485f; // 50%行き過ぎてから戻る
	static constexpr float BACK_60  = 5.61962f; // 60%行き過ぎてから戻る
	static constexpr float BACK_70  = 6.33456f; // 70%行き過ぎてから戻る
	static constexpr float BACK_80  = 7.04243f; // 80%行き過ぎてから戻る
	static constexpr float BACK_90  = 7.74502f; // 90%行き過ぎてから戻る
	static constexpr float BACK_100 = 8.44353f; //100%行き過ぎてから戻る

	static float one01(float t); // t の値に関係なく常に 1 を返す
	static float keep01(float t); // (t < 1) ? 0 : 1 と同じ。 t が 1 未満のときに 0 を返す。それ以外ならば 1 (tが1になった瞬間に0→1になる)
	static float step01(float t); // (t <= 0) ? 0 : 1 と同じ。 t が 0 以下のときに 0 を返す。それ以外ならば 1 (tが0を超えた瞬間に0→1になる)
	static float linear01(float t);
	static float inSine01(float t);
	static float outSine01(float t);
	static float inOutSine01(float t);
	static float inQuad01(float t);
	static float outQuad01(float t);
	static float inOutQuad01(float t);
	static float inExp01(float t);
	static float outExp01(float t);
	static float inOutExp01(float t);
	static float inCubic01(float t);
	static float outCubic01(float t);
	static float inOutCubic01(float t);
	static float inQuart01(float t);
	static float outQuart01(float t);
	static float inOutQuart01(float t);
	static float inQuint01(float t);
	static float outQuint01(float t);
	static float inOutQuint01(float t);
	static float inCirc01(float t);
	static float outCirc01(float t);
	static float inOutCirc01(float t);
	static float inBack01(float t);
	static float outBack01(float t);
	static float inOutBack01(float t);
	static float inBackEx01(float t, float s);
	static float outBackEx01(float t, float s);
	static float inOutBackEx01(float t, float s);

	static float one(float t, float a, float b);
	static float keep(float t, float a, float b);
	static float step(float t, float a, float b);
	static float linear(float t, float a, float b);
	static float inSine(float t, float a, float b);
	static float outSine(float t, float a, float b);
	static float inOutSine(float t, float a, float b);
	static float inQuad(float t, float a, float b);
	static float outQuad(float t, float a, float b);
	static float inOutQuad(float t, float a, float b);
	static float inExp(float t, float a, float b);
	static float outExp(float t, float a, float b);
	static float inOutExp(float t, float a, float b);
	static float inCubic(float t, float a, float b);
	static float outCubic(float t, float a, float b);
	static float inOutCubic(float t, float a, float b);
	static float inQuart(float t, float a, float b);
	static float outQuart(float t, float a, float b);
	static float inOutQuart(float t, float a, float b);
	static float inQuint(float t, float a, float b);
	static float outQuint(float t, float a, float b);
	static float inOutQuint(float t, float a, float b);
	static float inCirc(float t, float a, float b);
	static float outCirc(float t, float a, float b);
	static float inOutCirc(float t, float a, float b);
	static float inBack(float t, float a, float b, float s=BACK_10);
	static float outBack(float t, float a, float b, float s=BACK_10);
	static float inOutBack(float t, float a, float b, float s=BACK_10);

	/// 値 v0 から v1 へのエルミート補間
	/// @param t 補間係数
	/// @param v0 t=0での値
	/// @param v1 t=1での値
	/// @param slope0 t=0における補完曲線の傾き
	/// @param slope1 t=1における補完曲線の傾き
	static float hermite(float t, float v0, float v1, float slope0, float slope1);

	/// サインカーブ
	///
	/// t=0.0 のとき a になり、t=0.5 で b に達し、t=1.0 で再び a になる
	/// @param t 補間係数
	/// @param a t=0.0 および 1.0 での値
	/// @param b t=0.5 での値
	static float wave(float t, float a, float b);

	static float easing01(float t, Expr expr);
	static float easing(float t, float a, float b, Expr expr);

public:
	KEasing();
	void clear();
	KEasing & addKey(int time, float value, Expr expr=EXPR_LINEAR);
	float getValue(int time) const;
	int getKeyCount() const;
	bool getKey(int index, int *time, float *value, Expr *expr) const;
	void setKey(int index, int *time, float *value, Expr *expr);
	int getDuration() const;
	int getIndexByTime(int time) const;

private:
	struct TimeKey {
		TimeKey() {
			expr = EXPR_STEP;
			time = -1;
			value = 0;
		}
		TimeKey(int _time, float _val, Expr _expr) {
			time = _time;
			value = _val;
			expr = _expr;
		}
		int time; // このキーの時刻
		float value; // このキーでの値
		Expr expr; // 直前の値からこのキーの値に変化させるための補間関数
	};
	struct SortByTime {
		bool operator()(const TimeKey &a, const TimeKey &b) const {
			return a.time < b.time;
		}
	};
	std::vector<TimeKey> m_Keys;
};

} // namespace


