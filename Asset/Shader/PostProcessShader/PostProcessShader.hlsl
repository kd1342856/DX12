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

PSInput VS(uint vI : SV_VertexID)
{
    PSInput Out;
    Out.UV = float2((vI << 1) & 2, vI & 2);
    Out.Pos = float4(Out.UV * float2(2, -2) + float2(-1, 1), 0, 1);
    return Out;
}

// ACES Filmic ToneMapping
float3 ACESFilmicToneMapping(float3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float4 PS(PSInput In) : SV_Target0
{
    float4 texColor = g_tex.Sample(g_ss, In.UV);
    texColor.rgb = ACESFilmicToneMapping(texColor.rgb * g_Exposure);
    return texColor;
}