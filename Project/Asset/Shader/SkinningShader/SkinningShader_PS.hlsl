#include "inc_SkinningShader.hlsli"

Texture2D g_diffuseTex : register(t0);
Texture2D g_normalTex : register(t1);
Texture2D g_RoughnessMetallicTex : register(t2);
Texture2D g_emissiveTex : register(t3);
Texture2D g_occlusionTex : register(t4);
// 平行光のシャドウマップ(LitShaderと同じレジスタ)
Texture2D g_dirLightShadowMap : register(t7);

// サンプラーはLitShader(ShaderCore.hlsli)と同じレジスタ。影の関数(inc_Lighting.hlsli)が使うので先に宣言する
SamplerState g_ss : register(s0);
SamplerState g_ss_point_clamp : register(s5);
SamplerComparisonState g_ss_comparison : register(s15);

#include "../Common/inc_LightData.hlsli"
#include "../Common/inc_PBRCommon.hlsli"
#include "../Common/inc_Lighting.hlsli"

float Hash2D(float2 p)
{
    return frac(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453);
}

float Noise2D(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    float2 u = f * f * (3.0 - 2.0 * f);
    return lerp(lerp(Hash2D(i), Hash2D(i + float2(1, 0)), u.x),
                lerp(Hash2D(i + float2(0, 1)), Hash2D(i + float2(1, 1)), u.x), u.y);
}

// 平行光の影(4点のPCF)。動くキャラクターは小さいので、LitShaderより軽いフィルタで済ませる
float SampleDirectionalShadow(float3 worldPos, float NdotL)
{
    if (g_DL_ShadowPower <= 0)
    {
        return 1.0;
    }
    float4 lightPos = mul(float4(worldPos, 1), g_DL_mLightVP[0]);
    lightPos.xyz /= lightPos.w;
    if (abs(lightPos.x) > 1 || abs(lightPos.y) > 1 || lightPos.z > 1)
    {
        return 1.0;
    }
    float2 uv = lightPos.xy * float2(1, -1) * 0.5 + 0.5;
    float bias = g_DL_DirLightShadowBias * lerp(1.0, 16.0, 1.0 - saturate(NdotL));
    float depth = lightPos.z - bias;

    float2 texelSize;
    float levels;
    g_dirLightShadowMap.GetDimensions(0, texelSize.x, texelSize.y, levels);
    texelSize = 1.0 / max(texelSize, 1.0);

    float shadow = 0;
    [unroll]
    for (int sampleIndex = 0; sampleIndex < 4; sampleIndex++)
    {
        shadow += g_dirLightShadowMap.SampleCmpLevelZero(g_ss_comparison, uv + g_poissonDisk[sampleIndex] * 2.0 * texelSize, depth);
    }
    shadow *= 0.25;
    return lerp(1.0, shadow, g_DL_ShadowPower);
}

