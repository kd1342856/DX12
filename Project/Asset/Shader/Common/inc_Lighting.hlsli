// 直接光と環境光の計算。LitShaderとSkinningShaderで共有する。
// 先にinc_LightData.hlsli(ライトの定数バッファ)とinc_PBRCommon.hlsli(BRDF)を読み込んでおくこと
#ifndef INC_LIGHTING_HLSLI
#define INC_LIGHTING_HLSLI

// ライトの計算に使う表面の情報
struct SurfaceData
{
    float3 position;      // ワールド座標
    float3 normal;        // ワールド法線(正規化済み)
    float3 viewDir;       // 表面からカメラへの向き(正規化済み)
    float  NdotV;
    float3 diffuseColor;  // 拡散の色(金属ほど0)
    float3 specularColor; // 鏡面反射の色(F0)
    float  roughness;     // 知覚的なラフネス(拡散のBurleyに使う)
    float  alpha;         // GGXのα(ラフネスの2乗)
    // 以下はInitializeSurfaceBRDF()で埋める
    float2 environmentBRDF;    // 環境BRDFの(A, B)。環境光の鏡面 = F0 × A + B
    float3 energyCompensation; // マルチスキャッタのエネルギー補償。鏡面に掛ける
};

//------------------------------------------
// 環境BRDF(スプリットサム近似のLUT。C++のEnvironmentBRDFが作る)
//------------------------------------------
Texture2D<float2> g_environmentBRDF : register(t21);

// LUTが無いとき(作る前・コンピュートに対応していないGPU)の解析的な近似(Karis, UE4 Mobile)
float2 AnalyticEnvironmentBRDF(float perceptualRoughness, float NdotV)
{
    const float4 kEnvC0 = float4(-1.0, -0.0275, -0.572, 0.022);
    const float4 kEnvC1 = float4(1.0, 0.0425, 1.04, -0.04);
    const float4 envR = perceptualRoughness * kEnvC0 + kEnvC1;
    const float envA004 = min(envR.x * envR.x, exp2(-9.28 * NdotV)) * envR.x + envR.y;
    return float2(-1.04, 1.04) * envA004 + envR.zw;
}

// LUTを読む。サンプラーを増やさないよう、4テクセルを読んで自分で補間する
// (テクセルの中心がちょうど0と1になる並び。EnvironmentBRDF_CS.hlslと同じ)
float2 SampleEnvironmentBRDF(float perceptualRoughness, float NdotV)
{
    float width, height;
    g_environmentBRDF.GetDimensions(width, height);
    const float2 position = saturate(float2(NdotV, perceptualRoughness)) * (float2(width, height) - 1.0);
    const int2 base = min(int2(position), int2(width, height) - 2);
    const float2 t = position - float2(base);
    const float2 v00 = g_environmentBRDF.Load(int3(base, 0));
    const float2 v10 = g_environmentBRDF.Load(int3(base + int2(1, 0), 0));
    const float2 v01 = g_environmentBRDF.Load(int3(base + int2(0, 1), 0));
    const float2 v11 = g_environmentBRDF.Load(int3(base + int2(1, 1), 0));
    return lerp(lerp(v00, v10, t.x), lerp(v01, v11, t.x), t.y);
}

// 表面の環境BRDFとエネルギー補償を求める。SurfaceDataの他の値を埋めた後に呼ぶ
void InitializeSurfaceBRDF(inout SurfaceData surface)
{
    // Geometric Specular AAで広げたαに合わせる
    const float perceptualRoughness = sqrt(surface.alpha);
    float2 environmentBRDF = SampleEnvironmentBRDF(perceptualRoughness, surface.NdotV);
    if (environmentBRDF.x + environmentBRDF.y <= 0.0)
    {
        environmentBRDF = AnalyticEnvironmentBRDF(perceptualRoughness, surface.NdotV);
    }
    surface.environmentBRDF = environmentBRDF;
    // 1回の反射で返るエネルギーはA + B(F0 = 1)。足りない分は、何度も反射して最後には出てくる光なので、
    // F0の色を付けて足し戻す(Fdez-Aguera 2019、Filamentと同じ)。ラフな金属が暗くなりすぎるのを防ぐ
    surface.energyCompensation = 1.0 + surface.specularColor * (1.0 / max(environmentBRDF.x + environmentBRDF.y, 1e-3) - 1.0);
}

