#include "../Common/ShaderCore.hlsli"

// SSAO/SSRのためだけの軽量な法線+深度プリパス。
// マテリアルのテクスチャ等は一切参照せず、ビュー空間法線だけを書き出す
// (深度は通常のZバッファそのものをそのままSRVとして後段で読む)。
struct VSOutput
{
    float4 Pos : SV_Position;
    float3 vN : TEXCOORD0; // ビュー空間法線
    float2 UV : TEXCOORD1;
};

// SSR(Opaque)が粗い面(壁・天井等)を鏡にしないよう、粗さだけはマテリアルから読んで
// アルファに書き出す。レイアウトはLitShaderのcbPerMaterialと同じ(MaterialConstants)。
cbuffer cbPerMaterial : register(b2)
{
    float4 g_baseColorFactor;
    float g_metallicFactor;
    float g_roughnessFactor;
    float g_normalScale;
    float g_occlusionStrength;
    float g_emissiveStrength;
    float3 g_emissiveFactor;
    int g_alphaMode;
    int g_proceduralType;
    float g_dissolveAmount;
    int g_reflectionSlot;
};

Texture2D g_metallicRoughnessMap_white : register(t2); // GLTF: G=Roughness, B=Metallic
