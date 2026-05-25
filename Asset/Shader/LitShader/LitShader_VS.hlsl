#include "LitShader.hlsli"

//-------------------------------
// 頂点シェーダ
//================================

// 頂点シェーダ
VSOutput VS(  float4 pos : POSITION,  // 頂点座標
                float2 uv : TEXCOORD0,  // テクスチャUV座標
                float3 normal : NORMAL,  // 法線ベクトル
                float4 color : COLOR,    // 頂点カラー
                float3 tangent : TANGENT // 接線ベクトル
)
{
    VSOutput Out;

    // 座標変換
    Out.Pos = mul(pos, g_mW);       // ローカル座標系 -> ワールド座標系へ変換
    Out.wPos = Out.Pos.xyz;
    Out.Pos = mul(Out.Pos, g_mV);   // ワールド座標系 -> ビュー座標系へ変換
    Out.Pos = mul(Out.Pos, g_mP);   // ビュー座標系 -> 投影座標系へ変換

    // 法線
    Out.wT = normalize(mul(tangent, (float3x3)g_mW));
    Out.wN = normalize(mul(normal, (float3x3)g_mW));
    Out.wB = normalize(cross(Out.wN, Out.wT));
    
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

    // 座標変換
    Out.Pos = mul(pos, g_mW); // ローカル座標系 -> ワールド座標系へ変換
    Out.Pos = mul(Out.Pos, g_mV); // ワールド座標系 -> ビュー座標系へ変換
    Out.Pos = mul(Out.Pos, g_mP); // ビュー座標系 -> 投影座標系へ変換

    Out.wvpPos = Out.Pos;
    
    // UV座標
    Out.UV = uv;
    
    // 出力
    return Out;
}