// Geometric Specular AA(Tokuyoshi & Kaplanyan 2019)。画面上で法線が急に変わる所(細かい凹凸や遠くの曲面)は、
// 1ピクセルの中にいろいろな向きの法線が含まれる。その広がりの分だけαを大きくし、ハイライトのちらつきを抑える
float ApplyGeometricSpecularAA(float alpha, float3 normal)
{
    const float3 normalDdx = ddx(normal);
    const float3 normalDdy = ddy(normal);
    const float variance = 0.25 * (dot(normalDdx, normalDdx) + dot(normalDdy, normalDdy));
    const float kernelRoughness2 = min(2.0 * variance, 0.18);
    return sqrt(saturate(alpha * alpha + kernelRoughness2));
}

// 1つの光源からの直接光(拡散+鏡面)。lightDirは表面から光源への向き、illuminanceは表面での照度(×露出)
float3 EvaluateDirectLight(SurfaceData surface, float3 lightDir, float3 illuminance)
{
    const float NdotL = saturate(dot(surface.normal, lightDir));
    const float3 H = normalize(lightDir + surface.viewDir);
    const float LdotH = saturate(dot(lightDir, H));
    const float NdotH = saturate(dot(surface.normal, H));
    const float3 diffuse = Diffuse_Burley(surface.diffuseColor, NdotL, surface.NdotV, LdotH, surface.roughness);
    const float3 specular = Specular_BRDF(surface.alpha, surface.specularColor, surface.NdotV, NdotL, LdotH, NdotH) * surface.energyCompensation;
    return NdotL * illuminance * (diffuse + specular);
}

// 大きさのある球の光源からの直接光(拡散+鏡面)。toLightは表面から光源の中心への(正規化していない)ベクトル。
// 鏡面は代表点法(Karis 2013): 反射ベクトルに最も近い光源の球の上の点へ向けて評価し、ハイライトを光源の大きさに広げる。
// 広げた分だけ明るくなりすぎないよう、光源の見込み角の分だけ広げたαとの比(α/α')^2で正規化する
float3 EvaluateSphereLight(SurfaceData surface, float3 toLight, float sourceRadius, float3 illuminance)
{
    const float distanceToLight = max(length(toLight), 0.0001);
    const float3 L = toLight / distanceToLight;

    // 拡散は光源の中心の向きで評価する
    const float NdotL = saturate(dot(surface.normal, L));
    const float3 H = normalize(L + surface.viewDir);
    const float LdotH = saturate(dot(L, H));
    const float3 diffuse = Diffuse_Burley(surface.diffuseColor, NdotL, surface.NdotV, LdotH, surface.roughness);

    // 鏡面は代表点の向きで評価する
    const float3 R = reflect(-surface.viewDir, surface.normal);
    const float3 centerToRay = dot(toLight, R) * R - toLight;
    const float3 closestPoint = toLight + centerToRay * saturate(sourceRadius / max(length(centerToRay), 0.0001));
    const float3 specularL = normalize(closestPoint);
    const float specularNdotL = saturate(dot(surface.normal, specularL));
    const float3 specularH = normalize(specularL + surface.viewDir);
    const float specularLdotH = saturate(dot(specularL, specularH));
    const float specularNdotH = saturate(dot(surface.normal, specularH));
    const float widenedAlpha = saturate(surface.alpha + sourceRadius / (2.0 * distanceToLight));
    const float normalization = (surface.alpha * surface.alpha) / max(widenedAlpha * widenedAlpha, 1e-6);
    const float3 specular = Specular_BRDF(surface.alpha, surface.specularColor, surface.NdotV, specularNdotL, specularLdotH, specularNdotH) * normalization * surface.energyCompensation;

    return illuminance * (NdotL * diffuse + specularNdotL * specular);
}

// スポットライト(影は含まない。EvaluateSpotLightShadowを掛ける)。light.Colorは光度(cd)×露出
float3 EvaluateSpotLight(SurfaceData surface, SpotLight light)
{
    float3 L = light.Pos - surface.position;
    const float distanceToLight = length(L);
    L = L / max(distanceToLight, 0.0001);
    const float attenuation = DistanceAttenuation(distanceToLight, light.Range);
    const float cosAngle = dot(-L, normalize(light.Dir));
    const float coneAttenuation = saturate((cosAngle - light.OuterCorn) / (light.InnerCorn - light.OuterCorn));
    return EvaluateSphereLight(surface, light.Pos - surface.position, light.SourceRadius, light.Color * (attenuation * coneAttenuation));
}

// 点光源(影は含まない。EvaluatePointLightShadowを掛ける)。light.Colorは光束(lm)を光度(cd)に直したもの×露出
float3 EvaluatePointLight(SurfaceData surface, PointLight light)
{
    float3 L = light.Pos - surface.position;
    const float distanceToLight = length(L);
    L = L / max(distanceToLight, 0.0001);
    const float attenuation = DistanceAttenuation(distanceToLight, light.Range);
    return EvaluateSphereLight(surface, light.Pos - surface.position, light.SourceRadius, light.Color * attenuation);
}

