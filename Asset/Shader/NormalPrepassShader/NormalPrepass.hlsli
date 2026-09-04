#include "../Common/ShaderCore.hlsli"

// SSAO/SSRのためだけの軽量な法線+深度プリパス。
// マテリアルのテクスチャ等は一切参照せず、ビュー空間法線だけを書き出す
// (深度は通常のZバッファそのものをそのままSRVとして後段で読む)。
struct VSOutput
{
    float4 Pos : SV_Position;
    float3 vN : TEXCOORD0; // ビュー空間法線
};
