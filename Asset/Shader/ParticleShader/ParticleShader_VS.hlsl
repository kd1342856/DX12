#include "../Common/ShaderCore.hlsli"

// 1粒 = カメラを向いた四角形(2三角形・6頂点)。頂点バッファは使わず、SV_VertexIDから
// 何番目の粒のどの角かを求める。粒のデータはC++側(ParticleShader::Draw)が定数バッファで送る。
struct ParticleInstance
{
    float3 Pos;   // ワールド座標(中心)
    float Size;   // 半径(m)
    float4 Color; // rgb: HDRの色(Bloomに乗る), a: 不透明度
};

cbuffer cbParticles : register(b2)
{
    ParticleInstance g_Particles[256];
};

struct VSOutput
{
    float4 Pos : SV_Position;
    float2 UV : TEXCOORD0; // -1..1 (中心が0)
    float4 Color : COLOR0;
};

static const float2 kCorners[6] =
    {
        float2(-1, -1),
        float2(-1, 1),
        float2(1, 1),
        float2(-1, -1),
        float2(1, 1),
        float2(1, -1),
};

VSOutput main(uint vI : SV_VertexID)
{
    ParticleInstance p = g_Particles[vI / 6];
    float2 corner = kCorners[vI % 6];

    // ビュー空間でXYに広げるだけで常にカメラの方を向く
    float4 viewPos = mul(float4(p.Pos, 1.0f), g_mV);
    viewPos.xy += corner * p.Size;

    VSOutput Out;
    Out.Pos = mul(viewPos, g_mP);
    Out.UV = corner;
    Out.Color = p.Color;
    return Out;
}
