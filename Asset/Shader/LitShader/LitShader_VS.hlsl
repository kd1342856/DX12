#include "LitShader.hlsli"

//-------------------------------
// 頂点シェーダ
//================================

// 頂点シェーダ
VSOutput main(  float3 pos : POSITION,  // 頂点座標
                float2 uv : TEXCOORD0,  // テクスチャUV座標
                float3 normal : NORMAL,  // 法線ベクトル
                float4 color : COLOR,    // 頂点カラー
                float3 tangent : TANGENT // 接線ベクトル
)
{
    VSOutput Out;

    // 変換
    float4x4 mVP = mul(g_mV, g_mP);
    float4x4 mWVP = mul(g_mW, mVP);
    Out.Pos = mul(float4(pos, 1.0f), mWVP);       // ローカル座標 -> 投影座標へ一括変換
    Out.wPos = mul(float4(pos, 1.0f), g_mW).xyz;

    // 法線
    float3 t = mul(tangent, (float3x3)g_mW);
    float3 n = mul(normal, (float3x3)g_mW);
    if (dot(t, t) < 0.0001) t = float3(1, 0, 0);
    if (dot(n, n) < 0.0001) n = float3(0, 1, 0);
    
    Out.wT = normalize(t);
    Out.wN = normalize(n);
    float3 b = cross(Out.wN, Out.wT);
    if (dot(b, b) < 0.0001) b = float3(0, 0, 1);
    Out.wB = normalize(b);
    
    // UV座標
    Out.UV = uv;
    
    // 出力
    return Out;
}

//-------------------------------
// シャドウマップ生成用 頂点シェーダ
//-------------------------------
ShadowCasterVSOutput ShadowCasterVS(float4 pos : POSITION, // 頂点座標
                        float2 uv : TEXCOORD0 // テクスチャUV座標
)
{
    ShadowCasterVSOutput Out;

    // 変換
    float4x4 mVP = mul(g_mV, g_mP);
    float4x4 mWVP = mul(g_mW, mVP);
    Out.Pos = mul(pos, mWVP); // ローカル座標 -> 投影座標へ一括変換

    Out.wvpPos = Out.Pos;
    
    // UV座標
    Out.UV = uv;
    
    // 出力
    return Out;
}
