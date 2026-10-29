//================================
//
// GIのプローブの表示(エディターのデバッグビュー)
//
//   DDGIのプローブを、そのプローブの放射照度で照らした球としてシーンの色に描き込む。
//   1. ClearMain: 画素ごとの「一番手前の球」を空にする
//   2. SplatMain: プローブごとに画面の上の範囲で視線と球の交点を求め、シーンの深度より手前なら
//      「距離(上位17ビット)とプローブの番号(下位15ビット)」をInterlockedMinで残す
//   3. ShadeMain: 残ったプローブの球の法線の向きの放射照度を読み、シーンの色に重ねて書く
//   使えない(壁や物に埋まった)プローブは赤くする。
//
//================================

struct Constants
{
    uint SceneDepthIndex;   // シーンの深度(SRV)
    uint ProbeDepthIndex;   // 画素ごとの一番手前の球(UAV、R32_UINT)
    uint SceneColorIndex;   // シーンの色(SRV)
    uint OutputIndex;       // 書き込み先(UAV、シーンの色と同じ形式)
    uint IrradianceIndex;   // DDGIの放射照度(SRV)
    uint Width;
    uint Height;
    uint ProbeCount;
    float Radius;           // 球の半径(m)
    float Exposure;         // 前露出(放射照度は絶対の値なので掛ける)
    float MaxDistance;      // 距離をまとめるときの上限(m)
    float ProjectionScaleY; // 投影行列の_22(球の画面の上の大きさを求める)
    float BrightnessScale;  // 球の明るさを持ち上げる倍率(見え方だけ)
};
ConstantBuffer<Constants> g_constants : register(b0);

cbuffer cbFrame : register(b1)
{
    row_major float4x4 g_mViewProj;
    row_major float4x4 g_mInvViewProj;
    float4 g_cameraPosition;
    float4 g_ddgiOrigin; // xyz: 最初のプローブの位置、w: 間隔(m)
    int4 g_ddgiCounts;   // xyz: 各軸の数、w: テクスチャの1行に並べる数
};

SamplerState g_linearClamp : register(s1);

static const uint kEmpty = 0xFFFFFFFF;
static const uint kIndexBits = 15;
static const uint kIndexMask = (1u << kIndexBits) - 1u;
static const float kDistanceSteps = 131071.0; // 17ビット
static const int kIrradianceTexels = 6;       // inc_DDGI.hlsliと同じ
// 1つの球が画面の上で調べる範囲の上限(ピクセル)
static const int kMaxSplatSize = 256;
// 使えないプローブの色(前露出の後の明るさ)
static const float3 kInvalidColor = float3(0.5, 0.02, 0.02);
// 暗くても球の形が分かるよう、縁に足す薄い明かり(前露出の後の明るさ)
static const float kRimBrightness = 0.06;

float3 ProbePosition(uint probeIndex)
{
    const int3 counts = g_ddgiCounts.xyz;
    const int3 coord = int3(probeIndex % counts.x, (probeIndex / counts.x) % counts.y, probeIndex / (counts.x * counts.y));
    return g_ddgiOrigin.xyz + float3(coord) * g_ddgiOrigin.w;
}

// 画素の中心を通る視線の向き
float3 PixelRayDirection(uint2 pixel)
{
    const float2 uv = (float2(pixel) + 0.5) / float2(g_constants.Width, g_constants.Height);
    const float2 ndc = uv * float2(2.0, -2.0) + float2(-1.0, 1.0);
    float4 farPoint = mul(float4(ndc, 1.0, 1.0), g_mInvViewProj);
    farPoint /= farPoint.w;
    return normalize(farPoint.xyz - g_cameraPosition.xyz);
}

// 視線と球の交点までの距離。当たらなければ負
float IntersectSphere(float3 rayDirection, float3 center)
{
    const float3 offset = g_cameraPosition.xyz - center;
    const float b = dot(offset, rayDirection);
    const float c = dot(offset, offset) - g_constants.Radius * g_constants.Radius;
    const float h = b * b - c;
    if (h < 0.0)
    {
        return -1.0;
    }
    return -b - sqrt(h);
}

// 画素のシーンの深度までの距離
float SceneDistance(uint2 pixel)
{
    Texture2D<float> depthTex = ResourceDescriptorHeap[g_constants.SceneDepthIndex];
    const float depth = depthTex.Load(int3(pixel, 0));
    const float2 uv = (float2(pixel) + 0.5) / float2(g_constants.Width, g_constants.Height);
    const float2 ndc = uv * float2(2.0, -2.0) + float2(-1.0, 1.0);
    float4 worldPosition = mul(float4(ndc, depth, 1.0), g_mInvViewProj);
    if (abs(worldPosition.w) < 1e-6)
    {
        return 1e9;
    }
    return length(worldPosition.xyz / worldPosition.w - g_cameraPosition.xyz);
}

float2 DDGIOctahedralEncode(float3 direction)
{
    const float3 n = direction / (abs(direction.x) + abs(direction.y) + abs(direction.z));
    const float2 signNotZero = float2(n.x >= 0.0 ? 1.0 : -1.0, n.y >= 0.0 ? 1.0 : -1.0);
    return (n.z >= 0.0) ? n.xy : (1.0 - abs(n.yx)) * signNotZero;
}

