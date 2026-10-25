// DDGI(Dynamic Diffuse Global Illumination、Majercik et al. 2019)のプローブを読む。
// 描画(LitShader・SkinningShader)と、プローブの更新(レイが当たった所の跳ね返り)で共有する。
// 先にinc_LightData.hlsli(g_DDGI*の定数)を読み込んでおくこと
#ifndef INC_DDGI_HLSLI
#define INC_DDGI_HLSLI

// プローブ1つのテクセル数(内側。外周に1テクセルの縁を足して並べる)。C++のDDGIVolumeと合わせる
static const int kDDGIIrradianceTexels = 6;
static const int kDDGIDistanceTexels = 14;

float2 DDGISignNotZero(float2 v)
{
    return float2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0);
}

// 向きを八面体の展開の座標(-1〜1)へ
float2 DDGIOctahedralEncode(float3 direction)
{
    const float3 n = direction / (abs(direction.x) + abs(direction.y) + abs(direction.z));
    return (n.z >= 0.0) ? n.xy : (1.0 - abs(n.yx)) * DDGISignNotZero(n.xy);
}

// 八面体の展開の座標(-1〜1)から向きへ
float3 DDGIOctahedralDecode(float2 coord)
{
    float3 n = float3(coord, 1.0 - abs(coord.x) - abs(coord.y));
    if (n.z < 0.0)
    {
        n.xy = (1.0 - abs(n.yx)) * DDGISignNotZero(n.xy);
    }
    return normalize(n);
}

int3 DDGIProbeCoord(int probeIndex)
{
    const int3 counts = g_DDGICounts.xyz;
    return int3(probeIndex % counts.x, (probeIndex / counts.x) % counts.y, probeIndex / (counts.x * counts.y));
}

int DDGIProbeIndex(int3 coord)
{
    const int3 counts = g_DDGICounts.xyz;
    return coord.x + coord.y * counts.x + coord.z * counts.x * counts.y;
}

float3 DDGIProbePosition(int3 coord)
{
    return g_DDGIOrigin.xyz + float3(coord) * g_DDGIOrigin.w;
}

// プローブの、ある向きのテクセルのUV(縁を含めたタイルを、1行にg_DDGICounts.w個並べてある)
float2 DDGIProbeUV(int probeIndex, float3 direction, int interiorTexels, float2 textureSize)
{
    const int tileSize = interiorTexels + 2;
    const int tilesPerRow = g_DDGICounts.w;
    const int2 tile = int2(probeIndex % tilesPerRow, probeIndex / tilesPerRow);
    const float2 octahedral = DDGIOctahedralEncode(direction) * 0.5 + 0.5;
    const float2 pixel = float2(tile * tileSize) + 1.0 + octahedral * float(interiorTexels);
    return pixel / textureSize;
}

// 位置と法線での放射照度(を余弦で平均した放射輝度、E / π)。絶対の値(露出を掛ける前)。
// coverageはボリュームの中で1、外で0(呼ぶ側で他の環境光と混ぜる)。viewDirは表面からカメラへの向き
float3 SampleDDGIIrradiance(float3 position, float3 normal, float3 viewDir,
    Texture2D<float4> irradianceTex, Texture2D<float2> distanceTex, SamplerState linearSampler, out float coverage)
{
    coverage = 0.0;
    if (g_DDGIParams.x <= 0.0)
    {
        return 0.0;
    }
    const float spacing = g_DDGIOrigin.w;
    const int3 counts = g_DDGICounts.xyz;

    // ボリュームの外側の1つ分の間隔で、他の環境光へ滑らかに切り替える
    const float3 volumeMax = g_DDGIOrigin.xyz + float3(counts - 1) * spacing;
    const float3 outside = max(g_DDGIOrigin.xyz - position, position - volumeMax);
    coverage = saturate(1.0 - max(outside.x, max(outside.y, outside.z)) / spacing);
    if (coverage <= 0.0)
    {
        return 0.0;
    }

    // 壁の中のプローブを拾わないよう、法線とカメラの向きへ少しずらした位置で調べる
    const float3 biasedPosition = position + normal * g_DDGIParams.y + viewDir * g_DDGIParams.z;
    const float3 gridPosition = (biasedPosition - g_DDGIOrigin.xyz) / spacing;
    const int3 baseCoord = clamp(int3(floor(gridPosition)), int3(0, 0, 0), counts - 2);
    const float3 alpha = saturate(gridPosition - float3(baseCoord));

    float2 irradianceSize;
    float2 distanceSize;
    irradianceTex.GetDimensions(irradianceSize.x, irradianceSize.y);
    distanceTex.GetDimensions(distanceSize.x, distanceSize.y);

    float3 irradianceSum = 0.0;
    float weightSum = 0.0;
    [unroll]
    for (int i = 0; i < 8; ++i)
    {
        const int3 offset = int3(i, i >> 1, i >> 2) & 1;
        const int3 probeCoord = baseCoord + offset;
        const int probeIndex = DDGIProbeIndex(probeCoord);
        const float3 probePosition = DDGIProbePosition(probeCoord);

        // 表面の裏側にあるプローブは弱める(完全に0にすると、隙間で黒くなる)
        const float3 toProbe = normalize(probePosition - position);
        const float wrapShading = (dot(toProbe, normal) + 1.0) * 0.5;
        float weight = wrapShading * wrapShading + 0.2;

        // プローブから見て、この位置が遮られていないか(距離の平均と分散、チェビシェフの不等式)
        const float3 probeToPoint = biasedPosition - probePosition;
        const float distanceToProbe = length(probeToPoint);
        const float2 moments = distanceTex.SampleLevel(linearSampler, DDGIProbeUV(probeIndex, probeToPoint / max(distanceToProbe, 1e-4), kDDGIDistanceTexels, distanceSize), 0);
        const float variance = abs(moments.x * moments.x - moments.y);
        float chebyshev = 1.0;
        if (distanceToProbe > moments.x)
        {
            const float difference = distanceToProbe - moments.x;
            chebyshev = variance / (variance + difference * difference);
            chebyshev = chebyshev * chebyshev * chebyshev;
        }
        weight *= max(0.05, chebyshev);
        weight = max(1e-6, weight);
        // とても小さい重みはさらに小さくし、漏れを抑える
        const float kCrushThreshold = 0.2;
        if (weight < kCrushThreshold)
        {
            weight *= weight * weight / (kCrushThreshold * kCrushThreshold);
        }

        const float3 trilinear = lerp(1.0 - alpha, alpha, float3(offset));
        weight *= trilinear.x * trilinear.y * trilinear.z;

        const float4 irradiance = irradianceTex.SampleLevel(linearSampler, DDGIProbeUV(probeIndex, normal, kDDGIIrradianceTexels, irradianceSize), 0);
        // aはプローブが使えるか(壁や物の中に埋まったプローブは0)
        weight *= irradiance.a;
        irradianceSum += irradiance.rgb * weight;
        weightSum += weight;
    }
    if (weightSum <= 1e-5)
    {
        coverage = 0.0;
        return 0.0;
    }
    return irradianceSum / weightSum;
}

#endif
