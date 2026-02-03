#include "inc_Study.hlsli"
Texture2D<float4> g_inputTex : register(t0);

SamplerState g_ss : register(s0);

float4 main(VSOutput In) : SV_TARGET
{
    float4 color = g_inputTex.Sample(g_ss, In.uv);
    
    return color;
}