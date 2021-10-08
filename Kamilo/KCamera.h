/// @file
/// Copyright (c) 2020 Heliodor
/// This software is released under the MIT License.
/// http://opensource.org/licenses/mit-license.php

#pragma once
#include "KRef.h"
#include "KVideo.h"
#include "KNode.h"

namespace Kamilo {

class KMatrix4;



/// 処理対象かどうかを判定するためのコールバック
class KEntityFilterCallback {
public:
	virtual void on_entityfilter_check(KNode *node, bool *skip) = 0;
};



class KCamera: public KComp {
public:
	static void install();
	static void uninstall();

	static void attach(KNode *node);
	static KCamera * of(KNode *node);
	static bool isAttached(KNode *node);
	static KNode * findCameraFor(const KNode *node);
	static int getCameraNodes(KNodeArray &list);

public:
	/// 描画順序のソート方法
	enum Order {
		ORDER_HIERARCHY, ///< ヒエラルキーを深さ優先でスキャンした順番で描画
		ORDER_ZDEPTH,    ///< Z値にしたがって描画
		ORDER_CUSTOM,    ///< ユーザー定義のソート方法。@see KRenderCallback::on_render_custom_sort()
	};

	/// 投影方法
	enum Proj {
		PROJ_ORTHO,         ///< 正射影
		PROJ_ORTHO_CABINET, ///< 斜投影（キャビネット投影）
		PROJ_FRUSTUM,       ///< 透視射影
	};

	KCamera();
	void on_inspector();

	// 描画パス
	// カメラは同一レイヤーに属しているノードを描画する
	// レイヤーは KNode::setLayer で設定し、その子ツリーはすべて同じレイヤーに属することになる
	// 描画順はツリーの上から順になる
	// 描画空間を別にしたい場合は pass を設定する。
	// （パスごとに１つのレンダーターゲットが作成される）
	void setPass(int pass);
	int getPass() const;

	/// ワールド座標からカメラスクリーン座標に変換するための行列を得る
	/// カメラスクリーン座標は、ビューポート中央を(0, 0) 右上を(1, 1) 左下を (-1, -1) とする座標系。
	/// 成功すれば true を返す。カメラが NULL だったり、camera が非カメラだった場合は false を返す
	bool getWorld2ViewMatrix(KMatrix4 *out_matrix);
	bool getView2WorldMatrix(KMatrix4 *out_matrix);
	bool getView2WorldVector(const KVec3 &view_point, KVec3 *out_world_point, KVec3 *out_normalized_direction);
	bool getView2WorldPoint(KVec3 *points, int num_points);
	bool getWorld2ViewPoint(KVec3 *points, int num_points);
	KVec3 getView2WorldPoint(const KVec3 &point);
	KVec3 getWorld2ViewPoint(const KVec3 &point);

	// node のローカル座標をこのカメラのレンダーターゲットテクスチャにおけるUV座標に変換するための行列を得る
	// シェーダーの mo__ScreenTexture とともに利用し、node のローカル座標に対応するスクリーンテクスチャのUVを得るために使う

	// 矩形メッシュの四隅の座標を p0, p1, p2, p3 としたとき、それに対応する
	// スクリーンテクスチャのUV座標の取得コードは以下の通り
	// ※ vec3_to_vec2 は vec3 の x, y を Vec2 として取り出す関数
	//
	// KMatrix4 matrix = camera->getTargetLocal2ViewUVMatrix(my_mesh_node);
	// uv0 = vec3_to_vec2(matrix.transform(p0)));
	// uv1 = vec3_to_vec2(matrix.transform(p1)));
	// uv2 = vec3_to_vec2(matrix.transform(p2)));
	// uv3 = vec3_to_vec2(matrix.transform(p3)));
	//
	KMatrix4 getTargetLocal2ViewUVMatrix(KNode *target);

	void copyCamera(KCamera *other);
	bool isTarget(const KNode *object);
	float getZoom() const;
	void setZoom(float value);
	float getProjectionZoomW() const;
	float getProjectionZoomH() const;
	float getProjectionW() const;
	float getProjectionH() const;
	void setProjectionOffset(const KVec3 &offset);
	KVec3 getProjectionOffset() const;
	void setProjectionW(float value);
	void setProjectionH(float value);
	const KMatrix4 & getProjectionMatrix() const;
	float getZFar() const;
	float getZNear() const;
	void setZFar(float z);
	void setZNear(float z);
	void setRenderTarget(KTEXID tex);

	// このカメラでの描画結果を格納しているテクスチャを得る
	// ※このカメラよりも前に別の画像が描画されている場合は、
	// その画像にこのカメラで上書きした結果を得る
	// カメラ単位ではなくパス単位での取得結果を得るには KScreen::getPerPassRenderTarget を使う
	KTEXID getRenderTarget() const;

	void setProjectionType(Proj type);
	Proj getProjectionType() const;
	void setCabinetDxDz(float dx_dz);
	void setCabinetDyDz(float dy_dz);
	float getCabinetDxDz() const;
	float getCabinetDyDz() const;
	void setMaterialEnabled(bool value);
	bool isMaterialEnabled() const;
	void setRenderingOrder(Order order);
	Order getRenderingOrder() const;
	void setMaterial(const KMaterial &m);
	void setZEnabled(bool z);
	bool isZEnabled() const;
	const KMaterial & getMaterial() const;
	bool isWorldAabbInFrustum(const KVec3 &aabb_min, const KVec3 &aabb_max);
	std::string getWarningString() { return ""; }

	bool isSnapIntEnabled() const;
	void setSnapIntEnabled(bool b);
	bool isOffsetHalfPixelEnabled() const;
	void setOffsetHalfPixelEnabled(bool b);


private:
	void update_projection_matrix();
	struct Data {
		Data() {
			size_w = 2.0f;
			size_h = 2.0f;
			znear  = 0.0f;
			zfar   = 1.0f;
			zoom   = 1.0f;
			cabinet_dx_dz = 0.5f;
			cabinet_dy_dz = 1.0f;
			// Ortho行列が単位行列になるように初期化
			projection_matrix = KMatrix4::fromOrtho(size_w, size_h, znear, zfar);
			projection_type = KCamera::PROJ_ORTHO;
			rendering_order = KCamera::ORDER_HIERARCHY;
			filter_cb = NULL;
			render_target = NULL;
			material_enabled = false;
			pass = 0;
			z_enabled = false;
			snap_int_enabled = false;
			offset_half_pixel_enabled = false;
		}
		KMatrix4 projection_matrix;
		KCamera::Proj projection_type;
		KVec3 projection_offset;
		float size_w;
		float size_h;
		float znear;
		float zfar;
		float cabinet_dx_dz;
		float cabinet_dy_dz;
		float zoom;

		/// このカメラで描画されるエンティティの描画順
		/// @see KCameraManager::setSort
		KCamera::Order rendering_order;

		/// 描画されるオブジェクトの選別用コールバック
		KEntityFilterCallback *filter_cb;

		/// 描画結果
		KTEXID render_target;

		/// 描画パス。
		/// 同じパスに複数のカメラがある場合は、それぞれの描画結果を重ねていく。
		/// パスが異なれば完全に独立して描画する（例えば自分のレンダーターゲットに自分が描画したものだけを反映させたい場合など）
		int pass;

		KMaterial material;
		bool material_enabled;

		bool z_enabled;
		bool snap_int_enabled;
		bool offset_half_pixel_enabled;
	};
	Data m_Data;
};

} // namespace
