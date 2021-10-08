#include "COutline.h"

using namespace Kamilo;

static const char *g_OutlineHLSL =
	"uniform float4x4 mo__MatrixView; \n"
	"uniform float4x4 mo__MatrixProj; \n"
	"uniform texture  mo__Texture; \n"
	"uniform float4   mo__Color; \n"
	"uniform float2   mo__TextureSize; \n"
	"uniform float4   outline_color = {0, 0, 0, 1}; \n"
	"sampler samMain = sampler_state { \n"
	"	Texture = <mo__Texture>; \n"
	"	MinFilter=Point; MagFilter=Point; MipFilter=Point; \n"
	"	AddressU=Clamp;	AddressV=Clamp; \n"
	"}; \n"
	"struct RET { float4 pos:POSITION; float2 uv:TEXCOORD0; float4 color:COLOR0; }; \n"
	"RET vs(float4 pos: POSITION, float2 uv: TEXCOORD0, float4 color: COLOR0) {	\n"
	"	float4x4 mat = mul(mo__MatrixView, mo__MatrixProj);	\n"
	"	RET ret = (RET)0; \n"
	"	ret.pos=mul(pos, mat); ret.uv=uv; ret.color=color; \n"
	"	return ret; \n"
	"} \n"
	"float4 ps(float2 uv: TEXCOORD0): COLOR { \n"
	"	float4 color = tex2D(samMain, uv) * mo__Color; \n"
	"	float dx = 1.0 / mo__TextureSize.x; \n"
	"	float dy = 1.0 / mo__TextureSize.y; \n"
	"	if (color.a == 0) { \n"
	"		float b = tex2D(samMain, uv + float2( dx, 0)).a; \n"
	"		float c = tex2D(samMain, uv + float2(-dx, 0)).a; \n"
	"		float d = tex2D(samMain, uv + float2(0, -dy)).a; \n"
	"		float e = tex2D(samMain, uv + float2(0,  dy)).a; \n"
	"		if (b+c+d+e > 0) { color = outline_color; } \n"
	"	} \n"
	"	if (color.a > 0) { \n"
	"		color.a += (1.0f - color.a) * 0.7; \n" // アンチエイリアス部分の不透明度を上げておく（アウトラインの黒と重なる部分なので）
	"		color.rgb = lerp(outline_color, color, color.a); \n" // 文字色とアウトライン色を混合する
	"		color.a = 1.0f; \n"
	"	} \n"
	"	return color; \n"
	"} \n"
	"technique main { pass P0 { \n"
	"	VertexShader = compile vs_2_0 vs(); \n"
	"	PixelShader  = compile ps_2_0 ps(); \n"
	"}} \n"
;

static KSHADERID g_TextShader = nullptr;

static KSHADERID _GetTextShader() {
	if (g_TextShader == nullptr) {
		g_TextShader = KVideo::createShaderFromHLSL(g_OutlineHLSL, "COutline_hlsl");
	}
	return g_TextShader;
}

COutline::COutline() {
	m_Color = KColor::BLACK;
}
void COutline::apply(KNode *node) {
	KDrawable *drawable = KDrawable::of(node);
	if (drawable) {
		drawable->setGrouping(true);
		drawable->getGroupingMaterial()->shader = _GetTextShader();
		drawable->getGroupingMaterial()->cb = this;
	}
}
void COutline::unapply(KNode *node) {
	KDrawable *drawable = KDrawable::of(node);
	if (drawable) {
		drawable->getGroupingMaterial()->shader = nullptr;
		drawable->getGroupingMaterial()->cb = nullptr;
	}
}
void COutline::onMaterial_SetShaderVariable(const KMaterial *material) {
	KVideo::setShaderFloatArray("outline_color", m_Color.floats(), 4);
}
