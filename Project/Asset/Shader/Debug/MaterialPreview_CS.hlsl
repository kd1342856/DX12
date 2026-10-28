//================================
//
// マテリアルのプレビュー(シェーダーエディターのマテリアルのパネル)
//
//   画面いっぱいの球を解析的に描き、マテリアルの値とテクスチャで照らす。BRDFは描画と同じinc_PBRCommon。
//   光は固定(斜め上からの平行光と、空と地面のグラデーションの環境光)なので、シーンのライトや露出に左右されない。
//   明るい所を丸めてsRGBにし、ImGuiでそのまま出せるRGBA8に書く。
//
//================================
#include "../Common/inc_PBRCommon.hlsli"

struct Constants
{
    uint DestinationIndex;  // 書き込み先(UAV、RGBA8)
    uint Size;              // 書き込み先の幅と高さ(正方形)
    uint BaseColorIndex;    // テクスチャ(SRV)。無ければ白(法線は平らな法線)
    uint NormalIndex;
    uint MetallicRoughnessIndex;
    uint OcclusionIndex;
    uint EmissiveIndex;
    uint AlphaMode;         // 0: 不透明、1: アルファテスト、2: 半透明
    float TextureLod;       // テクスチャのミップ(球の大きさに合わせる)
};
ConstantBuffer<Constants> g_constants : register(b0);

cbuffer cbMaterial : register(b1)
{
    float4 g_baseColorFactor;
    float3 g_emissiveFactor;
    float g_emissiveStrength;   // nit
    float g_metallicFactor;
    float g_roughnessFactor;
    float g_normalScale;
    float g_occlusionStrength;
    float g_emissiveFrontScale;
    float3 g_padding;
};

SamplerState g_linearWrap : register(s2);

static const float kSpecularCoefficient = 0.04;
// 平行光の照度と空の明るさ(画面の白を1とした値。明るい所は後で丸める)
static const float3 kLightDirection = float3(-0.55, 0.65, -0.52); // 表面から光源への向き(左上手前)
static const float3 kLightIlluminance = float3(2.6, 2.5, 2.4);
static const float3 kSkyRadiance = float3(0.32, 0.36, 0.42);
static const float3 kGroundRadiance = float3(0.10, 0.09, 0.08);
// 発光はこの輝度(nit)を画面の白にする(glTFの発光1.0 = 100 nit)
static const float kEmissiveWhiteNits = 100.0;

// 環境BRDFの解析的な近似(Karis, UE4 Mobile)。inc_Lightingと同じ式
float2 AnalyticEnvironmentBRDF(float perceptualRoughness, float NdotV)
{
    const float4 kEnvC0 = float4(-1.0, -0.0275, -0.572, 0.022);
    const float4 kEnvC1 = float4(1.0, 0.0425, 1.04, -0.04);
    const float4 envR = perceptualRoughness * kEnvC0 + kEnvC1;
    const float envA004 = min(envR.x * envR.x, exp2(-9.28 * NdotV)) * envR.x + envR.y;
    return float2(-1.04, 1.04) * envA004 + envR.zw;
}

// 向きの空の明るさ。粗いほど上下を混ぜてぼかす
float3 SkyRadiance(float3 direction, float roughness)
{
    const float blend = saturate(direction.y * (1.0 - roughness * 0.8) * 0.5 + 0.5);
    return lerp(kGroundRadiance, kSkyRadiance, blend);
}

float3 LinearToSrgb(float3 linearColor)
{
    const float3 low = linearColor * 12.92;
    const float3 high = 1.055 * pow(linearColor, 1.0 / 2.4) - 0.055;
    return select(linearColor <= 0.0031308, low, high);
}

// 後ろの市松模様(透けるマテリアルが分かるように)
float3 Background(uint2 pixel)
{
    const uint cell = ((pixel.x / 12) + (pixel.y / 12)) & 1;
    return cell ? float3(0.16, 0.16, 0.17) : float3(0.11, 0.11, 0.12);
}