//------------------------------------------
// DDGI(C++のDDGIVolumeが更新する拡散の環境光のプローブ)
//------------------------------------------
#include "inc_DDGI.hlsli"
Texture2D<float4> g_ddgiIrradiance : register(t23);
Texture2D<float2> g_ddgiDistance : register(t24);

//------------------------------------------
// レイトレースの影と反射(ピクセルシェーダーがシェーダーモデル6.6のときだけ)
//------------------------------------------
#if (__SHADER_TARGET_MAJOR > 6) || (__SHADER_TARGET_MAJOR == 6 && __SHADER_TARGET_MINOR >= 6)
#define LIGHTING_HAS_RAYTRACING 1
// 読み込む側でg_linearWrap(リニア・ラップのサンプラー)を定義しておくこと
#include "../Lighting/inc_Raytracing.hlsli"
#endif

static const uint kRaytracedShadowFlag = 1;
static const uint kRaytracedReflectionFlag = 2;
// レイトレースの反射を使う、知覚的なラフネスの上限(これより粗い面は反射プローブだけ)
static const float kRaytracedReflectionMaxRoughness = 0.35;

bool IsRaytracedShadowEnabled()
{
#ifdef LIGHTING_HAS_RAYTRACING
    return g_RaytracingTLASIndex >= 0 && (g_RaytracingFlags & kRaytracedShadowFlag) != 0;
#else
    return false;
#endif
}

// ピクセルとフレームごとに散らばった0〜1の値を2つ(Interleaved Gradient Noise)
float2 RaytracingNoise(float2 screenPos)
{
    const float3 magic = float3(0.06711056, 0.00583715, 52.9829189);
    const float2 offset = 5.588238 * float(g_RaytracingFrame % 64);
    return float2(frac(magic.z * frac(dot(screenPos + offset, magic.xy))),
                  frac(magic.z * frac(dot(screenPos.yx + offset * 1.7 + 13.0, magic.xy))));
}

// 光源の球へ向かうシャドウレイ。球の見かけの円板から毎フレーム1点ずつ選ぶので、TAAで平均すると柔らかい影になる
float TraceShadowToPoint(float3 position, float3 normal, float3 lightPos, float sourceRadius, float2 screenPos)
{
#ifdef LIGHTING_HAS_RAYTRACING
    RaytracingAccelerationStructure scene = ResourceDescriptorHeap[g_RaytracingTLASIndex];
    const float3 toLight = lightPos - position;
    const float3 direction = toLight / max(length(toLight), 1e-4);
    const float3 axisA = normalize(cross(direction, abs(direction.y) < 0.99 ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0)));
    const float3 axisB = cross(direction, axisA);
    const float2 noise = RaytracingNoise(screenPos);
    const float radius = sourceRadius * sqrt(noise.x);
    const float angle = noise.y * 6.28318530718;
    const float3 target = lightPos + (axisA * cos(angle) + axisB * sin(angle)) * radius;
    const float3 origin = position + normal * 0.01;
    const float3 toTarget = target - origin;
    const float distanceToTarget = length(toTarget);
    // 光源のすぐまわり(電球や芯のメッシュ)では止める(シャドウマップの手前の面と同じ5cm強)
    return IsVisible(scene, origin, toTarget / max(distanceToTarget, 1e-4), max(distanceToTarget - 0.06, 0.0)) ? 1.0 : 0.0;
#else
    return 1.0;
#endif
}

// 平行光(月)へのシャドウレイ。月の見かけの大きさ(半径約0.26度)の中で向きを散らす
float TraceShadowToDirection(float3 position, float3 normal, float3 toLight, float2 screenPos)
{
#ifdef LIGHTING_HAS_RAYTRACING
    RaytracingAccelerationStructure scene = ResourceDescriptorHeap[g_RaytracingTLASIndex];
    const float kTanAngularRadius = 0.0045;
    const float3 axisA = normalize(cross(toLight, abs(toLight.y) < 0.99 ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0)));
    const float3 axisB = cross(toLight, axisA);
    const float2 noise = RaytracingNoise(screenPos);
    const float radius = kTanAngularRadius * sqrt(noise.x);
    const float angle = noise.y * 6.28318530718;
    const float3 direction = normalize(toLight + (axisA * cos(angle) + axisB * sin(angle)) * radius);
    return IsVisible(scene, position + normal * 0.01, direction, 1000.0) ? 1.0 : 0.0;
