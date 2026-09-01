#include "../Common/ShaderCore.hlsli"

struct VSOutput
{
    float4 Pos : SV_Position;
    float2 UV : TEXCOORD0;
};

VSOutput main(float3 pos : POSITION, float2 uv : TEXCOORD0)
{
    VSOutput Out;
    
    // We want the sky sphere to always be centered around the camera so it appears infinitely far away.
    // However, our sphere is radius 400. We can just render it with the standard world matrix,
    // assuming its position is set to the camera position in the application, OR we can remove the translation
    // from the view matrix here. Since we will just snap the sphere position to camera position in RenderSystem, 
    // standard mWVP is fine.
    
    float4x4 mVP = mul(g_mV, g_mP);
    float4x4 mWVP = mul(g_mW, mVP);
    
    Out.Pos = mul(float4(pos, 1.0f), mWVP);
    
    // Trick to always render at maximum depth (Z = 1.0 after perspective divide)
    // D3D12 depth is 0 to 1.
    Out.Pos.z = Out.Pos.w;
    
    // Mirror the texture horizontally to make it perfectly seamless
    Out.UV = uv;
    Out.UV.x = (uv.x < 0.5f) ? (uv.x * 2.0f) : (2.0f - uv.x * 2.0f);
    
    return Out;
}
