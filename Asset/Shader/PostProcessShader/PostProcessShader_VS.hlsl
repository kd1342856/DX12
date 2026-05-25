#include "PostProcessShader.hlsli"

PSInput VS(uint vI : SV_VertexID)
{
    PSInput Out;
    Out.UV = float2((vI << 1) & 2, vI & 2);
    Out.Pos = float4(Out.UV * float2(2, -2) + float2(-1, 1), 0, 1);
    return Out;
}
