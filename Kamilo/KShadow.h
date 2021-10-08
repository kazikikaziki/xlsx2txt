#pragma once
#include "KNode.h"
#include "KVideo.h"
#include "keng_game.h"

namespace Kamilo {

/// 2D用の簡易影管理
class KShadow: public KComp {
public:
	struct GlobalSettings {
		KColor color; // 影の色
		KBlend blend; // 影の合成方法
		int vertexCount; // 影の楕円の頂点数
		float maxAltitude; // 影が映る最大高度のデフォルト値
		float defaultRadiusX;
		float defaultRadiusY;
		int worldLayer;
		bool useYPositionAsAltitude;

		// 高度による自動スケーリング値を使う
		// 有効にした場合、max_height の高さでの影のサイズを 0.0、
		// 接地しているときの影のサイズを 1.0 としてサイズが変化する
		// maxheight が 0 の場合は規定値を使う
		bool scaleByAltitude; // 高度によって影の大きさを変える

		bool gradient; // 不透明から透明へグラデさせる

		GlobalSettings() {
			defaultRadiusX = 32;
			defaultRadiusY = 12;
			worldLayer = 1;
			useYPositionAsAltitude = false;
			vertexCount = 24;
			maxAltitude = 400;
			color = KColor(0.0f, 0.0f, 0.0f, 0.7f);
			blend = KBlend_ALPHA;
			scaleByAltitude = true;
			gradient = true;
		}
	};

	static void install();
	static void uninstall();
	static void attach(KNode *node);
	static bool isAttached(KNode *node);
	static KShadow * of(KNode *node);
	static GlobalSettings * globalSettings();
public:
	KShadow();

	struct Data {
		KVec3 offset;
		EID shadow_e;
		KPath shadow_tex_name;
		float radius_x;
		float radius_y;
		float scale;
		float max_height;
		float altitude; // read only
		bool scale_by_height;
		bool enabled;
		bool use_sprite;
		int delay;

		Data() {
			shadow_e = nullptr;
			radius_x = 0;
			radius_y = 0;
			scale = 0;
			max_height = 0;
			scale_by_height = false;
			enabled = false;
			use_sprite = false;
			altitude = -1;
			delay = 0;
		}
	};
	Data m_Data;
	/// 地上での影の表示位置
	void setOffset(const KVec3 &value);

	/// 楕円影の標準サイズ
	/// @see setScaleFactor
	void setRadius(float horz, float vert);
	void getRadius(float *horz, float *vert) const;

	/// 楕円影のスケーリング
	/// @see setRadius
	void setScaleFactor(float value);

	/// スプライト影の変形
	void setMatrix(const KMatrix4 &matrix);

	/// スプライト影を使う
	void setUseSprite(bool value);

	void setScaleByHeight(bool value, float maxheight=0);
	bool getAltitude(float *altitude);

	virtual void on_comp_attach() override;
	virtual void on_comp_detach() override;
	void _StepSystemAction();
	void _Inspector();
	void update();
	bool compute_shadow_transform(const Data &data, KVec3 *out_pos, KVec3 *out_scale, float *out_alt);

};


} // namespace

