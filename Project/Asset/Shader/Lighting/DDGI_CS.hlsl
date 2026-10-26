//================================
//
// DDGIのプローブの更新(インラインのレイトレース、DXR 1.1)
//
//   1. TraceRays: プローブごとにkRaysPerProbe本のレイを全方向へ飛ばし、当たった所の明るさ
//      (発光 + 直接光(影はシャドウレイ) + 前のフレームのプローブからの跳ね返り)と距離を記録する
//   2. UpdateIrradiance / UpdateDistance: レイの結果を八面体に展開したテクセルへ集め、前の値と混ぜる
//   3. UpdateBorder: テクセルを補間で読めるよう、タイルの外周の縁を反対側の内側から写す
//   値は絶対の明るさ(露出を掛ける前)で持つ。ライトの定数バッファは前露出なので、最後に露出で割る。
//
//================================
#include "../Common/inc_LightData.hlsli"
#include "../Common/inc_DDGI.hlsli"

struct Constants
{
    uint TLASIndex;          // TLAS
    uint GeometryIndex;      // インスタンスごとの頂点・マテリアル(StructuredBuffer<GeometryInfo>)
    uint RayDataIndex;       // レイの結果(RWTexture2D<float4>、横がレイ、縦がプローブ。UAV)
    uint IrradianceUAVIndex; // 放射照度(UAV)
    uint DistanceUAVIndex;   // 距離(UAV)
    uint IrradianceSRVIndex; // 放射照度(SRV。跳ね返りを求めるときに前のフレームの値を読む)
    uint DistanceSRVIndex;   // 距離(SRV)
    uint RaysPerProbe;
    uint ProbeCount;
    float Hysteresis;        // 前の値を残す割合
    uint ResetHistory;       // 1なら前の値を使わない
    uint BorderTargetIndex;  // UpdateBorder: 縁を写すテクスチャ(UAV)
    uint BorderInterior;     // UpdateBorder: タイルの内側のテクセル数
    float MaxRayDistance;    // 距離に記録する上限(m)
    float SkyAverage;        // 空のテクスチャの平均の明るさ(空の輝度の何倍か)
};
ConstantBuffer<Constants> g_constants : register(b0);

cbuffer cbDDGIFrame : register(b1)
{
    // レイの向きをフレームごとにランダムに回す(同じ向きばかり調べて縞にならないように)
    row_major float4x4 g_RayRotation;
};

SamplerState g_linearClamp : register(s1);
SamplerState g_linearWrap : register(s2);

#include "inc_Raytracing.hlsli"

static const float kPi = 3.14159265;

// レイの向き(球面フィボナッチ格子をフレームごとに回したもの)
float3 RayDirection(uint rayIndex, uint rayCount)
{
    const float goldenRatio = 1.6180339887;
    const float phi = 2.0 * kPi * frac(float(rayIndex) * (goldenRatio - 1.0));
    const float cosTheta = 1.0 - (2.0 * float(rayIndex) + 1.0) / float(rayCount);
    const float sinTheta = sqrt(saturate(1.0 - cosTheta * cosTheta));
    const float3 direction = float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
    return normalize(mul(float4(direction, 0.0), g_RayRotation).xyz);
}

