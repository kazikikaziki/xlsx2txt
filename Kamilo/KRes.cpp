#include "KRes.h"

#include <algorithm>
#include "keng_game.h"
#include "KImGui.h"
#include "KInternal.h"
#include "KInspector.h"
#include "KFont.h"
#include "KDirectoryWalker.h"
#include "KSig.h"

#define INVALID_OPERATION    K__WARNING("INVALID_OPERATION at %s(%d): %s", __FILE__, __LINE__, __FUNCTION__)

#if defined(_DEBUG) && 0
#	define K_DEBUG_IMAGE_FILTER 1
#else
#	define K_DEBUG_IMAGE_FILTER 0
#endif



namespace Kamilo {

#pragma region internal
static const int THUMBNAIL_SIZE = 256;

static int _CeilPow2(int x) {
	// x 未満でない最大の 2^n を返す
	int b = 1;
	while (b < x) b *= 2;
	return b;
}
static const char * s_primitive_names[KPrimitive_ENUM_MAX] = {
	"PointList",     // KPrimitive_POINTS
	"LineList",      // KPrimitive_LINES
	"LineStrip",     // KPrimitive_LINE_STRIP
	"TriangleList",  // KPrimitive_TRIANGLES
	"TriangleStrip", // KPrimitive_TRIANGLE_STRIP
	"TriangleFan",   // KPrimitive_TRIANGLE_FAN
};
static const char * _GetPrimitiveName(KPrimitive p) {
	if (0 <= p && p < KPrimitive_ENUM_MAX) {
		return s_primitive_names[p];
	}
	return nullptr;
}
static void _MeshSaveToFile(const KMesh *mesh, KOutputStream &output) {
	std::string s; // utf8
	s += "<Mesh>\n";
	int num_vertices = mesh->getVertexCount();
	// Position
	if (num_vertices > 0) {
		s += "\t<Pos3D>\n";
		for (int i=0; i<num_vertices; i++) {
			KVec3 pos = mesh->getPosition(i);
			s += K::str_sprintf("\t\t%f %f %f\n", pos.x, pos.y, pos.z);
		}
		s += "\t</Pos3D>\n";
	}
	// UV
	if (num_vertices > 0) {
		s += "\t<UV>\n";
		for (int i=0; i<num_vertices; i++) {
			KVec2 uv = mesh->getTexCoord(i);
			s += K::str_sprintf("\t\t%f %f\n", uv.x, uv.y);
		}
		s += "\t</UV>\n";
	}
	// UV2
	if (num_vertices > 0) {
		s += "\t<UV2>\n";
		for (int i=0; i<num_vertices; i++) {
			KVec2 uv = mesh->getTexCoord2(i);
			s += K::str_sprintf("\t\t%f %f\n", uv.x, uv.y);
		}
		s += "\t</UV2>\n";
	}
	// Color
	if (num_vertices > 0) {
		s += "\t<Colors><!-- 0xAARRGGBB --> \n";
		for (int i=0; i<num_vertices; i++) {
			KColor32 dif = mesh->getColor32(i);
			s += K::str_sprintf("\t\t0x%08X\n", dif.toUInt32());
		}
		s += "\t</Colors>\n";
	}
	// Specular
	if (num_vertices > 0) {
		s += "\t<Speculars><!-- 0xAARRGGBB --> \n";
		for (int i=0; i<num_vertices; i++) {
			KColor32 spe = mesh->getSpecular32(i);
			s += K::str_sprintf("\t\t0x%08X\n", spe.toUInt32());
		}
		s += "\t</Speculars>\n";
	}
	// Submeshes
	if (num_vertices > 0) {
		K__ASSERT(mesh->getSubMeshCount() > 0);
		const int *indices = mesh->getIndices();
		for (int sidx=0; sidx<mesh->getSubMeshCount(); ++sidx) {
			const KSubMesh *sub = mesh->getSubMesh(sidx);
			int start = sub->start;
			int count = sub->count;
			switch (sub->primitive) {
			case KPrimitive_POINTS:
				s += "\t<PointList>\n";
				s += "\t\t";
				for (int i=0; i<count; i++) {
					int idx = indices ? indices[start + i] : (start + i);
					s += K::str_sprintf("%d", idx);
					if ((i+1) % 20 == 0 && (i+1) < count) {
						s += "\n";
						s += "\t\t";
					} else {
						s += " ";
					}
				}
				s += "\n";
				s += "\t</PointList>\n";
				break;
			case KPrimitive_LINES:
				s += "\t<LineList>\n";
				s += "\t\t";
				for (int i=0; i<count; i++) {
					int idx = indices ? indices[start + i] : (start + i);
					s += K::str_sprintf("%d", idx);
					if ((i+1) % 20 == 0 && (i+1) < count) {
						s += "\n";
						s += "\t\t";
					} else {
						s += " ";
					}
				}
				s += "\n";
				s += "\t</LineList>\n";
				break;
			case KPrimitive_LINE_STRIP:
				s += "\t<LineStrip>\n";
				s += "\t\t";
				for (int i=0; i<count; i++) {
					int idx = indices ? indices[start + i] : (start + i);
					s += K::str_sprintf("%d", idx);
					if ((i+1) % 20 == 0 && (i+1) < count) {
						s += "\n";
						s += "\t\t";
					} else {
						s += " ";
					}
				}
				s += "\n";
				s += "\t</LineStrip>\n";
				break;
			case KPrimitive_TRIANGLES:
				s += "\t<TriangleList>\n";
				s += "\t\t";
				for (int i=0; i<count; i++) {
					int idx = indices ? indices[start + i] : (start + i);
					s += K::str_sprintf("%d", idx);
					if ((i+1) % 3 == 0 && (i+1) < count) {
						s += "\n";
						s += "\t\t";
					} else {
						s += " ";
					}
				}
				s += "\n";
				s += "\t</TriangleList>\n";
				break;
			case KPrimitive_TRIANGLE_STRIP:
				s += "\t<TriangleStrip>\n";
				s += "\t\t";
				for (int i=0; i<count; i++) {
					int idx = indices ? indices[start + i] : (start + i);
					s += K::str_sprintf("%d", idx);
					if ((i+1) % 20 == 0 && (i+1) < count) {
						s += "\n";
						s += "\t\t";
					} else {
						s += " ";
					}
				}
				s += "\n";
				s += "\t</TriangleStrip>\n";
				break;
			case KPrimitive_TRIANGLE_FAN:
				s += "\t<TriangleFan>\n";
				s += "\t\t";
				for (int i=0; i<count; i++) {
					int idx = indices ? indices[start + i] : (start + i);
					s += K::str_sprintf("%d", idx);
					if ((i+1) % 20 == 0 && (i+1) < count) {
						s += "\n";
						s += "\t\t";
					} else {
						s += " ";
					}
				}
				s += "\n";
				s += "\t</TriangleFan>\n";
				break;
			default:
				K__ERROR("NOT SUPPORTED");
				break;
			}
			if (1) {
				s += "\t<Material>\n";
				s += K::str_sprintf("\t\tcolor   =#%08x\n", sub->material.color.toColor32());
				s += K::str_sprintf("\t\tspecular=#%08x\n", sub->material.specular.toColor32());
				s += K::str_sprintf("\t\tblend   =(KBlend)%d\n", sub->material.blend);
				s += K::str_sprintf("\t\filter   =(KFilter)%d\n", sub->material.filter);
				s += K::str_sprintf("\t\ttexture =%p\n", sub->material.texture);
				s += K::str_sprintf("\t\tshader  =%p\n", sub->material.shader);
				s += K::str_sprintf("\t\twrap    =%d\n", sub->material.wrap);
				s += K::str_sprintf("\t\tcb      =%p\n", sub->material.cb);
				s += K::str_sprintf("\t\tcb_data =%p\n", sub->material.cb_data);
				s += "\t</Material>\n";
			}
			if (1) {
				const KMatrix4 &tr = sub->transform;
				s += "\t<Transform>\n";
				s += K::str_sprintf("\t\t%f %f %f %f\n", tr.m[ 0], tr.m[ 1], tr.m[ 2], tr.m[ 3]);
				s += K::str_sprintf("\t\t%f %f %f %f\n", tr.m[ 4], tr.m[ 5], tr.m[ 6], tr.m[ 7]);
				s += K::str_sprintf("\t\t%f %f %f %f\n", tr.m[ 8], tr.m[ 9], tr.m[10], tr.m[11]);
				s += K::str_sprintf("\t\t%f %f %f %f\n", tr.m[12], tr.m[13], tr.m[14], tr.m[15]);
				s += "\t</Transform>\n";
			}
		}
	}
	s += "</Mesh>\n";
	
	// Total
	if (1) {
		s += "<!--\n";
		for (int i=0; i<num_vertices; i++) {
			KVec3 pos = mesh->getPosition(i);
			KVec2 tex = mesh->getTexCoord(i);
			KVec2 tex2 = mesh->getTexCoord2(i);
			s += K::str_sprintf("[%4d] xyz(%f %f %f) uv(%f %f) uv2(%f %f)\n", i, pos.x, pos.y, pos.z, tex.x, tex.y, tex2.x, tex2.y);
		}
		s += "-->\n";
	}
	output.write(s.data(), s.size());
}
static bool _MeshSaveToFileName(const KMesh *mesh, const KPath &filename) {
	KOutputStream file;
	if (file.openFileName(filename.u8())) {
		_MeshSaveToFile(mesh, file);
		return true;
	} else {
		K__ERROR("_MeshSaveToFileName: file can not be opened");
		return false;
	}
}
#pragma endregion // internal







#pragma region KSpriteRes
KSpriteRes::KSpriteRes() {
	clear();
}
void KSpriteRes::clear() {
	m_ImageW = 0;
	m_ImageH = 0;
	m_AtlasX = 0;
	m_AtlasY = 0;
	m_AtlasW = 0;
	m_AtlasH = 0;
	m_Pivot.x = 0.0f;
	m_Pivot.y = 0.0f;
	m_SubMeshIndex = -1;
	m_PaletteCount = 0;
	m_DefaultBlend = KBlend_INVALID;
	m_PivotInPixels = false;
	m_UsingPackedTexture = false;
}
KVec3 KSpriteRes::getRenderOffset() const {
	int bmp_x, bmp_y;

	// 左上を原点としたときのピボット座標（ビットマップ座標系）
	if (m_PivotInPixels) {
		bmp_x = (int)m_Pivot.x;
		bmp_y = (int)m_Pivot.y;
	} else {
		bmp_x = (int)(m_AtlasW * m_Pivot.x);
		bmp_y = (int)(m_AtlasH * m_Pivot.y);
	}

	// 左下を原点としたときのピボット座標（オブジェクト座標系）
	int obj_x = bmp_x;
	int obj_y = m_AtlasH - bmp_y;
		
	// 基準点を現在のスプライトオブジェクトの原点に合わせるためのオフセット量
	return KVec3(-obj_x, -obj_y, 0);
}
KVec3 KSpriteRes::bmpToLocal(const KVec3 &bmp) const {
	// ピクセル単位でのピボット座標
	KVec3 piv;
	if (m_PivotInPixels) {
		piv.x = m_Pivot.x;
		piv.y = m_Pivot.y;
	} else {
		piv.x = m_AtlasW * m_Pivot.x;
		piv.y = m_AtlasH * m_Pivot.y;
	}
	KVec3 off = bmp - piv;
	off.y = -off.y; // BMP座標はY軸下向きなので、これを上向きに直す
	return KVec3(off.x, off.y, 0.0f);
}
/// KImage からスプライトを作成する
/// img_w, img_h 画像サイズ【スプライトサイズと同じ）
/// texture_name テクスチャの名前
/// image_index 複数の画像を切り取る場合、そのインデックス
/// blend ブレンド方法が既に分かっているなら、そのブレンド値。不明なら KBlend_INVALID
/// pack パックされたテクスチャを使っているなら true
bool KSpriteRes::buildFromImageEx(int img_w, int img_h, const KPath &texture_name, int image_index, KBlend blend, bool packed) {
	clear();
	if (img_w <= 0) return false;
	if (img_h <= 0) return false;
	if (texture_name.empty()) return false;

	// 切り取り範囲
	int atlas_x = 0;
	int atlas_y = 0;
	int atlas_w = img_w;
	int atlas_h = img_h;

	// 基準点
	float pivot_x = floorf(atlas_w * 0.5f);
	float pivot_y = floorf(atlas_h * 0.5f);

	// あまり変な値が入ってたら警告
	K__ASSERT(0 <= pivot_x && pivot_x <= atlas_w);
	K__ASSERT(0 <= pivot_y && pivot_y <= atlas_h);

	// スプライト構築
	m_ImageW = img_w;
	m_ImageH = img_h;
	m_AtlasX = atlas_x;
	m_AtlasY = atlas_y;
	m_AtlasW = atlas_w;
	m_AtlasH = atlas_h;
	m_Pivot.x = pivot_x;
	m_Pivot.y = pivot_y;
	m_PivotInPixels = true;
	m_UsingPackedTexture = packed;
	m_PaletteCount = 0; // K_PALETTE_IMAGE_SIZE
	m_Mesh = KMesh();
	m_SubMeshIndex = image_index; // <-- 何番目の画像を取り出すか？
	m_TextureName = texture_name;
	m_DefaultBlend = blend;
	return true;
}
bool KSpriteRes::buildFromPng(const void *png_data, int png_size, const KPath &texname) {
	clear();

	// 画像サイズを得る
	int img_w = 0;
	int img_h = 0;
	KCorePng::readsize(png_data, png_size, &img_w, &img_h);
	if (img_w == 0 || img_h == 0) {
		return false; // pngではない
	}

	// スプライト作成
	buildFromImageEx(img_w, img_h, texname, -1, KBlend_INVALID, false);
	return true;
}

#pragma endregion // KSpriteRes



#pragma region KClipImpl
KClipRes::KClipRes(const std::string &name) {
	m_Name = name;
	m_EditInfoXml = nullptr;
	clear();
}
KClipRes::~KClipRes() {
	clear();
}
void KClipRes::clear() {
	m_Flags = 0;
	for (int i=0; i<(int)m_Keys.size(); i++) {
		K__DROP(m_Keys[i].xml_data);
	}
	m_Length = 0;
	m_Keys.clear();
	K__DROP(m_EditInfoXml);
}
const char * KClipRes::getName() const {
	return m_Name.u8();
}
int KClipRes::getLength() const {
	return m_Length;
}
bool KClipRes::getFlag(KClipRes::Flag f) const {
	return (m_Flags & f) != 0;
}
void KClipRes::setFlag(KClipRes::Flag f, bool value) {
	if (value) {
		m_Flags |= f;
	} else {
		m_Flags &= ~f;
	}
}
void KClipRes::setTag(KName tag) {
	KNameList_pushback_unique(m_Tags, tag);
}
bool KClipRes::hasTag(KName tag) const {
	return KNameList_contains(m_Tags, tag);
}
const KNameList & KClipRes::getTags() const {
	return m_Tags;
}

static std::string _MarkToString(KMark mark, KMark next) {
	std::string s;
	switch (mark) {
	case KMark_A: s += "MARKA|"; break;
	case KMark_B: s += "MARKB|"; break;
	case KMark_C: s += "MARKC|"; break;
	case KMark_D: s += "MARKD|"; break;
	case KMark_E: s += "MARKE|"; break;
	}
	switch (next) {
	case KMark_A: s += "NEXTA|"; break;
	case KMark_B: s += "NEXTB|"; break;
	case KMark_C: s += "NEXTC|"; break;
	case KMark_D: s += "NEXTD|"; break;
	case KMark_E: s += "NEXTE|"; break;
	case KMark_END: s += "END|"; break;
	}
	if (s.size() > 0 && s.back()=='|') {
		s.pop_back();
	}
	return s;
}


// <EdgeAnimation> タグ向けに保存する
// EDGEファイルからロードすることが前提なので、EDGEを見ればわかるような内容は保存しない
void KClipRes::saveForEdge(KXmlElement *xml, const KPath &homeDir) {
	KPath edge_name = m_EdgeFile.getRelativePathFrom(homeDir);
	KPath clip_name = m_Name.getRelativePathFrom(homeDir);

	xml->setAttrString("file", edge_name.u8()); // source edge file name
	xml->setAttrString("name", clip_name.u8()); // this clip name
	if (m_Flags & FLAG_LOOP) {
		xml->setAttrInt("loop", 1);
	}
	if (m_Flags & FLAG_KILL_SELF) {
		xml->setAttrInt("autokill", 1);
	}
	if (m_EditInfoXml) {
		K__ASSERT(m_EditInfoXml->hasTag("EditInfo"));
		xml->addChild(m_EditInfoXml);
	}

	for (int i=0; i<getKeyCount(); i++) {
		const SPRITE_KEY *key = getKey(i);
		KXmlElement *elm = xml->addChild("Page");
		elm->setAttrInt("page", key->edge_page);
		elm->setAttrInt("dur", key->duration);
		if (key->this_mark != KMark_NONE) {
			char s[256] = {0};
			K_MarkToStr(key->this_mark, s);
			elm->setAttrString("mark", s);
		} else {
			elm->removeAttr("mark");
		}
		if (key->next_mark != KMark_NONE) {
			char s[256] = {0};
			K_MarkToStr(key->next_mark, s);
			elm->setAttrString("next", s);
		} else {
			elm->removeAttr("next");
		}
		if (!key->next_clip.empty()) {
			elm->setAttrString("next_clip", key->next_clip.u8());
		} else {
			elm->removeAttr("next_clip");
		}

		if (key->edge_name.empty() || key->edge_name.compare(m_EdgeFile) == 0) {
			// このページでは、クリップ元Edgeファイルと同じ画像を使っている
			elm->removeAttr("extern_edge");
			elm->removeAttr("extern_page");

		} else {
			// このページでは、クリップ元と異なるEdgeを使っている
			KPath edge_erelpath = key->edge_name.getRelativePathFrom(homeDir);
			elm->setAttrString("extern_edge", edge_erelpath.u8()); // xml のあるフォルダからの相対パスで記録する
			elm->setAttrInt("extern_page", key->edge_page);
		}

		if (key->user_parameters.size() > 0) {
			KXmlElement *user = elm->addChild("Parameters");
			key->user_parameters.saveToXml(user, true);
		}
		if (key->xml_data) {
			KXmlElement *dup = key->xml_data->clone();
			dup->setTag("Data");
			elm->addChild(dup);
			dup->drop();
		}
	}
}

// <Clip> タグ向けに保存する。レイヤーの内容やスプライトファイル名もすべて記録する
void KClipRes::saveForClip(KXmlElement *xml, const KPath &homeDir) {
	KPath file = m_EdgeFile.getRelativePathFrom(homeDir);
	xml->setAttrString("name", file.u8());
	if (m_Flags & FLAG_LOOP) {
		xml->setAttrInt("loop", 1);
	}
	if (m_Flags & FLAG_KILL_SELF) {
		xml->setAttrInt("autokill", 1);
	}
	if (m_EditInfoXml) {
		K__ASSERT(m_EditInfoXml->hasTag("EditInfo"));
		xml->addChild(m_EditInfoXml);
	}
	for (int i=0; i<getKeyCount(); i++) {
		const SPRITE_KEY *key = getKey(i);
		KXmlElement *elm = xml->addChild("SpriteKey");
		elm->setAttrInt("dur", key->duration);
		if (key->this_mark != KMark_NONE) {
			char s[256] = {0};
			K_MarkToStr(key->this_mark, s);
			elm->setAttrString("mark", s);
		} else {
			elm->removeAttr("mark");
		}
		if (key->next_mark != KMark_NONE) {
			char s[256] = {0};
			K_MarkToStr(key->next_mark, s);
			elm->setAttrString("next", s);
		} else {
			elm->removeAttr("next");
		}
		if (!key->next_clip.empty()) {
			elm->setAttrString("next_clip", key->next_clip.u8());
		} else {
			elm->removeAttr("next_clip");
		}
		elm->setAttrInt("numlayers", key->num_layers);
		elm->setAttrInt("edge_page", key->edge_page);
		for (int l=0; l<key->num_layers; l++) {
			KXmlElement *xLay = elm->addChild("Layer");
			const SPRITE_LAYER &slay = key->layers[l];
			if (!slay.sprite.empty()) {
				xLay->setAttrString("sprite", slay.sprite.u8());
			}
			if (!slay.label.empty()) {
				xLay->setAttrString("label", slay.label.u8());
			}
			if (!slay.command.empty()) {
				xLay->setAttrString("commands", slay.command.u8());
			}
		}

		if (key->user_parameters.size() > 0) {
			KXmlElement *user = elm->addChild("Parameters");
			key->user_parameters.saveToXml(user, true);
		}
		if (key->xml_data) {
			KXmlElement *dup = key->xml_data->clone();
			dup->setTag("Data");
			elm->addChild(dup);
			dup->drop();
		}
	}
}

void KClipRes::on_track_gui_state(float frame) {
#ifndef NO_IMGUI
	int framenumber = 0;
	for (size_t i=0; i<m_Keys.size(); i++) {
		const SPRITE_KEY &key = m_Keys[i];
		if ((framenumber <= frame) && (frame < framenumber + key.duration)) {
			KImGui::PushTextColor(KImGui::COLOR_WARNING);
		} else {
			KImGui::PushTextColor(KImGui::COLOR_DEFAULT());
		}
		ImGui::PushID(KImGui::ID(i));
		gui_key(key, framenumber);
		ImGui::PopID();
		KImGui::PopTextColor();

		framenumber += key.duration;
	}
#endif // !NO_IMGUI
}
void KClipRes::on_track_gui() {
#ifndef NO_IMGUI
	// アニメーションカーブをエクスポートする
	for (size_t ki=0; ki<m_Keys.size(); ki++) {
		if (ImGui::TreeNode(KImGui::ID(ki), "Page[%d]", ki)) {
			const SPRITE_KEY &key = m_Keys[ki];
			ImGui::Text("Dur : %d", key.duration);
			ImGui::Text("ThisMark: %d", key.this_mark);
			ImGui::Text("NextMark: %d", key.next_mark);
			for (int li=0; li<key.num_layers; li++) {
				if (ImGui::TreeNode(KImGui::ID(li), "Layer[%d]", li)) {
					ImGui::Text("Sprite : %s", key.layers[li].sprite.u8());
					ImGui::Text("Label  : %s", key.layers[li].label.u8());
					ImGui::Text("Command: %s", key.layers[li].command.u8());
					if (ImGui::TreeNode(KImGui::ID(li), "Params")) {
						for (int ni=0; ni<key.user_parameters.size(); ni++) {
							ImGui::Text("%s: %s", key.user_parameters.getName(ni), key.user_parameters.getString(ni));
						}
						ImGui::TreePop();
					}
					ImGui::TreePop();
				}
			}
			ImGui::TreePop();
		}
	}
	if (ImGui::Button("Export")) {
		std::string s;
		s += K::str_sprintf("Num pages: %d\n", m_Keys.size());
		for (size_t ki=0; ki<m_Keys.size(); ki++) {
			s += K::str_sprintf("Page[%d] {\n", ki);
			const SPRITE_KEY &key = m_Keys[ki];
			s += K::str_sprintf("\tDur : %d\n", key.duration);

			s += K::str_sprintf("\tThisMark: %d\n", key.this_mark);
			s += K::str_sprintf("\tNextMark: %d\n", key.next_mark);
			
			for (int li=0; li<key.num_layers; li++) {
				s += K::str_sprintf("\tLayer[%d]\n", li);
				s += K::str_sprintf("\t\tSprite : %s\n", key.layers[li].sprite.u8());
				s += K::str_sprintf("\t\tLabel  : %s\n", key.layers[li].label.u8());
				s += K::str_sprintf("\t\tCommand: %s\n", key.layers[li].command.u8());
				s += K::str_sprintf("\t\tParams {\n");
				for (int ni=0; ni<key.user_parameters.size(); ni++) {
					s += K::str_sprintf("\t\t\t%s: %s\n", key.user_parameters.getName(ni), key.user_parameters.getString(ni));
				}
				s += K::str_sprintf("\t\t} // Params\n");
				s += K::str_sprintf("\t} // Layer\n");
			}
			s += K::str_sprintf("} // Page\n");
		}
		s += '\n';

		KOutputStream file;
		file.openFileName("~sprite_curve.txt");
		file.write(s.data(), s.size());
	}
#endif // !NO_IMGUI
}
void KClipRes::gui_key(const SPRITE_KEY &key, int framenumber) const {
#ifndef NO_IMGUI
	ImGui::Text("F%03d: (Dur=%03d ThisMark='%d' NextMark='%d')", framenumber, key.duration, key.this_mark, key.next_mark);
	ImGui::Indent();
	for (int i=0; i<key.num_layers; i++) {
		ImGui::Text("'%s' (Label: %s)", key.layers[i].sprite.u8(), key.layers[i].label.u8());
	}
	ImGui::Unindent();
	if (key.user_parameters.size() > 0) {
		ImGui::Indent();
		if (ImGui::TreeNodeEx("User parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
			for (int i=0; i<(int)key.user_parameters.size(); i++) {
				std::string k = key.user_parameters.getName(i);
				std::string v = key.user_parameters.getString(i);
				ImGui::BulletText("%s: '%s'", k.c_str(), v.c_str());
			}
			if (key.xml_data) {
				std::string s = key.xml_data->toString(0);
				ImGui::Text("%s", s.c_str());
			}
			ImGui::TreePop();
		}
		ImGui::Unindent();
	}
#endif // !NO_IMGUI
}
const KClipRes::SPRITE_KEY * KClipRes::getKeyByFrame(float frame) const {
	int page = getPageByFrame(frame, nullptr);
	const SPRITE_KEY *key = getKey(page);
	return key;
}
KClipRes::SPRITE_KEY * KClipRes::getKeyByFrame(float frame) {
	int page = getPageByFrame(frame, nullptr);
	SPRITE_KEY *key = getKey(page);
	return key;
}
int KClipRes::findPageByMark(int mark) const {
	if (mark > 0) {
		for (int i=0; i<(int)m_Keys.size(); i++) {
			if (m_Keys[i].this_mark == mark) {
				return i;
			}
		}
	}
	return -1;
}
void KClipRes::addKey(const KClipRes::SPRITE_KEY &key, int pos) {
	if (key.duration < 0) {
		INVALID_OPERATION;
		return;
	}
	if (0 <= pos && pos < (int)m_Keys.size()) {
		m_Keys.insert(m_Keys.begin() + pos, key);
	} else {
		m_Keys.push_back(key);
	}
	recalculateKeyTimes();
}
void KClipRes::deleteKey(int index) {
	if (0 <= index && index < (int)m_Keys.size()) {
		K__DROP(m_Keys[index].xml_data);
		m_Keys.erase(m_Keys.begin() + index);
		recalculateKeyTimes();
	}
}
const KClipRes::SPRITE_KEY * KClipRes::getKey(int index) const {
	if (0 <= index && index < (int)m_Keys.size()) {
		return &m_Keys[index];
	}
	return nullptr;
}
KClipRes::SPRITE_KEY * KClipRes::getKey(int index) {
	if (0 <= index && index < (int)m_Keys.size()) {
		return &m_Keys[index];
	}
	return nullptr;
}
int KClipRes::getKeyTime(int index) const {
	if (0 <= index && index < (int)m_Keys.size()) {
		return m_Keys[index].time;
	} else {
		INVALID_OPERATION;
		return 0;
	}
}
int KClipRes::getKeyCount() const {
	return m_Keys.size();
}
int KClipRes::getMaxLayerCount() const {
	int bound = 0;
	for (auto it=m_Keys.begin(); it!=m_Keys.end(); ++it) {
		if (it->num_layers > bound) {
			bound = it->num_layers;
		}
	}
	return bound;
}
int KClipRes::getPageByFrame(float frame, int *out_pageframe, int *out_pagedur) const {
	if (frame < 0) {
		INVALID_OPERATION;
		return -1;
	}
	int fr0 = 0;
	int fr1 = 0;
	for (size_t i=0; i<m_Keys.size(); i++) {
		fr0 = fr1;
		fr1 += m_Keys[i].duration;
		if (frame < fr1) {
			if (out_pageframe) {
				*out_pageframe = (int)(frame - fr0);
			}
			if (out_pagedur) {
				*out_pagedur = m_Keys[i].duration;
			}
			return i;
		}
	}

	// 総フレーム数を超えていた場合は、最後のセグメントインデックスを返す
	if (out_pageframe) {
		*out_pageframe = (int)(frame - fr0);
	}
	if (out_pagedur) {
		if (m_Keys.empty()) {
			*out_pagedur = 0;
		} else {
			*out_pagedur = m_Keys.back().duration;
		}
	}
	return (int)m_Keys.size() - 1;
}
int KClipRes::getSpritesInPage(int page, int max_layers, KPath *layer_sprite_names) const {
	if (page < 0) return 0;
	if (page >= (int)m_Keys.size()) return 0;
	const SPRITE_KEY &key = m_Keys[page];
	int n = KMath::min(max_layers, key.num_layers);
	for (int i=0; i<n; i++) {
		layer_sprite_names[i] = key.layers[i].sprite;
	}
	return n;
}
int KClipRes::getLayerCount(int page) const {
	if (page < 0) return 0;
	if (page >= (int)m_Keys.size()) return 0;
	return m_Keys[page].num_layers;
}
bool KClipRes::getNextPage(int page, int mark, std::string *p_new_clip, int *p_new_page) const {
	K__ASSERT(page >= 0);
	K__ASSERT(p_new_clip);
	K__ASSERT(p_new_page);

	const SPRITE_KEY *key = getKey(page);
	
	// 行き先が負の値になっているならクリップ終了
	if (key->next_mark < 0) {
		*p_new_page = -1;
		return true;
		
	}

	// 行き先クリップが指定されている場合は、クリップ終了
	if (!key->next_clip.empty()) {
		*p_new_page = 0;
		*p_new_clip = key->next_clip.u8();
		return true;
	}

	// 行先マークとして有効な値が指定されている。そのマークを持つページに飛ぶ
	if (key->next_mark > 0) {
		int idx = findPageByMark(key->next_mark);
		if (idx >= 0) {
			*p_new_page = idx;
			return true;
		}
		K__WARNING("W_ANI: No any pages marked '%d'", idx); // マークが見つからない
	}

	// 行先指定なし
	return false;
}
void KClipRes::recalculateKeyTimes() {
	int t = 0;
	for (auto it=m_Keys.begin(); it!=m_Keys.end(); ++it) {
		it->time = t;
		t += it->duration;
	}
	m_Length = t;
}
#pragma endregion // KClipRes




#pragma region KTextureRes
KTextureRes::KTextureRes() {
	m_TexId = nullptr;
	m_Protected = false;
	m_Width = 0;
	m_Height = 0;
	m_IsRenderTex = 0;
}
void KTextureRes::release() {
	if (m_TexId) {
		K__VERBOSE("Del texture: %s", m_Name.u8());
		KVideo::deleteTexture(m_TexId);
		m_TexId = nullptr;
	}
}
bool KTextureRes::loadEmptyTexture(int w, int h, KTexture::Format fmt, bool protect) {
	m_Width = w;
	m_Height = h;
	m_IsRenderTex = false;
	m_TexId = KVideo::createTexture(w, h, fmt);
	m_Protected = protect;
	return m_TexId != nullptr;
}
bool KTextureRes::loadRenderTexture(int w, int h, KTexture::Format fmt, bool protect) {
	m_Width = w;
	m_Height = h;
	m_IsRenderTex = true;
	m_TexId = KVideo::createRenderTexture(w, h, fmt);
	m_Protected = protect;
	return m_TexId != nullptr;
}
#pragma endregion // KTextureRes



#pragma region KShaderRes
KShaderRes::KShaderRes() {
	m_ShaderId = nullptr;
}
void KShaderRes::clear() {
	m_ShaderId = nullptr;
}
KSHADERID KShaderRes::getId() {
	return m_ShaderId;
}
void KShaderRes::release() {
	KVideo::deleteShader(m_ShaderId);
	clear();
}
bool KShaderRes::loadFromHLSL(const char *name, const char *code) {
	clear();
	const char *unbom_code = K::strSkipBom(code);
	KSHADERID sh = KVideo::createShaderFromHLSL(unbom_code, name);
	if (sh == nullptr) {
		K__ERROR("E_SHADER: failed to build HLSL shader.");
		return false;
	}
	m_ShaderId = sh;
	return true;
}
bool KShaderRes::loadFromStream(KInputStream &input, const char *name) {
	bool result = false;
	if (input.isOpen()) {
		std::string code = input.readBin();
		if (loadFromHLSL(name, code.c_str())) {
			result = true;
		}
	}
	return result;
}


#pragma endregion // KShaderRes


#pragma region KFontRes
KFontRes::KFontRes() {
}
KFontRes::~KFontRes() {
}
void KFontRes::release() {
}
bool KFontRes::loadFromFont(KFont &font) {
	m_Font = font;
	return true;
}
bool KFontRes::loadFromStream(KInputStream &input, int ttc_index) {
	release();
	// ファイルをロード
	std::string bin = input.readBin();
	if (!bin.empty()) {
		KFont font = KFont::createFromMemory(bin.data(), bin.size());
		if (font.isOpen()) {
			if (loadFromFont(font)) {
				return true;
			}
		}
	}
	return false;
}
bool KFontRes::loadFromSystemFontDirectory(const char *filename, int ttc_index) {
	release();
	std::string sysfont = K::pathJoin(KPlatformFonts::getFontDirectory(), filename);
	KFont font = KFont::createFromFileName(sysfont.c_str(), ttc_index);
	return loadFromFont(font);
}
#pragma endregion // KFontRes





#pragma region CTextureBankImpl
#define NAME_SEPARATOR  '&'
class CTextureBankImpl: public KTextureBank {
	std::unordered_map<std::string, KAutoRef<KTextureRes>> m_Items;
	mutable std::recursive_mutex m_Mutex;
public:
	CTextureBankImpl() {
	}
	virtual ~CTextureBankImpl() {
		clearTextures(true);
	}
	void init() {
	}
	KTextureAuto findById(KTEXID sid) const {
		for (auto it=m_Items.begin(); it!=m_Items.end(); ++it) {
			const KTextureAuto &sh = it->second;
			if (sh->m_TexId == sid) {
				return sh;
			}
		}
		return nullptr;
	}
	KTextureAuto findByName(const KPath &name) const {
		for (auto it=m_Items.begin(); it!=m_Items.end(); ++it) {
			if (it->first == name.toUtf8()) {
				return it->second;
			}
		}
		return nullptr;
	}
	virtual int getTextureCount() override { 
		return (int)m_Items.size();
	}
	virtual std::vector<std::string> getTextureNameList() override {
		std::vector<std::string> names;
		m_Mutex.lock();
		{
			names.reserve(m_Items.size());
			for (auto it=m_Items.begin(); it!=m_Items.end(); ++it) {
				names.push_back(it->first);
			}
			std::sort(names.begin(), names.end());
		}
		m_Mutex.unlock();
		return names;
	}
	virtual std::vector<std::string> getRenderTextureNameList() override {
		std::vector<std::string> names;
		m_Mutex.lock();
		{
			for (auto it=m_Items.begin(); it!=m_Items.end(); ++it) {
				KAutoRef<KTextureRes> texres = it->second;
				if (texres->m_IsRenderTex) {
					names.push_back(texres->m_Name.u8());
				}
			}
			std::sort(names.begin(), names.end());
		}
		m_Mutex.unlock();
		return names;
	}
	virtual void clearTextures(bool remove_protected_textures) override {
		m_Mutex.lock();
		for (auto it=m_Items.begin(); it!=m_Items.end(); /*++it*/) {
			KAutoRef<KTextureRes> texres = it->second;
			if (!remove_protected_textures && texres->m_Protected) {
				// 保護テクスチャ。削除してはいけない
				it++;
			} else {
				texres->release();
				it = m_Items.erase(it);
			}
		} 
	//	m_Items.clear();
		m_Mutex.unlock();
	}
	virtual void removeTexture(KTEXID texid) override {
		m_Mutex.lock();
		for (auto it=m_Items.begin(); it!=m_Items.end(); ++it) {
			auto texres = it->second;
			if (texres->m_TexId == texid) {
				texres->release();
				m_Items.erase(it);
				break;
			}
		}
		m_Mutex.unlock();
	}
	virtual void removeTexture(const KPath &name) override {
		m_Mutex.lock();
		auto it = m_Items.find(name.u8());
		if (it != m_Items.end()) {
			it->second->release();
			m_Items.erase(it);
		}
		m_Mutex.unlock();
	}
	virtual void removeTexturesByTag(const std::string &_tag) override {
		KName tag = _tag;
		m_Mutex.lock();
		{
			for (auto it=m_Items.begin(); it!=m_Items.end(); /*none*/) {
				KTextureRes *tex = it->second;
				if (tex->hasTag(tag)) {
					tex->release();
					it = m_Items.erase(it);
				} else {
					it++;
				}
			}
		}
		m_Mutex.unlock();
	}
	virtual bool hasTexture(const KPath &name) const override {
		m_Mutex.lock();
		bool ret = m_Items.find(name.u8()) != m_Items.end();
		m_Mutex.unlock();
		return ret;
	}

	virtual void setProtect(KTEXID id, bool value) override {
		KTextureAuto sh = findById(id);
		if (sh != nullptr) {
			sh->m_Protected = value;
		}
	}
	virtual bool getProtect(KTEXID id) override {
		KTextureAuto sh = findById(id);
		if (sh != nullptr) {
			return sh->m_Protected;
		}
		return false;
	}

	virtual void setTag(KTEXID id, KName tag) override {
		m_Mutex.lock();
		{
			KTextureAuto sh = findById(id);
			if (sh != nullptr) {
				sh->addTag(tag);
			}
		}
		m_Mutex.unlock();
	}
	virtual bool hasTag(KTEXID id, KName tag) const override {
		bool result = false;
		m_Mutex.lock();
		{
			KTextureAuto sh = findById(id);
			if (sh != nullptr && sh->hasTag(tag)) {
				result = true;
			}
		}
		m_Mutex.unlock();
		return result;
	}
	virtual const KNameList & getTags(KTEXID id) const override {
		static KNameList s_empty;

		KNameList &result = s_empty;
		m_Mutex.lock();
		{
			KTextureAuto sh = findById(id);
			if (sh != nullptr) {
				result = sh->getTags();
			}
		}
		m_Mutex.unlock();
		return result;
	}








	virtual KPath getTextureName(KTEXID tex) const override {
		KPath name;
		m_Mutex.lock();
		for (auto it=m_Items.begin(); it!=m_Items.end(); ++it) {
			auto texres = it->second;
			if (texres->m_TexId == tex) {
				name = texres->m_Name;
				break;
			}
		}
		m_Mutex.unlock();
		return name;
	}
	virtual bool isRenderTexture(const KPath &tex_path) override {
		KTexture *tex = KVideo::findTexture(findTextureRaw(tex_path, false));
		return tex && tex->isRenderTarget();
	}
	virtual KTEXID getTextureEx(const KPath &tex_path, int modifier, bool should_exist, KNode *node_for_mod) override {
		return findTexture(tex_path, modifier, false, node_for_mod);
	}
	virtual KTEXID getTexture(const KPath &tex_path) override {
		return findTextureRaw(tex_path, false);
	}
	virtual KTEXID findTextureRaw(const KPath &name, bool should_exist) const override {
		if (name.empty()) {
			return nullptr;
		}
		KTEXID ret = nullptr;
		m_Mutex.lock();
		auto it = m_Items.find(name.u8());
		if (it != m_Items.end()) {
			ret = it->second->m_TexId;
		} else if (should_exist) {
			// もしかして:
			// 大小文字を区別しない、フォルダ名部分を無視して比較してみる。
			// それで見つかった場合、名前の指定を間違えている可能性がある
			std::string probably;
			for (auto it2=m_Items.begin(); it2!=m_Items.end(); it2++) {
				if (K::pathEndsWith(it2->first, name.u8())) {
					probably = it2->first;
					break;
				}
				if (K::str_stricmp(it2->first, name.u8())==0) {
					probably = it2->first;
					break;
				}
			}
			if (probably.empty()) {
				K__ERROR(u8"E_TEX: テクスチャ '%s' はロードされていません. xres のロード忘れ、 xres 内での定義忘れ、KTextureBank::addTexture での登録忘れの可能性があります", name.u8());
			} else {
				K__ERROR(u8"E_TEX: テクスチャ '%s' はロードされていません。もしかして: '%s'", name.u8(), probably.c_str());
			}
		}
		m_Mutex.unlock();
		return ret;
	}
	virtual KPath makeTextureNameWithModifier(const KPath &name, int modifier) const override {
		return KPath::fromFormat("%s%c%d", name.u8(), NAME_SEPARATOR, modifier);
	}

	// モディファイ識別子を含んだテクスチャ名から、識別子を得る
	// テクスチャ名は "myTexture.tex&100" のような名前になっている。
	// この "100" の部分がモディファイ番号になっている。
	// これは "myTexture.tex" のテクスチャを 100 という方式に従って変更した画像という意味
	virtual int getTextureModifier(const KPath &modname) const override {
		// & を境にして左右２個のトークンに分ける
		auto tok = K::strSplit(modname.u8(), "&", 2);

		// トークン[1] が modifier 番号を示している
		int val = 0;
		if (tok.size()>=2 && K::strToInt(tok[1], &val)) {
			return val;
		}
		return 0;
	}
	virtual void clearTextureModifierCaches() override {
		std::vector<std::string> texnames;
		m_Mutex.lock();
		for (auto it=m_Items.begin(); it!=m_Items.end(); ++it) {
			const std::string &name = it->first;
			if (getTextureModifier(name)) {
				texnames.push_back(name); // 削除対象
			}
		}
		m_Mutex.unlock();

		for (auto it=texnames.begin(); it!=texnames.end(); ++it) {
			removeTexture(*it);
		}
	}
	virtual void clearTextureModifierCaches(int modifier_start, int count) override {
		std::vector<std::string> texnames;
		m_Mutex.lock();
		for (auto it=m_Items.begin(); it!=m_Items.end(); ++it) {
			const std::string &name = it->first;
			int mod = getTextureModifier(name);
			if (getTextureModifier(name)) {
				if (modifier_start <= mod && mod < modifier_start + count) {
					texnames.push_back(name); // 削除対象
				}
			}
		}
		m_Mutex.unlock();

		for (auto it=texnames.begin(); it!=texnames.end(); ++it) {
			removeTexture(*it);
		}
	}
	virtual KTEXID findTexture(const KPath &name, int modifier, bool should_exist, KNode *node_for_mod) override {
		// modifier が指定されていない場合は、普通に探して普通に返す
		if (modifier == 0) {
			return findTextureRaw(name, should_exist);
		}
		// modifierが指定されている。
		// modifier適用済みのテクスチャが生成済みであれば、それを返す
		KPath modname = makeTextureNameWithModifier(name, modifier);
		{
			KTEXID tex = findTextureRaw(modname, false);
			if (tex) {
				return tex;
			}
		}
		// modifier適用済みのテクスチャが存在しない
		// modifierを適用するため、元のテクスチャ画像を得る
		KTEXID basetex = findTextureRaw(name, should_exist);
		if (basetex == nullptr) {
			return nullptr;
		}

		// modifierを適用するための新しいテクスチャを作成する。
		// 念のため、空っぽのテクスチャではなく元画像のコピーで初期化しておく
		KTEXID newtex = addTextureFromTexture(modname, basetex);
		{
			KSig sig(K_SIG_TEXBANK_MODIFIER);
			sig.setNode("node", node_for_mod);
			sig.setPointer("newtex", (void*)newtex);
			sig.setString("basetexname", name.u8());
			sig.setPointer("basetex", (void*)basetex);
			sig.setInt("modifier", modifier);
			KEngine::broadcastSignal(sig);
		}
		return newtex;
	}
	virtual KTEXID addRenderTexture(const KPath &name, int w, int h, Flags flags) override {
		if (name.empty() || w <= 0 || h <= 0) {
			K__ERROR("E_TEXTUREBANK: Invalid argument at addRenderTexture");
			return nullptr;
		}

		if (flags & F_OVERWRITE_ANYWAY) {
			KAutoRef<KTextureRes> texres;
			m_Mutex.lock();
			{
				// 同名のテクスチャが存在する場合には無条件で上書きする
				if (m_Items.find(name.u8()) != m_Items.end()) {
					K__WARNING("W_TEXTURE_OVERWRITE: Texture named '%s' already exists. The texture is overritten by new one", name.u8());
					removeTexture(name);
				}
				texres.make();
				texres->loadEmptyTexture(
					w, h,
					(flags & F_FLOATING_FORMAT) ? KTexture::FMT_ARGB64F : KTexture::FMT_ARGB32,
					(flags & F_PROTECT)!=0
				);
				texres->setName(name);
				m_Items[name.u8()] = texres;
				K__VERBOSE("Add Render Texture: '%s'", name.c_str());
			}
			m_Mutex.unlock();
			return texres->m_TexId;
		}

		{
			KTEXID tex = nullptr;

			m_Mutex.lock();
			auto it = m_Items.find(name.u8());
			if (it==m_Items.end() || it->second->m_TexId==nullptr) {
				//
				// 同名のテクスチャはまだ存在しない
				//
				KAutoRef<KTextureRes> texres;
				texres.make();
				texres->loadRenderTexture(
					w, h,
					(flags & F_FLOATING_FORMAT) ? KTexture::FMT_ARGB64F : KTexture::FMT_ARGB32,
					(flags & F_PROTECT)!=0
				);
				texres->setName(name);
				m_Items[name.u8()] = texres;
				K__VERBOSE("Add Render Texture: '%s'", name.c_str());
				tex = texres->m_TexId;

			} else {
				KAutoRef<KTextureRes> oldres = it->second;
				if (! oldres->m_IsRenderTex) {
					//
					// 既に同じ名前の非レンダーテクスチャが存在する
					//
					if (flags & F_OVERWRITE_USAGE_NOT_MATCHED) {
						// 古いテクスチャを削除し、新しいレンダーテクスチャを作成して上書きする
						removeTexture(name);
						oldres = nullptr;

						KAutoRef<KTextureRes> texres;
						texres.make();
						texres->loadRenderTexture(
							w, h, 
							(flags & F_FLOATING_FORMAT) ? KTexture::FMT_ARGB64F : KTexture::FMT_ARGB32,
							(flags & F_PROTECT)!=0
						);
						texres->setName(name);
						m_Items[name.u8()] = texres;
						K__VERBOSE("Add Render Texture: '%s'", name.c_str());
						tex = texres->m_TexId;
					
					} else {
						// 使いまわしできない
						K__ERROR("incompatible texture found (type not matched): '%s'", name.u8());
					}

				} else if (oldres->m_Width != w || oldres->m_Height != h) {
					//
					// 既に同じ名前で異なるサイズのレンダーテクスチャが存在する
					//
					if (flags & F_OVERWRITE_SIZE_NOT_MATCHED) {
						// 古いテクスチャを削除し、新しいレンダーテクスチャを作成して上書きする
						removeTexture(name);
						oldres = nullptr;
						
						KAutoRef<KTextureRes> texres;
						texres.make();
						texres->loadRenderTexture(
							w, h,
							(flags & F_FLOATING_FORMAT) ? KTexture::FMT_ARGB64F : KTexture::FMT_ARGB32,
							(flags & F_PROTECT)!=0
						);
						texres->setName(name);
						m_Items[name.u8()] = texres;
						tex = texres->m_TexId;
						K__VERBOSE("Add Render Texture: '%s'", name.c_str());

					} else {
						// 使いまわしできない
						K__ERROR("incompatible texture found (size not matched): '%s'", name.u8());
					}

				} else {
					//
					// 既に同じ名前で同じサイズのレンダーテクスチャが存在する
					//
					tex = oldres->m_TexId;
				}
			}
			m_Mutex.unlock();
			return tex;
		}
	}
	virtual KTEXID addTextureFromSize(const KPath &name, int w, int h) override {
		if (name.empty()) {
			K__ERROR("E_TEXBANK: Texture name is empty");
			return nullptr;
		}
		if (w <= 0 || h <= 0) {
			K__ERROR("E_TEXBANK: Invalid texture size: %dx%d", w, h);
			return nullptr;
		}
		KTEXID tex = nullptr;
		m_Mutex.lock();
		auto it = m_Items.find(name.u8());
		if (it!=m_Items.end() && it->second->m_TexId!=nullptr) {
			KAutoRef<KTextureRes> &texres = it->second;
			if (texres->m_IsRenderTex) {
				// 同じ名前のテクスチャが存在するが、レンダーターゲットとして作成されているために使いまわしできない
				K__ERROR("E_TEXBANK: Incompatible texture found (type not matched): '%s", name.u8());
			} else if (texres->m_Width != w || texres->m_Height != h) {
				// 同じ名前のテクスチャが存在するが、サイズが変わった
				// K__WARNING("E_TEXBANK: textur size changed: '%s'", name.u8());
				tex = texres->m_TexId;
			} else {
				tex = texres->m_TexId;
			}
		} else {
			KAutoRef<KTextureRes> texres;
			texres.make();
			texres->loadEmptyTexture(w, h);
			texres->setName(name);
			m_Items[name.u8()] = texres;
			tex = texres->m_TexId;
			K__VERBOSE("Add Texture: '%s'", name.c_str());
		}
		m_Mutex.unlock();

		return tex;
	}
	virtual KTEXID addTextureFromImage(const KPath &name, const KImage &image) override {
		int img_w = image.getWidth();
		int img_h = image.getHeight();
		int tex_w = _CeilPow2(img_w); // 最も近い 2^n にサイズを合わせる
		int tex_h = _CeilPow2(img_h);
		KTEXID texid = addTextureFromSize(name, tex_w, tex_h);
		if (texid) {
			KTexture *tex = KVideo::findTexture(texid);
			if (tex) tex->writeImageToTexture(image, 1.0f, 1.0f);
		}
		return texid;
	}
	virtual KTEXID addTextureFromTexture(const KPath &name, KTEXID sourceid) override {
		KTexture *tex = KVideo::findTexture(sourceid);
		if (tex == nullptr) return nullptr;
		KImage img = tex->exportTextureImage();
		KTEXID ret = addTextureFromImage(name, img);
		return ret;
	}
	virtual KTEXID addTextureFromFileName(const KPath &name) override {
		KImage img = KImage::createFromFileName(name.c_str());
		return addTextureFromImage(name, img);
	}
	virtual KTEXID addTextureFromStream(const KPath &name, KInputStream &input) override {
		KImage img = KImage::createFromStream(input);
		return addTextureFromImage(name, img);
	}
	virtual void guiTexture(const KPath &tex_path, int box_size) override {
		if (tex_path.empty()) {
			ImGui::Text("(nullptr)");
			return;
		}
		KTEXID tex = findTextureRaw(tex_path, true);
		KPath texname = getTextureName(tex);
		guiTextureInfo(tex, box_size, texname);
	}
	virtual void guiTextureInfo(KTEXID texid, int box_size, const KPath &texname) override {
		KTexture *tex = KVideo::findTexture(texid);

		if (tex == nullptr) {
			ImGui::Text("(nullptr)");
			return;
		}
		int tex_w = tex->getWidth();
		int tex_h = tex->getHeight();
		if (tex_w == 0 || tex_h == 0 || tex->getDirect3DTexture9() == nullptr) {
			ImGui::TextColored(KImGui::COLOR_ERROR, "Texture not loaded");
			return;
		}
		int w = tex_w;
		int h = tex_h;
		bool is_pow2 = KMath::isPow2(w) && KMath::isPow2(h);
		if (!is_pow2) {
			ImGui::TextColored(KImGui::COLOR_WARNING, "Not power of 2");
		}

		if (box_size > 0) {
			if (w >= h) {
				w = box_size;
				h = tex_h * w / tex_w;
			} else {
				h = box_size;
				w = tex_w * h / tex_h;
			}
		} else {
			// 原寸で表示
		}
		static const int BORDER_COLORS_SIZE = 4;
		static const KColor BORDER_COLORS[BORDER_COLORS_SIZE] = {
			KColor(0, 0, 0, 0),
			KColor(1, 1, 1, 1),
			KColor(0, 1, 0, 1),
			KColor(0, 0, 0, 1),
		};
		static int s_border_mode = 1;

		KImGuiImageParams p;
		p.w = w;
		p.h = h;
		p.border_color = BORDER_COLORS[s_border_mode % BORDER_COLORS_SIZE];
		KImGui::Image(texid, &p);
		ImGui::Text("Is render texture: %s", tex->isRenderTarget() ? "Yes" : "No");
		ImGui::Text("Size: %dx%d", tex_w, tex_h);
		ImGui::Text("TexID: 0x%X", tex);
		ImGui::PushID(tex);
		if (ImGui::Button("Border")) {
			s_border_mode++;
		}
		if (1) {
			// テクスチャ画像を png でエクスポートする
			std::string filename = K::str_sprintf("__export__%s.png", KGamePath::escapeFileName(texname).u8());
			std::string fullname = K::pathGetFull(filename);
			KImGui::ImageExportButton("Export", texid, fullname.c_str(), false);
		}
		if (1) {
			// テクスチャ画像のアルファマスクを png でエクスポートする
			std::string filename = K::str_sprintf("__export__%s_a.png", KGamePath::escapeFileName(texname).u8());
			std::string fullname = K::pathGetFull(filename);
			KImGui::ImageExportButton("Export Alpha", texid, fullname.c_str(), true);
		}
		ImGui::PopID();
	}
	virtual void guiTextureHeader(const KPath &name, bool realsize, bool filter_rentex, bool filter_tex2d) override {
		KTexture *tex = KVideo::findTexture(findTextureRaw(name, true));
		if (tex == nullptr) return;

		bool isrentex = tex->isRenderTarget();

		if (isrentex && !filter_rentex) return;
		if (!isrentex && !filter_tex2d) return;

		bool isopen = ImGui::TreeNode(name.u8());
		ImGui::SameLine();
		ImGui::Spacing();

		// レンダーテクスチャ
		if (isrentex) {
			ImGui::SameLine();
			ImGui::TextColored(KImGui::COLOR_WARNING, "[R]");
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("[R] Render texture");
			}
		}

		// ２の累乗でないサイズ
		int w = tex->getWidth();
		int h = tex->getHeight();
		if (!KMath::isPow2(w) || !KMath::isPow2(h)) {
			ImGui::SameLine();
			ImGui::TextColored(KImGui::COLOR_WARNING, "[!2]");
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("[!2] Texture size is not power of 2");
			}
		}

		// テクスチャ情報
		if (isopen) {
			guiTexture(name, realsize ? 0 : THUMBNAIL_SIZE);
			ImGui::TreePop();
		}
	}
	virtual void guiTextureBank() override {
	#ifndef NO_IMGUI
		if (ImGui::TreeNode(KImGui::ID(0), "Textures (%d)", getTextureCount())) {
			// 表示フィルタ
			static char s_filter[256] = {0};
			ImGui::InputText("Filter", s_filter, sizeof(s_filter));

			std::vector<std::string> allnames = getTextureNameList();

			// 名前リスト
			std::vector<std::string> names;
			names.reserve(allnames.size());
			if (s_filter[0]) {
				// フィルターあり。
				// 部分一致したものだけリストに入れる
				for (auto it=allnames.cbegin(); it!=allnames.cend(); ++it) {
					if (K::strFind(*it, s_filter) >= 0) {
						names.push_back(*it);
					}
				}
			} else {
				// フィルターなし
				names.assign(allnames.begin(), allnames.end());
			}

			// ソート
			std::sort(names.begin(), names.end());

			// 表示オプション
			static bool s_realsize = false;
			ImGui::Checkbox("Real size", &s_realsize);

			static bool s_rentex = true;
			static bool s_tex2d = true;
			ImGui::Checkbox("Render texture", &s_rentex);
			ImGui::Checkbox("Normal texture", &s_tex2d);

			if (ImGui::Button("Clear Modifiers")) {
				clearTextureModifierCaches();
				// テクスチャを削除したので、names 配列は削除済みのテクスチャを含んでいる可能性がある。
				// このフレームではリスト表示しない

			} else {
				// リスト表示
				for (auto it=names.cbegin(); it!=names.cend(); ++it) {
					const std::string &name = *it;
					guiTextureHeader(name, s_realsize, s_rentex, s_tex2d);
				}
			}
			ImGui::TreePop();
		}
	#endif // !NO_IMGUI
	}
	virtual bool guiTextureSelector(const char *label, KTEXID *p_texture) override {
		std::string cur_path = (p_texture && *p_texture) ? getTextureName(*p_texture).u8() : "";
		KPath new_path;
		if (guiTextureSelector(label, cur_path, &new_path)) {
			K__ASSERT(p_texture);
			*p_texture = findTextureRaw(new_path, true);
			return true;
		}
		return false;
	}
	virtual bool guiTextureSelector(const char *label, const KPath &selected, KPath *new_selected) override {
		// ソート済みのアイテム名リストを得る
		std::vector<std::string> names = getTextureNameList();
		names.insert(names.begin(), "(nullptr)"); // nullptr を選択肢に含める
		//
		int sel_index = -1;
		if (KImGui::Combo(label, names, selected.u8(), &sel_index)) {
			if (sel_index > 0) {
				*new_selected = names[sel_index];
			} else {
				*new_selected = KPath::Empty; // nullptr が選択された
			}
			return true;
		}
		return false;
	}
	virtual bool guiRenderTextureSelector(const char *label, const KPath &selected, KPath *new_selected) override {
		// ソート済みのアイテム名リストを得る
		std::vector<std::string> names = getRenderTextureNameList();
		names.insert(names.begin(), "(nullptr)"); // nullptr を選択肢に含める
		//
		int sel_index = -1;
		if (KImGui::Combo(label, names, selected.u8(), &sel_index)) {
			if (sel_index > 0) {
				*new_selected = names[sel_index];
			} else {
				*new_selected = KPath::Empty; // nullptr が選択された
			}
			return true;
		}
		return false;
	}
};
#pragma endregion // CTextureBankImpl


