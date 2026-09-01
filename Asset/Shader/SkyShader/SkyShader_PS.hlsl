#include "../Common/ShaderCore.hlsli"

Texture2D g_skyTex : register(t0);

struct PSInput
{
    float4 Pos : SV_Position;
    float2 UV : TEXCOORD0;
};

float4 main(PSInput In) : SV_Target0
{
    // Sample the sky texture
    float4 color = g_skyTex.Sample(g_ss_linear_wrap, In.UV);
    
    // Convert from sRGB to Linear space to fix gamma (brightness)
    color.rgb = pow(color.rgb, 2.2f);
    
    // Can add color tinting, rotation, or twinkling here later.
    return color;
}
