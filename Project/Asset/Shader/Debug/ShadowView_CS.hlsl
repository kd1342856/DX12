//================================
//
// 影の深度の可視化(シェーダーエディターの「影の中身」)
//
//   影用の深度テクスチャを、近いほど白い灰色にしてRGBA8に書く。
//   スポットと点光源は投影の深度を距離に戻し、届く範囲で割る(何も無い所は黒)。
//   点光源のキューブは6面を3×2に並べる(上の段 +X・-X・+Y、下の段 -Y・+Z・-Z)。
//
//================================

struct Constants
{
    uint SourceIndex;       // 影の深度(SRV)
    uint DestinationIndex;  // 書き込み先(UAV、RGBA8)
    uint DestinationWidth;
    uint DestinationHeight;
    uint Mode;              // 0: 月(2D)、1: スポット(2D配列の1枚)、2: 点光源(キューブ配列の1個)
    uint Layer;             // 2D配列の枚の番号、またはキューブの個の番号
    float NearZ;            // 投影の手前の面(m)
    float FarZ;             // 投影の奥の面(m)
};
ConstantBuffer<Constants> g_constants : register(b0);

SamplerState g_pointClamp : register(s0);

static const uint kModeDirectional = 0;
static const uint kModeSpot = 1;
static const uint kModeCube = 2;

// 透視投影の深度(0〜1)から、手前からの距離(m)へ
float DepthToViewZ(float depth)
{
    const float nearZ = g_constants.NearZ;
    const float farZ = g_constants.FarZ;
    return nearZ * farZ / max(farZ - depth * (farZ - nearZ), 1e-6);
}

// キューブの面の番号とUVから向きへ(D3Dのキューブの面の並び)
float3 CubeFaceDirection(uint face, float2 uv)
{
    const float s = uv.x * 2.0 - 1.0;
    const float t = uv.y * 2.0 - 1.0;
    switch (face)
    {
        case 0:  return float3(1.0, -t, -s);
        case 1:  return float3(-1.0, -t, s);
        case 2:  return float3(s, 1.0, t);
        case 3:  return float3(s, -1.0, -t);
        case 4:  return float3(s, -t, 1.0);
        default: return float3(-s, -t, -1.0);
    }
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_constants.DestinationWidth || dispatchId.y >= g_constants.DestinationHeight)
    {
        return;
    }
    RWTexture2D<float4> destination = ResourceDescriptorHeap[g_constants.DestinationIndex];
    const float2 uv = (float2(dispatchId.xy) + 0.5) / float2(g_constants.DestinationWidth, g_constants.DestinationHeight);

    float brightness = 0.0;
    if (g_constants.Mode == kModeDirectional)
    {
        // 平行投影なので深度はそのまま距離に比例する
        Texture2D<float> depthTex = ResourceDescriptorHeap[g_constants.SourceIndex];
        brightness = 1.0 - depthTex.SampleLevel(g_pointClamp, uv, 0);
    }
    else if (g_constants.Mode == kModeSpot)
    {
        Texture2DArray<float> depthTex = ResourceDescriptorHeap[g_constants.SourceIndex];
        const float depth = depthTex.SampleLevel(g_pointClamp, float3(uv, g_constants.Layer), 0);
        brightness = 1.0 - saturate(DepthToViewZ(depth) / g_constants.FarZ);
    }
    else
    {
        // 3×2に並べた面のどれか
        const uint2 cell = min(uint2(uv * float2(3.0, 2.0)), uint2(2, 1));
        const uint face = cell.y * 3 + cell.x;
        const float2 faceUV = frac(uv * float2(3.0, 2.0));
        TextureCubeArray<float> depthTex = ResourceDescriptorHeap[g_constants.SourceIndex];
        const float depth = depthTex.SampleLevel(g_pointClamp, float4(CubeFaceDirection(face, faceUV), g_constants.Layer), 0);
        brightness = 1.0 - saturate(DepthToViewZ(depth) / g_constants.FarZ);
        // 面の境目に細い線を引く
        const float2 edge = min(faceUV, 1.0 - faceUV) * float2(g_constants.DestinationWidth / 3.0, g_constants.DestinationHeight / 2.0);
        if (min(edge.x, edge.y) < 1.0)
        {
            destination[dispatchId.xy] = float4(0.25, 0.25, 0.3, 1.0);
            return;
        }
    }
    destination[dispatchId.xy] = float4(brightness.xxx, 1.0);
}
