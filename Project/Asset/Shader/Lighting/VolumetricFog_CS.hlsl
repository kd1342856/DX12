//================================
//
// ボリュメトリックフォグ(フロクセル)
//
//   視錐台を画面の16x16ピクセルごと × 奥行き64枚(手前ほど薄い指数の分割)の箱(フロクセル)に分け、
//   1. InjectScattering: 箱ごとに、霧の濃さ(消散係数)と、ライトから霧に散乱して目に向かう光を求める。
//      影はシャドウマップ、環境光はDDGIのプローブ。前のフレームの結果を同じ位置へ戻して混ぜる(時間方向の再利用)
//   2. IntegrateScattering: 手前から奥へ、透過率と散乱光を積み上げる(Hillaire 2015の式)
//   3. ApplyFog: 画面のピクセルの深さまで積み上げた値で、色 = 色 × 透過率 + 散乱光 にする
//   光の値は前露出。霧の濃さはハント中に濃くなる(ShaderManagerが求める)。
//
//================================
#include "../Common/inc_LightData.hlsli"
#include "../Common/inc_DDGI.hlsli"

struct Constants
{
    uint ScatteringUAVIndex; // 今のフレームの散乱(RWTexture3D)
    uint HistorySRVIndex;    // 前のフレームの散乱(Texture3D)
    uint IntegratedUAVIndex; // 積み上げた結果(RWTexture3D)
    uint IntegratedSRVIndex; // 積み上げた結果(Texture3D)
    uint SceneColorSRVIndex; // シーンの色
    uint DepthSRVIndex;      // シーンの深度
    uint OutputUAVIndex;     // 霧を掛けた色
    uint DirectionalShadowSRVIndex;
    uint SpotShadowSRVIndex;
    uint CubeShadowSRVIndex;
    uint DDGIIrradianceSRVIndex;
    uint DDGIDistanceSRVIndex;
    uint HasHistory;
};
ConstantBuffer<Constants> g_constants : register(b0);

cbuffer cbFog : register(b1)
{
    row_major float4x4 g_InvViewProj;  // ジッターを含まないビュー×投影の逆
    row_major float4x4 g_PrevViewProj; // 前のフレームのビュー×投影
    float3 g_CameraPos;
    float g_FogNear;          // フロクセルの最初の面(m)
    float3 g_CameraForward;
    float g_FogFar;           // 最後の面(m)。これより奥は最後の面までの霧を掛ける
    float3 g_Albedo;          // 散乱の割合(色)。消散のうち散乱になる割合
    float g_Extinction;       // 消散係数(1/m)
    uint3 g_GridSize;
    float g_PhaseG;           // ヘニエイ・グリーンスタインの非対称パラメーター(正で前方散乱)
    float2 g_ScreenSize;
    float g_JitterZ;          // 奥行きの中で調べる位置(0〜1)をフレームごとにずらす
    float g_HistoryWeight;    // 前のフレームの結果を残す割合
};

SamplerState g_pointClamp : register(s0);
SamplerState g_linearClamp : register(s1);

static const float kPi = 3.14159265;
// C++のLocalShadowManagerと同じ
static const float kShadowNear = 0.05;

float DepthFromSlice(float slice)
{
    return g_FogNear * pow(g_FogFar / g_FogNear, slice / float(g_GridSize.z));
}

float SliceFromDepth(float depth)
{
    return log(max(depth, 1e-4) / g_FogNear) / log(g_FogFar / g_FogNear) * float(g_GridSize.z);
}

// 画面のUVを通る視線の向き(ワールド)
float3 ViewRayDirection(float2 uv)
{
    const float2 ndc = uv * float2(2.0, -2.0) + float2(-1.0, 1.0);
    const float4 farPoint = mul(float4(ndc, 1.0, 1.0), g_InvViewProj);
    return normalize(farPoint.xyz / farPoint.w - g_CameraPos);
}

// ヘニエイ・グリーンスタインの位相関数。cosThetaは光の進む向きと、目へ向かう向きのなす角
float PhaseHenyeyGreenstein(float cosTheta, float g)
{
    const float g2 = g * g;
    return (1.0 - g2) / (4.0 * kPi * pow(max(1.0 + g2 - 2.0 * g * cosTheta, 1e-4), 1.5));
}

float DirectionalShadow(float3 position)
{
    Texture2D<float> shadowMap = ResourceDescriptorHeap[g_constants.DirectionalShadowSRVIndex];
    const float4 lightClip = mul(float4(position, 1.0), g_DL_mLightVP[0]);
    const float3 ndc = lightClip.xyz / lightClip.w;
    if (abs(ndc.x) > 1.0 || abs(ndc.y) > 1.0 || ndc.z > 1.0)
    {
        return 1.0;
    }
    const float2 uv = ndc.xy * float2(0.5, -0.5) + 0.5;
    return (shadowMap.SampleLevel(g_pointClamp, uv, 0) >= ndc.z - g_DL_DirLightShadowBias) ? 1.0 : 0.0;
}

