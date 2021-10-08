#pragma once

namespace Kamilo {

class KJoystick {
public:
	static constexpr int MAX_CONNECT = 2;

	enum Axis {
		AXIS_NONE = -1,
		AXIS_X, ///< X軸（負=左 正=右）
		AXIS_Y, ///< Y軸（負=上 正=下）
		AXIS_Z, ///< Z軸
		AXIS_R, ///< X回転
		AXIS_U, ///< Y回転
		AXIS_V, ///< Z回転
		AXIS_ENUM_MAX,
	};
	enum Button {
		BUTTON_NONE = -1,
		BUTTON_1,
		BUTTON_2,
		BUTTON_3,
		BUTTON_4,
		BUTTON_5,
		BUTTON_6,
		BUTTON_7,
		BUTTON_8,
		BUTTON_9,
		BUTTON_10,
		BUTTON_11,
		BUTTON_12,
		BUTTON_13,
		BUTTON_14,
		BUTTON_15,
		BUTTON_16,
		BUTTON_ENUM_MAX, ///< 物理ボタンの最大数
	};
	static bool init();
	static void shutdown();
	static bool isInit();

	/// ジョイスティックが利用可能かどうか
	static bool isConnected(int index);

	/// ハットスイッチ(POV)を持っているかどうか
	static bool hasPov(int index);

	/// 接続中のジョイスティックについている軸の数
	static int getAxisCount(int index);

	/// 接続中のジョイスティックについているボタンの数
	static int getButtonCount(int index);

	/// 軸の入力値を -1.0 以上 1.0 以下の値で返す
	/// threshold には入力感度を指定する
	static float getAxis(int index, int axis, float threshold=0.2f);

	/// ボタンが押されているかどうか
	static bool getButton(int index, int btn);

	/// ハットスイッチの方向を調べる。
	/// スイッチが押されているなら、その向きを x, y, deg にセットして true を返す。
	/// ニュートラル状態の場合は何もせずに false を返す
	///
	/// x ハットスイッチの入力向きを XY 成分で表したときの X 値。-1, 0, 1 のどれかになる
	/// y ハットスイッチの入力向きを XY 成分で表したときの Y 値。-1, 0, 1 のどれかになる
	/// ハットスイッチの入力向き。上方向を 0 度として時計回りに360未満の値を取る
	/// X負 = 左
	/// X正 = 右
	/// Y負 = 上
	/// Y正 = 下
	static bool getPov(int index, int *x, int *y, int *deg);

	/// ジョイスティックの入力状態を更新する
	static void poll(int index);
};

} // namespace