#pragma region CSpriteBankImpl
class CSpriteBankImpl: public KSpriteBank {
	std::unordered_map<std::string, KSpriteAuto> m_Items;
	mutable std::recursive_mutex m_Mutex;
	KTextureBank *m_texbank;
public:
	CSpriteBankImpl() {
		m_texbank = nullptr;
	}
	virtual ~CSpriteBankImpl() {
		clearSprites();
	}
	void init(KTextureBank *tex_bank) {
		K__ASSERT(tex_bank);
		m_texbank = tex_bank;
	}
	virtual int getSpriteCount() override {
		return (int)m_Items.size();
	}
	virtual void removeSprite(const KPath &name) override {
		m_Mutex.lock();
		{
			auto it = m_Items.find(name.u8());
			if (it != m_Items.end()) {
				m_Items.erase(it);
			}
		}
		m_Mutex.unlock();
	}
	virtual void removeSpritesByTag(const std::string &_tag) override {
		KName tag = _tag;
		m_Mutex.lock();
		{
			for (auto it=m_Items.begin(); it!=m_Items.end(); /*none*/) {
				KSpriteRes *sprite = it->second;
				if (sprite->hasTag(tag)) {
					it = m_Items.erase(it);
				} else {
					it++;
				}
			}
		}
		m_Mutex.unlock();
	}
	virtual void clearSprites() override {
		m_Mutex.lock();
		{
			m_Items.clear();
		}
		m_Mutex.unlock();
	}
	virtual bool hasSprite(const KPath &name) const override {
		m_Mutex.lock();
		bool ret = m_Items.find(name.u8()) != m_Items.end();
		m_Mutex.unlock();
		return ret;
	}
	virtual void guiSpriteBank(KTextureBank *texbank) override {
	#ifndef NO_IMGUI
		m_Mutex.lock();
		if (ImGui::TreeNode("Sprites", "Sprites (%d)", m_Items.size())) {
			if (ImGui::Button("Clear")) {
				clearSprites();
			}

			bool export_images = false;
			if (ImGui::Button("Export Listed Sprites")) {
				export_images = true;
			}

			// 表示フィルタ
			static char s_filter[256] = {0};
			ImGui::InputText("Filter", s_filter, sizeof(s_filter));

			// ソート済みスプライト名一覧
			std::vector<std::string> names;
			names.reserve(m_Items.size());
			if (s_filter[0]) {
				// フィルターあり
				for (auto it=m_Items.cbegin(); it!=m_Items.cend(); ++it) {
					if (it->first.find(s_filter, 0) != std::string::npos) {
						names.push_back(it->first);
					}
				}
			} else {
				// フィルターなし
				for (auto it=m_Items.cbegin(); it!=m_Items.cend(); ++it) {
					names.push_back(it->first);
				}
			}
			std::sort(names.begin(), names.end());

			// リスト表示
			for (auto it=names.cbegin(); it!=names.cend(); ++it) {
				const KPath &name = *it;
				if (ImGui::TreeNode(name.u8())) {
					guiSpriteInfo(name, texbank);
					ImGui::TreePop();
				}
			}

			if (export_images) {
				KPath dir = "~ExportedSprites";
				if (K::fileMakeDir(dir.u8())) {
					for (auto it=names.cbegin(); it!=names.cend(); ++it) {
						const KPath &name = *it;
						KImage img = exportSpriteImage(name, texbank, 0, nullptr);
						if (!img.empty()) {
							// nameはパス区切りなども含んでいるため、エスケープしつつフラットなパスにする。
							KPath flatten = KGamePath::escapeFileName(name);
							KPath filename = dir.join(flatten).changeExtension(".png");
							img.saveToFileName(filename.u8());
						}
					}
				}
			}
			ImGui::TreePop();
		}
		m_Mutex.unlock();
	#endif // !NO_IMGUI
	}
	virtual void guiSpriteInfo(const KPath &name, KTextureBank *texbank) override {
	#ifndef NO_IMGUI
		KSpriteAuto sprite = findSprite(name, false);
		if (sprite == nullptr) {
			ImGui::Text("(NO SPRITE)");
			return;
		}
#if 1
		KTEXID texid = texbank->findTexture(sprite->m_TextureName, 0, false);
		KTexture::Desc desc;
		KVideo::tex_getDesc(texid, &desc);
		ImGui::Text("Source image size: %d x %d", desc.original_w, desc.original_h);
#else
		ImGui::Text("Source image size: %d x %d", sprite->m_ImageW, sprite->m_ImageH);
#endif
		ImGui::Text("Atlas Pos : %d, %d", sprite->m_AtlasX, sprite->m_AtlasY);
		ImGui::Text("Atlas Size: %d, %d", sprite->m_AtlasW, sprite->m_AtlasH);
		if (sprite->m_SubMeshIndex >= 0) {
			ImGui::Text("SubMesh index: %d", sprite->m_SubMeshIndex);
		} else {
			ImGui::Text("SubMesh index: ALL");
		}
		if (1) {
			ImGui::DragFloat2("Pivot", (float*)&sprite->m_Pivot);
			ImGui::Checkbox("Pivot in pixels", &sprite->m_PivotInPixels);
			ImGui::Text("Packed : %s", sprite->m_UsingPackedTexture ? "Yes" : "No");
		}
		if (1) {
			if (ImGui::TreeNode(KImGui::ID(1), "Tex: %s", sprite->m_TextureName.u8())) {
				guiSpriteTextureInfo(name, texbank);
				ImGui::TreePop();
			}
		}
		// 描画時のスプライト画像を再現
		if (sprite->m_UsingPackedTexture) {
			KPath sprite_path = name;
			KPath original_tex_path = KGamePath::getTextureAssetPath(sprite_path);
			KPath escpath = KGamePath::escapeFileName(original_tex_path);
			KPath relpath = KPath::fromFormat("__restore__%s.png", escpath.u8());
			KPath export_filename = relpath.getFullPath();
			if (texbank && ImGui::Button("Export sprite image")) {
				KImage img = exportSpriteImage(sprite_path, texbank, 0, nullptr);
				if (!img.empty() && img.saveToFileName(export_filename.u8())) {
					K__PRINT("Export texture image '%s' in '%s' ==> %s", original_tex_path.u8(), sprite->m_TextureName.u8(), export_filename.u8());
				}
			}

			if (K::pathExists(export_filename.u8())) {
				ImGui::SameLine();
				KPath label = KPath::fromFormat("Open: %s", export_filename.u8());
				if (ImGui::Button(label.u8())) {
					K::fileShellOpen(export_filename.u8());
				}
			}
		}
	#endif // !NO_IMGUI
	}
	virtual KTEXID getBakedSpriteTexture(const KPath &sprite_path, int modifier, bool should_exist, KTextureBank *texbank) override {
		// 正しいテクスチャ名とパレット名の組み合わせを得る
		KPath tex_path;
		const KSpriteAuto sp = findSprite(sprite_path, should_exist);
		if (sp == nullptr) return nullptr;
		if (sp->m_UsingPackedTexture) {
			// スプライトには、ブロック化テクスチャが指定されている
			const KPath &bctex_path = sp->m_TextureName;

			// このスプライトの本来の画像を復元したものを保持するためのテクスチャ名
			tex_path = KGamePath::getTextureAssetPath(sprite_path);

		} else {
			// スプライトには通常テクスチャが割り当てられている
			tex_path = sp->m_TextureName;
		}
		// 得られた組み合わせで作成されたテクスチャを得る
		return texbank->getTextureEx(tex_path, modifier, should_exist, nullptr);
	}
	virtual KVec3 bmpToLocal(const KPath &name, const KVec3 &bmp) override {
		KSpriteAuto sp = findSprite(name, false);
		return sp != nullptr ? sp->bmpToLocal(bmp) : KVec3();
	}
	virtual KImage exportSpriteImage(const KPath &sprite_path, KTextureBank *texbank, int modofier, KNode *node_for_mod) override {
		// 準備
		const KSpriteAuto sp = findSprite(sprite_path, false);
		if (sp == nullptr) {
			K__ERROR("Failed to exportSpriteImage: No sprite named '%s'", sprite_path.u8());
			return KImage();
		}
		const KMesh *mesh = &sp->m_Mesh;
		if (mesh == nullptr) {
			K__ERROR("Failed to exportSpriteImage: No mesh in '%s'", sprite_path.u8());
			return KImage();
		}
		if (mesh->getVertexCount() == 0) {
			K__ERROR("Failed to exportSpriteImage: No vertices in '%s", sprite_path.u8());
			return KImage();
		}
		if (mesh->getSubMeshCount() == 0) {
			K__ERROR("Failed to exportSpriteImage: No submeshes in '%s", sprite_path.u8());
			return KImage();
		}
	//	KTexture *tex = KVideo::findTexture(texbank->findTextureRaw(sp->m_TextureName, false));
		KTexture *tex = KVideo::findTexture(texbank->findTexture(sp->m_TextureName, modofier, false, node_for_mod));
		if (tex == nullptr) {
			K__ERROR("Failed to exportSpriteImage: No texture named '%s'", sp->m_TextureName.u8());
			return KImage();
		}

		// 実際のテクスチャ画像を取得。ブロック化されている場合はこの画像がパズル状態になっている
		KImage raw_img = tex->exportTextureImage();
		if (raw_img.empty()) {
			K__ERROR("Failed to exportSpriteImage: Failed to Texture::exportImage() for texture '%s'", sp->m_TextureName.u8());
			return KImage();
		}
		// 完成画像の保存先
		KImage image = KImage::createFromPixels(sp->m_AtlasW, sp->m_AtlasH, KColorFormat_RGBA32, nullptr);
		// 元の画像を復元する
		export_sprite_image(image, raw_img, sp, mesh);
		// 終了
		return image;
	}
	void export_sprite_image(KImage &dest_image, const KImage &src_image, const KSpriteAuto &sprite, const KMesh *mesh) {
		K__ASSERT(sprite != nullptr);
		K__ASSERT(mesh);
		K__ASSERT(mesh->getSubMeshCount() > 0);
		K__ASSERT(mesh->getVertexCount() > 0);
		if (mesh->getIndexCount() > 0) {
			// インデックス配列を使って定義されている

			// 画像を復元する。この部分は KImgPackR::getMeshPositionArray によるメッシュ作成手順に依存していることに注意！
			const KSubMesh *sub = mesh->getSubMesh(0);
			K__ASSERT(sub->primitive == KPrimitive_TRIANGLES);
			K__ASSERT(mesh->getIndexCount() % 6 == 0); // 頂点数が6の倍数であること
			const int index_count = mesh->getIndexCount();
			const int *indices = mesh->getIndices();
			const int src_w = src_image.getWidth();
			const int src_h = src_image.getHeight();
			for (int i=0; i<index_count; i+=6) {
				K__ASSERT(indices[i] == i);
				const KVec3 p0 = mesh->getPosition(i + 0); // 0 ---- 1 左上から時計回りに 0, 1, 2, 3 になっている
				const KVec3 p1 = mesh->getPosition(i + 1); // |      |
				const KVec3 p2 = mesh->getPosition(i + 2); // |      |
				const KVec3 p3 = mesh->getPosition(i + 3); // 3------2
				// p0, p1, p2, p3 は座標軸に平行な矩形になっている
				K__ASSERT(p0.x == p3.x);
				K__ASSERT(p1.x == p2.x);
				K__ASSERT(p0.y == p1.y);
				K__ASSERT(p2.y == p3.y);
				int left   = (int)p0.x;
				int right  = (int)p2.x;
				int top    = (int)p0.y;
				int bottom = (int)p2.y;
				if (bottom < top) {
					// 左下を原点とした座標を使っている
					top    = sprite->m_ImageH - top;
					bottom = sprite->m_ImageH - bottom;
				}
				// 必ずひとつ以上のピクセルを含んでいる。
				// 元画像が不透明ピクセルを一切含んでいない場合は、そもそもメッシュが生成されないはず
				K__ASSERT(left < right);
				K__ASSERT(top < bottom);
				if (! sprite->m_UsingPackedTexture) {
					// 元画像が中途半端なサイズだった場合、ブロックが元画像範囲からはみ出ている場合がある事に注意
					if (sprite->m_ImageW < right ) right  = sprite->m_ImageW;
					if (sprite->m_ImageH < bottom) bottom = sprite->m_ImageH;
				}
				const KVec2 t0 = mesh->getTexCoord(i + 0);
				const KVec2 t1 = mesh->getTexCoord(i + 1);
				const KVec2 t2 = mesh->getTexCoord(i + 2);
				const KVec2 t3 = mesh->getTexCoord(i + 3);
				const int cpy_w = right - left;
				const int cpy_h = bottom - top;
				const int src_x = (int)(src_w * t0.x);
				const int src_y = (int)(src_h * t0.y);
				dest_image.copyRect(left, top, src_image, src_x, src_y, cpy_w, cpy_h);
			}

		} else {
			// インデックス配列なしで定義されている

			// 画像を復元する。この部分は KImgPackR::getMeshPositionArray によるメッシュ作成手順に依存していることに注意！
			const KSubMesh *sub = mesh->getSubMesh(0);
			const int vertex_count = mesh->getVertexCount();
			if (sub->primitive == KPrimitive_TRIANGLES) {
				K__ASSERT(vertex_count % 6 == 0); // 頂点数が6の倍数であること
				const int src_w = src_image.getWidth();
				const int src_h = src_image.getHeight();
				for (int i=0; i<vertex_count; i+=6) {
					const KVec3 p0 = mesh->getPosition(i + 0); // 0 ---- 1 左上から時計回りに 0, 1, 2, 3 になっている
					const KVec3 p1 = mesh->getPosition(i + 1); // |      |
					const KVec3 p2 = mesh->getPosition(i + 2); // |      |
					const KVec3 p3 = mesh->getPosition(i + 3); // 3------2
					// p0, p1, p2, p3 は座標軸に平行な矩形になっている
					K__ASSERT(p0.x == p3.x);
					K__ASSERT(p1.x == p2.x);
					K__ASSERT(p0.y == p1.y);
					K__ASSERT(p2.y == p3.y);
					int left   = (int)p0.x;
					int right  = (int)p2.x;
					int top    = (int)p0.y;
					int bottom = (int)p2.y;
					if (bottom < top) {
						// 左下を原点とした座標を使っている
						top    = sprite->m_ImageH - top;
						bottom = sprite->m_ImageH - bottom;
					}
					// 必ずひとつ以上のピクセルを含んでいる。
					// 元画像が不透明ピクセルを一切含んでいない場合は、そもそもメッシュが生成されないはず
					K__ASSERT(left < right);
					K__ASSERT(top < bottom);
					if (! sprite->m_UsingPackedTexture) {
						// 元画像が中途半端なサイズだった場合、ブロックが元画像範囲からはみ出ている場合がある事に注意
						if (sprite->m_ImageW < right ) right  = sprite->m_ImageW;
						if (sprite->m_ImageH < bottom) bottom = sprite->m_ImageH;
					}
					const KVec2 t0 = mesh->getTexCoord(i + 0);
					const KVec2 t1 = mesh->getTexCoord(i + 1);
					const KVec2 t2 = mesh->getTexCoord(i + 2);
					const KVec2 t3 = mesh->getTexCoord(i + 3);
					const int cpy_w = right - left;
					const int cpy_h = bottom - top;
					const int src_x = (int)(src_w * t0.x);
					const int src_y = (int)(src_h * t0.y);
					dest_image.copyRect(left, top, src_image, src_x, src_y, cpy_w, cpy_h);
				}
			} else if (sub->primitive == KPrimitive_TRIANGLE_STRIP) {
				K__ASSERT(vertex_count == 4);
				const int src_w = src_image.getWidth();
				const int src_h = src_image.getHeight();
				for (int i=0; i<vertex_count; i+=4) {
					const KVec3 p0 = mesh->getPosition(i + 0); // 0 ---- 1 左上からZ字型に 0, 1, 2, 3 になっている
					const KVec3 p1 = mesh->getPosition(i + 1); // |      |
					const KVec3 p2 = mesh->getPosition(i + 2); // |      |
					const KVec3 p3 = mesh->getPosition(i + 3); // 2------3
					// p0, p1, p2, p3 は座標軸に平行な矩形になっている
					K__ASSERT(p0.x == p2.x);
					K__ASSERT(p1.x == p3.x);
					K__ASSERT(p0.y == p1.y);
					K__ASSERT(p2.y == p3.y);
					int left   = (int)p0.x;
					int right  = (int)p3.x;
					int top    = (int)p0.y;
					int bottom = (int)p3.y;
					if (bottom < top) {
						// 左下を原点とした座標を使っている
						top    = sprite->m_ImageH - top;
						bottom = sprite->m_ImageH - bottom;
					}
					// 必ずひとつ以上のピクセルを含んでいる。
					// 元画像が不透明ピクセルを一切含んでいない場合は、そもそもメッシュが生成されないはず
					K__ASSERT(left < right);
					K__ASSERT(top < bottom);
					if (! sprite->m_UsingPackedTexture) {
						// 元画像が中途半端なサイズだった場合、ブロックが元画像範囲からはみ出ている場合がある事に注意
						if (sprite->m_ImageW < right ) right  = sprite->m_ImageW;
						if (sprite->m_ImageH < bottom) bottom = sprite->m_ImageH;
					}
					const KVec2 t0 = mesh->getTexCoord(i + 0);
					const KVec2 t1 = mesh->getTexCoord(i + 1);
					const KVec2 t2 = mesh->getTexCoord(i + 2);
					const KVec2 t3 = mesh->getTexCoord(i + 3);
					const int cpy_w = right - left;
					const int cpy_h = bottom - top;
					const int src_x = (int)(src_w * t0.x);
					const int src_y = (int)(src_h * t0.y);
					dest_image.copyRect(left, top, src_image, src_x, src_y, cpy_w, cpy_h);
				}
			} else {
				K__ASSERT(0 && "Invalid primitive type");
			}
		}
	}
	virtual void guiSpriteTextureInfo(const KPath &name, KTextureBank *texbank) override {
		KSpriteAuto sprite = findSprite(name, false);
		if (sprite != nullptr) {
			K__ASSERT(texbank);
			texbank->guiTexture(sprite->m_TextureName, THUMBNAIL_SIZE);
		}
	}
	virtual bool guiSpriteSelector(const char *label, KPath *path) override {
		//int sel_index = -1;
		//if (KImGui_Combo(label, names, *path, &sel_index)) {
		//	if (sel_index > 0) {
		//		*path = names[sel_index];
		//	} else {
		//		*path = ""; // nullptr が選択された
		//	}
		//	return true;
		//}
		return false;
	}
	virtual KSpriteAuto findSprite(const KPath &name, bool should_exist) override {
		KSpriteAuto sp = nullptr;
		m_Mutex.lock();
		auto it = m_Items.find(name.u8());
		if (it != m_Items.end()) {
			sp = it->second;
			K__ASSERT(sp != nullptr);
		} else if (should_exist) {
			// もしかして
			// 例えば "player.sprite" は "chara/player.sprite" とはマッチしない。
			// ディレクトリ名を忘れているかどうかチェックする
			KPath probably;
			for (auto it2=m_Items.begin(); it2!=m_Items.end(); it2++) {
				if (K::pathEndsWith(it2->first, name.u8())) {
					probably = it2->first;
					break;
				}
			}
			if (probably.empty()) {
				K__ERROR(
					u8"E_SPRITE: スプライト '%s' はロードされていません。"
					u8"拡張子を間違えていませんか？ '.png' や '.tex' はテクスチャであり、スプライトではありません"
					u8"スプライトには通常 .sprite を用います"
					u8"xres のロード忘れ, xres 内での定義忘れ, "
					u8"KTextureBank::addTexture での登録忘れの可能性があります",
					name.u8()
				);
			} else {
				K__ERROR(
					u8"E_SPRITE: スプライト '%s' はロードされていません。もしかして: '%s'",
					name.u8(),
					probably.u8()
				);
			}
		}
		m_Mutex.unlock();
		return sp;
	}
	virtual bool setSpriteTexture(const KPath &name, const KPath &texture) override {
		KSpriteAuto sp = findSprite(name, false);
		if (sp != nullptr) {
			sp->m_TextureName = texture;
			return true;
		}
		return false;
	}
	virtual KTEXID getSpriteTextureAtlas(const KPath &name, float *u0, float *v0, float *u1, float *v1) override {
		const KSpriteAuto sp = findSprite(name, false);
		if (sp == nullptr) return nullptr;

		K__ASSERT_RETURN_ZERO(m_texbank);

		KTexture *tex = KVideo::findTexture(m_texbank->findTextureRaw(sp->m_TextureName, true));
		if (tex) {
			int texw = tex->getWidth();
			int texh = tex->getHeight();
			*u0 = (float)sp->m_AtlasX / texw;
			*v0 = (float)sp->m_AtlasY / texh;
			*u1 = (float)(sp->m_AtlasX + sp->m_AtlasW) / texw;
			*v1 = (float)(sp->m_AtlasY + sp->m_AtlasH) / texh;
			return tex->getId();
		}
		return nullptr;
	}
	virtual bool addSpriteFromDesc(const KPath &name, KSpriteAuto sp, bool update_mesh) override {
		if (name.empty()) { K__ERROR("Sprite name is empty"); return false; }
		if (sp->m_TextureName.empty()) { K__ERROR("Texture name is empty"); return false; }
		if (sp->m_AtlasW == 0 || sp->m_AtlasH == 0) { K__ERROR("Sprite size is zero"); return false; }

		if (hasSprite(name)) return false;

		// 同名のリソースがあれば削除する
		removeSprite(name);

		m_Mutex.lock();
		{
			m_Items[name.u8()] = sp;
			K__VERBOSE("ADD_SPRITE: %s", name.u8());
		}
		m_Mutex.unlock();

		if (update_mesh) {
			KSpriteAuto item = m_Items[name.u8()];
			if (item->m_Mesh.getVertexCount() == 0) {
				K__ASSERT(m_texbank);
				updateSpriteMesh(item, m_texbank);
			} else {
				item->m_Mesh.copyFrom(&sp->m_Mesh);
			}
		}
		return true;
	}
	virtual bool addSpriteFromTextureRect(const KPath &sprite_name, const KPath &tex_name, int x, int y, int w, int h, int ox, int oy) override {
		KSpriteRes *sp = new KSpriteRes();
		sp->m_TextureName = tex_name;
	//	sp->m_ImageW = img.getWidth();
	//	sp->m_ImageH = img.getHeight();
		sp->m_AtlasX = x;
		sp->m_AtlasY = y;
		sp->m_AtlasW = w;
		sp->m_AtlasH = h;
		sp->m_Pivot.x = (float)ox;
		sp->m_Pivot.y = (float)oy;
		sp->m_PivotInPixels = true;
		bool ret = addSpriteFromDesc(sprite_name, KSpriteAuto(sp), true);
		sp->drop();
		return ret;
	}
	virtual bool addSpriteAndTextureFromImage(const KPath &spr_name, const KPath &tex_name, const KImage &img, int ox, int oy) override {
		// .png 画像をテクスチャおよびスプライトとして登録する
		if (img.empty()) return false;

		// テクスチャを登録する
		K__ASSERT(m_texbank);
		m_texbank->addTextureFromImage(tex_name, img);

		bool ok = false;
		KSpriteRes *sp = new KSpriteRes();
		if (sp->buildFromImageEx(img.getWidth(), img.getHeight(), tex_name, -1, KBlend_INVALID, false)) {
			sp->m_Pivot.x = (float)ox;
			sp->m_Pivot.y = (float)oy;
			sp->m_PivotInPixels = true;
			K__ASSERT(sp->m_AtlasW > 0);
			K__ASSERT(sp->m_AtlasH > 0);
			if (addSpriteFromDesc(spr_name, KSpriteAuto(sp), true)) {
				ok = true;
			}
		}
		sp->drop();
		return ok;
	}
private:
	static void updateSpriteMesh(KSpriteAuto &sp, KTextureBank *tbank) {
		K__ASSERT(sp != nullptr);
		K__ASSERT(tbank);

		// インデックスなし。ただの png ファイルを参照する
		const KPath &tex_name = sp->m_TextureName;
		KTexture *tex = KVideo::findTexture(tbank->findTextureRaw(tex_name, true));
		K__ASSERT_RETURN(tex);
		
		int tex_w = tex->getWidth();
		int tex_h = tex->getHeight();

		int x = 0;
		int y = 0;

		int tx0 = sp->m_AtlasX;
		int ty0 = sp->m_AtlasY;
		int tx1 = sp->m_AtlasX + sp->m_AtlasW;
		int ty1 = sp->m_AtlasY + sp->m_AtlasH;

		float u0 = (float)tx0 / tex_w;
		float v0 = (float)ty0 / tex_h;
		float u1 = (float)tx1 / tex_w;
		float v1 = (float)ty1 / tex_h;

		MeshShape::makeRect(&sp->m_Mesh, KVec2(x, y), KVec2(x+sp->m_AtlasW, y+sp->m_AtlasH), KVec2(u0, v0), KVec2(u1, v1), KColor32::WHITE);
	}
};
#pragma endregion // CSpriteBankImpl


