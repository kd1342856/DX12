#include "inc_Velocity.hlsli"

// スキニングのモデル
VelocityVSOutput main(float3 pos : POSITION, float2 uv : TEXCOORD, float3 normal : NORMAL, float4 color : COLOR, float3 tangent : TANGENT, uint4 skinIndex : SKININDEX, float4 skinWeight : SKINWEIGHT)
{
    VelocityVSOutput Out;

    // SkinningShader_VSと1ビットも違わない深度にするため、同じ式をpreciseで計算する
    precise row_major float4x4 mBones = 0;
    precise row_major float4x4 mPrevBones = 0;
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        mBones += g_mBones[skinIndex[i]] * skinWeight[i];
        mPrevBones += g_mPrevBones[skinIndex[i]] * skinWeight[i];
    }

    precise float4 pos4 = mul(float4(pos, 1.0f), mBones);
    precise float4 worldPos = mul(pos4, g_mW);
    precise float4 clipPos = mul(mul(worldPos, g_mV), g_mP);
    Out.Pos = clipPos;

    float4 prevWorldPos = mul(mul(float4(pos, 1.0f), mPrevBones), g_mPrevW);
    Out.CurrClip = mul(worldPos, g_mCurrVPNoJitter);
    Out.PrevClip = mul(prevWorldPos, g_mPrevVPNoJitter);
    Out.UV = uv;
    return Out;
}
