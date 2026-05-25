Texture2D g_tex : register(t0);
SamplerState g_ss : register(s0);

cbuffer cbPostProcess : register(b0)
{
    float g_Exposure;
};

struct PSInput
{
    float4 Pos : SV_Position;
    float2 UV : TEXCOORD0;
};