float SpotShadow(SpotLight light, float3 position)
{
    if (light.ShadowIndex >= 0.0)
    {
        Texture2DArray<float> shadowMaps = ResourceDescriptorHeap[g_constants.SpotShadowSRVIndex];
        const float4 lightClip = mul(float4(position, 1.0), light.mLightVP);
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
        return (shadowMaps.SampleLevel(g_pointClamp, float3(uv, light.ShadowIndex), 0) >= ndc.z - 0.0005) ? 1.0 : 0.0;
    }
    return 1.0;
}

float CubeShadow(float3 lightPos, float range, float cubeIndex, float3 position)
{
    if (cubeIndex < 0.0)
    {
        return 1.0;
    }
    TextureCubeArray<float> shadowMaps = ResourceDescriptorHeap[g_constants.CubeShadowSRVIndex];
    const float3 toPixel = position - lightPos;
    const float3 absToPixel = abs(toPixel);
    const float receiverZ = max(absToPixel.x, max(absToPixel.y, absToPixel.z));
    if (receiverZ <= kShadowNear)
    {
        return 1.0;
    }
    const float nearZ = kShadowNear;
    const float farZ = max(range, 1.0);
    const float receiverNdc = farZ / (farZ - nearZ) - nearZ * farZ / ((farZ - nearZ) * receiverZ);
    return (shadowMaps.SampleLevel(g_pointClamp, float4(toPixel, cubeIndex), 0) >= receiverNdc - 0.0005) ? 1.0 : 0.0;
}

// 霧のある位置で、目の方向(toCamera)へ散乱する光(前露出)。位相関数を掛けた放射輝度
float3 InScatteredLight(float3 position, float3 toCamera)
{
    float3 radiance = 0.0;

    // 平行光(月)。室内には届かない(LitShaderと同じ)
    if (any(g_DL_Color > 0.0) && !IsIndoor(position))
    {
        radiance += g_DL_Color * PhaseHenyeyGreenstein(dot(g_DL_Dir, toCamera), g_PhaseG) * DirectionalShadow(position);
    }

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
        const float cone = saturate((dot(-L, normalize(light.Dir)) - light.OuterCorn) / (light.InnerCorn - light.OuterCorn));
        if (cone <= 0.0)
        {
            continue;
        }
        float shadow = SpotShadow(light, position);
        if (light.ShadowIndex < 0.0)
        {
            shadow = CubeShadow(light.Pos, light.Range, light.ShadowCubeIndex, position);
        }
        radiance += light.Color * (DistanceAttenuation(distanceToLight, light.Range) * cone * shadow) * PhaseHenyeyGreenstein(dot(-L, toCamera), g_PhaseG);
    }

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
        const float shadow = CubeShadow(light.Pos, light.Range, light.ShadowCubeIndex, position);
        radiance += light.Color * (DistanceAttenuation(distanceToLight, light.Range) * shadow) * PhaseHenyeyGreenstein(dot(-L, toCamera), g_PhaseG);
    }

    // 環境光: DDGIのプローブ(上向きと下向きの平均を、全方向から来る光の平均とみなす)。家の外は空の明るさ
    Texture2D<float4> irradianceTex = ResourceDescriptorHeap[g_constants.DDGIIrradianceSRVIndex];
    Texture2D<float2> distanceTex = ResourceDescriptorHeap[g_constants.DDGIDistanceSRVIndex];
    float coverageUp;
    float coverageDown;
    const float3 up = SampleDDGIIrradiance(position, float3(0.0, 1.0, 0.0), toCamera, irradianceTex, distanceTex, g_linearClamp, coverageUp);
    const float3 down = SampleDDGIIrradiance(position, float3(0.0, -1.0, 0.0), toCamera, irradianceTex, distanceTex, g_linearClamp, coverageDown);
    const float coverage = min(coverageUp, coverageDown);
    // 等方的に散乱するので、平均の放射輝度がそのまま目へ向かう(位相関数の積分は1)
    radiance += lerp(g_AmbientLight, (up + down) * 0.5 * g_Exposure, coverage);
    return radiance;
}

