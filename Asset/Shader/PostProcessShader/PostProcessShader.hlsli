Texture2D g_tex : register(t0);
Texture2D g_texBloom : register(t1);
SamplerState g_ss : register(s0);

cbuffer cbPostProcess : register(b0)
{
    float g_Exposure;
    float g_BloomThreshold;
    float g_BloomIntensity;
    float g_BlurDirectionX;
    float g_BlurDirectionY;
    float g_Gamma;
    uint g_EnableHDR;
    float g_Pad;
};

struct PSInput
{
    float4 Pos : SV_Position;
    float2 UV : TEXCOORD0;
};
