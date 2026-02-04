#include "inc_Study.hlsli"

VSOutput main(float4 pos : POSITION, float2 uv : TEXCOORD, float3 normal: NORMAL, float4 color: COLOR, float3 tangent: TANGENT)
{
    VSOutput Out;
    
    Out.pos = mul(pos, c_mView);
    Out.pos = mul(Out.pos, c_mProj);
    Out.uv = uv;
    
    Out.Color = color;
    Out.Normal = normal;
    Out.Tangent = tangent;
    
    return Out;
}