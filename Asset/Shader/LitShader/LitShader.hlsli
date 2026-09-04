#include "../Common/ShaderCore.hlsli"
#include "../Common/inc_PBRCommon.hlsli"

// 16?T???v?? Poisson Disk
static const float2 g_poissonDisk16[16] = {
    float2( -0.94201624, -0.39906216 ),
    float2( 0.94558609, -0.76890725 ),
    float2( -0.094184101, -0.92938870 ),
    float2( 0.34495938, 0.29387760 ),
    float2( -0.91588581, 0.45771432 ),
    float2( -0.81544232, -0.87912464 ),
    float2( -0.38277543, 0.27676845 ),
    float2( 0.97484398, 0.75648379 ),
    float2( 0.44323325, -0.97511554 ),
    float2( 0.53742981, -0.47373420 ),
    float2( -0.26496911, -0.41893023 ),
    float2( 0.79197514, 0.19090188 ),
    float2( -0.24188840, 0.99706507 ),
    float2( -0.81409955, 0.91437590 ),
    float2( 0.19984126, 0.78641367 ),
    float2( 0.14383161, -0.14100790 )
};

// ?C???^?[???[?u???z?m?C?Y
float InterleavedGradientNoise(float2 screenPos)
{
    float3 magic = float3(0.06711056f, 0.00583715f, 52.9829189f);
    return frac(magic.z * frac(dot(screenPos, magic.xy)));
}

//================================
//
// ?}?e???A??
//
//================================
cbuffer cbPerMaterial : register(b2)
{
    float4 g_baseColorFactor;
    float g_metallicFactor;
    float g_roughnessFactor;
    float g_normalScale;
    float g_occlusionStrength;
    float g_emissiveStrength;
    float3 g_emissiveFactor;
    int g_alphaMode; // 0:Opaque, 1:Mask, 2:Blend
    int g_proceduralType; // 0:None, 1:Blood, 2:Cobweb, 3:Glass, 4:Dissolve
    float g_dissolveAmount; // Dissolve用: 0=通常表示, 1=完全に消滅
    float pad;
};

// ?e?N?X?`??
Texture2D g_baseMap_white : register(t0); // ?x?[?X?J???[?e?N?X?`??
Texture2D g_normalMap_normal : register(t1); // ?@???}?b?v
Texture2D g_metallicRoughnessMap_white : register(t2); // GLTF: G=Roughness, B=Metallic
Texture2D g_emissiveMap_white : register(t3); // ????}?b?v
Texture2D g_occlusionMap_white : register(t4); // ?I?N???[?W?????}?b?v (GLTF: R=Occlusion)

Texture2D g_opaqueDepth : register(t9); // Opaqueパスで書き込まれた深度バッファ
Texture2D g_refractionMap : register(t10); // Opaqueパスのカラー結果（屈折用）
Texture2D g_planarReflectionMap : register(t11); // 平面反射パスのカラー結果
Texture2D g_ssaoMap : register(t12); // SSAO結果(画面空間、環境光に掛ける)
Texture2D g_pointLightShadowMap : register(t13); // 最も近いポイントライトの簡易シャドウマップ

//========================================================
// ???`??p?\????
//========================================================

// ???_?V?F?[?_????o?????f?[?^
struct VSOutput
{
    float4 Pos : SV_Position; // ???e???W
    float2 UV : TEXCOORD0; // UV???W
    float3 wT : TEXCOORD1; // ???[???h???
    float3 wB : TEXCOORD2; // ???[???h?]?@??
    float3 wN : TEXCOORD3; // ???[???h?@??
    float3 wPos : TEXCOORD5; // ???[???h???W
};

// ?s?N?Z???V?F?[?_?[????o?????f?[?^
struct PSOutput
{
    float4 color : SV_Target0;
};

//========================================================
// ?V???h?E?}?b?v?????p?\????
//========================================================

// ???_?V?F?[?_????o?????f?[?^
struct ShadowCasterVSOutput
{
    float4 Pos : SV_Position; // ???e???W
    float2 UV : TEXCOORD0; // UV???W
    float4 wvpPos : TEXCOORD1;
};
