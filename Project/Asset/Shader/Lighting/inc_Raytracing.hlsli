// インラインのレイトレース(DXR 1.1)の共通部分。DDGIのプローブの更新とリファレンスのパストレーサーで使う。
// 先にinc_LightData.hlsli(ライトの定数)を読み込み、リニア・ラップのサンプラーをg_linearWrap(s2)として宣言しておくこと
#ifndef INC_RAYTRACING_HLSLI
#define INC_RAYTRACING_HLSLI

// C++のRaytracingScene::GeometryInfoと同じ並び
struct GeometryInfo
{
    uint VertexBufferIndex;
    uint IndexBufferIndex;
    uint BaseColorTextureIndex;
    uint EmissiveTextureIndex;
    float4 BaseColorFactor;
    float3 Emissive;
    float Pad;
};

static const uint kInvalidIndex = 0xFFFFFFFF;
// MeshVertexの並び(位置12、UV8、法線12 ...)
static const uint kVertexStride = 72;
static const uint kUVOffset = 12;
static const uint kNormalOffset = 20;
static const float kRaytracingPi = 3.14159265;

// レイが当たった所の表面
struct HitSurface
{
    float3 position;
    float3 normal;   // ワールドの法線(頂点の法線を補間したもの)
    float3 albedo;
    float3 emissive; // 発光の輝度(nit、絶対の値)
    float distance;
};

// 当たった三角形から、位置・法線・色を求める。引数はRayQueryのCommitted〜の値。textureMipは色を読むミップ(粗いほど速い)
HitSurface GetHitSurface(StructuredBuffer<GeometryInfo> geometries, uint instanceId, uint primitiveIndex, float2 barycentrics,
    float hitDistance, float3x4 objectToWorld, float3 origin, float3 direction, float textureMip)
{
    HitSurface surface;
    surface.distance = hitDistance;
    surface.position = origin + direction * surface.distance;

    const GeometryInfo geometry = geometries[instanceId];
    ByteAddressBuffer indexBuffer = ResourceDescriptorHeap[geometry.IndexBufferIndex];
    ByteAddressBuffer vertexBuffer = ResourceDescriptorHeap[geometry.VertexBufferIndex];
    const uint3 indices = indexBuffer.Load3(primitiveIndex * 12);
    const float3 weights = float3(1.0 - barycentrics.x - barycentrics.y, barycentrics.x, barycentrics.y);

    float3 localNormal = 0.0;
    float2 uv = 0.0;
    [unroll]
    for (int i = 0; i < 3; ++i)
    {
        const uint vertexOffset = indices[i] * kVertexStride;
        localNormal += asfloat(vertexBuffer.Load3(vertexOffset + kNormalOffset)) * weights[i];
        uv += asfloat(vertexBuffer.Load2(vertexOffset + kUVOffset)) * weights[i];
    }
    surface.normal = normalize(mul(objectToWorld, float4(localNormal, 0.0)));

    surface.albedo = geometry.BaseColorFactor.rgb;
    if (geometry.BaseColorTextureIndex != kInvalidIndex)
    {
        Texture2D<float4> baseColorTex = ResourceDescriptorHeap[geometry.BaseColorTextureIndex];
        surface.albedo *= baseColorTex.SampleLevel(g_linearWrap, uv, textureMip).rgb;
    }
    surface.emissive = geometry.Emissive;
    if (geometry.EmissiveTextureIndex != kInvalidIndex && any(surface.emissive > 0.0))
    {
        Texture2D<float4> emissiveTex = ResourceDescriptorHeap[geometry.EmissiveTextureIndex];
        surface.emissive *= emissiveTex.SampleLevel(g_linearWrap, uv, textureMip).rgb;
    }
    return surface;
}

// 2点の間がさえぎられていないか(シャドウレイ)
bool IsVisible(RaytracingAccelerationStructure scene, float3 origin, float3 direction, float maxDistance)
{
    RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_FORCE_OPAQUE> query;
    RayDesc ray;
    ray.Origin = origin;
    ray.TMin = 0.0;
    ray.Direction = direction;
    ray.TMax = maxDistance;
    query.TraceRayInline(scene, RAY_FLAG_NONE, 0xFF, ray);
    query.Proceed();
    return query.CommittedStatus() == COMMITTED_NOTHING;
}

// 表面に届く直接光の照度(拡散の計算に使う。前露出)。影はシャドウレイで調べる
float3 EvaluateDirectIrradiance(RaytracingAccelerationStructure scene, float3 position, float3 normal)
{
    const float3 origin = position + normal * 0.02;
    float3 irradiance = 0.0;

    // 平行光(月)
    const float3 toMoon = -g_DL_Dir;
    const float moonNdotL = saturate(dot(normal, toMoon));
    if (moonNdotL > 0.0 && any(g_DL_Color > 0.0) && IsVisible(scene, origin, toMoon, 1000.0))
    {
        irradiance += g_DL_Color * moonNdotL;
    }

    // スポットライト
    for (int i = 0; i < g_SL_Count; ++i)
    {
        const SpotLight light = g_SL[i];
        const float3 toLight = light.Pos - position;
        const float distanceToLight = length(toLight);
        if (distanceToLight >= light.Range)
        {
            continue;
        }
        const float3 L = toLight / max(distanceToLight, 1e-4);
        const float NdotL = saturate(dot(normal, L));
        const float cone = saturate((dot(-L, normalize(light.Dir)) - light.OuterCorn) / (light.InnerCorn - light.OuterCorn));
        const float3 illuminance = light.Color * (DistanceAttenuation(distanceToLight, light.Range) * cone * NdotL);
        if (any(illuminance > 0.0) && IsVisible(scene, origin, L, max(distanceToLight - 0.05, 0.0)))
        {
            irradiance += illuminance;
        }
    }

    // 点光源
    for (int j = 0; j < g_PL_Count; ++j)
    {
        const PointLight light = g_PL[j];
        const float3 toLight = light.Pos - position;
        const float distanceToLight = length(toLight);
        if (distanceToLight >= light.Range)
        {
            continue;
        }
        const float3 L = toLight / max(distanceToLight, 1e-4);
        const float NdotL = saturate(dot(normal, L));
        const float3 illuminance = light.Color * (DistanceAttenuation(distanceToLight, light.Range) * NdotL);
        if (any(illuminance > 0.0) && IsVisible(scene, origin, L, max(distanceToLight - 0.05, 0.0)))
        {
            irradiance += illuminance;
        }
    }
    return irradiance;
}

#endif