//--------------------------------
// 1. レイを飛ばす。1グループが1プローブ、1スレッドが1本
//--------------------------------
[numthreads(64, 1, 1)]
void TraceRays(uint3 groupId : SV_GroupID, uint3 groupThreadId : SV_GroupThreadID)
{
    const uint probeIndex = groupId.x;
    const uint rayIndex = groupThreadId.x;
    if (rayIndex >= g_constants.RaysPerProbe || probeIndex >= g_constants.ProbeCount)
    {
        return;
    }
    RaytracingAccelerationStructure scene = ResourceDescriptorHeap[g_constants.TLASIndex];
    StructuredBuffer<GeometryInfo> geometries = ResourceDescriptorHeap[g_constants.GeometryIndex];
    RWTexture2D<float4> rayData = ResourceDescriptorHeap[g_constants.RayDataIndex];

    const float3 probePosition = DDGIProbePosition(DDGIProbeCoord(probeIndex));
    const float3 direction = RayDirection(rayIndex, g_constants.RaysPerProbe);
    const float exposure = max(g_Exposure, 1e-20);

    RayQuery<RAY_FLAG_FORCE_OPAQUE> query;
    RayDesc ray;
    ray.Origin = probePosition;
    ray.TMin = 0.0;
    ray.Direction = direction;
    ray.TMax = 1000.0;
    query.TraceRayInline(scene, RAY_FLAG_NONE, 0xFF, ray);
    query.Proceed();

    if (query.CommittedStatus() != COMMITTED_TRIANGLE_HIT)
    {
        // 何にも当たらなければ空(窓の外)。空の輝度は前露出なので割り戻す
        rayData[uint2(rayIndex, probeIndex)] = float4((g_SkyLuminance * g_constants.SkyAverage / exposure).xxx, g_constants.MaxRayDistance);
        return;
    }

    const HitSurface hit = GetHitSurface(geometries, query.CommittedInstanceID(), query.CommittedPrimitiveIndex(), query.CommittedTriangleBarycentrics(),
        query.CommittedRayT(), query.CommittedObjectToWorld3x4(), probePosition, direction, 3.0);
    const float hitDistance = hit.distance;
    const float3 normal = hit.normal;

    // 裏側に当たった(壁や物の中にあるプローブ)。距離を負にして印を付け、明るさは0
    if (dot(normal, direction) > 0.0)
    {
        rayData[uint2(rayIndex, probeIndex)] = float4(0.0, 0.0, 0.0, -hitDistance * 0.2);
        return;
    }
    const float3 albedo = hit.albedo;
    const float3 emissive = hit.emissive;

    const float3 hitPosition = probePosition + direction * hitDistance;
    // 直接光(前露出)を絶対の値へ
    const float3 direct = EvaluateDirectIrradiance(scene, hitPosition, normal) / exposure;

    // 前のフレームのプローブからの跳ね返り(何度も跳ね返った光が、フレームを重ねて積もる)
    Texture2D<float4> irradianceTex = ResourceDescriptorHeap[g_constants.IrradianceSRVIndex];
    Texture2D<float2> distanceTex = ResourceDescriptorHeap[g_constants.DistanceSRVIndex];
    float coverage;
    const float3 bounce = SampleDDGIIrradiance(hitPosition, normal, -direction, irradianceTex, distanceTex, g_linearClamp, coverage) * coverage;

    const float3 radiance = emissive + albedo / kPi * direct + albedo * bounce;
    rayData[uint2(rayIndex, probeIndex)] = float4(radiance, hitDistance);
}

// プローブのタイルの、内側のテクセルの位置
uint2 DDGITexelPosition(uint probeIndex, uint2 texel, uint interiorTexels)
{
    const uint tileSize = interiorTexels + 2;
    const uint tilesPerRow = uint(g_DDGICounts.w);
    return uint2(probeIndex % tilesPerRow, probeIndex / tilesPerRow) * tileSize + 1 + texel;
}

//--------------------------------
// 2. 放射照度。1グループが1プローブ、1スレッドが1テクセル
//--------------------------------
[numthreads(kDDGIIrradianceTexels, kDDGIIrradianceTexels, 1)]
void UpdateIrradiance(uint3 groupId : SV_GroupID, uint3 groupThreadId : SV_GroupThreadID)
{
    const uint probeIndex = groupId.x;
    if (probeIndex >= g_constants.ProbeCount)
    {
        return;
    }
    RWTexture2D<float4> rayData = ResourceDescriptorHeap[g_constants.RayDataIndex];
    RWTexture2D<float4> irradianceTex = ResourceDescriptorHeap[g_constants.IrradianceUAVIndex];

    const float3 texelDirection = DDGIOctahedralDecode((float2(groupThreadId.xy) + 0.5) / float(kDDGIIrradianceTexels) * 2.0 - 1.0);
    float3 radianceSum = 0.0;
    float weightSum = 0.0;
    uint backfaceCount = 0;
    for (uint rayIndex = 0; rayIndex < g_constants.RaysPerProbe; ++rayIndex)
    {
        const float4 data = rayData[uint2(rayIndex, probeIndex)];
        if (data.a < 0.0)
        {
            ++backfaceCount;
        }
        const float weight = saturate(dot(texelDirection, RayDirection(rayIndex, g_constants.RaysPerProbe)));
        radianceSum += data.rgb * weight;
        weightSum += weight;
    }
    const float3 irradiance = (weightSum > 0.0) ? radianceSum / weightSum : 0.0;
    // レイの4分の1以上が裏側に当たるプローブは、壁や物の中にあるので使わない
    const float validity = (backfaceCount * 4 < g_constants.RaysPerProbe) ? 1.0 : 0.0;

    const uint2 pixel = DDGITexelPosition(probeIndex, groupThreadId.xy, kDDGIIrradianceTexels);
    const float hysteresis = (g_constants.ResetHistory != 0) ? 0.0 : g_constants.Hysteresis;
    const float3 previous = irradianceTex[pixel].rgb;
    irradianceTex[pixel] = float4(lerp(irradiance, previous, hysteresis), validity);
}