float4 SampleTexture(uint index, float2 uv)
{
    Texture2D<float4> texture = ResourceDescriptorHeap[index];
    return texture.SampleLevel(g_linearWrap, uv, g_constants.TextureLod);
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_constants.Size || dispatchId.y >= g_constants.Size)
    {
        return;
    }
    RWTexture2D<float4> destination = ResourceDescriptorHeap[g_constants.DestinationIndex];
    const float3 background = Background(dispatchId.xy);

    // 画面の中の位置(-1〜1、Yは上が正)。少し余白を空けて球を置く
    const float2 position = ((float2(dispatchId.xy) + 0.5) / float(g_constants.Size) * 2.0 - 1.0) * float2(1.0, -1.0) * 1.08;
    const float radiusSquared = dot(position, position);
    if (radiusSquared >= 1.0)
    {
        destination[dispatchId.xy] = float4(LinearToSrgb(background), 1.0);
        return;
    }

    // 正射影で見た球の表面。カメラは-Z側から+Zを向く
    const float3 sphereNormal = float3(position, -sqrt(1.0 - radiusSquared));
    const float3 toCamera = float3(0.0, 0.0, -1.0); // 表面からカメラへの向き
    // 経緯度のUV(テクスチャを2回り巻く)
    const float longitude = atan2(sphereNormal.x, -sphereNormal.z);
    const float latitude = acos(clamp(sphereNormal.y, -1.0, 1.0));
    const float2 uv = float2(longitude / (2.0 * PI) * 2.0, latitude / PI);
    // 接線(Uが増える向き)と従法線
    const float3 tangent = normalize(float3(-sphereNormal.z, 0.0, sphereNormal.x) + float3(1e-5, 0.0, 0.0));
    const float3 bitangent = cross(sphereNormal, tangent);

    const float4 baseColor = SampleTexture(g_constants.BaseColorIndex, uv) * g_baseColorFactor;
    if (g_constants.AlphaMode == 1 && baseColor.a < 0.5)
    {
        destination[dispatchId.xy] = float4(LinearToSrgb(background), 1.0);
        return;
    }

    float3 tangentNormal = SampleTexture(g_constants.NormalIndex, uv).rgb * 2.0 - 1.0;
    tangentNormal.xy *= g_normalScale;
    const float3 normal = normalize(tangent * tangentNormal.x - bitangent * tangentNormal.y + sphereNormal * tangentNormal.z);

    const float4 metallicRoughness = SampleTexture(g_constants.MetallicRoughnessIndex, uv);
    const float metallic = saturate(metallicRoughness.b * g_metallicFactor);
    const float roughness = saturate(metallicRoughness.g * g_roughnessFactor);
    const float alpha = max(roughness * roughness, 0.002);
    const float occlusion = lerp(1.0, SampleTexture(g_constants.OcclusionIndex, uv).r, g_occlusionStrength);

    const float3 diffuseColor = lerp(baseColor.rgb * (1.0 - kSpecularCoefficient), 0.0, metallic);
    const float3 specularColor = lerp(kSpecularCoefficient, baseColor.rgb, metallic);
    const float NdotV = max(dot(normal, toCamera), 1e-4);

    // 平行光
    const float3 lightDir = normalize(kLightDirection);
    const float NdotL = saturate(dot(normal, lightDir));
    const float3 halfVector = normalize(lightDir + toCamera);
    const float LdotH = saturate(dot(lightDir, halfVector));
    const float NdotH = saturate(dot(normal, halfVector));
    const float2 environmentBRDF = AnalyticEnvironmentBRDF(roughness, NdotV);
    const float3 energyCompensation = 1.0 + specularColor * (1.0 / max(environmentBRDF.x + environmentBRDF.y, 1e-3) - 1.0);
    float3 color = NdotL * kLightIlluminance * (Diffuse_Burley(diffuseColor, NdotL, NdotV, LdotH, roughness)
        + Specular_BRDF(alpha, specularColor, NdotV, NdotL, LdotH, NdotH) * energyCompensation);

    // 環境光(拡散は法線の向きの空、鏡面は反射の向きの空)
    const float3 reflection = reflect(-toCamera, normal);
    color += diffuseColor * SkyRadiance(normal, 1.0) * occlusion;
    color += SkyRadiance(reflection, roughness) * (specularColor * environmentBRDF.x + environmentBRDF.y) * energyCompensation * occlusion;

    // 発光(球の外側は表の面)
    const float3 emissive = SampleTexture(g_constants.EmissiveIndex, uv).rgb * g_emissiveFactor * g_emissiveFrontScale;
    color += emissive * (g_emissiveStrength / kEmissiveWhiteNits);

    // 半透明は後ろと混ぜる
    if (g_constants.AlphaMode == 2)
    {
        color = lerp(background, color, saturate(baseColor.a));
    }

    // 明るい所を丸めてからsRGBにする(見るためだけの丸め)
    color = max(color, 0.0);
    destination[dispatchId.xy] = float4(LinearToSrgb(color / (1.0 + color)), 1.0);
}