#else
    return 1.0;
#endif
}

// レイトレースの反射。滑らかな面(知覚的なラフネスがkRaytracedReflectionMaxRoughnessより小さい)だけ、
// GGXの広がりの中で反射の向きを毎フレーム1本選んで飛ばし、当たった所をDDGIの環境光と発光で照らす。
// 粗くなるほど、反射プローブなどの値(fallbackRadiance)へ混ぜて戻す
float3 TraceReflection(SurfaceData surface, float3 fallbackRadiance, SamplerState linearSampler, float2 screenPos)
{
#ifdef LIGHTING_HAS_RAYTRACING
    if (g_RaytracingTLASIndex < 0 || (g_RaytracingFlags & kRaytracedReflectionFlag) == 0)
    {
        return fallbackRadiance;
    }
    const float perceptualRoughness = sqrt(surface.alpha);
    const float weight = saturate(1.0 - perceptualRoughness / kRaytracedReflectionMaxRoughness);
    if (weight <= 0.0)
    {
        return fallbackRadiance;
    }

    // GGXの法線分布でハーフベクトルを選び、反射の向きにする
    const float2 noise = RaytracingNoise(screenPos);
    const float alpha = max(surface.alpha, 1e-3);
    const float phi = 6.28318530718 * noise.x;
    const float cosTheta = sqrt((1.0 - noise.y) / (1.0 + (alpha * alpha - 1.0) * noise.y));
    const float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    const float3 N = surface.normal;
    const float3 tangentX = normalize(cross(abs(N.y) < 0.999 ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0), N));
    const float3 tangentY = cross(N, tangentX);
    const float3 H = tangentX * (sinTheta * cos(phi)) + tangentY * (sinTheta * sin(phi)) + N * cosTheta;
    float3 direction = reflect(-surface.viewDir, H);
    if (dot(direction, N) <= 0.0)
    {
        direction = reflect(-surface.viewDir, N);
    }

    RaytracingAccelerationStructure scene = ResourceDescriptorHeap[g_RaytracingTLASIndex];
    StructuredBuffer<GeometryInfo> geometries = ResourceDescriptorHeap[g_RaytracingGeometryIndex];
    const float3 origin = surface.position + N * 0.01;
    RayQuery<RAY_FLAG_FORCE_OPAQUE> query;
    RayDesc ray;
    ray.Origin = origin;
    ray.TMin = 0.0;
    ray.Direction = direction;
    ray.TMax = 50.0;
    query.TraceRayInline(scene, RAY_FLAG_NONE, 0xFF, ray);
    query.Proceed();
    if (query.CommittedStatus() != COMMITTED_TRIANGLE_HIT)
    {
        return fallbackRadiance;
    }
    HitSurface hit = GetHitSurface(geometries, query.CommittedInstanceID(), query.CommittedPrimitiveIndex(), query.CommittedTriangleBarycentrics(),
        query.CommittedRayT(), query.CommittedObjectToWorld3x4(), origin, direction, 2.0);
    if (dot(hit.normal, direction) > 0.0)
    {
        hit.normal = -hit.normal;
    }
    // 当たった所の拡散: DDGIのプローブ(直接光も含む)。家の外は空の明るさ
    float coverage;
    const float3 irradiance = SampleDDGIIrradiance(hit.position, hit.normal, -direction, g_ddgiIrradiance, g_ddgiDistance, linearSampler, coverage);
    const float3 hitRadiance = hit.emissive * g_Exposure + hit.albedo * lerp(g_AmbientLight, irradiance * g_Exposure, coverage);
    return lerp(fallbackRadiance, hitRadiance, weight);
#else
    return fallbackRadiance;
#endif
}

//------------------------------------------
// 反射プローブ(C++のReflectionProbesが部屋ごとに撮る)
//------------------------------------------
TextureCubeArray g_reflectionProbes : register(t22);

// 箱投影(Lagarde 2012)。反射の向きの光線が部屋の箱にぶつかる点を、撮った位置から見た向きにする。
// 無限遠の景色として読むと、部屋の中の物の映り込みの位置がずれるため
float3 BoxProjectedDirection(float3 position, float3 direction, float3 boxMin, float3 boxMax, float3 capturePosition)
{
    const float3 firstPlane = (boxMax - position) / direction;
    const float3 secondPlane = (boxMin - position) / direction;
    const float3 furthestPlane = max(firstPlane, secondPlane);
    const float distanceToBox = min(furthestPlane.x, min(furthestPlane.y, furthestPlane.z));
    return position + direction * distanceToBox - capturePosition;
}

