//================================
//
// リファレンスのパストレーサー(開発用。リアルタイムである必要はない)
//
//   カメラから1ピクセルにつき1本ずつ光線を飛ばし、拡散の反射を繰り返して(最大g_constants.MaxBounces回)
//   当たった所の発光と直接光(シャドウレイ、次のイベント推定)を集める。カメラが止まっている間は
//   毎フレームの結果を足し続けて平均を取り、ノイズの無い正解の絵に近づけていく。
//   ラスタライズの描画(DDGI・反射プローブ・影)の正しさを、エンジンの中ですぐに見比べるために使う。
//   鏡面の反射は入れていない(拡散だけの正解)。スキニングのモデルと半透明は加速構造に無いので写らない。
//   値は前露出(ライトの定数バッファと同じ)。
//
//================================
#include "../Common/inc_LightData.hlsli"

struct Constants
{
    uint TLASIndex;
    uint GeometryIndex;
    uint AccumulationUAVIndex; // 足し合わせた結果(RWTexture2D<float4>、RGBA32F)
    uint OutputUAVIndex;       // 平均(RWTexture2D<float4>)
    uint Width;
    uint Height;
    uint SampleIndex;          // 足し始めてから何フレーム目か(0で足し直し)
    uint MaxBounces;
    float SkyAverage;          // 空のテクスチャの平均の明るさ(空の輝度の何倍か。DDGIと同じ)
};
ConstantBuffer<Constants> g_constants : register(b0);

cbuffer cbPathTrace : register(b1)
{
    row_major float4x4 g_InvViewProj; // ジッターを含まないビュー×投影の逆
    float3 g_CameraPos;
    float g_PathTracePad;
};

SamplerState g_linearWrap : register(s2);

#include "inc_Raytracing.hlsli"

// 乱数(PCG)
uint PcgHash(uint value)
{
    const uint state = value * 747796405u + 2891336453u;
    const uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float NextRandom(inout uint state)
{
    state = PcgHash(state);
    return float(state) / 4294967296.0;
}

// 法線のまわりの、余弦に比例した向き(拡散の反射の重点的な選び方。pdf = cosθ / π)
float3 SampleCosineHemisphere(float3 normal, float u1, float u2)
{
    const float radius = sqrt(u1);
    const float phi = 2.0 * kRaytracingPi * u2;
    const float3 tangentX = normalize(cross(abs(normal.y) < 0.999 ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0), normal));
    const float3 tangentY = cross(normal, tangentX);
    return normalize(tangentX * (radius * cos(phi)) + tangentY * (radius * sin(phi)) + normal * sqrt(max(1.0 - u1, 0.0)));
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_constants.Width || dispatchId.y >= g_constants.Height)
    {
        return;
    }
    RaytracingAccelerationStructure scene = ResourceDescriptorHeap[g_constants.TLASIndex];
    StructuredBuffer<GeometryInfo> geometries = ResourceDescriptorHeap[g_constants.GeometryIndex];
    RWTexture2D<float4> accumulationTex = ResourceDescriptorHeap[g_constants.AccumulationUAVIndex];
    RWTexture2D<float4> outputTex = ResourceDescriptorHeap[g_constants.OutputUAVIndex];

    uint randomState = PcgHash(dispatchId.x + dispatchId.y * g_constants.Width + PcgHash(g_constants.SampleIndex) * 9781u);

    // ピクセルの中のランダムな位置を通る、カメラからの光線
    const float2 uv = (float2(dispatchId.xy) + float2(NextRandom(randomState), NextRandom(randomState))) / float2(g_constants.Width, g_constants.Height);
    const float2 ndc = uv * float2(2.0, -2.0) + float2(-1.0, 1.0);
    const float4 farPoint = mul(float4(ndc, 1.0, 1.0), g_InvViewProj);
    float3 origin = g_CameraPos;
    float3 direction = normalize(farPoint.xyz / farPoint.w - g_CameraPos);

    float3 radiance = 0.0;
    float3 throughput = 1.0;
    for (uint bounce = 0; bounce <= g_constants.MaxBounces; ++bounce)
    {
        RayQuery<RAY_FLAG_FORCE_OPAQUE> query;
        RayDesc ray;
        ray.Origin = origin;
        ray.TMin = 0.0;
        ray.Direction = direction;
        ray.TMax = 1000.0;
        query.TraceRayInline(scene, RAY_FLAG_NONE, 0xFF, ray);
        query.Proceed();

        if (query.CommittedStatus() != COMMITTED_TRIANGLE_HIT)
        {
            // 空(前露出)
            radiance += throughput * g_SkyLuminance * g_constants.SkyAverage;
            break;
        }
        HitSurface hit = GetHitSurface(geometries, query.CommittedInstanceID(), query.CommittedPrimitiveIndex(), query.CommittedTriangleBarycentrics(),
            query.CommittedRayT(), query.CommittedObjectToWorld3x4(), origin, direction, 0.0);
        // 描画と同じく両面とも表として扱う
        if (dot(hit.normal, direction) > 0.0)
        {
            hit.normal = -hit.normal;
        }

        // 発光(絶対の値なので露出を掛ける)と、直接光(ライトはシャドウレイで調べる)
        radiance += throughput * hit.emissive * g_Exposure;
        radiance += throughput * hit.albedo / kRaytracingPi * EvaluateDirectIrradiance(scene, hit.position, hit.normal);

        // 次の拡散の反射。余弦で選ぶので、重みはアルベドだけになる
        throughput *= hit.albedo;
        if (bounce >= 2)
        {
            // ロシアンルーレット: 弱くなった光線は確率で打ち切り、残ったものを強める(偏りは出ない)
            const float survival = saturate(max(throughput.r, max(throughput.g, throughput.b)));
            if (NextRandom(randomState) >= survival)
            {
                break;
            }
            throughput /= max(survival, 1e-4);
        }
        origin = hit.position + hit.normal * 0.002;
        direction = SampleCosineHemisphere(hit.normal, NextRandom(randomState), NextRandom(randomState));
    }

    // 壊れた値(NaNなど)は足さない
    if (any(isnan(radiance)) || any(isinf(radiance)))
    {
        radiance = 0.0;
    }
    const float4 accumulated = (g_constants.SampleIndex == 0) ? float4(radiance, 1.0) : accumulationTex[dispatchId.xy] + float4(radiance, 1.0);
    accumulationTex[dispatchId.xy] = accumulated;
    outputTex[dispatchId.xy] = float4(accumulated.rgb / max(accumulated.a, 1.0), 1.0);
}
