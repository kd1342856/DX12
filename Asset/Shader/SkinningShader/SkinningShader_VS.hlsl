#include "inc_SkinningShader.hlsli"

VSOutput main(float3 pos : POSITION, float2 uv : TEXCOORD, float3 normal: NORMAL, float4 color: COLOR, float3 tangent: TANGENT, uint4 skinIndex : SKININDEX, float4 skinWeight : SKINWEIGHT)
{
    VSOutput Out = (VSOutput)0;
    
    // Compute bone matrix
    row_major matrix mBones = 0;
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        mBones += c_mBones[skinIndex[i]] * skinWeight[i];
    }
    
    // Apply bone matrix
    float4 pos4 = mul(float4(pos, 1.0f), mBones); 
    normal = mul(normal, (float3x3) mBones);
    tangent = mul(tangent, (float3x3) mBones);

    Out.pos = mul(pos4, c_mWorld);
    Out.pos = mul(Out.pos, c_mView);
    Out.pos = mul(Out.pos, c_mProj);
    Out.uv = uv;
    
    Out.Color = color;
    Out.Normal = normalize(normal);
    Out.Tangent = normalize(tangent);
    
    return Out;
}