// プローブの重み。箱の中ほど1で、箱の縁のkProbeFadeDistance(m)で0に落とす(部屋の境目で混ぜる)
static const float kProbeFadeDistance = 0.3;
float ReflectionProbeWeight(float3 position, float3 boxMin, float3 boxMax)
{
    const float3 distanceToFace = min(position - boxMin, boxMax - position);
    return saturate(min(distanceToFace.x, min(distanceToFace.y, distanceToFace.z)) / kProbeFadeDistance);
}

// 反射プローブから鏡面の環境光(放射輝度)を読む。aは覆えた割合(0なら、どのプローブの中でもない)
float4 SampleReflectionProbes(SurfaceData surface, SamplerState probeSampler)
{
    const float3 R = reflect(-surface.viewDir, surface.normal);
    const float mip = sqrt(surface.alpha) * (g_ProbeMipCount - 1.0);
    float3 radiance = 0.0;
    float weightSum = 0.0;
    for (int i = 0; i < g_ProbeCount; ++i)
    {
        const float exposureRatio = g_ProbeCapturePosition[i].w;
        if (exposureRatio <= 0.0)
        {
            continue;
        }
        const float weight = ReflectionProbeWeight(surface.position, g_ProbeBoxMin[i].xyz, g_ProbeBoxMax[i].xyz);
        if (weight <= 0.0)
        {
            continue;
        }
        const float3 direction = BoxProjectedDirection(surface.position, R, g_ProbeBoxMin[i].xyz, g_ProbeBoxMax[i].xyz, g_ProbeCapturePosition[i].xyz);
        radiance += g_reflectionProbes.SampleLevel(probeSampler, float4(direction, i), mip).rgb * (exposureRatio * weight);
        weightSum += weight;
    }
    // 重なった部屋(出入り口)では重みで平均する。どの部屋にも入りきらない分は呼ぶ側で補う
    if (weightSum > 1.0)
    {
        radiance /= weightSum;
        weightSum = 1.0;
    }
    return float4(radiance, weightSum);
}

// 鏡面の環境光。反射プローブから読み、どの部屋のプローブにも入らないところは
// fallbackRadiance(一様な空の明るさ、前露出のnit)を使う
float3 EvaluateEnvironmentSpecular(SurfaceData surface, float specularOcclusion, float3 fallbackRadiance, SamplerState probeSampler, float2 screenPos)
{
    const float4 probe = SampleReflectionProbes(surface, probeSampler);
    // 滑らかな面は、レイトレースの反射で置き換える(画質「最高」)
    const float3 radiance = TraceReflection(surface, probe.rgb + fallbackRadiance * (1.0 - probe.a), probeSampler, screenPos);

    // 環境BRDF(スプリットサム近似)。粗さとフレネルで反射量を決める
    // (粗いほど正面の反射が弱まり、浅い角度ほど強まる)。マルチスキャッタの分も足す
    const float3 envBRDF = (surface.specularColor * surface.environmentBRDF.x + surface.environmentBRDF.y) * surface.energyCompensation;
    return radiance * envBRDF * specularOcclusion;
}

//------------------------------------------
// 局所光源(スポット・点光源)の影
//------------------------------------------
// 読み込む側で g_ss_comparison(s15) と g_ss_point_clamp(s5) を宣言しておくこと
Texture2DArray g_spotShadowMaps : register(t13);   // 狭いスポット(1枚に1灯)
TextureCubeArray g_cubeShadowMaps : register(t14); // 点光源と広いスポット(6面で1灯)

// C++のLocalShadowManagerと合わせる
static const float kShadowNear = 0.05;
static const float kSpotShadowFovMargin = 0.0349066; // 2度

static const float2 kShadowDisk[16] = {
    float2(-0.94201624, -0.39906216), float2(0.94558609, -0.76890725),
    float2(-0.094184101, -0.92938870), float2(0.34495938, 0.29387760),
    float2(-0.91588581, 0.45771432), float2(-0.81544232, -0.87912464),
    float2(-0.38277543, 0.27676845), float2(0.97484398, 0.75648379),
    float2(0.44323325, -0.97511554), float2(0.53742981, -0.47373420),
    float2(-0.26496911, -0.41893023), float2(0.79197514, 0.19090188),
    float2(-0.24188840, 0.99706507), float2(-0.81409955, 0.91437590),
    float2(0.19984126, 0.78641367), float2(0.14383161, -0.14100790)
};

