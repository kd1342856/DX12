#include "inc_SkinningShader.hlsli"

VSOutput main(float3 pos : POSITION, float2 uv : TEXCOORD, float3 normal: NORMAL, float4 color: COLOR, float3 tangent: TANGENT, uint4 skinIndex : SKININDEX, float4 skinWeight : SKINWEIGHT)
{
    VSOutput Out = (VSOutput)0;
    
    // 速度のパス(VelocitySkinned_VS)と1ビットも違わない深度にするため、位置はpreciseで計算する
    // (最適化で計算の順番やFMAの使い方が変わると、速度のパスの深度テストに通らないピクセルが出る)
    precise row_major matrix mBones = 0;
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        mBones += c_mBones[skinIndex[i]] * skinWeight[i];
    }
    
    // Apply bone matrix
    precise float4 pos4 = mul(float4(pos, 1.0f), mBones); 
    normal = mul(normal, (float3x3) mBones);
    tangent = mul(tangent, (float3x3) mBones);

    precise float4 worldPos = mul(pos4, c_mWorld);
    precise float4 clipPos = mul(mul(worldPos, c_mView), c_mProj);
    Out.pos = clipPos;
    Out.wPos = worldPos.xyz;
    Out.uv = uv;
    
    Out.Color = color;
    // ライティングはワールド空間で行うので、法線と接線もワールドへ移す(スケールは均一を前提にする)
    Out.Normal = normalize(mul(normal, (float3x3) c_mWorld));
    Out.Tangent = normalize(mul(tangent, (float3x3) c_mWorld));
    
    return Out;
}
