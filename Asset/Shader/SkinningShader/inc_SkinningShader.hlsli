struct VSOutput
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
    float4 Color : COLOR;
    float3 wPos : TEXCOORD1;
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
    int g_alphaMode;
    int g_proceduralType; // 5: Dissolve from bottom (world Y)
    float g_dissolveAmount;
    int g_reflectionSlot;
    float3 g_dissolveEdgeColor; // HDR color of the glowing band just above the cut
    float g_dissolveHeight;     // world Y below which pixels are clipped
    float g_dissolveEdgeWidth;  // width of the glowing band / raggedness of the cut (m)
    float g_emissiveFrontScale; // emissive multiplier for front faces (matches MaterialConstants)
    float g_emissiveBackScale;  // emissive multiplier for back faces
};