#pragma region CShaderBankImpl
class CShaderBankImpl: public KShaderBank {
	std::unordered_map<KPath, KShaderAuto> m_Items;
	mutable std::recursive_mutex m_Mutex;
	bool m_hlsl_available;
	bool m_glsl_available;
public:
	CShaderBankImpl() {
		m_hlsl_available = false;
		m_glsl_available = false;
	}
	virtual ~CShaderBankImpl() {
		clearShaders();
	}
	KShaderAuto findById(KSHADERID sid) const {
		for (auto it=m_Items.begin(); it!=m_Items.end(); ++it) {
			const KShaderAuto &sh = it->second;
			if (sh->getId() == sid) {
				return sh;
			}
		}
		return nullptr;
	}
	KShaderAuto findByName(const KPath &name) const {
		for (auto it=m_Items.begin(); it!=m_Items.end(); ++it) {
			if (it->first == name) {
				return it->second;
			}
		}
		return nullptr;
	}
	void init() {
		int hlsl = 0;
		int glsl = 0;
		KVideo::getParameter(KVideo::PARAM_HAS_HLSL, &hlsl);
		KVideo::getParameter(KVideo::PARAM_HAS_GLSL, &glsl);
		m_hlsl_available = hlsl != 0;
		m_glsl_available = glsl != 0;
	}
	virtual KPath getShaderName(KSHADERID s) const override {
		const KShaderAuto &sh = findById(s);
		KPath name;
		m_Mutex.lock();
		if (sh != nullptr) {
			name = sh->m_Name;
		}
		m_Mutex.unlock();
		return name;
	}
	virtual std::vector<std::string> getShaderNameList() const override {
		std::vector<std::string> names;
		{
			names.reserve(m_Items.size());
			for (auto it=m_Items.cbegin(); it!=m_Items.cend(); ++it) {
				names.push_back(it->first.u8());
			}
			std::sort(names.begin(), names.end());
		}
		return names;
	}
	virtual void setTag(KSHADERID id, KName tag) override {
		m_Mutex.lock();
		{
			KShaderAuto sh = findById(id);
			if (sh != nullptr) {
				sh->addTag(tag);
			}
		}
		m_Mutex.unlock();
	}
	virtual bool hasTag(KSHADERID id, KName tag) const override {
		bool result = false;
		m_Mutex.lock();
		{
			KShaderAuto sh = findById(id);
			if (sh != nullptr && sh->hasTag(tag)) {
				result = true;
			}
		}
		m_Mutex.unlock();
		return result;
	}
	virtual const KNameList & getTags(KSHADERID id) const override {
		static KNameList s_empty;

		KNameList &result = s_empty;
		m_Mutex.lock();
		{
			KShaderAuto sh = findById(id);
			if (sh != nullptr) {
				result = sh->getTags();
			}
		}
		m_Mutex.unlock();
		return result;
	}
	virtual void addShader(const KPath &name, KShaderAuto shader) override {
		m_Mutex.lock();
		{
			auto it = m_Items.find(name);
			if (it != m_Items.end()) {
				KShaderAuto &sh = it->second;
				sh->release(); // 同名のシェーダーが存在する。古いものを削除する
			}
			shader->addTag(name.u8()); // 作成元のファイル名をタグとして追加しておく setTag
			m_Items[name] = shader;
		}
		m_Mutex.unlock();
	}
	virtual KSHADERID addShaderFromHLSL(const KPath &name, const char *code) override {
		KSHADERID sid = nullptr;
		KShaderRes *sh = new KShaderRes();
		if (sh->loadFromHLSL(name.u8(), code)) {
			addShader(name, KShaderAuto(sh));
			sid = sh->getId();
			sh->drop();
		}
		return sid;
	}
	virtual KSHADERID addShaderFromStream(KInputStream &input, const char *name) override {
		KSHADERID sh = nullptr;
		if (input.isOpen()) {
			std::string code = input.readBin();
			sh = addShaderFromHLSL(name, code.c_str());
		}
		return sh;
	}
	virtual KSHADERID addShaderFromGLSL(const KPath &name, const char *vs_code, const char *fs_code) override {
#if 0
		KSHADERID sh = m_video->createShaderFromGLSL(vs_code, fs_code, name.u8());
		if (sh == nullptr) {
			K__ERROR("E_SHADER: failed to build GLSL shader.");
			return nullptr;
		}
		m_Mutex.lock();
		{
			if (m_Items.find(name) != m_Items.end()) {
				// 同名のシェーダーが存在する。古いものを削除する
				m_video->deleteShader(m_Items[name]);
			}
			m_Items[name] = sh;
		}
		m_Mutex.unlock();
		return sh;
#else
		return nullptr;
#endif
	}
	virtual int getShaderCount() const override { 
		return (int)m_Items.size();
	}
	virtual void removeShader(const KPath &name) override {
		m_Mutex.lock();
		{
			auto it = m_Items.find(name);
			if (it != m_Items.end()) {
				KShaderAuto &sh = it->second;
				sh->release();
				m_Items.erase(it);
				K__VERBOSE("Del shader: %s", name.u8());
			}
		}
		m_Mutex.unlock();
	}
	virtual void removeShadersByTag(KName tag) override {
		m_Mutex.lock();
		{
			for (auto it=m_Items.begin(); it!=m_Items.end(); /*no expr*/) {
				KShaderAuto &sh = it->second;
				if (sh->hasTag(tag)) {
					sh->release();
					K__VERBOSE("Del shader: %s (by tag \"%s\")", it->first.u8(), tag.c_str());
					it = m_Items.erase(it);
				} else {
					it++;
				}
			}
		}
		m_Mutex.unlock();
	}
	virtual void clearShaders() override {
		m_Mutex.lock();
		{
			for (auto it=m_Items.begin(); it!=m_Items.end(); ++it) {
				KShaderAuto &sh = it->second;
				sh->release();
			}
			m_Items.clear();
		}
		m_Mutex.unlock();
	}
	virtual bool hasShader(const KPath &name) const override {
		m_Mutex.lock();
		bool ret = m_Items.find(name) != m_Items.end();
		m_Mutex.unlock();
		return ret;
	}
	virtual KSHADERID findShader(const KPath &name, bool should_exist) override {
		KShaderAuto sh = findShaderAuto(name, should_exist);
		if (sh != nullptr) {
			return sh->getId();
		}
		return nullptr;
	}
	virtual KShaderAuto findShaderAuto(const KPath &name, bool should_exist) override {
		KShaderAuto ret = nullptr;
		m_Mutex.lock();
		{
			auto it = m_Items.find(name);
			if (it != m_Items.end()) {
				ret = it->second;
			}
		}
		m_Mutex.unlock();
		if (ret != nullptr) return ret;
		if (should_exist) {
			K__ERROR("No shader named '%s'", name.u8());
		}
		return nullptr;
	}
	virtual bool guiShaderSelector(const char *label, KSHADERID *p_shader) override {
		KPath cur_path = *p_shader ? getShaderName(*p_shader) : KPath::Empty;
		KPath new_path;
		if (guiShaderSelector(label, cur_path, &new_path)) {
			*p_shader = findShader(new_path, false); // 「シェーダーなし」が選択される可能性に注意
			return true;
		}
		return false;
	}
	virtual bool guiShaderSelector(const char *label, const KPath &selected, KPath *new_selected) override {
		#ifndef NO_IMGUI
		// ソート済みのアイテム名リストを得る
		std::vector<std::string> names = getShaderNameList();
		names.insert(names.begin(), "(nullptr)"); // nullptr を選択肢に含める

		int sel_index = -1;
		if (KImGui::Combo(label, names, selected.u8(), &sel_index)) {
			if (sel_index > 0) {
				*new_selected = names[sel_index];
			} else {
				*new_selected = KPath::Empty; // nullptr が選択された
			}
			return true;
		}
		#endif // !NO_IMGUI
		return false;
	}
};
#pragma endregion // CShaderBankImpl


