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

float4 main(PSInput In) : SV_Target0
{
    float3 sceneColor = g_tex.Sample(g_ss, In.UV).rgb;
    float3 bloomColor = g_texBloom.Sample(g_ss, In.UV).rgb;

    // Additive Bloom
    sceneColor += bloomColor * g_BloomIntensity;

    // ToneMapping
    if (g_EnableHDR != 0)
    {
        sceneColor = ACESFilmicToneMapping(sceneColor * g_Exposure);
    }
    
    // Gamma Correction
    sceneColor = pow(sceneColor, 1.0f / g_Gamma);

    return float4(sceneColor, 1.0);
}
