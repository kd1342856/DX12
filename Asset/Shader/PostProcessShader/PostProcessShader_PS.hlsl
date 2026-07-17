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
    float4 texColor = g_tex.Sample(g_ss, In.UV);
    texColor.rgb = ACESFilmicToneMapping(texColor.rgb * g_Exposure);
    return texColor;
}
