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
    matrix c_mView; 
    matrix c_mProj;
}

cbuffer cbWorld : register(b1)
{
    matrix c_mWorld; 
}

cbuffer cbBones : register(b2)
{
    matrix c_mBones[256]; 
}