#pragma region KAnimationBank
class CAnimationBank: public KAnimationBank {
	std::unordered_map<KPath, KClipRes *> m_clips;
	mutable std::recursive_mutex m_Mutex;
public:
	virtual void clearClipResources() override {
		m_Mutex.lock();
		{
			for (auto it=m_clips.begin(); it!=m_clips.end(); ++it) {
				K__DROP(it->second);
			}
			m_clips.clear();
		}
		m_Mutex.unlock();
	}
	virtual void addClipResource(const std::string &name, KClipRes *clip) override {
		if (clip == nullptr) {
			INVALID_OPERATION;
			return;
		}

		m_Mutex.lock();
		{
			if (m_clips.find(name) != m_clips.end()) {
				K__PRINT("W_CLIP_OVERWRITE: %s", name.c_str());
				removeClipResource(name);
			}
			K__ASSERT(m_clips.find(name) == m_clips.end());
			clip->grab();
			m_clips[name] = clip;
			K__VERBOSE("ADD_CLIP: %s", name.c_str());
		}
		m_Mutex.unlock();

	}
	virtual void removeClipResource(const std::string &name) override {
		m_Mutex.lock();
		{
			auto it = m_clips.find(name);
			if (it != m_clips.end()) {
				it->second->drop();
				m_clips.erase(it);
			}
		}
		m_Mutex.unlock();
	}
	virtual void removeClipResourceByTag(const std::string &_tag) override {
		KName tag = _tag;
		m_Mutex.lock();
		{
			for (auto it=m_clips.begin(); it!=m_clips.end(); /*none*/) {
				KClipRes *clip = it->second;
				if (clip->hasTag(tag)) {
					clip->drop();
					it = m_clips.erase(it);
				} else {
					it++;
				}
			}
		}
		m_Mutex.unlock();
	}
	virtual KClipRes * getClipResource(const std::string &name) const override {
		KClipRes *ret;
		m_Mutex.lock();
		{
			auto it = m_clips.find(name);
			ret = (it!=m_clips.end()) ? it->second : nullptr;
		}
		m_Mutex.unlock();
		return ret;
	}
	virtual KClipRes * find_clip(const std::string &clipname) override {
		// 指定されたクリップを得る。
		// クリップ名が指定されているのに見つからなかった場合はエラーログ出す
		KClipRes *clip = getClipResource(clipname);
		if (!clipname.empty() && clip == nullptr) {
			if (clip == nullptr) {
				K__ERROR(
					u8"E_CLIP: アニメ '%s' はロードされていません。"
					u8"xres のロード忘れ, xres 内での定義忘れ, "
					u8"KAnimationBank::addClipResource での登録忘れの可能性があります",
					clipname.c_str()
				);
			}
		}
		return clip;
	}
	virtual void guiClips() const override {
	#ifndef NO_IMGUI
		if (ImGui::TreeNode(KImGui::ID(0), u8"スプライトアニメ(%d)", m_clips.size())) {
			// 表示フィルタ
			static char s_filter[256] = {0};
			ImGui::InputText("Filter", s_filter, sizeof(s_filter));

			// 名前リスト
			KPathList names;
			names.reserve(m_clips.size());
			if (s_filter[0]) {
				// フィルターあり
				// 部分一致したものだけリストに入れる
				for (auto it=m_clips.cbegin(); it!=m_clips.cend(); ++it) {
					const char *s = it->first.u8();
					if (K::strFind(s, s_filter) >= 0) {
						names.push_back(it->first);
					}
				}
			} else {
				// フィルターなし
				for (auto it=m_clips.cbegin(); it!=m_clips.cend(); ++it) {
					names.push_back(it->first);
				}
			}

			// ソート
			std::sort(names.begin(), names.end());

			// リスト表示
			for (auto it=names.cbegin(); it!=names.cend(); ++it) {
				const KPath &name = *it;
				if (ImGui::TreeNode(name.u8())) {
					auto cit = m_clips.find(name);
					K__ASSERT(cit != m_clips.end());
					KClipRes *clip = cit->second;
					guiClip(clip);
					ImGui::TreePop();
				}
			}
			ImGui::TreePop();
		}
	#endif // !NO_IMGUI
	}
	virtual bool guiClip(KClipRes *clip) const override {
	#ifndef NO_IMGUI
		if (clip == nullptr) {
			ImGui::Text("(NO CLIP)");
			return false;
		}
		ImGui::Text("Length: %d", clip->getLength());
		ImGui::Text("Loop: %s", clip->getFlag(KClipRes::FLAG_LOOP) ? "Yes" : "No");
		ImGui::Text("Remove entity at end: %s", clip->getFlag(KClipRes::FLAG_KILL_SELF) ? "Yes" : "No");
		const KNameList &tags = clip->getTags();
		ImGui::Text("Tags: ");
		if (tags.size() > 0) {
			for (auto it=tags.begin(); it!=tags.end(); ++it) {
				ImGui::SameLine();
				ImGui::Text("[%s]", it->c_str());
			}
		} else {
			ImGui::SameLine();
			ImGui::Text("no tags");
		}

		if (clip) {
			ImGui::Text("%s", clip->getName());
			if (ImGui::TreeNode(KImGui::ID(-2), "segments")) {
				clip->on_track_gui();
				ImGui::TreePop();
			}
		}
	#endif // NO_IMGUI
		return false;
	}
	virtual KPathList getClipNames() const override {
		KPathList list;
		for (auto it=m_clips.begin(); it!=m_clips.end(); ++it) {
			list.push_back(it->first);
		}
		return list;
	}
};
#pragma endregion // KAnimationBank



#pragma region LuaBank
KLuaBank::KLuaBank() {
	m_Cb = nullptr;
}
KLuaBank::~KLuaBank() {
	K__DROP(m_Storage);
	clear();
}
void KLuaBank::setStorage(KStorage *storage) {
	K__DROP(m_Storage);
	m_Storage = storage;
	K__GRAB(m_Storage);
}
void KLuaBank::setCallback(KLuaBankCallback *cb) {
	m_Cb = cb;
}
lua_State * KLuaBank::addEmptyScript(const std::string &name) {
	K__VERBOSE("LUA_BANK: new script '%s'", name.c_str());
	lua_State *ls = luaL_newstate();
	luaL_openlibs(ls);
	m_Mutex.lock();
	{
		if (m_Items.find(name) != m_Items.end()) {
			K__WARNING("W_SCRIPT_OVERWRITE: Resource named '%s' already exists. The resource data will be overwriten by new one", name.c_str());
			this->remove(name);
		}
		K__ASSERT(m_Items[name] == nullptr);
		m_Items[name] = ls;
	}
	m_Mutex.unlock();
	return ls;
}
lua_State * KLuaBank::findScript(const std::string &name) const {
	lua_State *ret;
	m_Mutex.lock();
	{
		auto it = m_Items.find(name);
		ret = (it!=m_Items.end()) ? it->second : nullptr;
	}
	m_Mutex.unlock();
	return ret;
}
bool KLuaBank::addScript(const std::string &name, const std::string &code) {
	if (name.empty()) {
		K__ERROR("LUA_BANK: Failed to addScript. Invalid name");
		return false;
	}
	if (code.empty()) {
		K__ERROR("LUA_BANK: Failed to addScript named '%s': Invaliad code or size", name.c_str());
		return false;
	}

	lua_State *ls = addEmptyScript(name);
	K__ASSERT(ls);
	// ロードする
	if (luaL_loadbuffer(ls, code.c_str(), code.size(), name.c_str()) != LUA_OK) {
		const char *msg = luaL_optstring(ls, -1, __FUNCTION__);
		K__ERROR("LUA_BANK: Failed to addScript named '%s': %s", name.c_str(), msg);
		this->remove(name);
		return false;
	}

	// チャンク実行よりも前にライブラリをインストールしたい時などに使う
	if (m_Cb) m_Cb->on_luabank_load(ls, name);

	// 再外周のチャンクを実行し、グローバル変数名や関数名を解決する
	if (lua_pcall(ls, 0, 0, 0) != LUA_OK) {
		const char *msg = luaL_optstring(ls, -1, __FUNCTION__);
		K__ERROR("LUA_BANK: Failed to addScript named '%s': %s", name.c_str(), msg);
		this->remove(name);
		return false;
	}
	return true;
}
lua_State * KLuaBank::queryScript(const std::string &name, bool reload) {
	// 危険。makeThread ですでに派生スレッドを作っていた場合、
	// ここで元ステートを削除してしまうと生成済みの派生スレッドが無効になってしまう。
	#if 1
	if (reload) {
		this->remove(name);
	}
	#endif

	lua_State *ls = findScript(name);
	if (ls == nullptr) {
		std::string bin = m_Storage->loadBinary(name.c_str());
		if (addScript(name, bin)) {
			ls = findScript(name);
		}
	}
	return ls;
}
lua_State * KLuaBank::makeThread(const std::string &name) {
	lua_State *ls = queryScript(name);
	K__ASSERT(ls);
	return lua_newthread(ls);
}

bool KLuaBank::contains(const std::string &name) const {
	bool ret;
	m_Mutex.lock();
	{
		ret = m_Items.find(name) != m_Items.end();
	}
	m_Mutex.unlock();
	return ret;
}
void KLuaBank::remove(const std::string &name) {
	m_Mutex.lock();
	{
		auto it = m_Items.find(name);
		if (it != m_Items.end()) {
			lua_State *ls = it->second;
			lua_close(ls);
			K__VERBOSE("LUA_BANK: remove '%s'", name.c_str());
			m_Items.erase(it);
		}
	}
	m_Mutex.unlock();
}
void KLuaBank::clear() {
	m_Mutex.lock();
	{
		for (auto it=m_Items.begin(); it!=m_Items.end(); ++it) {
			lua_State *ls = it->second;
			lua_close(ls);
		}
		m_Items.clear();
	}
	m_Mutex.unlock();
}
#pragma endregion // KLuaBank



#pragma region CFontBankImpl
class CFontBankImpl: public KFontBank {
	mutable std::unordered_map<std::string, KFont> m_fonts;

	// デフォルトフォント。
	// フォントが全くロードされていない時でも利用可能なフォント。
	// 何もしなくても、必要な時に自動的にロードされる
	mutable KFont m_system_default_font;

