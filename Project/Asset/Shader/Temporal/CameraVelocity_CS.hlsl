//================================
//
// カメラの動きによる速度
//
//   シーンの深度からワールドの位置を戻し、今と前のフレームの(ジッターを含まない)行列で
//   画面に写したUVの差を速度として書く。動く物はこの後にVelocityShaderで上書きする。
//
//================================

struct Constants
{
    uint DepthIndex;    // シーンの深度(SRV)
    uint VelocityIndex; // 速度(UAV)
    uint Width;
    uint Height;
};
ConstantBuffer<Constants> g_constants : register(b0);

cbuffer cbFrame : register(b1)
{
    row_major float4x4 g_mInvJitteredVP; // 深度を描いたときの(ジッターを含む)ビュー×投影の逆行列
    row_major float4x4 g_mCurrVP;
    row_major float4x4 g_mPrevVP;
};

float2 ClipToUV(float4 clipPos)
{
    return (clipPos.xy / clipPos.w) * float2(0.5, -0.5) + 0.5;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_constants.Width || dispatchId.y >= g_constants.Height)
    {
        return;
    }
    Texture2D<float> depthTex = ResourceDescriptorHeap[g_constants.DepthIndex];
    RWTexture2D<float2> velocityTex = ResourceDescriptorHeap[g_constants.VelocityIndex];

    const float depth = depthTex.Load(int3(dispatchId.xy, 0));
    const float2 uv = (float2(dispatchId.xy) + 0.5) / float2(g_constants.Width, g_constants.Height);
    const float2 ndc = uv * float2(2.0, -2.0) + float2(-1.0, 1.0);

    float4 worldPos = mul(float4(ndc, depth, 1.0), g_mInvJitteredVP);
    worldPos /= worldPos.w;

    const float2 currUV = ClipToUV(mul(worldPos, g_mCurrVP));
    const float2 prevUV = ClipToUV(mul(worldPos, g_mPrevVP));
    velocityTex[dispatchId.xy] = currUV - prevUV;
}
