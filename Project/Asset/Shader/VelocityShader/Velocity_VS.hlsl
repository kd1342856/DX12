#include "inc_Velocity.hlsli"

// 動いたスタティックメッシュ(扉など)
VelocityVSOutput main(float3 pos : POSITION, float2 uv : TEXCOORD0, float3 normal : NORMAL, float4 color : COLOR, float3 tangent : TANGENT)
{
    VelocityVSOutput Out;

    // LitShader_VS・NormalPrepass_VSと1ビットも違わない深度にするため、同じ式をpreciseで計算する
    precise float4x4 mVP = mul(g_mV, g_mP);
    precise float4x4 mWVP = mul(g_mW, mVP);
    precise float4 clipPos = mul(float4(pos, 1.0f), mWVP);
    Out.Pos = clipPos;

    float4 worldPos = mul(float4(pos, 1.0f), g_mW);
    float4 prevWorldPos = mul(float4(pos, 1.0f), g_mPrevW);
    Out.CurrClip = mul(worldPos, g_mCurrVPNoJitter);
    Out.PrevClip = mul(prevWorldPos, g_mPrevVPNoJitter);
    Out.UV = uv;
    return Out;
}
