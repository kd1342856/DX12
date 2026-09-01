#include "../Common/ShaderCore.hlsli"

// テクスチャ
Texture2D g_tex : register(t0); // ベースカラーテクスチャ
// サンプラ
//SamplerState g_ss_clamp : register(s1);
#define g_ss g_ss_linear_clamp

struct PSInput
{
    float4 Pos : SV_Position; // 射影座標
    float2 UV : TEXCOORD0; // UV座標
};

// ACES Filmic ToneMapping
// HDRの色をLDRに変換する
// x       … HDRカラー(RGB)
// 戻り値  … ToneMap後の色
float3 ACESFilmicToneMapping(float3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;

    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

// ACES Filmic トーンマッピング
float4 ACESFilmicPS(PSInput In) : SV_Target0
{
    const float linearExposure = 0.7;
    
    float4 texColor = g_tex.Sample(g_ss, In.UV);
    texColor.rgb = ACESFilmicToneMapping(texColor.rgb * linearExposure);
    
    return texColor;
}