	// フォールバックフォント。
	// 指定されたフォントが見つからない場合に使う代替フォント名。
	// （フォント名は getFont で指定されたもの）
	std::string m_fallback_name;
	std::vector<std::string> m_fontnames;
public:
	CFontBankImpl() {
	}
	virtual ~CFontBankImpl() {
		clearFonts();
	}
	void init(KTextureBank *texbank) {
		KFont::globalSetup(texbank);
	}
	void shutdown() {
		KFont::globalShutdown();
		clearFonts();
	}
	virtual void clearFonts() override {
		m_fonts.clear();
	}
	virtual bool addFont(const std::string &alias, KFont &font) override {
		if (alias.empty()) return false;
		if (!font.isOpen()) return false;
		deleteFont(alias);
		m_fonts[alias] = font;
		m_fontnames.push_back(alias);
		return true;
	}
	virtual bool addFontFromStream(const std::string &alias, KInputStream &input, const std::string &filename, int ttc_index, KFont *out_font) override {
		// ファイルをロード
		std::string bin = input.readBin();
		if (!bin.empty()) {
			KFont font = KFont::createFromMemory(bin.data(), bin.size());
			if (font.isOpen()) {
				if (addFont(alias, font)) {
					if (out_font) {
						*out_font = font;
					}
					return true;
				}
			}
		}
		return false;
	}
	virtual bool addFontFromFileName(const std::string &alias, const std::string &filename, int ttc_index, bool should_exists, KFont *out_font) override {
		// ファイルをロード
		KInputStream file;
		if (file.openFileName(filename)) {
			if (addFontFromStream(alias, file, filename, ttc_index, out_font)) {
				return true;
			}
		}
		if (should_exists) {
			K__ERROR(u8"E_FONT: フォントファイル '%s' のロードに失敗しました", filename.c_str());
		}
		return false;
	}
	virtual bool addFontFromInstalledFonts(const std::string &alias, const std::string &filename, int ttc_index, bool should_exists, KFont *out_font) override {
		// ファイルをロード
		std::string sysfont = K::pathJoin(KPlatformFonts::getFontDirectory(), filename);
		KFont font = KFont::createFromFileName(sysfont.c_str(), ttc_index);
		if (font.isOpen()) {
			if (addFont(alias, font)) {
				if (out_font) {
					*out_font = font;
				}
				return true;
			}
		}
		if (should_exists) {
			K__ERROR(u8"E_FONT: フォントファイル '%s' はシステムにインストールされていません", filename.c_str());
		}
		return false;
	}
	virtual void deleteFont(const std::string &name) override {
		{
			auto it = m_fonts.find(name);
			if (it != m_fonts.end()) {
				m_fonts.erase(it);
			}
		}
		{
			auto it = std::find(m_fontnames.begin(), m_fontnames.end(), name);
			if (it != m_fontnames.end()) {
				m_fontnames.erase(it);
			}
		}
	}
	virtual const std::vector<std::string> & getFontNames() const override {
		return m_fontnames;
	}
	virtual KFont getDefaultFont() const override {
		// m_fallback_name で指定されたフォントを探す
		if (1) {
			auto it = m_fonts.find(m_fallback_name);
			if (it!=m_fonts.end()) return it->second;
		}

		// フォントテーブルの最初のフォントを得る
		if (1) {
			auto it = m_fonts.begin();
			if (it!=m_fonts.end()) return it->second;
		}

		// フォントが全くロードされていない。
		// システムのデフォルトフォントを取得しておく
		if (!m_system_default_font.isOpen()) {
			std::string name = K::pathJoin(KPlatformFonts::getFontDirectory(), "arial.ttf");
			m_system_default_font = KFont::createFromFileName(name.c_str());
			if (!m_system_default_font.isOpen()) {
				// システムフォントのロードにすら失敗した
				K__ERROR("E_FONT: Failed to load default font: %s", name.c_str());
			}
		}
		return m_system_default_font;
	}
	virtual KFont getFont(const std::string &name, bool fallback) const override {
		// name で指定されたフォントを探す
		{
			auto it = m_fonts.find(name);
			if (it!=m_fonts.end()) return it->second;
		}

		// fallback が許可されているならデフォルトフォントを返す
		if (fallback) {
			return getDefaultFont();
		}

		return KFont();
	}
	virtual void setDefaultFont(const std::string &name) override {
		m_fallback_name = name;
	}
};
#pragma endregion // CFontBankImpl


#pragma region CVideoBankImpl
class CVideoBankImpl: public KVideoBank, public KInspectorCallback {
	CAnimationBank m_anibank;
	CTextureBankImpl m_texbank;
	CSpriteBankImpl m_spritebank;
	CShaderBankImpl m_shaderbank;
	CFontBankImpl m_fontbank;
public:
	CVideoBankImpl() {
	}
	virtual ~CVideoBankImpl() {
		shutdown();
	}
	void init() {
		m_texbank.init();
		m_spritebank.init(&m_texbank);
		m_fontbank.init(&m_texbank);
		if (KInspector::isInstalled()) {
			KEngine::addInspectorCallback(this, u8"リソース"); // KInspectorCallback
			KInspectableDesc *desc = KInspector::getDesc(this);
			if (desc) {
				desc->sort_primary = 10; // 末尾にもってく
			}
		}
	}
	void shutdown() {
		m_anibank.clearClipResources();
		m_texbank.clearTextures(true);
		m_spritebank.clearSprites();
		m_shaderbank.clearShaders();
		m_fontbank.shutdown();
	}
	virtual KAnimationBank * getAnimationBank() override {
		return &m_anibank;
	}
	virtual KTextureBank * getTextureBank() override {
		return &m_texbank;
	}
	virtual KSpriteBank * getSpriteBank() override {
		return &m_spritebank;
	}
	virtual KShaderBank * getShaderBank() override {
		return &m_shaderbank;
	}
	virtual KFontBank * getFontBank() override {
		return &m_fontbank;
	}
	virtual void removeData(const KPath &name) override {
		m_anibank.removeClipResource(name.u8());
		m_texbank.removeTexture(name);
		m_spritebank.removeSprite(name);
		m_shaderbank.removeShader(name);
	}
	virtual void guiMeshInfo(const KMesh *mesh) override {
		if (mesh == nullptr) {
			ImGui::Text("(nullptr)");
			return;
		}
		ImGui::Text("Vertex: %d", mesh->getVertexCount());
		ImGui::Text("Indices: %d", mesh->getIndexCount());
		KVec3 minp, maxp;
		mesh->getAabb(&minp, &maxp);
		ImGui::Text("AABB min(%g, %g, %g)", minp.x, minp.y, minp.z);
		ImGui::Text("AABB max(%g, %g, %g)", maxp.x, maxp.y, maxp.z);
		if (ImGui::TreeNode(KImGui::ID(mesh), "Submesh (%d)", mesh->getSubMeshCount())) {
			for (int i=0; i<mesh->getSubMeshCount(); i++) {
				if (ImGui::TreeNode(KImGui::ID(i), "[%d]", i)) {
					const KSubMesh *sub = mesh->getSubMesh(i);
					ImGui::Text("Start %d", sub->start);
					ImGui::Text("Count %d", sub->count);
					ImGui::Text("Primitive %s", _GetPrimitiveName(sub->primitive));
					ImGui::TreePop();
				}
			}
			ImGui::TreePop();
		}
		if (1) {
			// メッシュファイルをエクスポートする時に使うファイル名を作成する
			KPath export_filename = KPath("__export__mesh.txt").getFullPath();
			if (ImGui::Button("Export")) {
				if (_MeshSaveToFileName(mesh, export_filename)) {
					K__PRINT("Export mesh '%s'", export_filename.u8());
				}
			}
			if (K::pathExists(export_filename.u8())) {
				std::string s =  K::str_sprintf("Open: %s", export_filename.u8());
				ImGui::SameLine();
				if (ImGui::Button(s.c_str())) {
					K::fileShellOpen(export_filename.u8());
				}
			}
		}
	}
	virtual bool guiMaterialInfo(KMaterial *mat) override {
	#ifndef NO_IMGUI
		if (mat == nullptr) {
			ImGui::Text("(nullptr)");
			return false;
		}
		bool changed = false;
		// Shader
		if (1) {
			m_shaderbank.guiShaderSelector("Shader", &mat->shader);
		}
		// Texture
		if (1) {
			m_texbank.guiTextureSelector("Main texture", &mat->texture);
			KPath name = m_texbank.getTextureName(mat->texture);
			m_texbank.guiTextureInfo(mat->texture, 256, name);
		}
		// Color
		if (1) {
			ImGui::ColorEdit4("Main color", reinterpret_cast<float*>(&mat->color));
		}
		// Specular
		if (1) {
			ImGui::ColorEdit4("Specular", reinterpret_cast<float*>(&mat->specular));
		}
		// KBlend
		if (1) {
			changed |= KDebugGui::K_DebugGui_InputBlend("Blend", &mat->blend);
		}
		// Filter
		KFilter filter = mat->filter;
		if (KDebugGui::K_DebugGui_InputFilter("Filter", &filter)) {
			mat->filter = filter;
			changed = true;
		}
		// Shader params
		if (ImGui::TreeNode("Shader parameters")) {
			{
				KTEXID tex = mat->texture;
				if (tex) {
					KPath tex_path = m_texbank.getTextureName(tex);
					if (m_texbank.guiTextureSelector(tex_path.u8(), &tex)) {
						mat->texture = tex;
						changed = true;
					}
					m_texbank.guiTextureInfo(mat->texture, 256, tex_path);
					ImGui::Indent();
					if (ImGui::TreeNode("Texture info")) {
						m_texbank.guiTexture(tex_path, THUMBNAIL_SIZE);
						ImGui::TreePop();
					}
					ImGui::Unindent();
				}
			}
			{
				if (ImGui::ColorEdit4("MainColor", reinterpret_cast<float*>(&mat->color))) {
					changed = true;
				}
			}
			{
				if (ImGui::ColorEdit4("Specular", reinterpret_cast<float*>(&mat->specular))) {
					changed = true;
				}
			}
			ImGui::TreePop();
		}
		return changed;
	#else
		return false;
	#endif // NO_IMGUI
	}
	/// KInspectorCallback
	virtual void onInspectorGui() override {
	#ifndef NO_IMGUI
		ImGui::PushID(KImGui::ID(0)); // Textures
		m_texbank.guiTextureBank();
		ImGui::PopID();

		ImGui::PushID(KImGui::ID(1)); // Sprites
		m_spritebank.guiSpriteBank(&m_texbank);
		ImGui::PopID();

		ImGui::PushID(KImGui::ID(2)); // Shader
		gui_main();
		ImGui::PopID();

		ImGui::PushID(KImGui::ID(3)); // Animations
		m_anibank.guiClips();
		ImGui::PopID();
	#endif // !NO_IMGUI
	}
private:
	void gui_main() {
	#ifndef NO_IMGUI
		if (ImGui::TreeNode("Shader", "Shader(%d)", m_shaderbank.getShaderCount())) {
			if (ImGui::Button("Clear")) {
				m_shaderbank.clearShaders();
			}
			std::vector<std::string> names = m_shaderbank.getShaderNameList();
			for (auto it=names.cbegin(); it!=names.cend(); ++it) {
				const std::string &name = *it;
				if (ImGui::TreeNode(name.c_str())) {
					KSHADERID shader = m_shaderbank.findShader(name, false);
					gui_shader_info(shader);
					ImGui::TreePop();
				}
			}
			ImGui::TreePop();
		}
	#endif // !NO_IMGUI
	}
	void gui_shader_info(KSHADERID shader) {
	#ifndef NO_IMGUI
		if (shader == nullptr) {
			ImGui::Text("(nullptr)");
			return;
		}
		KShader::Desc desc;
		KVideo::getShaderDesc(shader, &desc);
		ImGui::Text("Name: %s", m_shaderbank.getShaderName(shader).u8());
		ImGui::Text("Type: %s", desc.type);
	#endif // !NO_IMGUI
	}
};
#pragma endregion // CVideoBankImpl

KVideoBank * KVideoBank::create() {
	CVideoBankImpl *impl = new CVideoBankImpl();
	impl->init();
	return impl;
}






#pragma region Edge

/// Edgeファイルのページとレイヤーに指定されているラベル文字列を読み取り、管理する
class CEdgeLabelList {
	std::vector<KPathList> mPages;
	KPathList mInheritedNames; // 前のページから引き継いだラベル名
public:
	CEdgeLabelList() {
	}
	/// KEdgeDocument からラベルを読みとる
	/// debug_name: Edge の名前。エラーメッセージに表示するために使う
	void load(const KEdgeDocument &edge, const KPath &debug_name) {
		mPages.clear();
		mInheritedNames.clear();

		bool is_first_page = true;
		for (int i=0; i<edge.getPageCount(); i++) {
			// レイヤの名前を更新。
			// このページのレイヤのうち、名前がついているものの枚数を得る
			int num_named_layer = read_next_page(edge, i, mInheritedNames);

			if (is_first_page && num_named_layer > 0) {
				if (0) {
					// これは名前付きレイヤが存在する最初のページである。
					// ページ[0]からこのページまでのレイヤにも、同じ名前を適用する。
					// 例えばページ[0], [1], [2] にレイヤー名が指定されておらず、ページ[3]で初めてレイヤー名が指定された場合、
					// ページ[0]までさかのぼって各レイヤに名前を付ける
					if (i > 0) {
						K__WARNING(
							u8"E_EDGELABELLIST: '%s' の最初のページには、どのレイヤーにも名前が付いていません。"
							u8"ページ[%d]で指定されているレイヤー名 '%s' をページ[0]にも適用します",
							debug_name.u8(), i, edge.getPage(i)->getLayer(0)->m_label_u8.c_str());
					}
					for (int j=0; j<i; j++) {
						mPages[j] = mInheritedNames;
					}
				} else {
					// ページをさかのぼらない。
					// 例えばページ[0], [1], [2] にレイヤー名が指定されておらず、ページ[3]で初めてレイヤー名が指定された場合、
					// ページ[0][1][2] のレイヤー名は未指定のままである
				}
				is_first_page = false;
			}
			mPages.push_back(mInheritedNames);
		}
	}

	/// ページとレイヤー番号を指定して、そのラベル文字列を得る。
	/// ページ、レイヤーが存在しない場合は空文字列を返す
	const KPath & getLabel(int page, int layer) const {
		if (0 <= page && page < (int)mPages.size()) {
			return mPages[page][layer];
		}
		return KPath::Empty;
	}

private:
	int read_next_page(const KEdgeDocument &edge, int page_index, KPathList &layer_names) const {
		const KEdgePage *page = edge.getPage(page_index);

		int num_named_layers = 0;

		int num_layers = page->getLayerCount();
		if (1) {
			layer_names.resize(num_layers);
		} else {
			// 拡大方向にのみリサイズする。
			// 途中でレイヤー数が減っても、それより前に存在したレイヤの名前を継承したい場合はこちらを使う
			if ((int)layer_names.size() < num_layers) {
				layer_names.resize(num_layers);
			}
		}

		for (int i=0; i<num_layers; i++) {
			const KEdgeLayer *layer = page->getLayer(i);
			const std::string label = layer->m_label_u8;

			if (K::strStartsWith(label, "#")) {
				// ラベル名が # で始まっている。# を取り除いた部分をレイヤー名として抜き出す。
				// 例えば "#body" と指定されている場合、"body" がこのレイヤの名前になる
				std::string name_str = label.substr(1);

				// ラベル名がコマンド開始記号 '@' を含んでいる場合、その直前までを名前文字列とする。
				// つまり、元のラベルが "#body@blend=add" ならば "body" の部分だけを取り出すことになる
				int len = K::strFindChar(name_str, '@');
				if (len >= 0) {
					name_str = name_str.substr(0, len);
				}

				// 前後の空白をカット
				K::strTrim(name_str);

				// レイヤー名を決定
				KPath name_new = KPath::fromUtf8(name_str.c_str());
				
				// 前のページから継承したレイヤー名が重複している場合は、
				// 名前付きレイヤが移動したとみなして古いレイヤから名前を削除する。
				//
				// 例えば、前ページから継承したレイヤー名が
				//
				// [0] "Head"
				// [1] "Body"
				// [2] "Leg"
				// [3] ""
				//
				// であり、現在のページのレイヤー名が
				//
				// [0] ""
				// [1] ""
				// [2] "Leg"
				// [3] "Body"
				//
				// と指定されているとき、このページの実際のレイヤー名は以下のようになる
				// 
				// [0] "Head"  <-- 前ページから継承
				// [1] ""      <-- 前ページまでは "Body" だったが、レイヤー[3]が新しく "Body" になったため、Body という名前のレイヤが[1]から[3]に移動したとみなす
				// [2] "Leg"   <-- 同レイヤに同名が指定されたので、結果的に変化なし
				// [3] "Body"  <-- あたらしく "Body" という名前が付いたが、これは前レイヤー[1]と同名なので、レイヤー[1]がレイヤー[3]に移動したものとする
				//
				for (auto it=layer_names.begin(); it!=layer_names.end(); ++it) {
					const KPath &name_inhert = *it;
					if (name_inhert.compare(name_new) == 0) {
						*it = KPath::Empty;
					}
				}
				// レイヤー番号とレイヤー名を関連付ける
				layer_names[i] = name_new;
				num_named_layers++;
			}
		}
		return num_named_layers;
	}
};


/// Edgeファイルのレイヤに指定されているコマンド文字列を読み取り、管理する
class CEdgeCommandList {
	typedef std::vector<std::string> STRINGLIST;
	std::vector<STRINGLIST> mPages;
public:
	CEdgeCommandList() {
	}
	/// KEdgeDocument からラベルを読みとる
	void load(const KEdgeDocument &edge, const KPath &debug_name) {
		mPages.clear();
		for (int i=0; i<edge.getPageCount(); i++) {
			STRINGLIST cmds = read_next_page(edge, i);
			mPages.push_back(cmds);
		}
	}

	/// ページとレイヤー番号を指定して、そのコマンド文字列を得る。ページやレイヤが存在しない場合は空文字列を返す
	const char * getCommand(int page, int layer) const {
		if (0 <= page && page < (int)mPages.size()) {
			return mPages[page][layer].c_str();
		}
		return nullptr;
	}
private:
	STRINGLIST read_next_page(const KEdgeDocument &edge, int page_index) const {
		const KEdgePage *page = edge.getPage(page_index);
		int num_layers = page->getLayerCount();
		STRINGLIST cmds(num_layers);
		for (int i=0; i<num_layers; i++) {
			const KEdgeLayer *layer = page->getLayer(i);

			// ラベル文字列が '@' を含んでいる場合、それよりも後の部分がコマンド文字列になる
			int pos = K::strFindChar(layer->m_label_u8, '@');
			if (pos >= 0) {
				// 文字列が '@' を含んでいた。それよりも後ろの部分を得る
				cmds[i] = layer->m_label_u8.substr(pos + 1);
			}
		}
		return cmds;
	}
};


/// 各ページのレイヤーやスプライト設定などを行って KClipRes を作成するためのクラス。
/// ページやレイヤーを順不同で設定できるため、順番どおりに push_back 等する必要がない
class CLayeredSpriteClipBuilder {
	std::vector<KClipRes::SPRITE_KEY> m_Keys;
	bool mLoop;
	bool mAutoKill;
public:
	CLayeredSpriteClipBuilder() {
		mLoop = false;
		mAutoKill = false;
	}
	~CLayeredSpriteClipBuilder() {
		for (int i=0; i<(int)m_Keys.size(); i++) {
			K__DROP(m_Keys[i].xml_data);
		}
	}
	void setLoop(bool value) {
		mLoop = value;
	}
	void setKillSelf(bool value) {
		mAutoKill = value;
	}
	void setSprite(int page, int layer, const char *sprite) {
		if (sprite && sprite[0]) {
			KClipRes::SPRITE_KEY *key = getkey(page);
			if (layer < KClipRes::MAX_LAYER_COUNT) {
				if (key->num_layers <= layer) {
					key->num_layers = layer + 1;
				}
				key->layers[layer].sprite = sprite;
			} else {
				K__ERROR(u8"E_LAYEREDSPRITECLIP_SPRITE: スプライトのレイヤー数が多すぎます");
			}
		}
	}
	void setLabel(int page, int layer, const char *label) {
		if (label && label[0]) {
			if (layer < KClipRes::MAX_LAYER_COUNT) {
				KClipRes::SPRITE_KEY *key = getkey(page);
				if (key->num_layers <= layer) {
					key->num_layers = layer + 1;
				}
				key->layers[layer].label = label;
			} else {
				K__ERROR(u8"E_LAYEREDSPRITECLIP_LABEL: スプライトのレイヤー数が多すぎます");
			}
		}
	}
	void setCommand(int page, int layer, const char *command) {
		if (command && command[0]) {
			if (layer < KClipRes::MAX_LAYER_COUNT) {
				KClipRes::SPRITE_KEY *key = getkey(page);
				if (key->num_layers <= layer) {
					key->num_layers = layer + 1;
				}
				key->layers[layer].command = command;
			} else {
				K__ERROR(u8"E_LAYEREDSPRITECLIP_CMD: スプライトのレイヤー数が多すぎます");
			}
		}
	}
	void setDuration(int page, int duration) {
		KClipRes::SPRITE_KEY *key = getkey(page);
		key->duration = duration;
	}
	void setPageMark(int page, KMark mark) {
		KClipRes::SPRITE_KEY *key = getkey(page);
		key->this_mark = mark;
	}
	void setPageNext(int page, const KPath clip, KMark mark) {
		KClipRes::SPRITE_KEY *key = getkey(page);
		key->next_clip = clip;
		key->next_mark = mark;
	}
	void setPageSource(int page, int src_page, const KPath &src_name) {
		KClipRes::SPRITE_KEY *key = getkey(page);
		key->edge_page = src_page;
		key->edge_name = src_name;
	}
	void addParams(int page, const KNamedValues *params) {
		if (params && params->size() > 0) {
			KClipRes::SPRITE_KEY *key = getkey(page);
			key->user_parameters.append(*params);
		}
	}
	void addData(int page, const KXmlElement *xml) {
		K__ASSERT_RETURN(xml);
		KClipRes::SPRITE_KEY *key = getkey(page);
		if (key->xml_data) {
			K__WARNING(u8"<Data> ノードが上書きされました。古い Data ノードは破棄されます: ページ %d", page);
			key->xml_data->drop();
		}
		key->xml_data = xml->clone();
	}
	KClipRes * createClip(const char *name) {
		K__ASSERT_RETURN_ZERO(name);
		KClipRes *clip = new KClipRes(name);
		clip->setFlag(KClipRes::FLAG_LOOP, mLoop);
		clip->setFlag(KClipRes::FLAG_KILL_SELF, mAutoKill);
		for (auto it=m_Keys.begin(); it!=m_Keys.end(); ++it) {
			K__GRAB(it->xml_data);
			clip->addKey(*it);
		}
		return clip;
	}
private:
	KClipRes::SPRITE_KEY * getkey(int page) {
		if (page < (int)m_Keys.size()) {
			return &m_Keys[page];
		} else {
			m_Keys.resize(page + 1);
			return &m_Keys[page];
		}
	}
};


class CEdgeFile {
	KEdgeDocument mEdge;
	CEdgeLabelList mLabels;
	CEdgeCommandList mCmds;
public:
	bool loadEdge(KInputStream &edgefile, const char *edgename) {
		// Edge ファイルをロードする
		// 無視ページやレイヤを削除した状態で取得するため、
		// KEdgeDocument::loadFromFile ではなく KGameEdgeBuilder を使う
		if (!KGameEdgeBuilder::loadFromStream(&mEdge, edgefile, edgename)) {
			return false;
		}

		// Edge の各ページ、レイヤーのラベル情報を得る
		mLabels.load(mEdge, edgename);

		// Edge の各ページ、レイヤーのコマンド情報（@で始まる部分）を得る
		mCmds.load(mEdge, edgename);
		return true;
	}
	int getPageCount() const {
		return mEdge.getPageCount();
	}
	int getLayerCount(int page) const {
		const KEdgePage *edgepage = mEdge.getPage(page);
		return edgepage ? edgepage->getLayerCount() : 0;
	}
	const KPath & getLabel(int page, int layer) const {
		return mLabels.getLabel(page, layer);
	}
	const char * getCommand(int page, int layer) const {
		return mCmds.getCommand(page, layer);
	}
};


static void _ReadPageMarkAttr(CLayeredSpriteClipBuilder &builder, KXmlElement *xPage, int clip_page_index, const char *xmlName) {
	const char *mark_str = xPage->getAttrString("mark");
	if (mark_str && mark_str[0]) {
		KMark mark = KMark_NONE;
		if (K_StrToMark(mark_str, &mark)) {
			builder.setPageMark(clip_page_index, mark);
		} else {
			K__WARNING("unknown next attr: '%s' %s(%d)", mark_str, xmlName, xPage->getLineNumber());
		}
	}
}

static void _ReadPageNextAttr(CLayeredSpriteClipBuilder &builder, KXmlElement *xPage, int clip_page_index, const char *xmlName) {
	const char *next_str = xPage->getAttrString("next", "");
	const char *nextclip = xPage->getAttrString("next_clip", "");

	KMark nextmark = KMark_NONE;
	if (!K_StrToMark(next_str, &nextmark)) {
		K__WARNING("unknown next attr: '%s' %s(%d)", next_str, xmlName, xPage->getLineNumber());
	}

	if (nextmark != KMark_NONE || nextclip[0]) {
		builder.setPageNext(clip_page_index, nextclip, nextmark);
	}
}
#pragma endregion // Edge








#pragma region KGamePath

#define RESOURCE_NAME_CHAR_SET          " ._()[]{}<>#$%&=~^@+-"
#define PATH_INDEX_SEPARATOR            '@' // パス名とインデックス番号の区切り文字

