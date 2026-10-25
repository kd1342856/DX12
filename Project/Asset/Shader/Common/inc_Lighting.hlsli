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
};

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
    const float3 specular = Specular_BRDF(surface.alpha, surface.specularColor, surface.NdotV, NdotL, LdotH, NdotH);
    return NdotL * illuminance * (diffuse + specular);
}

// スポットライト(影なし)。light.Colorは光度(cd)×露出
float3 EvaluateSpotLight(SurfaceData surface, SpotLight light)
{
    float3 L = light.Pos - surface.position;
    const float distanceToLight = length(L);
    L = L / max(distanceToLight, 0.0001);
    const float attenuation = DistanceAttenuation(distanceToLight, light.Range);
    const float cosAngle = dot(-L, normalize(light.Dir));
    const float coneAttenuation = saturate((cosAngle - light.OuterCorn) / (light.InnerCorn - light.OuterCorn));
    return EvaluateDirectLight(surface, L, light.Color * (attenuation * coneAttenuation));
}

// 点光源(影なし。影は呼び出し側で掛ける)。light.Colorは光束(lm)を光度(cd)に直したもの×露出
float3 EvaluatePointLight(SurfaceData surface, PointLight light)
{
    float3 L = light.Pos - surface.position;
    const float distanceToLight = length(L);
    L = L / max(distanceToLight, 0.0001);
    const float attenuation = DistanceAttenuation(distanceToLight, light.Range);
    return EvaluateDirectLight(surface, L, light.Color * attenuation);
}

// ダミーIBL(空と地面の2色)の拡散と鏡面。Step 5の反射プローブとGIで置き換えるまでの仮のもの。
// 2色の値1.0がg_IBLLuminance(nit×露出)になる
float3 EvaluateDummyIBL(SurfaceData surface, float specularOcclusion)
{
    const float3 skyColor = float3(0.5, 0.6, 0.8);
    const float3 groundColor = float3(0.3, 0.25, 0.2);

    // 拡散
    const float envWeight = surface.normal.y * 0.5 + 0.5;
    const float3 envDiff = lerp(groundColor, skyColor, envWeight) * 1.5 * g_IBLLuminance;
    const float3 diffuse = envDiff * surface.diffuseColor / 3.141592;

    // 鏡面
    const float3 vRef = reflect(-surface.viewDir, surface.normal);
    // 粗い面ほど反射像がぼやけるので、反射方向のグラデーションを法線方向(拡散と同じ)へ寄せる
    const float specWeight = lerp(vRef.y * 0.5 + 0.5, envWeight, surface.roughness);
    const float3 envSpec = lerp(groundColor, skyColor, specWeight) * 2.0 * g_IBLLuminance;
    // 環境BRDF近似(Karis, UE4 Mobile)。粗さとフレネルで反射量を決める
    // (粗いほど正面の反射が弱まり、浅い角度ほど強まる)
    const float4 kEnvC0 = float4(-1.0, -0.0275, -0.572, 0.022);
    const float4 kEnvC1 = float4(1.0, 0.0425, 1.04, -0.04);
    const float4 envR = surface.roughness * kEnvC0 + kEnvC1;
    const float envA004 = min(envR.x * envR.x, exp2(-9.28 * surface.NdotV)) * envR.x + envR.y;
    const float2 envAB = float2(-1.04, 1.04) * envA004 + envR.zw;
    const float3 envBRDF = surface.specularColor * envAB.x + envAB.y;

    return diffuse + envSpec * envBRDF * specularOcclusion;
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