// プローブの、ある向きのテクセルのUV(inc_DDGI.hlsliのDDGIProbeUVと同じ並び)
float2 ProbeUV(uint probeIndex, float3 direction, float2 textureSize)
{
    const int tileSize = kIrradianceTexels + 2;
    const int tilesPerRow = g_ddgiCounts.w;
    const int2 tile = int2(probeIndex % tilesPerRow, probeIndex / tilesPerRow);
    const float2 octahedral = DDGIOctahedralEncode(direction) * 0.5 + 0.5;
    const float2 pixel = float2(tile * tileSize) + 1.0 + octahedral * float(kIrradianceTexels);
    return pixel / textureSize;
}

[numthreads(8, 8, 1)]
void ClearMain(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_constants.Width || dispatchId.y >= g_constants.Height)
    {
        return;
    }
    RWTexture2D<uint> probeDepth = ResourceDescriptorHeap[g_constants.ProbeDepthIndex];
    probeDepth[dispatchId.xy] = kEmpty;
}

// 1つのスレッドグループが1つのプローブを受け持つ
[numthreads(8, 8, 1)]
void SplatMain(uint3 groupId : SV_GroupID, uint3 groupThreadId : SV_GroupThreadID)
{
    const uint probeIndex = groupId.x;
    if (probeIndex >= g_constants.ProbeCount)
    {
        return;
    }
    const float3 center = ProbePosition(probeIndex);
    const float4 clip = mul(float4(center, 1.0), g_mViewProj);
    // カメラの後ろや、カメラに重なる球は描かない
    if (clip.w <= g_constants.Radius)
    {
        return;
    }
    const float2 screenCenter = (clip.xy / clip.w * float2(0.5, -0.5) + 0.5) * float2(g_constants.Width, g_constants.Height);
    const float pixelRadius = g_constants.Radius / clip.w * g_constants.ProjectionScaleY * 0.5 * float(g_constants.Height) * 1.2 + 1.0;
    const int2 minPixel = max(int2(floor(screenCenter - pixelRadius)), int2(0, 0));
    const int2 maxPixel = min(int2(ceil(screenCenter + pixelRadius)), min(int2(g_constants.Width, g_constants.Height), minPixel + kMaxSplatSize));
    if (any(minPixel >= maxPixel))
    {
        return;
    }

    RWTexture2D<uint> probeDepth = ResourceDescriptorHeap[g_constants.ProbeDepthIndex];
    for (int y = minPixel.y + int(groupThreadId.y); y < maxPixel.y; y += 8)
    {
        for (int x = minPixel.x + int(groupThreadId.x); x < maxPixel.x; x += 8)
        {
            const uint2 pixel = uint2(x, y);
            const float hitDistance = IntersectSphere(PixelRayDirection(pixel), center);
            if (hitDistance <= 0.0 || hitDistance >= SceneDistance(pixel))
            {
                continue;
            }
            const uint packedDistance = uint(saturate(hitDistance / g_constants.MaxDistance) * kDistanceSteps);
            uint previous;
            InterlockedMin(probeDepth[pixel], (packedDistance << kIndexBits) | (probeIndex & kIndexMask), previous);
        }
    }
}

[numthreads(8, 8, 1)]
void ShadeMain(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_constants.Width || dispatchId.y >= g_constants.Height)
    {
        return;
    }
    Texture2D<uint> probeDepth = ResourceDescriptorHeap[g_constants.ProbeDepthIndex];
    Texture2D<float4> sceneColor = ResourceDescriptorHeap[g_constants.SceneColorIndex];
    RWTexture2D<float4> output = ResourceDescriptorHeap[g_constants.OutputIndex];

    float4 color = sceneColor.Load(int3(dispatchId.xy, 0));
    const uint packed = probeDepth.Load(int3(dispatchId.xy, 0));
    if (packed != kEmpty)
    {
        const uint probeIndex = packed & kIndexMask;
        const float3 center = ProbePosition(probeIndex);
        const float3 rayDirection = PixelRayDirection(dispatchId.xy);
        const float hitDistance = max(IntersectSphere(rayDirection, center), 0.0);
        const float3 normal = normalize(g_cameraPosition.xyz + rayDirection * hitDistance - center);

        Texture2D<float4> irradianceTex = ResourceDescriptorHeap[g_constants.IrradianceIndex];
        float2 textureSize;
        irradianceTex.GetDimensions(textureSize.x, textureSize.y);
        const float4 irradiance = irradianceTex.SampleLevel(g_linearClamp, ProbeUV(probeIndex, normal, textureSize), 0);
        // aはプローブが使えるか(壁や物の中に埋まったプローブは0)
        color.rgb = (irradiance.a > 0.5) ? irradiance.rgb * g_constants.Exposure * g_constants.BrightnessScale : kInvalidColor;
        const float rim = 1.0 - saturate(dot(normal, -rayDirection));
        color.rgb += kRimBrightness * rim * rim * rim;
    }
    output[dispatchId.xy] = color;
}