// サンプルの並びをピクセルごとに回し、少ないサンプル数でも縞にならないようにする
float2x2 ShadowSampleRotation(float2 screenPos)
{
    const float3 magic = float3(0.06711056, 0.00583715, 52.9829189);
    const float angle = frac(magic.z * frac(dot(screenPos, magic.xy))) * 6.28318530718;
    const float s = sin(angle);
    const float c = cos(angle);
    return float2x2(c, -s, s, c);
}

// 透視投影の深度(0..1)を、光源からの距離(視点空間のZ)に戻す
float ShadowDepthToViewZ(float depthNdc, float nearZ, float farZ)
{
    return nearZ * farZ / max(farZ - depthNdc * (farZ - nearZ), 1e-6);
}

// 深度の比較に使うバイアス(視点空間の長さ)。テクセル1つ分の大きさを基本に、
// 面が光に対して斜めなほど、またPCFでぼかす範囲が広いほど大きくする
float ShadowWorldBias(float texelWorldSize, float filterWorldRadius, float NdotL, float biasScale)
{
    const float cosTheta = max(NdotL, 0.05);
    const float tanTheta = sqrt(1.0 - cosTheta * cosTheta) / cosTheta;
    return (texelWorldSize * 1.5 + filterWorldRadius * min(tanTheta, 4.0)) * biasScale;
}

// スポットの影(2D配列)。PCSS(Fernando 2005):
//   1) 光源の円板と受け手を結ぶ範囲で、受け手より手前にある遮蔽物の平均の距離を求める
//   2) 半影の幅 = 光源の直径 × (受け手の距離 - 遮蔽物の距離) / 遮蔽物の距離
//   3) その幅でPCFする
float SampleSpotShadow(SpotLight light, float3 worldPos, float NdotL, float2 screenPos)
{
    const float4 lightClip = mul(float4(worldPos, 1.0), light.mLightVP);
    if (lightClip.w <= kShadowNear)
    {
        return 1.0;
    }
    const float3 ndc = lightClip.xyz / lightClip.w;
    if (abs(ndc.x) > 1.0 || abs(ndc.y) > 1.0 || ndc.z > 1.0)
    {
        return 1.0;
    }
    const float2 uv = ndc.xy * float2(0.5, -0.5) + 0.5;
    const float slice = light.ShadowIndex;
    const float nearZ = kShadowNear;
    const float farZ = max(light.Range, kShadowNear * 2.0);
    const float receiverZ = lightClip.w; // 透視投影(LH)ではwが光源からの視点空間のZ

    float2 texSize;
    float elements;
    g_spotShadowMaps.GetDimensions(texSize.x, texSize.y, elements);
    const float tanHalfFov = tan(acos(light.OuterCorn) + kSpotShadowFovMargin);
    // 受け手の深さで、1mがシャドウマップのUVでいくつになるか
    const float uvPerMeter = 1.0 / (2.0 * receiverZ * tanHalfFov);
    const float texelWorldSize = 1.0 / (uvPerMeter * texSize.x);
    // 視点空間の長さを、受け手の深さでの深度(0..1)の差に直す係数
    const float ndcPerMeter = nearZ * farZ / ((farZ - nearZ) * receiverZ * receiverZ);
    const float lightRadius = max(light.SourceRadius, 0.001);
    const float2x2 rotation = ShadowSampleRotation(screenPos);

    // 1) 遮蔽物の探索。光源の円板が手前の面(near)に作る範囲を、受け手の位置から見た広さ
    const float searchRadiusUV = clamp(lightRadius * (receiverZ - nearZ) / receiverZ / (2.0 * nearZ * tanHalfFov), 2.0 / texSize.x, 0.05);
    const float searchBias = ShadowWorldBias(texelWorldSize, searchRadiusUV / uvPerMeter, NdotL, light.ShadowBias) * ndcPerMeter;
    float blockerSum = 0.0;
    float blockerCount = 0.0;
    [unroll]
    for (int searchIndex = 0; searchIndex < 16; searchIndex++)
    {
        const float2 sampleUV = uv + mul(kShadowDisk[searchIndex], rotation) * searchRadiusUV;
        const int2 texel = clamp(int2(sampleUV * texSize), int2(0, 0), int2(texSize) - 1);
        const float depthNdc = g_spotShadowMaps.Load(int4(texel, slice, 0)).r;
        if (depthNdc < ndc.z - searchBias)
        {
            blockerSum += ShadowDepthToViewZ(depthNdc, nearZ, farZ);
            blockerCount += 1.0;
        }
    }
    if (blockerCount <= 0.0)
    {
        return 1.0;
    }
    const float blockerZ = blockerSum / blockerCount;

    // 2) 半影の幅(受け手の位置での長さ)
    const float penumbraWorld = 2.0 * lightRadius * (receiverZ - blockerZ) / max(blockerZ, nearZ);
    const float filterRadiusUV = clamp(penumbraWorld * 0.5 * uvPerMeter, 1.5 / texSize.x, 0.03);

    // 3) PCF
    const float compareBias = ShadowWorldBias(texelWorldSize, filterRadiusUV / uvPerMeter, NdotL, light.ShadowBias) * ndcPerMeter;
    float lit = 0.0;
    [unroll]
    for (int filterIndex = 0; filterIndex < 16; filterIndex++)
    {
        const float2 sampleUV = uv + mul(kShadowDisk[filterIndex], rotation) * filterRadiusUV;
        lit += g_spotShadowMaps.SampleCmpLevelZero(g_ss_comparison, float3(sampleUV, slice), ndc.z - compareBias);
    }
    return lit / 16.0;
}

