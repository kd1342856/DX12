#include "inc_SkinningShader.hlsli"

Texture2D g_diffuseTex : register(t0);
Texture2D g_normalTex : register(t1);
Texture2D g_RoughnessMetallicTex : register(t2);
Texture2D g_emissiveTex : register(t3);

SamplerState g_ss : register(s0);

PSOutput main(VSOutput In)
{
    PSOutput Out = (PSOutput)0;
    
    float4 color = g_diffuseTex.Sample(g_ss, In.uv);
    Out.color = color;
    Out.normal = float4(normalize(In.Normal) * 0.5 + 0.5, 1.0);
    Out.metallicRoughness = float4(0, 0.5, 0, 1);
    
    return Out;
}