//--------------------------------
// 2. 距離(平均と2乗の平均)。1グループが1プローブ、1スレッドが1テクセル
//--------------------------------
[numthreads(kDDGIDistanceTexels, kDDGIDistanceTexels, 1)]
void UpdateDistance(uint3 groupId : SV_GroupID, uint3 groupThreadId : SV_GroupThreadID)
{
    const uint probeIndex = groupId.x;
    if (probeIndex >= g_constants.ProbeCount)
    {
        return;
    }
    RWTexture2D<float4> rayData = ResourceDescriptorHeap[g_constants.RayDataIndex];
    RWTexture2D<float2> distanceTex = ResourceDescriptorHeap[g_constants.DistanceUAVIndex];

    const float3 texelDirection = DDGIOctahedralDecode((float2(groupThreadId.xy) + 0.5) / float(kDDGIDistanceTexels) * 2.0 - 1.0);
    float2 momentSum = 0.0;
    float weightSum = 0.0;
    for (uint rayIndex = 0; rayIndex < g_constants.RaysPerProbe; ++rayIndex)
    {
        const float hitDistance = min(abs(rayData[uint2(rayIndex, probeIndex)].a), g_constants.MaxRayDistance);
        // 向きが近いレイだけを強く効かせる(距離はくっきりさせたいので、放射照度よりずっと鋭い重み)
        const float weight = pow(saturate(dot(texelDirection, RayDirection(rayIndex, g_constants.RaysPerProbe))), 50.0);
        momentSum += float2(hitDistance, hitDistance * hitDistance) * weight;
        weightSum += weight;
    }
    const float2 moments = (weightSum > 0.0) ? momentSum / weightSum : float2(g_constants.MaxRayDistance, g_constants.MaxRayDistance * g_constants.MaxRayDistance);

    const uint2 pixel = DDGITexelPosition(probeIndex, groupThreadId.xy, kDDGIDistanceTexels);
    const float hysteresis = (g_constants.ResetHistory != 0) ? 0.0 : g_constants.Hysteresis;
    distanceTex[pixel] = lerp(moments, distanceTex[pixel], hysteresis);
}

//--------------------------------
// 3. タイルの縁。八面体の展開で隣り合う、反対側の内側のテクセルを写す
//--------------------------------
[numthreads(16, 16, 1)]
void UpdateBorder(uint3 groupId : SV_GroupID, uint3 groupThreadId : SV_GroupThreadID)
{
    const uint probeIndex = groupId.x;
    const uint tileSize = g_constants.BorderInterior + 2;
    const uint2 texel = groupThreadId.xy;
    if (probeIndex >= g_constants.ProbeCount || texel.x >= tileSize || texel.y >= tileSize)
    {
        return;
    }
    const uint last = tileSize - 1;
    const bool isBorderX = (texel.x == 0 || texel.x == last);
    const bool isBorderY = (texel.y == 0 || texel.y == last);
    if (!isBorderX && !isBorderY)
    {
        return;
    }
    uint2 source;
    if (isBorderX && isBorderY)
    {
        // 角は対角の内側の角
        source = uint2(texel.x == 0 ? last - 1 : 1, texel.y == 0 ? last - 1 : 1);
    }
    else if (isBorderY)
    {
        // 上下の辺は、左右を反転した内側の行
        source = uint2(last - texel.x, texel.y == 0 ? 1 : last - 1);
    }
    else
    {
        // 左右の辺は、上下を反転した内側の列
        source = uint2(texel.x == 0 ? 1 : last - 1, last - texel.y);
    }
    RWTexture2D<float4> target = ResourceDescriptorHeap[g_constants.BorderTargetIndex];
    const uint tilesPerRow = uint(g_DDGICounts.w);
    const uint2 tileOrigin = uint2(probeIndex % tilesPerRow, probeIndex / tilesPerRow) * tileSize;
    target[tileOrigin + texel] = target[tileOrigin + source];
}