//--------------------------------
// 1. フロクセルごとの散乱と消散
//--------------------------------
[numthreads(8, 8, 1)]
void InjectScattering(uint3 dispatchId : SV_DispatchThreadID)
{
    if (any(dispatchId >= g_GridSize))
    {
        return;
    }
    RWTexture3D<float4> scatteringTex = ResourceDescriptorHeap[g_constants.ScatteringUAVIndex];

    const float2 uv = (float2(dispatchId.xy) + 0.5) / float2(g_GridSize.xy);
    const float3 rayDirection = ViewRayDirection(uv);
    const float depth = DepthFromSlice(float(dispatchId.z) + g_JitterZ);
    const float3 position = g_CameraPos + rayDirection * (depth / max(dot(rayDirection, g_CameraForward), 1e-3));

    const float3 inScattered = InScatteredLight(position, -rayDirection);
    float4 result = float4(g_Albedo * g_Extinction * inScattered, g_Extinction);

    // 前のフレームの同じ位置の値と混ぜる(奥行きの位置をずらしながら調べた結果が平均される)
    if (g_constants.HasHistory != 0)
    {
        const float4 previousClip = mul(float4(position, 1.0), g_PrevViewProj);
        if (previousClip.w > 0.0)
        {
            const float2 previousUV = previousClip.xy / previousClip.w * float2(0.5, -0.5) + 0.5;
            const float previousSlice = SliceFromDepth(previousClip.w) / float(g_GridSize.z);
            if (all(previousUV >= 0.0) && all(previousUV <= 1.0) && previousSlice >= 0.0 && previousSlice <= 1.0)
            {
                Texture3D<float4> historyTex = ResourceDescriptorHeap[g_constants.HistorySRVIndex];
                const float4 history = historyTex.SampleLevel(g_linearClamp, float3(previousUV, previousSlice), 0);
                if (all(isfinite(history)))
                {
                    result = lerp(result, history, g_HistoryWeight);
                }
            }
        }
    }
    scatteringTex[dispatchId] = result;
}

//--------------------------------
// 2. 手前から奥へ積み上げる。1スレッドが画面の1列
//--------------------------------
[numthreads(8, 8, 1)]
void IntegrateScattering(uint3 dispatchId : SV_DispatchThreadID)
{
    if (any(dispatchId.xy >= g_GridSize.xy))
    {
        return;
    }
    Texture3D<float4> scatteringTex = ResourceDescriptorHeap[g_constants.HistorySRVIndex];
    RWTexture3D<float4> integratedTex = ResourceDescriptorHeap[g_constants.IntegratedUAVIndex];

    const float2 uv = (float2(dispatchId.xy) + 0.5) / float2(g_GridSize.xy);
    // 視線の長さ = 奥行き ÷ cos(視線とカメラの正面のなす角)
    const float lengthPerDepth = 1.0 / max(dot(ViewRayDirection(uv), g_CameraForward), 1e-3);

    float3 accumulated = 0.0;
    float transmittance = 1.0;
    float previousDepth = 0.0;
    for (uint slice = 0; slice < g_GridSize.z; ++slice)
    {
        const float sliceDepth = DepthFromSlice(float(slice + 1));
        const float stepLength = (sliceDepth - previousDepth) * lengthPerDepth;
        previousDepth = sliceDepth;

        const float4 value = scatteringTex.Load(int4(dispatchId.xy, slice, 0));
        const float extinction = max(value.a, 1e-7);
        const float stepTransmittance = exp(-extinction * stepLength);
        // 区間の中で、手前ほど多く届くことを考えた散乱光の積分(Hillaire 2015)
        const float3 integratedScattering = (value.rgb - value.rgb * stepTransmittance) / extinction;
        accumulated += transmittance * integratedScattering;
        transmittance *= stepTransmittance;
        integratedTex[uint3(dispatchId.xy, slice)] = float4(accumulated, transmittance);
    }
}

//--------------------------------
// 3. 画面のピクセルに掛ける
//--------------------------------
[numthreads(8, 8, 1)]
void ApplyFog(uint3 dispatchId : SV_DispatchThreadID)
{
    if (any(dispatchId.xy >= uint2(g_ScreenSize)))
    {
        return;
    }
    Texture2D<float4> sceneTex = ResourceDescriptorHeap[g_constants.SceneColorSRVIndex];
    Texture2D<float> depthTex = ResourceDescriptorHeap[g_constants.DepthSRVIndex];
    Texture3D<float4> integratedTex = ResourceDescriptorHeap[g_constants.IntegratedSRVIndex];
    RWTexture2D<float4> outputTex = ResourceDescriptorHeap[g_constants.OutputUAVIndex];

    const float2 uv = (float2(dispatchId.xy) + 0.5) / g_ScreenSize;
    const float depthNdc = depthTex.Load(int3(dispatchId.xy, 0));
    const float2 ndc = uv * float2(2.0, -2.0) + float2(-1.0, 1.0);
    const float4 worldPoint = mul(float4(ndc, depthNdc, 1.0), g_InvViewProj);
    const float viewDepth = min(dot(worldPoint.xyz / worldPoint.w - g_CameraPos, g_CameraForward), g_FogFar);

    // 積み上げた値はフロクセルの奥の面までのもの。最初の面より手前は霧なしから補間する
    const float slice = SliceFromDepth(viewDepth);
    float4 fog = integratedTex.SampleLevel(g_linearClamp, float3(uv, saturate((slice - 0.5) / float(g_GridSize.z))), 0);
    fog = lerp(float4(0.0, 0.0, 0.0, 1.0), fog, saturate(slice));

    const float4 sceneColor = sceneTex.Load(int3(dispatchId.xy, 0));
    outputTex[dispatchId.xy] = float4(sceneColor.rgb * fog.a + fog.rgb, sceneColor.a);
}
