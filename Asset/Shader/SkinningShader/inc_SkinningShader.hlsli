struct VSOutput
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
    float4 Color : COLOR;
};

struct PSOutput
{
    float4 color : SV_Target0;
    float4 normal : SV_Target1;
    float4 metallicRoughness : SV_Target2;
};

cbuffer cbCamera : register(b0)
{
    matrix c_mView; 
    matrix c_mInvView;
    matrix c_mProj;
    matrix c_mInvProj;
    matrix c_mVP;
    matrix c_mInvVP;
    float3 c_CamPos;
    float c_dummy;
}

cbuffer cbWorld : register(b1)
{
    matrix c_mWorld; 
}

cbuffer cbBones : register(b2)
{
    matrix c_mBones[256]; 
}
