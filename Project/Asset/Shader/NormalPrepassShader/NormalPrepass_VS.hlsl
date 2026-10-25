#include "NormalPrepass.hlsli"

VSOutput main(float3 pos : POSITION, float2 uv : TEXCOORD0, float3 normal : NORMAL, float4 color : COLOR, float3 tangent : TANGENT)
{
    VSOutput Out;

    // LitShader_VSと1ビットも違わない深度にするため、同じ式をpreciseで計算する(メインの描画はEQUALで深度を比べる)
    precise float4x4 mVP = mul(g_mV, g_mP);
    precise float4x4 mWVP = mul(g_mW, mVP);
    precise float4 clipPos = mul(float4(pos, 1.0f), mWVP);
    Out.Pos = clipPos;

    float3 n = mul(normal, (float3x3) g_mW);
    if (dot(n, n) < 0.0001) n = float3(0, 1, 0);
    n = normalize(n);
    Out.vN = normalize(mul(n, (float3x3) g_mV));
    Out.UV = uv;

    return Out;
}