/// ファイル名として安全な文字列にする
KPath KGamePath::escapeFileName(const KPath &name) {
	char tmp[KPath::SIZE];
	memset(tmp, 0, sizeof(tmp));
	const char *s = name.u8(); // 内部のポインタを得る
	for (size_t i=0; s[i]; i++) {
		if (isSafeAssetChar(s[i])) {
			tmp[i] = s[i];
		} else if (isascii(s[i])) {
			tmp[i] = '_';
		} else {
			tmp[i  ] = '0' + (uint8_t)(s[i]) / 16;
			tmp[i+1] = '0' + (uint8_t)(s[i]) % 16;
			i++; // 2文字書き込んでいるので、iを1回余分にインクリメントする必要がある
		}
	} // 終端のヌル文字にはmemsetで初期化したときの0を使う
	return KPath(tmp);
}
KPath KGamePath::evalPath(const KPath &name, const KPath &base_filename, const KPath &def_ext) {
	if (name.empty() || name.compare("auto")==0) {
		// 名前が省略されている。curr_path と同じファイル名で拡張子だけが異なるファイルが指定されているものとする
		return base_filename.changeExtension(def_ext);
	} else {
		// 名前が指定されている
		// base_filename と同じフォルダ内にあるファイルが指定されているものとする
		KPath dir = base_filename.directory();
		return dir.join(name).changeExtension(def_ext);
	}
}
KPath KGamePath::getSpriteAssetPath(const KPath &base_path, int index1, int index2) {
	KPath base = base_path.changeExtension("");
	return KPath::fromFormat("%s%c%d.%d.sprite", base.u8(), PATH_INDEX_SEPARATOR, index1, index2);
}
KPath KGamePath::getTextureAssetPath(const KPath &base_path) {
	return base_path.changeExtension(".tex");
}
KPath KGamePath::getEdgeAtlasPngPath(const KPath &edg_path) {
	return edg_path.joinString(".png");
}
bool KGamePath::isSafeAssetChar(char c) {
	if (c > 0 && isalnum(c)) {
		return true;
	}
	if (strchr(RESOURCE_NAME_CHAR_SET, c) != nullptr) {
		return true;
	}
	return false;
}

#pragma endregion // KGamePath












#pragma region KGameImagePack
void KGameImagePack::makeSpritelistFromPack(const KImgPackR &pack, const KImage &pack_image, const KPath &tex_name, KSpriteList *sprites) {

	sprites->m_TexName = tex_name;
	sprites->m_TexImage = pack_image;

	int num_images = pack.getImageCount();
	int tex_w = pack_image.getWidth();
	int tex_h = pack_image.getHeight();
	for (int ii=0; ii<num_images; ii++) {
		KImgPackExtraData extra;
		int img_w, img_h;
		pack.getImageSize(ii, &img_w, &img_h);
		pack.getImageExtra(ii, &extra);

		// スプライト作成
		KSpriteRes *sp = new KSpriteRes();
		{
			sp->m_ImageW = img_w;
			sp->m_ImageH = img_h;
			sp->m_AtlasX = 0;
			sp->m_AtlasY = 0;
			sp->m_AtlasW = img_w;
			sp->m_AtlasH = img_h;
			sp->m_Pivot.x = img_w / 2.0f; // テクスチャではなく、切り取ったアトラス範囲内での座標であることに注意
			sp->m_Pivot.y = img_h / 2.0f;
			sp->m_PivotInPixels = true;
			sp->m_UsingPackedTexture = true;
			sp->m_PaletteCount = 0; // K_PALETTE_IMAGE_SIZE
			sp->m_Mesh = KMesh();
			sp->m_SubMeshIndex = ii; // <-- 何番目の画像を取り出すか？
			sp->m_TextureName = tex_name;
			sp->m_DefaultBlend = (KBlend)extra.blend; // Edge側で指定されたブレンド。("@blend=add" など) 未指定の場合は KBlend_INVALID
		}
		// メッシュを作成
		{
			int num_vertices = pack.getVertexCount(sp->m_SubMeshIndex);
			if (num_vertices > 0) {
				const KVec3 *p = pack.getPositionArray(tex_w, tex_h, sp->m_SubMeshIndex);
				const KVec2 *t = pack.getTexCoordArray(tex_w, tex_h, sp->m_SubMeshIndex);
				sp->m_Mesh.setVertexCount(num_vertices);
				sp->m_Mesh.setPositions(0, p, sizeof(KVec3), num_vertices);
				sp->m_Mesh.setTexCoords(0, t, sizeof(KVec2), num_vertices);
				sp->m_Mesh.setColorInts(0, &KColor32::WHITE, 0, num_vertices); // stride=0 means copy from single value
			}
			KSubMesh sub;
			sub.start = 0;
			sub.count = num_vertices;
			sub.primitive = KPrimitive_TRIANGLES;
			sp->m_Mesh.addSubMesh(sub);
		}

		KSpriteList::ITEM item;
		item.def = sp;
		item.page = extra.page;
		item.layer = extra.layer;
		sprites->m_Items.push_back(item);

		sp->drop();
	}
}

// imageListName には元データのファイル名を指定する。例えば "player/player.edg" など
KImgPackR KGameImagePack::loadPackR_fromCache(KStorage *storage, const std::string &imageListName, KImage *result_image) {
	K__ASSERT(storage);

	if (result_image == nullptr) {
		return KImgPackR();
	}
	if (imageListName.empty()) {
		K__ERROR("Filename cannot be empty"); 
		return KImgPackR();
	}
	std::string metaName = imageListName + ".meta";
	std::string pngName = imageListName + ".png";

	std::string metaData = storage->loadBinary(metaName, false);
	std::string pngData = storage->loadBinary(pngName, false);

	if (metaData.size() == 0) {
		K__ERROR("Failed to read KImgPackR meta-data: %s", metaName.c_str()); 
		return KImgPackR();
	}
	if (pngData.size() == 0) {
		K__ERROR("Failed to read KImgPackR image-data: %s", pngName.c_str()); 
		return KImgPackR();
	}
	KImgPackR packR;
	if (!packR.loadFromMetaString(metaData)) {
		K__ERROR("Failed to makeread KImgPackR from meta-data: %s", metaName.c_str()); 
		return KImgPackR();
	}
	*result_image = KImage::createFromFileInMemory(pngData);
	if (result_image->empty()) {
		K__ERROR("Failed to load png: %s", pngName.c_str()); 
		return KImgPackR();
	}
	return packR;
}
bool KGameImagePack::loadSpriteList(KStorage *storage, KSpriteList *sprites, const std::string &imageListName) {
	KImage tex_image;
	KImgPackR packR = loadPackR_fromCache(storage, imageListName, &tex_image);
	if (packR.empty()) {
		K__ERROR("Failed to load packR: %s", imageListName.c_str());
		return false;
	}
	KPath tex_name = KGamePath::getEdgeAtlasPngPath(imageListName);
	makeSpritelistFromPack(packR, tex_image, tex_name, sprites);
	return true;
}
#pragma endregion // KGameImagePack













#pragma region KGameEdgeBuilder
/// Edge ファイルのページラベル、レイヤーラベルの色。
/// Edge2 ページ/レイヤー、ラベル、外観の色 の各値に対応
static const KColor32 LABEL_COLOR_WHITE = KColor32(255, 255, 255, 255);
static const KColor32 LABEL_COLOR_GRAY  = KColor32(192, 192, 192, 255);
static const KColor32 LABEL_COLOR_GREEN = KColor32(196, 255, 196, 255);
static const KColor32 LABEL_COLOR_BLUE  = KColor32(191, 223, 255, 255);
static const KColor32 LABEL_COLOR_RED   = KColor32(255, 168, 168, 255);
static const KColor32 LABEL_COLOR_IGNORE = LABEL_COLOR_RED;

/// 無視をあらわす色かどうかを判定
bool KGameEdgeBuilder::isIgnorableLabelColor(const KColor32 &color) {
	return color == LABEL_COLOR_IGNORE;
}

/// 無視可能なレイヤーならば true を返す（レイヤーのラベルの色によって判別）
bool KGameEdgeBuilder::isIgnorableLayer(const KEdgeLayer *layer) {
	K__ASSERT(layer);
	return isIgnorableLabelColor(KColor32(
		layer->m_label_color_rgbx[0],
		layer->m_label_color_rgbx[1],
		layer->m_label_color_rgbx[2],
		255
	));
}

/// 無視可能なページならば true を返す（ページのラベルの色によって判別）
bool KGameEdgeBuilder::isIgnorablePage(const KEdgePage *page) {
	K__ASSERT(page);
	return isIgnorableLabelColor(KColor32(
		page->m_label_color_rgbx[0],
		page->m_label_color_rgbx[1],
		page->m_label_color_rgbx[2],
		255
	));
}

/// 無視可能なレイヤーを全て削除する
void KGameEdgeBuilder::removeIgnorableLayers(KEdgePage *page) {
	K__ASSERT(page);
	for (int i=page->getLayerCount()-1; i>=0; i--) {
		const KEdgeLayer *layer = page->getLayer(i);
		if (isIgnorableLayer(layer)) {
			page->delLayer(i);
		}
	}
}

/// 無視可能なページを全て削除する
void KGameEdgeBuilder::removeIgnorablePages(KEdgeDocument *edge) {
	K__ASSERT(edge);
	for (int i=edge->getPageCount()-1; i>=0; i--) {
		const KEdgePage *page = edge->getPage(i);
		if (isIgnorablePage(page)) {
			edge->delPage(i);
		}
	}
}

/// 無視可能なページ、レイヤーを全て削除する
void KGameEdgeBuilder::removeIgnorableElements(KEdgeDocument *edge) {
	K__ASSERT(edge);
	removeIgnorablePages(edge);
	for (int i=0; i<edge->getPageCount(); i++) {
		KEdgePage *page = edge->getPage(i);
		removeIgnorableLayers(page);
	}
}

bool KGameEdgeBuilder::parseLabel(const KPath &label, KGameEdgeLayerLabel *out) {
	K__ASSERT(out);
	out->numcmds = 0;

	// コマンド解析

	// "#レイヤー名@コマンド" の書式に従う

	// レイヤー名だけが指定されている例
	// "#face"
	//   name = "face" 
	//   cmds = ""
	//
	// 何も認識しない例
	// "face"
	//   name = "" // 先頭が # でない場合は名前省略
	//   cmds = "" // @ が含まれていないのでコマンド無し
	//
	// コマンドだけを認識する例
	// "@blend=add"
	// "face@blend=add"
	//   name = "" // 先頭が # でない場合は名前省略
	//   cmds = "blend=add"
	//
	// レイヤー名とコマンドを認識する例
	// "#face@blend=add"
	//   name = "face" 
	//   cmds = "blend=add"
	//
	// レイヤー名と複数のコマンドを認識する例
	// "#face@blend=add@se=none"
	//   name = "face" 
	//   cmds = "blend=add@se=none"

	// 区切り文字 @ で分割
	auto tok = K::strSplit(label.u8(), "@");

	// 最初の部分が # で始まっているならレイヤー名が指定されている
	if (tok.size() > 0 && K::strStartsWith(tok[0], "#")) {
		out->name = tok[0].substr(1); // "#" の次の文字から
	}

	// 右部分
	out->numcmds = 0;
	if (tok.size() > 1) {
		if (tok.size()-1 < KGameEdgeLayerLabel::MAXCMDS) {
			out->numcmds = tok.size()-1;
		} else {
			K__ERROR(u8"E_EDGE_COMMAND: 指定されたコマンド '%s' の個数が、設定可能な最大数 %d を超えています", 
				label.u8(), KGameEdgeLayerLabel::MAXCMDS);
			out->numcmds = KGameEdgeLayerLabel::MAXCMDS;
		}
		for (int i=1; i<out->numcmds; i++) {
			out->cmd[i] = tok[i];
		}
	}
	return true;
}
bool KGameEdgeBuilder::loadFromStream(KEdgeDocument *edge, KInputStream &file, const std::string &debugname) {
	if (edge && edge->loadFromStream(file)) {
		// Edge ドキュメントに含まれる無視属性のページやレイヤーを削除する。
		// 無視属性のページは元から存在しないものとしてページ番号を再割り当てすることに注意。
		// 無視属性のレイヤーも同じ
		removeIgnorableElements(edge);
		return true;
	} else {
		K__ERROR("E_EDGE_FAIL: Filed to load: %s", debugname.c_str());
		return false;
	}
}
bool KGameEdgeBuilder::loadFromFileInMemory(KEdgeDocument *edge, const void *data, size_t size, const std::string &debugname) {
	K__ASSERT(edge);
	bool ret = false;
	KInputStream file;
	if (file.openMemory(data, size)) {
		ret = loadFromStream(edge, file, debugname);
	} else {
		K__ERROR("E_MEM_READER_FAIL: %s", debugname.c_str());
	}
	return ret;
}
bool KGameEdgeBuilder::loadFromFileName(KEdgeDocument *edge, const std::string &filename) {
	K__ASSERT(edge);
	bool ret = false;
	KInputStream file;
	if (file.openFileName(filename)) {
		ret = loadFromStream(edge, file, filename);
	} else {
		K__ERROR("E_EDGE_FAIL: STREAM ERROR: %s", filename.c_str());
	}
	return ret;
}
#pragma endregion // KGameEdgeBuilder











#pragma region K_GameUpdateBank
static const char *BANK_INFO_FILENAME = "$info.xml";

class CEdgeImporter {
public:
	bool importFile(const std::string &filepath, const std::string &bankDir, const std::string &dataDir, KGameUpdateBankFlags bankflags) {
		return importFileEx(filepath, bankDir, dataDir, 16, KPaletteImportFlag_DEFAULT, bankflags);
	}
	bool importFileEx(const std::string &filepath, const std::string &bankDir, const std::string &dataDir, int cellsize, KPaletteImportFlags palflags, KGameUpdateBankFlags bankflags) {
		if (filepath.empty()) {
			K__ERROR("Filename cannot be empty");
			return false;
		}
		if (!K::pathIsDir(bankDir)) {
			K__ERROR("Directory does not exist: %s", bankDir.c_str());
			return false;
		}

		// Edgeドキュメントを得る
		KEdgeDocument edgeDoc;
		if (!loadEdge(filepath, dataDir, &edgeDoc)) {
			return false;
		}

		// Edgeファイルから画像パックを作成
		KImgPackW packW = makePackW(edgeDoc, palflags);
		if (packW.empty()) {
			return false;
		}

		// 正しいディレクトリとファイル名に分ける
		std::string dir = K::pathGetParent(K::pathJoin(bankDir, filepath));
		std::string name = K::pathGetLast(filepath);

		// キャッシュとして画像パックを保存
		std::string packPngName = K::pathJoin(dir, name) + ".png";
		std::string packMetaName = K::pathJoin(dir, name) + ".meta";
		if (!packW.saveToFileNames(packPngName, packMetaName)) {
			return false;
		}

		// デバッグ用
		// インデックス画像での保存が ON の場合、RGB画像をデバッグ用として保存しておく
		bool with_index = (palflags & KPaletteImportFlag_MAKE_INDEXED_TRUE) || (palflags & KPaletteImportFlag_MAKE_INDEXED_AUTO);
		if ((bankflags & KGameUpdateBankFlags_SAVE_RGB) && with_index) {
			KImgPackW packW_d = makePackW(edgeDoc, KPaletteImportFlag_MAKE_INDEXED_FALSE);
			std::string packPngName = K::pathJoin(dir, "$" + name) + ".png";
			packW_d.saveToFileNames(packPngName, ""); // png のみ保存する。meta は不要
		}
		return true;
	}
private:
	bool loadEdge(const std::string &name, const std::string &dataDir, KEdgeDocument *result) {
		// edge からパック情報を作成する
		if (name.empty()) {
			K__ERROR("Filename cannot be empty");
			return false;
		}
		std::string nameInDataDir = K::pathJoin(dataDir, name);
		if (!K::pathExists(nameInDataDir)) {
			K__ERROR("File does not exist: %s", nameInDataDir.c_str());
			return false;
		}
		KInputStream file;
		if (!file.openFileName(nameInDataDir)) {
			K__ERROR("Failed to open file: %s", nameInDataDir.c_str());
			return false;
		}
		if (!KGameEdgeBuilder::loadFromStream(result, file, nameInDataDir)) {
			K__ERROR("Failed load edge document: %s", nameInDataDir.c_str());
			return false;
		}
		return true;
	}
	KImgPackW makePackW(const KEdgeDocument &edgeDoc, KPaletteImportFlags flags) {
		// edge からパック情報を作成する
		int num_palettes = edgeDoc.m_palettes.size();

		bool is_indexed = false;
		if (flags & KPaletteImportFlag_MAKE_INDEXED_TRUE) {
			is_indexed = true;
		} else if (flags & KPaletteImportFlag_MAKE_INDEXED_FALSE) {
			is_indexed = false;
		} else if (flags & KPaletteImportFlag_MAKE_INDEXED_AUTO) {
			is_indexed = num_palettes >= 2; // 複数のパレットがある場合のみインデックス画像
		}

		KImgPackW packW;

		// EDGE 内にあるすべてのページ、レイヤーの画像を得る。
		// パレットの有無によって得られる画像の種類が異なるので注意すること
		if (is_indexed) {
			// データビルド時ではなくゲーム実行時にパレットを適用できるように、インデックス画像を保存する。
			// ただし、1ピクセル8ビットで保存するのではなく、
			// インデックス値をそのまま輝度としてグレースケール化し、RGBA 画像として保存する。
			// パレットの透過色の設定とは関係なく、アルファ値は常に 255 になる
			for (int p=0; p<edgeDoc.getPageCount(); p++) {
				const KEdgePage *page = edgeDoc.getPage(p);
				for (int l=0; l<page->getLayerCount(); l++) {
					const KEdgeLayer *layer = page->getLayer(l);
					KImage img = edgeDoc.exportSurfaceRaw(p, l, edgeDoc.m_bg_palette.color_index);
					KImgPackExtraData extra;
					extra.page = p;
					extra.layer = l;
					extra.blend = getBlendHint(layer);
					packW.addImage(img, &extra);
				}
			}

		} else {
			// パレット適用済みの画像を作成し、RGBA カラーで保存する
			for (int p=0; p<edgeDoc.getPageCount(); p++) {
				const KEdgePage *page = edgeDoc.getPage(p);
				for (int l=0; l<page->getLayerCount(); l++) {
					const KEdgeLayer *layer = page->getLayer(l);
					int mask_index = -1; // no used
					int palette_index = 0; // always 0
					KImage img = edgeDoc.exportSurfaceWithAlpha(p, l, mask_index, palette_index);
					KImgPackExtraData extra;
					extra.page = p;
					extra.layer = l;
					extra.blend = getBlendHint(layer);
					packW.addImage(img, &extra);
				}
			}
		}
		return packW;
	}
	KBlend getBlendHint(const KEdgeLayer *layer) {
		// ブレンド指定を見る
		KGameEdgeLayerLabel label;
		KGameEdgeBuilder::parseLabel(layer->m_label_u8, &label);
		for (int i=0; i<label.numcmds; i++) {
			// = を境にして左右の文字列を得る
			auto tok = K::strSplit(label.cmd[i].u8(), "=", 2);
			if (tok.size() == 2) {
				const std::string &key = tok[0];
				const std::string &val = tok[1];
				if (key.compare("blend") == 0) {
					KBlend tmp = KVideoUtils::strToBlend(val.c_str(), KBlend_INVALID);
					if (tmp == KBlend_INVALID) {
						K__ERROR(u8"E_EDGE_BLEND: ブレンド指定方法が正しくありません: '%s'", label.cmd[i].u8());
						break;
					}
					return tmp;
				}
			}
		}
		return KBlend_INVALID;
	}
};



#define EXPORT_CONTENTS_DEBUG_DATA  0



class CAutoFile {
public:
	FILE *fp_;
	
	CAutoFile(const char *name) {
		fp_ = nullptr;
		if (EXPORT_CONTENTS_DEBUG_DATA) {
			fp_ = fopen(name, "w");
			write("\"FILE\", \"TYPE\", \"PAGE\", \"LAYER\", \"W\", \"H\", \"SIZE(BYTES)\"\n");
		}
	}
	~CAutoFile() {
		if (fp_) {
			fclose(fp_);
		}
	}
	void write(const char *s) {
		if (fp_) {
			fwrite(s, strlen(s), 1, fp_);
			fflush(fp_);
		}
	}
};
static CAutoFile g_autolog("__contents.csv");



class CBankUpdator {
public:
	CBankUpdator() {
		m_Flags = 0;
	}
	bool update(const std::string &bankDir, const std::string &dataDir, KPathList *p_updated_files, KGameUpdateBankFlags flags) {
		uint32_t starttime = K::clockMsec32();
		m_Flags = flags;

		K__PRINT("Update bank");
		if (bankDir.empty()) { K__ERROR("bankDir cannot be empty"); return false; }
		if (dataDir.empty()) { K__ERROR("dataDir cannot be empty"); return false; }

		// 生データフォルダは必須
		if (!K::pathIsDir(dataDir)) {
			K__ERROR("Data directory does not exist: %s", dataDir.c_str());
			m_Flags = 0;
			return false;
		}

		// 生バンクフォルダが無ければ作成する＆フォルダ構造がデータフォルダと同じになるようにする
		// （少なくとも、データフォルダ内にあるサブフォルダは必ずバンクフォルダ内にも存在するように）
		if (!updateBankDir(bankDir, dataDir)) {
			K__ERROR("Bank directory does not exist: %s", bankDir.c_str());
			m_Flags = 0;
			return false;
		}

		std::string xmlBankName = K::pathJoin(bankDir, BANK_INFO_FILENAME);

		// 管理データを得る。ロードできなければ作成する
		KXmlElement *xDoc = getXmlBankDoc(xmlBankName);
		KXmlElement *xBank = xDoc->getChild(0);

		// 生データフォルダ内のファイルをインポートする
		{
			ScanFiles cb;
			KDirectoryWalker::walk(dataDir, &cb);
			for (auto it=cb.mFiles.begin(); it!=cb.mFiles.end(); it++) {
				const char *name = it->u8();
				if (updateFile(name, bankDir, dataDir, xBank) > 0) {
					if (p_updated_files) {
						p_updated_files->push_back(name);
					}
				}
			}
		}

		// 管理データを保存する
		if (xDoc) {
			KOutputStream file;
			file.openFileName(xmlBankName);
			xDoc->writeDoc(file);
		}
		K__DROP(xDoc);
		K__PRINT("Bank updated (%d msec)", K::clockMsec32() - starttime);
		m_Flags = 0;
		return true;
	}

private:
	KGameUpdateBankFlags m_Flags;

	class ScanFiles: public KDirectoryWalker::Callback {
	public:
		KPathList mFiles;
		virtual void onDir(const std::string &name_u8, const std::string &parent_u8, bool *p_enter) override {
			*p_enter = true; // 再帰処理する
		}
		virtual void onFile(const std::string &name_u8, const std::string &parent_u8) override {
			mFiles.push_back(KPath(parent_u8).join(name_u8));
		}
	};
	class ScanDir: public KDirectoryWalker::Callback {
	public:
		KPathList mDirs;
		virtual void onDir(const std::string &name_u8, const std::string &parent_u8, bool *p_enter) override {
			mDirs.push_back(KPath(parent_u8).join(name_u8));
			*p_enter = true; // 再帰処理する
		}
	};

