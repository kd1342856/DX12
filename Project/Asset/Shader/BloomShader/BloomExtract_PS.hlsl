#include "../PostProcessShader/PostProcessShader.hlsli"

float4 main(PSInput In) : SV_Target0
{
    float3 color = g_tex.Sample(g_ss, In.UV).rgb;
    
    // Calculate luminance
    float luminance = dot(color, float3(0.299, 0.587, 0.114));
    
    // Extract values above threshold
    float contribution = max(0, luminance - g_BloomThreshold);
    contribution /= max(luminance, 0.00001);

    return float4(color * contribution, 1.0);
}