PSOutput main(VSOutput In)
{
    PSOutput Out = (PSOutput)0;

    float4 baseColor = g_diffuseTex.Sample(g_ss, In.uv) * g_baseColorFactor;

    float4 mr = g_RoughnessMetallicTex.Sample(g_ss, In.uv);
    float metallic = mr.b * g_metallicFactor;
    float roughness = mr.g * g_roughnessFactor;

    float occlusion = lerp(1.0, g_occlusionTex.Sample(g_ss, In.uv).r, g_occlusionStrength);

    // Dissolve from bottom: clip everything below a (slightly ragged) world-height cut line,
    // and glow in a thin band just above it, so the body looks like it burns away upward.
    float edgeGlow = 0;
    if (g_proceduralType == 5)
    {
        float2 np = float2(In.wPos.x + In.wPos.z * 0.7, In.wPos.z - In.wPos.x * 0.7);
        float n = Noise2D(np * 9.0) * 0.6 + Noise2D(np * 23.0 + 7.1) * 0.4;
        float cut = g_dissolveHeight + (n - 0.5) * g_dissolveEdgeWidth * 1.5;
        float above = In.wPos.y - cut;
        clip(above);
        float edge = 1.0 - saturate(above / max(g_dissolveEdgeWidth, 0.001));
        edgeGlow = edge * edge;
    }

    //------------------
    // 法線
    //------------------
    float3 vertexNormal = normalize(In.Normal);
    float3 wN = vertexNormal;
    // 接線の無いメッシュでは法線マップを使わない
    float3 tangent = In.Tangent - vertexNormal * dot(In.Tangent, vertexNormal);
    if (dot(tangent, tangent) > 1e-6)
    {
        tangent = normalize(tangent);
        float3 bitangent = cross(vertexNormal, tangent);
        float3 tangentNormal = g_normalTex.Sample(g_ss, In.uv).rgb * 2.0 - 1.0;
        tangentNormal.xy *= g_normalScale;
        float3 mappedNormal = tangentNormal.x * tangent + tangentNormal.y * bitangent + tangentNormal.z * vertexNormal;
        if (dot(mappedNormal, mappedNormal) > 1e-4)
        {
            wN = normalize(mappedNormal);
        }
    }

    float3 toCamera = c_CamPos - In.wPos;
    float camDist = length(toCamera);
    float3 vCam = toCamera / max(camDist, 0.0001);
    // 両面描画(カリングなし)なので、裏面を見ているときは法線を反転する
    if (dot(wN, vCam) < 0)
    {
        wN = -wN;
    }
    float NdotV = saturate(dot(wN, vCam));

    //------------------
    // 材質(LitShaderと同じ)
    //------------------
    static const float3 kSpecularCoefficient = float3(0.04, 0.04, 0.04);
    SurfaceData surface;
    surface.position = In.wPos;
    surface.normal = wN;
    surface.viewDir = vCam;
    surface.NdotV = NdotV;
    surface.diffuseColor = lerp(baseColor.rgb * (1.0 - kSpecularCoefficient), float3(0, 0, 0), metallic);
    surface.specularColor = lerp(kSpecularCoefficient, baseColor.rgb, metallic);
    surface.roughness = roughness;
    surface.alpha = ApplyGeometricSpecularAA(roughness * roughness, wN);
    InitializeSurfaceBRDF(surface);

    float3 color = 0;

    //------------------
    // 平行光(室内では届かない。LitShaderと同じ扱い)
    //------------------
    float moonVisibility = g_IndirectShadowFloor;
    if (!IsIndoor(In.wPos))
    {
        float shadow = SampleDirectionalShadow(In.wPos, saturate(dot(wN, -g_DL_Dir)));
        color += EvaluateDirectLight(surface, -g_DL_Dir, g_DL_Color) * shadow;
        moonVisibility = max(shadow, g_IndirectShadowFloor);
    }

    //------------------
    // スポットライト・点光源(影はLitShaderと同じ)
    //------------------
    for (int spotIndex = 0; spotIndex < g_SL_Count; spotIndex++)
    {
        float3 spotLight = EvaluateSpotLight(surface, g_SL[spotIndex]);
        if (any(spotLight > 0.0))
        {
            spotLight *= EvaluateSpotLightShadow(surface, g_SL[spotIndex], In.pos.xy);
        }
        color += spotLight;
    }
    for (int pointIndex = 0; pointIndex < g_PL_Count; pointIndex++)
    {
        float3 pointLight = EvaluatePointLight(surface, g_PL[pointIndex]);
        if (any(pointLight > 0.0))
        {
            pointLight *= EvaluatePointLightShadow(surface, g_PL[pointIndex], In.pos.xy);
        }
        color += pointLight;
    }

    //------------------
    // 環境光・ダミーIBL
    //------------------
    // 拡散の環境光: DDGIのプローブ(絶対の値なので露出を掛ける)。ボリュームの外は今までの環境光の定数
    float giCoverage;
    const float3 giIrradiance = SampleDDGIIrradiance(In.wPos, wN, vCam, g_ddgiIrradiance, g_ddgiDistance, g_ss, giCoverage);
    color += lerp(g_AmbientLight * baseColor.rgb * moonVisibility, surface.diffuseColor * giIrradiance * g_Exposure, giCoverage) * occlusion;
    color += EvaluateEnvironmentLighting(surface, occlusion, moonVisibility, giCoverage, g_ss);

    color = ApplyDistanceFog(color, camDist);

    //------------------
    // 発光
    //------------------
    // スキニングのモデルの発光は、ゴーストの浄化の光などの演出にだけ使っていて物理単位を持たない。
    // 物理単位にする前の露出(0.3)を固定で掛け、画面上の明るさを今までと同じに保つ
    const float kUnlitExposure = 0.3;
    float3 emissive = g_emissiveTex.Sample(g_ss, In.uv).rgb * g_emissiveFactor * g_emissiveStrength;
    color += (emissive + g_dissolveEdgeColor * edgeGlow) * kUnlitExposure;

    Out.color = float4(color, baseColor.a);

    return Out;
}