	// データフォルダ内の name ファイルが更新されたかどうか調べる。
	// 更新されているならキャッシュフォルダにコピーし、管理データを更新して 1 を返す
	// 無変更ならば何もせずに 0 を返す
	// エラーが発生したなら -1 を返す
	int updateFile(const std::string &name, const std::string &bankDir, const std::string &dataDir, KXmlElement *xmlBank) {
		// 現在のデータファイルのタイムスタンプ
		std::string nameInData = K::pathJoin(dataDir, name);
		if (!K::pathExists(nameInData)) {
			K__ERROR("Data file does not exist: %s", nameInData.c_str());
			return -1;
		}
		time_t timestampInData = K::fileGetTimeStamp_Modify(nameInData.c_str());

		// 前回バンクを更新したときに利用したデータファイルのタイムスタンプ
		std::string nameInBank = K::pathJoin(bankDir, name);
		time_t timestampInBank = readTimestamp(xmlBank, name);

		// データフォルダとバンクフォルダの両方にファイルが存在し、タイムスタンプも変わっていなければ更新不要
		if (timestampInData > 0 && timestampInBank > 0 && timestampInData == timestampInBank) {
			return 0;
		}

		// 更新する
		if (onUpdateFile(name, bankDir, dataDir)) {
			if (xmlBank) {
				writeTimestamp(xmlBank, name, timestampInData); // タイムスタンプを更新
			}
			return 1;
		}
		K__ERROR("Failed to import file: %s", nameInData.c_str());
		return -1;
	}
	bool onUpdateFile(const std::string &name, const std::string &bankDir, const std::string &dataDir) {
		if (K::pathHasExtension(name, ".edg")) {
			CEdgeImporter imp;
			if (imp.importFile(name, bankDir, dataDir, m_Flags)) {
#if 1
				// .edg自体もコピーする
				std::string nameInData = K::pathJoin(dataDir, name);
				std::string nameInBank = K::pathJoin(bankDir, name);
				K::fileCopy(nameInData, nameInBank, true);
#endif
				if (EXPORT_CONTENTS_DEBUG_DATA) {
					KEdgeDocument doc;
					if (doc.loadFromFileName(nameInData)) {
						for (int p=0; p<doc.getPageCount(); p++) {
							const KEdgePage *page = doc.getPage(p);
							for (int l=0; l<page->getLayerCount(); l++) {
								int w = page->getLayer(l)->m_image_rgb24.getWidth();
								int h = page->getLayer(l)->m_image_rgb24.getHeight();
								std::string s = K::str_sprintf("%s, %s, %d, %d, %d, %d, %d\n", name, "edg", p, l, w, h, w * h * 4);
								g_autolog.write(s.c_str());
							}
						}
					}
				}
				K__PRINT("Import: %s", name.c_str());
				return true;
			} else {
				return false;
			}
		}
		{
			std::string nameInData = K::pathJoin(dataDir, name);
			std::string nameInBank = K::pathJoin(bankDir, name);
			if (K::fileCopy(nameInData, nameInBank, true)) {
				if (EXPORT_CONTENTS_DEBUG_DATA) {
					if (K::pathHasExtension(name, ".png")) {
						std::string path = K::pathJoin(dataDir, name);
						KInputStream r;
						if (r.openFileName(path)) {
							uint8_t buf[32];
							r.read(buf, sizeof(buf));
							int w = 0, h = 0;
							KCorePng::readsize(buf, sizeof(buf), &w, &h);
							std::string s = K::str_sprintf("%s, %s, %d, %d, %d, %d, %d\n", name, "png", 0, 0, w, h, w * h * 4);
							g_autolog.write(s.c_str());
						}
					}
				}
				K__PRINT("Import: %s", name.c_str());
				return true;
			} else {
				K__ERROR("Failed to copy: %s ==> %s", nameInData.c_str(), nameInBank.c_str());
				return false;
			}
		}
	}
	KXmlElement * getXmlBankDoc(const std::string &xmlNameInBank) {
		KXmlElement *xDoc = nullptr;
		if (K::pathExists(xmlNameInBank)) {
			xDoc = KXmlElement::createFromFileName(xmlNameInBank);
		}
		if (xDoc == nullptr) {
			xDoc = KXmlElement::create();
		}
		if (xDoc->getChildCount() == 0) {
			xDoc->addChild("Bank");
		}
		return xDoc;
	}
	void writeTimestamp(KXmlElement *xmlBank, const std::string &name, time_t timestamp) {
		if (xmlBank == nullptr) return;
		if (name.empty()) return;
		K__ASSERT(xmlBank->hasTag("Bank"));

		int idx = getFileNodeIndex(xmlBank, name);
		KXmlElement *elm = nullptr;
		if (idx < 0) {
			elm = xmlBank->addChild("File");
			elm->setAttrString("name", name.c_str());
		} else {
			elm = xmlBank->getChild(idx);
		}
		K__ASSERT(elm);

		char timeVal[256] = {0};
		sprintf_s(timeVal, sizeof(timeVal), "%lld", timestamp);
		elm->setAttrString("time", timeVal);
	}
	time_t readTimestamp(const KXmlElement *xmlBank, const std::string &name) const {
		if (xmlBank == nullptr) return 0;
		if (name.empty()) return 0;
		K__ASSERT(xmlBank->hasTag("Bank"));

		// 各ノードを探していく
		// <... name="filename" time="123456789" />
		int cnt = xmlBank->getChildCount();
		for (int i=0; i<cnt; i++) {
			const KXmlElement *elm = xmlBank->getChild(i);
			K__ASSERT(elm->hasTag("File"));

			const std::string &nameVal = elm->getAttrString("name", "");
			if (nameVal.compare(name) == 0) {
				const char *timeVal = elm->getAttrString("time"); // time_t は64ビットなので getAttrInt を使ってはいけない
				time_t timestamp = 0;
				if (timeVal) {
					sscanf_s(timeVal, "%lld", &timestamp);
				}
				return timestamp;
			}
		}
		return 0; // not found
	}
	int getFileNodeIndex(const KXmlElement *xmlBank, const std::string &name) const {
		if (name.empty()) return -1;
		if (xmlBank == nullptr) return -1;
		K__ASSERT(xmlBank->hasTag("Bank"));

		int cnt = xmlBank->getChildCount();
		for (int i=0; i<cnt; i++) {
			const KXmlElement *elm = xmlBank->getChild(i);
			K__ASSERT(elm->hasTag("File"));

			const char *name_str = elm->getAttrString("name", "");
			if (name.compare(name_str) == 0) {
				return i;
			}
		}
		return -1;
	}
	bool updateBankDir(const std::string &bankDir, const std::string &dataDir) {
		if (!K::pathIsDir(bankDir)) {
			if (!K::fileMakeDir(bankDir)) {
				K__ERROR("Failed to make bank directory: %s", bankDir.c_str());
				return false;
			}
		}
		ScanDir cb;
		KDirectoryWalker::walk(dataDir, &cb);

		for (auto it=cb.mDirs.begin(); it!=cb.mDirs.end(); ++it) {
			std::string dirInBank = K::pathJoin(bankDir, it->c_str());

			if (K::pathIsFile(dirInBank)) {
				K__ERROR("Failed to make directory: %s\n(non-directory-file with the same name already exist)", dirInBank.c_str());
				return false;
			}

			if (!K::pathIsDir(dirInBank)) {
				if (!K::fileMakeDir(dirInBank)) {
					K__ERROR("Failed to make directory: %s", dirInBank.c_str());
					return false;
				}
			}
		}
		return true;
	}
};

void K_GameUpdateBank(const char *bankDir, const char *dataDir, KPathList *updated_files, KGameUpdateBankFlags flags) {
	CBankUpdator bank;
	bank.update(bankDir, dataDir, updated_files, flags);
}
#pragma endregion // K_GameUpdateBank









#pragma region CXresLoader

struct CONTENTS {
	KPath textureName;
	KImage textureImage;
	std::unordered_map<KPath, KSpriteAuto> sprites;
};

// <Texture>
class CXresTexture {
	CXresLoaderCallback *m_Callback;
	KStorage *m_Storage;
public:
	CXresTexture() {
		m_Callback = nullptr;
		m_Storage = nullptr;
	}
	virtual ~CXresTexture() {
		K__DROP(m_Storage);
	}

	void setup(KStorage *storage, CXresLoaderCallback *cb) {
		K__ASSERT_RETURN(KBank::getTextureBank()); 
		K__ASSERT_RETURN(KBank::getSpriteBank());
		K__DROP(m_Storage);
		m_Storage = storage;
		K__GRAB(m_Storage);
		m_Callback = cb;
	}

	// <Texture> ノードを処理する
	bool load_TextureNode(KXmlElement *xTex, const char *xml_name) {
		CONTENTS contents;
		if (!load_TextureNodeEx(xTex, xml_name, &contents)) {
			return false;
		}
		KBank::getTextureBank()->addTextureFromImage(contents.textureName, contents.textureImage);
		for (auto it=contents.sprites.begin(); it!=contents.sprites.end(); it++) {
			KBank::getSpriteBank()->addSpriteFromDesc(it->first, it->second);
		}
		return true;
	}

private:
	bool load_TextureNodeEx(KXmlElement *xTex, const char *xml_name, CONTENTS *contents) {
		if (!xTex->hasTag("Texture")) {
			return false; // <Texture> ではない
		}

		// 入力ファイル名
		// <Texture file="XXX">
		KPath image_name;
		{
			const char *file_str = xTex->getAttrString("file");
			if (file_str && file_str[0]) {
				// パスを含めたファイル名を得る（データフォルダ内での相対パス）
				// ※file_str は処理中の XML ファイルがあるフォルダからの相対パスで指定されているものとする
				KPath filename(file_str);

				if (filename.compare_str("nullptr") == 0) {
					// 特殊ファイル名。
					// 「画像ファイルが存在しない」ことを明示する
					image_name = KPath::Empty;
				} else {
					image_name = KGamePath::evalPath(filename, xml_name, ".png");
				}
			} else {
				K__ERROR(u8"E_FILELOADER_TEXNODE: <Texture> に file 属性が指定されていません: %s(%d)",
					xml_name, xTex->getLineNumber());
				return false;
			}
		}

		// テクスチャ名
		// <Texture name="XXX">
		KPath texture_name;
		{
			const char *name_str = xTex->getAttrString("name");
			if (name_str && name_str[0]) {
				KPath name = KPath(name_str);
				texture_name = KPath(xml_name).directory().join(name);

			} else {
				texture_name = image_name;
			//	K__ERROR(u8"E_FILELOADER_TEXNODE: <Texture> に name 属性が指定されていません: %s(%d)", xml_name, xml->getLineNumber());
			//	continue;
			}
		}

		// 指定された画像をロードする
		// フィルターが指定されている場合は画像を加工する
		// <Texture filter="XXX">
		KImage img;
		std::string filter = xTex->getAttrString("filter", "");

		if (image_name.empty() && filter.empty()) {
			K__ERROR(
				u8"E_FILELOADER_TEX: テクスチャの生成元画像ファイルが指定されていない場合は filter 属性で"
				u8"生成画像を指定する必要がありますが、生成元画像とフィルターの両方が省略されています: %s", texture_name.u8()
			);
			return false;
		}
		if (!getTextureImage(img, image_name.u8(), filter)) {
			K__ERROR(u8"E_FILELOADER_TEXNODE: <Texture> で指定された画像 '%s' をロードできないか、フィルター '%s' を適用できませんでした: %s(%d)",
				image_name.u8(), filter, xml_name, xTex->getLineNumber());
			return false;
		}


		if (K_DEBUG_IMAGE_FILTER) {
			if (filter != "") {
				std::string dir = "__image_filter";
				if (K::fileMakeDir(dir)) {
					std::string ss = image_name.u8();
					K::strReplace(ss, "/",  "###");
					std::string s = K::str_sprintf("%s/%s_(%s).png", dir.c_str(), ss.c_str(), filter.c_str());
					if (img.saveToFileName(s)) {
						K__WARNING(u8"K_DEBUG_IMAGE_FILTER が ON になっているため、フィルター結果を %s に出力しました", s.c_str());
					} else {
						K__ERROR("FAILED TO WRITE %s", s.c_str());
					}
				} else {
					K__ERROR("FAILED TO MAKE DIR");
				}
			}
		}


		// テクスチャとして登録する
		contents->textureName = texture_name;
		contents->textureImage = img;

		// スプライトの属性が Texture の属性として記述されている場合、それをスプライト属性のデフォルト値とする
		// <Texture blend="XXX" pivotX="XXX" pivotY="XXX">
		const char *def_blend_str = xTex->getAttrString("blend");
		const char *def_px_str = xTex->getAttrString("pivotX", "50%");
		const char *def_py_str = xTex->getAttrString("pivotY", "50%");

		// 子ノードにスプライトが指定されている場合は、スプライトを生成して追加する
		// <Sprite>
		// .png から画像を切り取る
		const int img_w = img.getWidth();
		const int img_h = img.getHeight();
		for (int i=0; i<xTex->getChildCount(); i++) {
			KXmlElement *xSprite = xTex->getChild(i);
			if (xSprite->hasTag("Sprite")) {
				const char *name_str  = xSprite->getAttrString("name", "");
				const char *blend_str = xSprite->getAttrString("blend", def_blend_str);
				const char *px_str = xSprite->getAttrString("pivotX", def_px_str);
				const char *py_str = xSprite->getAttrString("pivotY", def_py_str);
				const char *x_str  = xSprite->getAttrString("x", "0%");
				const char *y_str  = xSprite->getAttrString("y", "0%");
				const char *w_str  = xSprite->getAttrString("w", "100%");
				const char *h_str  = xSprite->getAttrString("h", "100%");

				KPath sprite_name;
				if (name_str && name_str[0]) {
					// name_str は、処理中の XML ファイルがあるフォルダからの相対パスで指定されているものとする
					sprite_name = KGamePath::evalPath(KPath(name_str), xml_name, ".sprite");
				} else {
					K__ERROR(u8"E_FILELOADER_TEXNODE: <Sprite> に name 属性が指定されていません: %s(%d)",
						xml_name, xSprite->getLineNumber()
					);
					return false;
				}

				{
					KSpriteRes *sp = new KSpriteRes();
					sp->m_TextureName = texture_name;
					sp->m_ImageW = img_w;
					sp->m_ImageH = img_h;
					sp->m_AtlasX = KNumval(x_str).valuei(img_w); // % 表記の場合は、画像全体に対する割合になる
					sp->m_AtlasY = KNumval(y_str).valuei(img_h);
					sp->m_AtlasW = KNumval(w_str).valuei(img_w);
					sp->m_AtlasH = KNumval(h_str).valuei(img_h);
					sp->m_Pivot.x = KNumval(px_str).valuef((float)sp->m_AtlasW); // % 表記の場合は、切り取り後の画像に対する割合になる
					sp->m_Pivot.y = KNumval(py_str).valuef((float)sp->m_AtlasH);
					sp->m_PivotInPixels = true;
					sp->m_UsingPackedTexture = false; // png から作っているので、パックはされていない
					sp->m_SubMeshIndex = -1; // <-- 何番目の画像を取り出すか？
					sp->m_DefaultBlend = KVideoUtils::strToBlend(blend_str, KBlend_INVALID);
					contents->sprites[sprite_name] = sp;
					sp->drop();
				}
			}
		}
		return true;
	}

	bool getTextureImage(KImage &result_image, const std::string &image_name, const std::string &filter) {
		// 画像をロード
		if (!image_name.empty()) {
			std::string bin = m_Storage->loadBinary(image_name, true);
			if (bin.empty()) {
				K__ERROR("Failed to read binary: %s", image_name.c_str());
				return false;
			}
			result_image = KImage::createFromFileInMemory(bin);
			if (result_image.empty()) {
				K__ERROR("Failed to load image: %s", image_name.c_str());
				return false;
			}
		}

		// フィルタを適用
		if (!filter.empty() && m_Callback) {
			if (!m_Callback->onImageFilter(filter, result_image)) {
				K__ERROR("Unknown image filter: %s", filter.c_str());
			}
		}
		return true;
	}
};

// <Clip>
class CXresClip {
public:
	CXresClip() {
	}
	void setup() {
		K__ASSERT_RETURN(KBank::getSpriteBank()); 
		K__ASSERT_RETURN(KBank::getAnimationBank()); 
	}
	KPath readClipNameAttr(KXmlElement *elm, const char *xml_name) {
		const char *str = elm->getAttrString("name");
		if (str && str[0]) {
			// name_str が指定されている。
			// 処理中の XML ファイルがあるフォルダからの相対パスで指定されているものとして、クリップ名を決める
			return KGamePath::evalPath(KPath(str), xml_name, ".clip");
		} else {
			K__ERROR(u8"E_FILELOADER_CLIPNODE: <Clip> に name 属性が指定されていません: %s(%d)",
				xml_name, elm->getLineNumber()
			);
			return KPath();
		}
	}

	// <Clip> ノードを処理する
	bool load_ClipNode(KXmlElement *xClip, const char *xml_name, KClipRes **out_clip) {
		if (!xClip->hasTag("Clip")) {
			return false; // <Clip> ではない
		}

		// クリップ名
		// <Clip name=<"XXX">
		KPath clipName = readClipNameAttr(xClip, xml_name);
		if (clipName.empty()) {
			return false;
		}

		CLayeredSpriteClipBuilder builder;

		// 再生設定
		// <Clip loop="XXX" autoKill="XXX">
		builder.setLoop(xClip->getAttrInt("loop") != 0);
		builder.setKillSelf(xClip->getAttrInt("autoKill") != 0);
		bool nosprite = xClip->getAttrInt("nosprite") != 0; // スプライトを使わないアニメである
		
		// デフォルトウェイト
		// <Clip pagedur="XXX">
		// ページの固有ウェイトが省略されている場合は、この値を使う
		int pagedur = xClip->getAttrInt("pagedur", 8);

		KXmlElement *xEditInfo = nullptr;
		KNamedValues defaultUserParams;
		for (int i=0; i<xClip->getChildCount(); i++) {
			KXmlElement *xElm = xClip->getChild(i);
			if (xElm->hasTag("Parameters")) {
				// <Clip> 直下に <Parameters> がある
				// 全ページに対するデフォルトパラメータとして取得しておく
				defaultUserParams.loadFromXml(xElm, true);
			}
			if (xElm->hasTag("EditInfo")) {
				K__DROP(xEditInfo);
				xEditInfo = xElm;
				K__GRAB(xEditInfo);
			}
		}

		// ページ定義
		int pageindex = 0;
		for (int i=0; i<xClip->getChildCount(); i++) {
			KXmlElement *xPage = xClip->getChild(i);
			// ページを追加
			if (xPage->hasTag("Page")) {
				// ページの基本情報
				_ReadPageMarkAttr(builder, xPage, pageindex, xml_name); // ページマーク
				_ReadPageNextAttr(builder, xPage, pageindex, xml_name); // 次のページ
			//	builder.setPageSource(pageindex, -1, ""); // このページの画像とページ番号（EDGEのみ）

				// ページ長さ
				// <Page dur="6" ... />
				int page_duration = pagedur;
				if (xPage->getAttrInt("dur", 0) > 0) {
					page_duration = xPage->getAttrInt("dur", 0);
				}
				if (xPage->getAttrInt("delay") > 0) {
					K__WARNING(u8"E_FILELOADER_CLIPNODE: delay 属性は削除されました。代わりに dur を使ってください");
					page_duration = xPage->getAttrInt("delay");
				}

				KNamedValues user_params = defaultUserParams.clone();
				int layerindex = 0;
				for (int j=0; j<xPage->getChildCount(); j++) {
					KXmlElement * xElm = xPage->getChild(j);
					if (xElm->hasTag("Parameters")) {
						// <Parameters> ノードが指定されている
						// ページ固有の追加パラメータを読む
						KNamedValues params;
						params.loadFromXml(xElm, true);
						user_params.append(params); // デフォルト設定を固有設定で上書きする
					}
					if (xElm->hasTag("Data")) {
						// <Data> ノードが指定されている。丸ごと取り込む
						builder.addData(pageindex, xElm);
					}
					if (xElm->hasTag("Layer")) {
						// <Layer> ノードが指定されている
						std::string spriePath;
						if (!nosprite) {
							{
								std::string sprite_name = xElm->getAttrString("sprite", "");
								if (sprite_name.empty()) {
									K__WARNING(u8"E_FILELOADER_CLIPNODE: <Layer> に sprite 属性が指定されていません: %s(%d)",
										xml_name, xElm->getLineNumber()
									);
								} else {
									spriePath = KGamePath::evalPath(sprite_name, xml_name, ".sprite").u8();
								}
							}
							if (KBank::getSpriteBank()->findSprite(spriePath, false) == nullptr) {
								K__WARNING(
									u8"E_FILELOADER_CLIPNODE: 未登録のスプライト %s が指定されています。"
									u8"<Texture> または <EdgeSprites> ノードを書き忘れていませんか？: %s(%d",
									spriePath.c_str(), xml_name, xElm->getLineNumber()
								);
							}
						}
						builder.setSprite(pageindex, layerindex, spriePath.c_str());
						builder.setLabel(pageindex, layerindex, xElm->getAttrString("label"));
						builder.setCommand(pageindex, layerindex, xElm->getAttrString("command"));
						layerindex++;
					}
				}

				if (layerindex == 0) {
					// <Layer> ノードが指定されていない。
					// このページは１枚のスプライトだけを含む
					KPath spriePath;
					if (!nosprite) {
						{
							std::string sprite_name = xPage->getAttrString("sprite", "");
							if (sprite_name.empty()) {
								K__WARNING(u8"E_FILELOADER_CLIPNODE: <Layer> に sprite 属性が指定されていません: %s(%d)",
									xml_name, xPage->getLineNumber()
								);
							} else {
								spriePath = KGamePath::evalPath(sprite_name, xml_name, ".sprite");
							}
						}
						if (KBank::getSpriteBank()->findSprite(spriePath, false) == nullptr) {
							K__WARNING(
								u8"E_FILELOADER_CLIPNODE: 未登録のスプライト %s が指定されています。"
								u8"<Texture> または <EdgeSprites> ノードを書き忘れていませんか？: %s(%d",
								spriePath.u8(), xml_name, xPage->getLineNumber()
							);
						}
					}
					builder.setSprite(pageindex, 0, spriePath.u8());
					builder.setLabel(pageindex, 0, xPage->getAttrString("label"));
					builder.setCommand(pageindex, 0, xPage->getAttrString("command"));
				}
				builder.addParams(pageindex, &user_params);
				builder.setDuration(pageindex, page_duration);
				pageindex++;
			}
		}

		if (KBank::getAnimationBank()) {
			KClipRes *new_clip = builder.createClip(clipName.u8());
			new_clip->setTag(xml_name); // 作成元の .xres ファイル名タグを追加
			new_clip->m_EditInfoXml = xEditInfo;
			K__GRAB(new_clip->m_EditInfoXml);
			KBank::getAnimationBank()->addClipResource(clipName.u8(), new_clip);
			K__DROP(new_clip);
			K__DROP(xEditInfo);
			if (out_clip) {
				*out_clip = KBank::getAnimationBank()->find_clip(clipName.u8());
			}
		}
		return false;
	}
};

// <EdgeSprites>
class CXresEdgeSprite {
	KStorage *m_Storage;
public:
	CXresEdgeSprite() {
		m_Storage = nullptr;
	}
	virtual ~CXresEdgeSprite() {
		K__DROP(m_Storage);
	}
	void setup(KStorage *storage) {
		K__ASSERT_RETURN(KBank::getTextureBank()); 
		K__ASSERT_RETURN(KBank::getSpriteBank()); 
		K__DROP(m_Storage);
		m_Storage = storage;
		K__GRAB(m_Storage);
	}
	// <EdgeSprites> ノードを処理する
	bool load_EdgeSpritesNode(KXmlElement *xEdgeSprites, const char *xml_name) {
		if (!xEdgeSprites->hasTag("EdgeSprites")) {
			return false; // <EdgeSprites> ではない
		}

		// 入力ファイル名
		// <EdgeSprites file="###" />
		std::string edge_name;
		{
			const char *file_str = xEdgeSprites->getAttrString("file");
			if (file_str && file_str[0]) {
				// パスを含めたファイル名を得る（データフォルダ内での相対パス）
				// ※file_str は処理中の XML ファイルがあるフォルダからの相対パスで指定されているものとする
				edge_name = KGamePath::evalPath(KPath(file_str), xml_name, ".edg").c_str();
			} else {
				K__ERROR(u8"E_FILELOADER_EDGESPRITENODE: <EdgeSprites> に file 属性が指定されていません: %s(%d)",
					xml_name, xEdgeSprites->getLineNumber()
				);
				return false;
			}
		}
		// .edg ファイル内の画像設定（一括）
		// <EdgeSprites pivotX="###" pivotY="###" blend="###"/>
		SPRITE_ATTR default_params; // すべてのページ、レイヤーに適用する設定
		default_params.readFromXmlAttr(xEdgeSprites);

		// Pivotが未設定だったら "50%" にしておく 
		if (!default_params.has_pivot_x) {
			default_params.pivot_x = 50; // 50%
			default_params.pivot_x_in_percent = true;
			default_params.has_pivot_x = true;
		}
		if (!default_params.has_pivot_y) {
			default_params.pivot_y = 50; // 50%
			default_params.pivot_y_in_percent = true;
			default_params.has_pivot_y = true;
		}

		// .edg ファイル内の画像をまとめ、スプライトを作成する
		// sprites には .edg ファイル内のページとレイヤーのうち、無視属性でないすべての絵がはいる。
		// 無視属性かどうかの判定は KGameEdgeBuilder::isIgnorablePage 等を参照すること
		KSpriteList sprites;
		if (!KGameImagePack::loadSpriteList(m_Storage, &sprites, edge_name)) {
			K__ERROR(u8"E_FILELOADER_EDGESPRITENODE: <EdgeSprites> で指定されたファイル '%s' からはスプライトを生成できませんでした: %s(%d)",
				edge_name.c_str(), xml_name, xEdgeSprites->getLineNumber()
			);
			return false;
		}

		// 得られたスプライトの Pivot や Blend はデフォルト値のままである。
		// 最初に <EdgeSprites> の属性で指定された値を全ページ、レイヤーに設定する
		for (auto it=sprites.m_Items.begin(); it!=sprites.m_Items.end(); ++it) {
			if (default_params.has_blend) {
				it->def->m_DefaultBlend = default_params.blend;
			}
			it->def->m_Pivot.x = default_params.getPivotXInPixels(it->def->m_AtlasW);
			it->def->m_Pivot.y = default_params.getPivotYInPixels(it->def->m_AtlasH);
			it->def->m_PivotInPixels = true;
		}

		// <EdgeSprites> が子ノード <Page> を持っていれば、ページごとに指定された値を上書きする
		int page_idx = 0;
		for (int i=0; i<xEdgeSprites->getChildCount(); i++) {
			KXmlElement *xPage = xEdgeSprites->getChild(i);
			if (xPage->hasTag("Page")) {
				SPRITE_ATTR page_params; // このページ固有の設定
				page_params.readFromXmlAttr(xPage);
				if (page_params.page >= 0) {
					page_idx = page_params.page;
				}
				// 指定されたページに対応するスプライトの設定を書き換える
				for (auto it=sprites.m_Items.begin(); it!=sprites.m_Items.end(); ++it) {
					if (it->page == page_idx) {
						if (page_params.has_blend) {
							it->def->m_DefaultBlend = page_params.blend;
						}
						if (page_params.has_pivot_x) {
							it->def->m_Pivot.x = page_params.getPivotXInPixels(it->def->m_AtlasW);
							K__ASSERT(it->def->m_PivotInPixels);
						}
						if (page_params.has_pivot_y) {
							it->def->m_Pivot.y = page_params.getPivotYInPixels(it->def->m_AtlasH);
							K__ASSERT(it->def->m_PivotInPixels);
						}
					}
				}
				page_idx++;
			}
		}

		// テクスチャを登録
		KBank::getTextureBank()->addTextureFromImage(sprites.m_TexName, sprites.m_TexImage);

		// スプライトを登録
		for (auto it=sprites.m_Items.begin(); it!=sprites.m_Items.end(); ++it) {
			KPath sprite_name = KGamePath::getSpriteAssetPath(edge_name, it->page, it->layer);

			// ここでスプライトを登録するが、第3引数 update_mesh は false にしておくこと。
			// it->def にはすでにメッシュが入っているが、空白のスプライトでは it->def.mesh の頂点数があえて 0 になっている場合がある。
			// 頂点数が 0 だと自動メッシュ設定が働いてしまうため、頂点数に関係なくメッシュを更新させないために false を指定する必要がある
			KBank::getSpriteBank()->addSpriteFromDesc(sprite_name, it->def, false);
		}
		return true;
	}
};





