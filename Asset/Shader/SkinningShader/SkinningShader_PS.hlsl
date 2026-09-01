#include "inc_SkinningShader.hlsli"

Texture2D g_diffuseTex : register(t0);
Texture2D g_normalTex : register(t1);
Texture2D g_RoughnessMetallicTex : register(t2);
Texture2D g_emissiveTex : register(t3);
Texture2D g_occlusionTex : register(t4);

SamplerState g_ss : register(s0);

PSOutput main(VSOutput In)
{
    PSOutput Out = (PSOutput)0;
    
    float4 color = g_diffuseTex.Sample(g_ss, In.uv) * g_baseColorFactor;
    
    float4 mr = g_RoughnessMetallicTex.Sample(g_ss, In.uv);
    float metallic = mr.b * g_metallicFactor;
    float roughness = mr.g * g_roughnessFactor;
    
    // Very simple output for now (just pass through color, normal, and mr)
    // Wait, let's also apply occlusion directly to base color as a simple fallback
    float occ = lerp(1.0, g_occlusionTex.Sample(g_ss, In.uv).r, g_occlusionStrength);
    color.rgb *= occ;

    // Emissive
    float3 emissive = g_emissiveTex.Sample(g_ss, In.uv).rgb * g_emissiveFactor * g_emissiveStrength;
    color.rgb += emissive;

    Out.color = color;
    
    return Out;
}