// 点光源(と広いスポット)の影(キューブ配列)。PCSSの考え方は2Dと同じで、ずらす量を方向の角度(tan)で扱う
float SampleCubeShadow(float3 lightPos, float range, float cubeIndex, float sourceRadius, float biasScale,
    float3 worldPos, float NdotL, float2 screenPos)
{
    const float3 toPixel = worldPos - lightPos;
    const float distanceToLight = length(toPixel);
    if (distanceToLight <= kShadowNear)
    {
        return 1.0;
    }
    const float3 direction = toPixel / distanceToLight;
    const float nearZ = kShadowNear;
    const float farZ = max(range, 1.0); // LocalShadowManager::GetCubeFarと同じ
    // 面の視点空間のZは、方向の主軸の成分
    const float3 absToPixel = abs(toPixel);
    const float receiverZ = max(absToPixel.x, max(absToPixel.y, absToPixel.z));
    const float receiverNdc = farZ / (farZ - nearZ) - nearZ * farZ / ((farZ - nearZ) * receiverZ);

    float faceSize;
    float faceHeight;
    float elements;
    g_cubeShadowMaps.GetDimensions(faceSize, faceHeight, elements);
    // 90度の面なので、受け手の深さでのテクセル1つの大きさは 2Z/解像度
    const float texelWorldSize = 2.0 * receiverZ / faceSize;
    const float texelAngle = 2.0 / faceSize;
    const float ndcPerMeter = nearZ * farZ / ((farZ - nearZ) * receiverZ * receiverZ);
    const float lightRadius = max(sourceRadius, 0.001);

    // 方向をずらすための、方向に垂直な2軸
    const float3 axisA = normalize(cross(direction, abs(direction.y) < 0.99 ? float3(0, 1, 0) : float3(1, 0, 0)));
    const float3 axisB = cross(direction, axisA);
    const float2x2 rotation = ShadowSampleRotation(screenPos);

    // 1) 遮蔽物の探索。受け手との距離の1/4より近い遮蔽物までを対象にする
    const float searchAngle = clamp(lightRadius * 3.0 / distanceToLight, texelAngle * 2.0, 0.1);
    const float searchBias = ShadowWorldBias(texelWorldSize, searchAngle * receiverZ, NdotL, biasScale) * ndcPerMeter;
    float blockerSum = 0.0;
    float blockerCount = 0.0;
    [unroll]
    for (int searchIndex = 0; searchIndex < 16; searchIndex++)
    {
        const float2 offset = mul(kShadowDisk[searchIndex], rotation) * searchAngle;
        const float3 sampleDir = direction + axisA * offset.x + axisB * offset.y;
        const float depthNdc = g_cubeShadowMaps.SampleLevel(g_ss_point_clamp, float4(sampleDir, cubeIndex), 0).r;
        if (depthNdc < receiverNdc - searchBias)
        {
            blockerSum += ShadowDepthToViewZ(depthNdc, nearZ, farZ);
            blockerCount += 1.0;
        }
    }
    if (blockerCount <= 0.0)
    {
        return 1.0;
    }
    const float blockerZ = blockerSum / blockerCount;

    // 2) 半影の幅を、光源から見た角度にする
    const float penumbraWorld = 2.0 * lightRadius * (receiverZ - blockerZ) / max(blockerZ, nearZ);
    const float filterAngle = clamp(penumbraWorld * 0.5 / receiverZ, texelAngle * 1.5, 0.06);

    // 3) PCF
    const float compareBias = ShadowWorldBias(texelWorldSize, filterAngle * receiverZ, NdotL, biasScale) * ndcPerMeter;
    float lit = 0.0;
    [unroll]
    for (int filterIndex = 0; filterIndex < 16; filterIndex++)
    {
        const float2 offset = mul(kShadowDisk[filterIndex], rotation) * filterAngle;
        const float3 sampleDir = direction + axisA * offset.x + axisB * offset.y;
        lit += g_cubeShadowMaps.SampleCmpLevelZero(g_ss_comparison, float4(sampleDir, cubeIndex), receiverNdc - compareBias);
    }
    return lit / 16.0;
}

