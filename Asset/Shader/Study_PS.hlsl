#include "inc_Study.hlsli"
Texture2D g_diffuseTex : register(t0);

Texture2D g_normalTex : register(t1);
Texture2D g_RoughnessMetallicTex : register(t2);
Texture2D g_emissiveTex : register(t3);

SamplerState g_ss : register(s0);

float4 main(VSOutput In) : SV_TARGET
{
    float4 color = g_diffuseTex.Sample(g_ss, In.uv);
    
    return color;
}