class CXresEdgeAnimation {
	KStorage *m_Storage;
public:
	CXresEdgeAnimation() {
		m_Storage = nullptr;
	}
	virtual ~CXresEdgeAnimation() {
		K__DROP(m_Storage);
	}
	void setup(KStorage *storage) {
		K__ASSERT_RETURN(KBank::getSpriteBank());
		K__ASSERT_RETURN(KBank::getAnimationBank());
		K__DROP(m_Storage);
		m_Storage = storage;
		K__GRAB(m_Storage);
	}
	// <EdgeAnimation> ノードを処理する
	bool load_EdgeAnimationNode(KXmlElement *xEdgeAnimation, const char *xml_name, KClipRes **out_clip) {
		if (!xEdgeAnimation->hasTag("EdgeAnimation")) {
			return false; // <EdgeAnimation> ではない
		}

		// 入力ファイル名
		KPath edge_name;
		{
			const char *file_str = xEdgeAnimation->getAttrString("file");
			if (file_str && file_str[0]) {
				// パスを含めたファイル名を得る（データフォルダ内での相対パス）
				// ※file_str は処理中の XML ファイルがあるフォルダからの相対パスで指定されているものとする
				edge_name = KGamePath::evalPath(KPath(file_str), xml_name, ".edg");
			} else {
				K__ERROR(u8"E_FILELOADER_EDGEANINODE: <EdgeClip> に file 属性が指定されていません: %s(%d)",
					xml_name, xEdgeAnimation->getLineNumber()
				);
				return false;
			}
		}

		// クリップ名
		KPath clipName;
		{
			const char *name_str = xEdgeAnimation->getAttrString("name");
			if (name_str && name_str[0]) {
				// name_str が指定されている。
				// 処理中の XML ファイルがあるフォルダからの相対パスで指定されているものとして、クリップ名を決める
				clipName = KGamePath::evalPath(KPath(name_str), xml_name, ".clip");
			} else {
				// name_str が省略されている。
				// 処理中の EDG ファイルと同名で拡張子だけが異なる名前にする
				clipName = edge_name.changeExtension(".clip");
			}
		}

		// Edge ファイルをロードする
		// 無視ページやレイヤを削除した状態で取得するため、
		// KEdgeDocument::loadFromFile ではなく KGameEdgeBuilder を使う
		KEdgeDocument edge;
		KInputStream edgefile = m_Storage->getInputStream(edge_name.u8());
		if (!edgefile.isOpen()) {
			K__ERROR(
				u8"E_FILELOADER_EDGEANINODE: "
				u8"<EdgeAnimation> で指定された Edge ファイル '%s' をロードできませんでした: %s(%d)",
				edge_name.u8(), xml_name, xEdgeAnimation->getLineNumber()
			);
			return false;
		}

		if (1) {
			bool ok = KGameEdgeBuilder::loadFromStream(&edge, edgefile, edge_name.u8());
			if (!ok) {
				K__ERROR(
					u8"E_FILELOADER_EDGEANINODE: "
					u8"<EdgeAnimation> で指定された Edge ファイル '%s' をロードできませんでした: %s(%d)",
					edge_name.u8(), xml_name, xEdgeAnimation->getLineNumber()
				);
				return false;
			}
		}

		// Edge の各ページ、レイヤーのラベル情報を得る
		CEdgeLabelList edgeLabels;
		edgeLabels.load(edge, edge_name);

		// Edge の各ページ、レイヤーのコマンド情報（@で始まる部分）を得る
		CEdgeCommandList edgeCmds;
		edgeCmds.load(edge, edge_name);

		CLayeredSpriteClipBuilder builder;

		// 再生設定
		// <EdgeAnimation loop="XXX" autoKill="XXX">
		builder.setLoop(xEdgeAnimation->getAttrInt("loop") != 0);
		builder.setKillSelf(xEdgeAnimation->getAttrInt("autoKill") != 0);

		KXmlElement *xEditInfo = nullptr;
		KNamedValues defaultUserParams;
		for (int i=0; i<xEdgeAnimation->getChildCount(); i++) {
			KXmlElement *xElm = xEdgeAnimation->getChild(i);
			if (xElm->hasTag("Parameters")) {
				// <EdgeAnimation> 直下に <Parameters> がある
				// 全ページに対するデフォルトパラメータとして取得しておく
				KNamedValues params;
				params.loadFromXml(xElm, true);
				defaultUserParams.append(params);
			}
			if (xElm->hasTag("EditInfo")) {
				// <EdgeAnimation> 直下に <EditInfo> がある
				// このクリップに対するエディタ用の情報
				K__DROP(xEditInfo);
				xEditInfo = xElm;
				K__GRAB(xEditInfo);
			}
		}

		// ページ定義
		int clip_page_index = 0;
		int auto_page_index = 0;
		int last_page_dur = 8;

		for (int i=0; i<xEdgeAnimation->getChildCount(); i++) {
			KXmlElement *xPage = xEdgeAnimation->getChild(i);
			if (xPage->hasTag("Page")) {
				// ページを追加
				const char *externEdgeName = xPage->getAttrString("extern_edge"); // 現在のフォルダからの相対パスであることに注意
				const int externEdgePage = xPage->getAttrInt("extern_page", -1);

				// 使用する EDGE ページ番号
				// <Page page="0" ... />
				// ページ番号が未指定ならば前回の page 番号から自動インクリメントされた値を使う
				const int edgePageIndex = xPage->getAttrInt("page", auto_page_index);

				// 使用する EDGE ページ
				const KEdgePage *edgepage = edge.getPage(edgePageIndex);
				KPath edgeForThisPage = edge_name; // 実際に絵に使っている edge の名前 (extern した場合は一時的に異なる edge を参照する場合があるため）
				int edgePageForThisPage = edgePageIndex; // 実際に絵に使っている edge のページ番号

				int page_duration = last_page_dur; // 直前のページのウェイトをデフォルト値にしておく
				if (externEdgeName && externEdgeName[0] && externEdgePage >= 0) {
					// 外部ページが指定されている。
					// edgePageIndex は使われないので、それを元に取得した edgepage も使わない
					edgepage = nullptr;
					edgeForThisPage = KGamePath::evalPath(KPath(externEdgeName), xml_name, ".edg"); // リソース内の絶対パスに直す
					edgePageForThisPage = externEdgePage;
				} else {
					if (edgepage == nullptr) {
						K__ERROR(
							u8"E_FILELOADER_EDGEANINODE: %s のページ番号 [%d] は存在しません。"
							u8"このファイルで指定できるページ番号は 0 から %d までです: %s(%d)",
							edge_name.u8(), edgePageIndex, edge.getPageCount()-1, 
							xml_name, xPage->getLineNumber());
						break;
					}
					page_duration = edgepage->m_delay;
				}

				// ページの基本情報
				_ReadPageMarkAttr(builder, xPage, clip_page_index, xml_name);; // <Page mark="MARKA" ..>
				_ReadPageNextAttr(builder, xPage, clip_page_index, xml_name);; // <Page next="MARKA" ..>
				builder.setPageSource(clip_page_index, edgePageForThisPage, edgeForThisPage); // 元Edgeのページ番号とファイル名

				// ページ長さ
				// <Page dur="6" ... />
				if (xPage->getAttrInt("dur") > 0) {
					page_duration = xPage->getAttrInt("dur");
				}

				// ページ固有の追加パラメータがあるならそれを読む
				// <Parameters>
				KNamedValues user_params = defaultUserParams.clone();
				for (int j=0; j<xPage->getChildCount(); j++) {
					KXmlElement *xElm = xPage->getChild(j);
					if (xElm->hasTag("Parameters")) {
						KNamedValues params;
						params.loadFromXml(xElm, true);
						user_params.append(params); // デフォルト設定を固有設定で上書きする
					}
					if (xElm->hasTag("Data")) {
						// <Data> ノードが指定されている。丸ごと取り込む
						builder.addData(clip_page_index, xElm);
					}
				}

				// レイヤーリスト
				// ページ内のすべてのレイヤを重ねたものを１枚のスプライトとして扱う
				if (externEdgeName && externEdgeName[0] && externEdgePage >= 0) {
					KPath tmp_edge_name = KGamePath::evalPath(KPath(externEdgeName), xml_name, ".edg"); // リソース内の絶対パスに直す
					KInputStream tmp_file = m_Storage->getInputStream(tmp_edge_name.u8());
					if (!tmp_file.isOpen()) {
						K__ERROR(
							u8"E_FILELOADER_EDGEANINODE: "
							u8"外部 Edge の指定が正しくありません: NO_FILE '%s'", 
							tmp_edge_name.u8()
						);
						continue;
					}
					CEdgeFile tmp_edge;
					bool ok = tmp_edge.loadEdge(tmp_file, tmp_edge_name.u8());
					if (!ok) {
						K__ERROR(
							u8"E_FILELOADER_EDGEANINODE: 外部 Edge の指定が正しくありません。"
							u8"Edge ファイル '%s' をロードできません",
							tmp_edge_name.u8()
						);
						continue;
					}
					int num_layers = tmp_edge.getLayerCount(externEdgePage);
					if (num_layers == 0) {
						K__ERROR(
							u8"E_FILELOADER_EDGEANINODE: 外部 Edge のページ指定が正しくありません。"
							u8"Edge ファイル '%s' にはページ %d (0起算) が存在しません",
							tmp_edge_name.u8(), externEdgePage
						);
						continue;
					}

					for (int li=0; li<num_layers; li++) {
						const KPath spriePath = KGamePath::getSpriteAssetPath(tmp_edge_name, externEdgePage, li);
						const KPath &label = tmp_edge.getLabel(externEdgePage, li);
						const char *command = tmp_edge.getCommand(externEdgePage, li);
						if (KBank::getSpriteBank()->findSprite(spriePath, false) == nullptr) {
							K__WARNING(
								u8"E_FILELOADER_EDGEANINODE: 未登録のスプライト %s が指定されています。"
								u8"<Texture> または <EdgeSprites> ノードを書き忘れていませんか？: %s(%d",
								spriePath.u8(), xml_name, xPage->getLineNumber()
							);
						}
						builder.setSprite(clip_page_index, li, spriePath.u8());
						builder.setLabel(clip_page_index, li, label.u8());
						builder.setCommand(clip_page_index, li, command);
					}

				} else {
					int clipLayerIndex = 0;
					for (int j=0; j<xPage->getChildCount(); j++) {
						KXmlElement *xLayer = xPage->getChild(j);
						if (xLayer->hasTag("Layer")) {
							// <Layer> ノードが指定されている
							// 例: <Layer layer="0"/>
							if (xLayer->getAttrString("layer") == nullptr) {
								K__ERROR(
									u8"E_FILELOADER_EDGEANINODE: <Layer> ノードにレイヤー番号指定 layer='XXX' がありません。"
									u8"必ずレイヤー番号を指定してください: %s(%d)",
									xml_name, xLayer->getLineNumber());
								break;
							}
							int edgeLayerIndex = xLayer->getAttrInt("layer", -1);
							if (edgeLayerIndex < 0 || edgepage->getLayerCount() <= edgeLayerIndex) {
								K__ERROR(
									u8"E_FILELOADER_EDGEANINODE: %s のページ [%d] にはレイヤー番号 %d が存在しません。"
									u8"このページに指定できるレイヤー番号は 0 から %d までです: %s(%d)",
									edge_name.u8(), edgePageIndex, edgeLayerIndex, edgepage->getLayerCount()-1,
									xml_name, xLayer->getLineNumber());
								break;
							}
							const KPath spriePath = KGamePath::getSpriteAssetPath(edge_name, edgePageIndex, edgeLayerIndex);
							const KPath &label = edgeLabels.getLabel(edgePageIndex, edgeLayerIndex);
							const char *command = edgeCmds.getCommand(edgePageIndex, edgeLayerIndex);

							if (KBank::getSpriteBank()->findSprite(spriePath, false) == nullptr) {
								K__WARNING(
									u8"E_FILELOADER_EDGEANINODE: 未登録のスプライト %s が指定されています。"
									u8"<Texture> または <EdgeSprites> ノードを書き忘れていませんか？: %s(%d",
									spriePath.u8(), xml_name, xLayer->getLineNumber());
							}
							builder.setSprite(clip_page_index, clipLayerIndex, spriePath.u8());
							builder.setLabel(clip_page_index, clipLayerIndex, label.u8());
							builder.setCommand(clip_page_index, clipLayerIndex, command);
							clipLayerIndex++;
						}
					}

					if (clipLayerIndex == 0) {
						// <Layer> ノードなし。全てのレイヤーを含む
						for (int li=0; li<edgepage->getLayerCount(); li++) {
							const KPath spriePath = KGamePath::getSpriteAssetPath(edge_name, edgePageIndex, li);
							const KPath &label = edgeLabels.getLabel(edgePageIndex, li);
							const char *command = edgeCmds.getCommand(edgePageIndex, li);

							if (KBank::getSpriteBank()->findSprite(spriePath, false) == nullptr) {
								K__WARNING(
									u8"E_FILELOADER_EDGEANINODE: 未登録のスプライト %s が指定されています。"
									u8"<Texture> または <EdgeSprites> ノードを書き忘れていませんか？: %s(%d",
									spriePath.u8(), xml_name, xPage->getLineNumber());
							}
							builder.setSprite(clip_page_index, li, spriePath.u8());
							builder.setLabel(clip_page_index, li, label.u8());
							builder.setCommand(clip_page_index, li, command);
						}
					}
				}
				builder.addParams(clip_page_index, &user_params);
				builder.setDuration(clip_page_index, page_duration);
				auto_page_index = edgePageIndex + 1;
				clip_page_index++;
				last_page_dur = page_duration;
			}
		}
		if (KBank::getAnimationBank()) {
			KClipRes *new_clip = builder.createClip(clipName.u8());
			new_clip->setTag(xml_name); // 作成元の .xres ファイル名タグを追加
			new_clip->setTag(edge_name.c_str()); // 作成元の .edg ファイル名タグを追加
			new_clip->m_EdgeFile = edge_name;
			new_clip->m_EditInfoXml = xEditInfo;
			K__GRAB(new_clip->m_EditInfoXml);
			KBank::getAnimationBank()->addClipResource(clipName.u8(), new_clip);
			K__DROP(new_clip);
			K__DROP(xEditInfo);
			if (out_clip) {
				*out_clip = KBank::getAnimationBank()->find_clip(clipName.u8());
			}
		}
		return true;
	}

};

// Edgeから直接アニメを作成する
bool K_makeClip(KClipRes **out_clip, KInputStream &edge_file, const KPath &edge_name, const KPath clip_name) {
	// Edge ファイルをロードする
	// 無視ページやレイヤを削除した状態で取得するため、
	// KEdgeDocument::loadFromFile ではなく KGameEdgeBuilder を使う
	KEdgeDocument edge;
	if (edge_file.isOpen()) {
		KGameEdgeBuilder::loadFromStream(&edge, edge_file, edge_name.u8());
	}

	// Edge の各ページ、レイヤーのラベル情報を得る
	CEdgeLabelList edgeLabels;
	edgeLabels.load(edge, edge_name);

	// Edge の各ページ、レイヤーのコマンド情報（@で始まる部分）を得る
	CEdgeCommandList edgeCmds;
	edgeCmds.load(edge, edge_name);

	CLayeredSpriteClipBuilder builder;

	// ページ定義
	int clip_page_index = 0;
	for (int P=0; P<edge.getPageCount(); P++) {
		const KEdgePage *page = edge.getPage(P);
		builder.setPageSource(clip_page_index, P, edge_name); // 元Edgeのページ番号とファイル名
		int page_duration = edge.getPage(P)->m_delay;
		builder.setDuration(clip_page_index, page_duration);

		// レイヤーリスト
		for (int L=0; L<page->getLayerCount(); L++) {
			const KPath spriePath = KGamePath::getSpriteAssetPath(edge_name, P, L);
			const KPath &label = edgeLabels.getLabel(P, L);
			const char *command = edgeCmds.getCommand(P, L);
			builder.setSprite(clip_page_index, L, spriePath.u8());
			builder.setLabel(clip_page_index, L, label.u8());
			builder.setCommand(clip_page_index, L, command);
		}
		clip_page_index++;
	}
	K__ASSERT(out_clip);
	KClipRes *new_clip = builder.createClip(clip_name.u8());
	new_clip->setTag(""); // 作成元の .xres ファイル名タグを追加
	new_clip->setTag(edge_name.u8()); // 作成元の .edg ファイル名タグを追加
	new_clip->m_EdgeFile = edge_name;
	new_clip->m_EditInfoXml = nullptr;
	if (out_clip) {
		*out_clip = new_clip;
	}
	return true;
}



class CXresShader {
	KStorage *m_Storage;
public:
	CXresShader() {
		m_Storage = nullptr;
	}
	virtual ~CXresShader() {
		K__DROP(m_Storage);
	}
	void setup(KStorage *storage) {
		K__ASSERT_RETURN(KBank::getShaderBank());
		K__DROP(m_Storage);
		m_Storage = storage;
		K__GRAB(m_Storage);
	}
	// <Shader> ノードを処理する
	bool load_ShaderNode(KXmlElement *xShader, const char *xml_name) {
		if (!xShader->hasTag("Shader")) {
			return false; // <Shader> ではない
		}

		// 入力ファイル名
		KPath source_name;
		{
			const char *file_str = xShader->getAttrString("file");
			if (file_str && file_str[0]) {
				// パスを含めたファイル名を得る（データフォルダ内での相対パス）
				// ※file_str は処理中の XML ファイルがあるフォルダからの相対パスで指定されているものとする
				source_name = KGamePath::evalPath(KPath(file_str), xml_name, ".fx");
			} else {
				K__ERROR(
					u8"E_FILELOADER_SHADERNODE: "
					u8"<Shader> に file 属性が指定されていません: %s(%d)",
					xml_name, xShader->getLineNumber()
				);
				return false;
			}
		}

		// 登録
		if (loadShader(source_name.u8(), xml_name)) {
			return true;
		} else {
			K__ERROR(
				u8"E_FILELOADER_SHADERNODE: "
				u8"<Shader> で指定されたシェーダー '%s' をロードできませんでした: %s(%d)",
				source_name.u8(), xml_name, xShader->getLineNumber()
			);
			return false;
		}
	}
	bool loadShader(const char *name, const char *xml_name) {
		// ファイル内容をロード
		// UTF8で保存されているという前提で、バイナリ無変換で文字列化する
		std::string hlsl_u8 = m_Storage->loadBinary(name);
		KSHADERID sid = KBank::getShaderBank()->addShaderFromHLSL(name, hlsl_u8.c_str());
		if (sid) {
			KBank::getShaderBank()->setTag(sid, xml_name); // 作成元の .xres ファイル名タグを追加
			KBank::getShaderBank()->setTag(sid, name); // 作成元の .fx ファイル名タグを追加
			return true;
		}
		return false;
	}
};

class CXresLoaderImpl: public KXresLoader {
	CXresTexture mXresTexture;
	CXresClip mXresClip;
	CXresEdgeSprite mXresEdgeSprite;
	CXresEdgeAnimation mXresEdgeAnimation;
	CXresShader mXresShader;
	CXresLoaderCallback *mCB;
	KStorage *m_Storage;
public:
	CXresLoaderImpl() {
		mCB = nullptr;
		m_Storage = nullptr;
	}
	virtual ~CXresLoaderImpl() {
		K__DROP(m_Storage);
	}
	void setup(KStorage *storage, CXresLoaderCallback *cb) {
		K__DROP(m_Storage);
		m_Storage = storage;
		K__GRAB(m_Storage);
		mXresTexture.setup(storage, cb);
		mXresClip.setup();
		mXresEdgeSprite.setup(storage);
		mXresEdgeAnimation.setup(storage);
		mXresShader.setup(storage);
		mCB = cb;
	}
	virtual void loadFromFile(const char *xml_name, bool should_exists) override {
		// ファイルの有無を調べる
		if (!m_Storage->contains(xml_name)) {
			if (should_exists) {
				K__ERROR(u8"E_XRES_FILE_NOT_FOUND: %s", xml_name);
			}
			return;
		}
		std::string raw_text = m_Storage->loadBinary(xml_name, should_exists);
		loadFromText(raw_text.c_str(), xml_name);
	}
	virtual void loadFromText(const char *raw_text, const char *xml_name) override {
		std::string xml_pp_u8 = K::strBinToUtf8(raw_text);
		if (xml_pp_u8.empty()) {
			K__ERROR(u8"E_FILELOADER_RES: ファイルには何も記述されていません: %s", xml_name);
			return;
		}

		// プリプロセッサを実行する
		std::string xml_u8;
		KLuapp_text(&xml_u8, xml_pp_u8.c_str(), xml_name, nullptr, 0);
		if (xml_u8.empty()) {
			K__ERROR(u8"E_FILELOADER_PP: プリプロセッサの実行結果が空文字列になりました: %s", xml_name);
			return;
		}

		// XMLを解析する
		KXmlElement *xDoc = KXmlElement::createFromString(xml_u8.c_str(), xml_name);
		if (xDoc == nullptr) {
			K__ERROR(u8"E_FILELOADER_RES: XMLの構文解析でエラーが発生しました: %s", xml_name);
			return;
		}
		for (int i=0; i<xDoc->getChildCount(); i++) {
			KXmlElement *xElm = xDoc->getChild(i);

			if (xElm->hasTag("Texture")) {
				// テクスチャだけを登録する
				mXresTexture.load_TextureNode(xElm, xml_name);
				continue;
			}
			if (xElm->hasTag("Clip")) {
				// スプライトからアニメクリップを作成する
				KClipRes *clip = nullptr;
				if (mXresClip.load_ClipNode(xElm, xml_name, &clip)) {
					if (mCB) mCB->onLoadClip(clip);
				}
				continue;
			}
			if (xElm->hasTag("EdgeSprites")) {
				// EDGEファイルからスプライトを作成する
				mXresEdgeSprite.load_EdgeSpritesNode(xElm, xml_name);
				continue;
			}
			if (xElm->hasTag("EdgeAnimation")) {
				// EDGEファイルからアニメクリップを作成する
				KClipRes *clip = nullptr;
				if (mXresEdgeAnimation.load_EdgeAnimationNode(xElm, xml_name, &clip)) {
					if (mCB) mCB->onLoadClip(clip);
				}
				continue;
			}
			if (xElm->hasTag("Shader")) {
				// シェーダーをロードする
				mXresShader.load_ShaderNode(xElm, xml_name);
				continue;
			}
			K__WARNING(u8"E_UNKNOWN_XML_NODE: 未知のノード %s をスキップしました: %s", xElm->getTag(), xml_name);
		}
		xDoc->drop();
	}
};

KXresLoader * KXresLoader::create(KStorage *storage, CXresLoaderCallback *cb) {
	CXresLoaderImpl *impl = new CXresLoaderImpl();
	impl->setup(storage, cb);
	return impl;
}

#pragma endregion // CXresLoader



#pragma region KBank
static 	KVideoBank *g_VideoBank = nullptr;

void KBank::install() {
	g_VideoBank = KVideoBank::create();
}
void KBank::uninstall() {
	if (g_VideoBank) {
		g_VideoBank->drop();
		g_VideoBank = nullptr;
	}
}
bool KBank::isInstalled() {
	return g_VideoBank != nullptr;
}
KVideoBank * KBank::getVideoBank() {
	return g_VideoBank;
}
KAnimationBank * KBank::getAnimationBank() {
	return g_VideoBank ? g_VideoBank->getAnimationBank() : nullptr;
}
KTextureBank * KBank::getTextureBank() {
	return g_VideoBank->getTextureBank();
}
KSpriteBank * KBank::getSpriteBank() {
	return g_VideoBank->getSpriteBank();
}
KShaderBank * KBank::getShaderBank() {
	return g_VideoBank->getShaderBank();
}
KFontBank * KBank::getFontBank() {
	return g_VideoBank->getFontBank();
}
#pragma endregion // KBank


} // namespace