//------------------------------------------
// コンタクトシャドウ
//------------------------------------------
// 深度プリパスの深度(メインの視点)。反射などの描画ではg_ContactShadowLengthが0なので読まない
Texture2D<float> g_contactShadowDepth : register(t25);

// 表面から光源の方向へ、画面の深度を短い距離だけたどり、何かの裏に入ったら影にする。
// シャドウマップの解像度では拾えない、物と床の接するところや細かい凹凸の影を足す。
// viewProjはメインの描画の(ジッターを含む)ビュー×投影、projectionZは投影行列の(_33, _43)
float EvaluateContactShadow(float3 position, float3 normal, float3 toLight, float maxDistance,
    float4x4 viewProj, float2 projectionZ, float2 screenPos)
{
    const float rayLength = min(g_ContactShadowLength, maxDistance);
    if (rayLength <= 0.0)
    {
        return 1.0;
    }
    float width, height;
    g_contactShadowDepth.GetDimensions(width, height);
    const int kStepCount = 8;
    // これより厚い段差は、奥にある別の面とみなして影にしない(薄い物の後ろが全部影になるのを防ぐ)
    const float kThickness = 0.1;
    const float3 magic = float3(0.06711056, 0.00583715, 52.9829189);
    const float noise = frac(magic.z * frac(dot(screenPos, magic.xy)));
    const float3 start = position + normal * 0.01;
    for (int i = 0; i < kStepCount; ++i)
    {
        const float t = (float(i) + noise) / float(kStepCount) * rayLength;
        const float4 clip = mul(float4(start + toLight * t, 1.0), viewProj);
        if (clip.w <= 0.0)
        {
            break;
        }
        const float2 uv = clip.xy / clip.w * float2(0.5, -0.5) + 0.5;
        if (any(uv < 0.0) || any(uv > 1.0))
        {
            break;
        }
        const float sceneNdc = g_contactShadowDepth.Load(int3(uv * float2(width, height), 0));
        // 深度(0..1)を視点空間のZに戻す(z_ndc = _33 + _43 / z)
        const float sceneZ = projectionZ.y / max(sceneNdc - projectionZ.x, 1e-6);
        const float difference = clip.w - sceneZ;
        if (difference > 0.005 && difference < kThickness)
        {
            // 光源に近いところで遮られるほど、影を弱くする(たどった先の端で途切れないように)
            return saturate(float(i) / float(kStepCount));
        }
    }
    return 1.0;
}

// スポットライトの影(影が割り当てられていなければ1)
float EvaluateSpotLightShadow(SurfaceData surface, SpotLight light, float2 screenPos)
{
    const float NdotL = saturate(dot(surface.normal, normalize(light.Pos - surface.position)));
    if (light.ShadowIndex >= 0.0)
    {
        return SampleSpotShadow(light, surface.position, NdotL, screenPos);
    }
    if (light.ShadowCubeIndex >= 0.0)
    {
        return SampleCubeShadow(light.Pos, light.Range, light.ShadowCubeIndex, light.SourceRadius, light.ShadowBias, surface.position, NdotL, screenPos);
    }
    return 1.0;
}

// 点光源の影(影が割り当てられていなければ1)
float EvaluatePointLightShadow(SurfaceData surface, PointLight light, float2 screenPos)
{
    if (light.ShadowCubeIndex < 0.0)
    {
        return 1.0;
    }
    const float NdotL = saturate(dot(surface.normal, normalize(light.Pos - surface.position)));
    return SampleCubeShadow(light.Pos, light.Range, light.ShadowCubeIndex, light.SourceRadius, light.ShadowBias, surface.position, NdotL, screenPos);
}

// 距離フォグ(g_DistanceFogColorは霧が返す光の輝度×露出)
float3 ApplyDistanceFog(float3 color, float distanceToCamera)
{
    if (g_DistanceFogDensity <= 0)
    {
        return color;
    }
    const float d = distanceToCamera * g_DistanceFogDensity;
    // 持ちフォグ 1(近い)から0(遠い)
    const float f = saturate(1.0 / exp(d * d));
    return lerp(g_DistanceFogColor, color, f);
}

#endif
