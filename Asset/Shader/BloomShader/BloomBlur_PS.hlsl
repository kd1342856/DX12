#include "../PostProcessShader/PostProcessShader.hlsli"

float4 main(PSInput In) : SV_Target0
{
    float2 texSize;
    g_tex.GetDimensions(texSize.x, texSize.y);
    float2 invSize = 1.0 / texSize;

    // ぼかし半径はcbPostProcessのg_BlurRadiusで可変(ShaderEditorから調整可能)
    float2 dir = float2(g_BlurDirectionX, g_BlurDirectionY) * invSize * g_BlurRadius;

    // Simple 9-tap Gaussian blur weights
    float weights[5] = { 0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216 };

    float3 color = g_tex.Sample(g_ss, In.UV).rgb * weights[0];

    for(int i = 1; i < 5; ++i)
    {
        color += g_tex.Sample(g_ss, In.UV + dir * i).rgb * weights[i];
        color += g_tex.Sample(g_ss, In.UV - dir * i).rgb * weights[i];
    }

    return float4(color, 1.0);
}
