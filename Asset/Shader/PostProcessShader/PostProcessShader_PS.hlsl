#include "PostProcessShader.hlsli"

// ACES Filmic ToneMapping
float3 ACESFilmicToneMapping(float3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

// 簡易ハッシュ関数(フィルムグレイン用)
float Hash(float2 p)
{
    return frac(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453123);
}

// 深度バッファ(NDC 0..1)からビュー空間の線形距離を求める。
// XMMatrixPerspectiveFovLH相当の投影を前提とした標準的な変換式。
float LinearizeDepth(float depthNdc, float nearZ, float farZ)
{
    return (nearZ * farZ) / (max(farZ - depthNdc * (farZ - nearZ), 0.0001));
}

float4 main(PSInput In) : SV_Target0
{
    //------------------------------------------
    // 色収差 (レンズの色ズレ)。中心から離れるほどRGBのサンプル位置をずらす。
    //------------------------------------------
    float3 sceneColor;
    if (g_ChromaticAberrationIntensity > 0.0001)
    {
        float2 dir = In.UV - 0.5;
        float2 offset = dir * g_ChromaticAberrationIntensity * 0.02;
        float r = g_tex.Sample(g_ss, In.UV + offset).r;
        float g = g_tex.Sample(g_ss, In.UV).g;
        float b = g_tex.Sample(g_ss, In.UV - offset).b;
        sceneColor = float3(r, g, b);
    }
    else
    {
        sceneColor = g_tex.Sample(g_ss, In.UV).rgb;
    }

    //------------------------------------------
    // 被写界深度(DOF)。ピント距離から外れるほど、事前にぼかし済みの
    // g_texDOFへブレンドする(実際のボケ量ではなく簡易近似)。
    //------------------------------------------
    if (g_EnableDOF != 0)
    {
        float depthNdc = g_texDepth.Load(int3(In.Pos.xy, 0)).r;
        float linearDepth = LinearizeDepth(depthNdc, g_CameraNear, g_CameraFar);
        float coc = saturate(abs(linearDepth - g_DOFFocusDistance) / max(g_DOFFocusRange, 0.001));

        float3 blurredColor = g_texDOF.Sample(g_ss, In.UV).rgb;
        sceneColor = lerp(sceneColor, blurredColor, coc);
    }

    //------------------------------------------
    // Bloom加算
    //------------------------------------------
    float3 bloomColor = g_texBloom.Sample(g_ss, In.UV).rgb;
    sceneColor += bloomColor * g_BloomIntensity;

    //------------------------------------------
    // God Rays(光条)加算
    //------------------------------------------
    if (g_EnableGodRays != 0)
    {
        float3 godRaysColor = g_texGodRays.Sample(g_ss, In.UV).rgb;
        sceneColor += godRaysColor * g_GodRaysIntensity;
    }

    //------------------------------------------
    // ToneMapping
    //------------------------------------------
    if (g_EnableHDR != 0)
    {
        sceneColor = ACESFilmicToneMapping(sceneColor * g_Exposure);
    }

    // Gamma Correction
    sceneColor = pow(max(sceneColor, 0.0), 1.0f / g_Gamma);

    //------------------------------------------
    // ビネット(周辺減光)
    //------------------------------------------
    if (g_VignetteIntensity > 0.0001)
    {
        float2 uv = In.UV - 0.5;
        float dist = length(uv) * g_VignetteIntensity;
        float vignette = 1.0 - smoothstep(g_VignetteSmoothness, 1.0, dist);
        sceneColor *= vignette;
    }

    //------------------------------------------
    // フィルムグレイン(粒状ノイズ)
    //------------------------------------------
    if (g_FilmGrainIntensity > 0.0001)
    {
        float grain = Hash(In.UV * float2(1920.0, 1080.0) + frac(g_Time) * 173.0);
        sceneColor += (grain - 0.5) * g_FilmGrainIntensity;
    }

    return float4(saturate(sceneColor), 1.0);
}
