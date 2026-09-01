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
};

cbuffer cbCamera : register(b0)
{
    row_major matrix c_mView; 
    row_major matrix c_mInvView;
    row_major matrix c_mProj;
    row_major matrix c_mInvProj;
    row_major matrix c_mVP;
    row_major matrix c_mInvVP;
    float3 c_CamPos;
    float c_dummy;
}

cbuffer cbWorld : register(b1)
{
    row_major matrix c_mWorld; 
}

cbuffer cbBones : register(b2)
{
    row_major matrix c_mBones[256]; 
}

cbuffer cbPerMaterial : register(b3)
{
    float4 g_baseColorFactor;
    float g_metallicFactor;
    float g_roughnessFactor;
    float g_normalScale;
    float g_occlusionStrength;
    float g_emissiveStrength;
    float3 g_emissiveFactor;
};
