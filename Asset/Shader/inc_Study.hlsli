struct VSOutput
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
    float4 Color : COLOR;
};

cbuffer cbCamera : register(b0)
{
    row_major matrix c_mView;
    row_major matrix c_mProj;
}

cbuffer cbWorld : register(b1)
{
    row_major matrix c_mWorld;
}