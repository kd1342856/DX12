#include "inc_SkinningShader.hlsli"

VSOutput main(float3 pos : POSITION, float2 uv : TEXCOORD, float3 normal: NORMAL, float4 color: COLOR, float3 tangent: TANGENT, uint4 skinIndex : SKININDEX, float4 skinWeight : SKINWEIGHT)
{
    VSOutput Out = (VSOutput)0;
    
    // ボーン行列の合成
    row_major matrix mBones = 0;
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        mBones += c_mBones[skinIndex[i]] * skinWeight[i];
    }
    
    // ボーン行列を適用
    float4 pos4 = mul(mBones, float4(pos, 1.0f)); 
    normal = mul((float3x3) mBones, normal);
    tangent = mul((float3x3) mBones, tangent);

    Out.pos = mul(c_mWorld, pos4);
    Out.pos = mul(c_mView, Out.pos);
    Out.pos = mul(c_mProj, Out.pos);
    Out.uv = uv;
    
    Out.Color = color;
    Out.Normal = normalize(normal);
    Out.Tangent = normalize(tangent);
    
    return Out;
}
