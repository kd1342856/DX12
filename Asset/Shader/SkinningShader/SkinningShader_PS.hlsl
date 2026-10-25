#include "inc_SkinningShader.hlsli"

Texture2D g_diffuseTex : register(t0);
Texture2D g_normalTex : register(t1);
Texture2D g_RoughnessMetallicTex : register(t2);
Texture2D g_emissiveTex : register(t3);
Texture2D g_occlusionTex : register(t4);

SamplerState g_ss : register(s0);

float Hash2D(float2 p)
{
    return frac(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453);
}

float Noise2D(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    float2 u = f * f * (3.0 - 2.0 * f);
    return lerp(lerp(Hash2D(i), Hash2D(i + float2(1, 0)), u.x),
                lerp(Hash2D(i + float2(0, 1)), Hash2D(i + float2(1, 1)), u.x), u.y);
}

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

    // Dissolve from bottom: clip everything below a (slightly ragged) world-height cut line,
    // and glow in a thin band just above it, so the body looks like it burns away upward.
    if (g_proceduralType == 5)
    {
        float2 np = float2(In.wPos.x + In.wPos.z * 0.7, In.wPos.z - In.wPos.x * 0.7);
        float n = Noise2D(np * 9.0) * 0.6 + Noise2D(np * 23.0 + 7.1) * 0.4;
        float cut = g_dissolveHeight + (n - 0.5) * g_dissolveEdgeWidth * 1.5;
        float above = In.wPos.y - cut;
        clip(above);
        float edge = 1.0 - saturate(above / max(g_dissolveEdgeWidth, 0.001));
        color.rgb += g_dissolveEdgeColor * edge * edge;
    }

    Out.color = color;

    return Out;
}
