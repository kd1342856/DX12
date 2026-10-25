#include "../PostProcessShader/PostProcessShader.hlsli"

// 光源(平行光)のスクリーン座標から放射状にサンプルして加算していく、
// 古典的な "Crepuscular Rays" (光条/God Rays) の実装。
// g_tex(t0)にはBloom結果(輝度の高い部分)を渡す想定 - 明るい窓/光源から
// 光が漏れているように見える。
float4 main(PSInput In) : SV_Target0
{
    if (g_EnableGodRays == 0)
    {
        return float4(0, 0, 0, 1);
    }

    float2 lightUV = float2(g_GodRaysLightU, g_GodRaysLightV);

    uint numSamples = max(g_GodRaysNumSamples, 1);
    float2 deltaUV = (In.UV - lightUV) * (g_GodRaysDensity / (float) numSamples);

    float2 uv = In.UV;
    float illuminationDecay = 1.0;
    float3 color = 0;

    for (uint i = 0; i < numSamples; i++)
    {
        uv -= deltaUV;
        float2 sampleUV = saturate(uv);
        float3 s = g_tex.Sample(g_ss, sampleUV).rgb;
        s *= illuminationDecay * g_GodRaysWeight;
        color += s;
        illuminationDecay *= g_GodRaysDecay;
    }

    return float4(color * g_GodRaysExposure, 1.0);
}
