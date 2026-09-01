static const float PI = 3.14159265f;
static const float EPSILON = 1e-6f;

//=============================
//
// PBR共通
//
//=============================

float Pow5(float x)
{
    float xSq = x * x;
    return xSq * xSq * x;
}

// (F)Shilickフレネル近似
// https://en.wikipedia.org/wiki/Schlick%27s_approximation
float3 Fresnel_Shlick(in float3 f0, in float3 f90, in float x)
{
    return f0 + (f90 - f0) * Pow5(1.0f - x);
}

// 拡散光(Burley Diffuse)
// Burley B. "Physically Based Shading at Disney"
// SIGGRAPH 2012 Course: Practical Physically Based Shading in Film and Game Production, 2012.
float3 Diffuse_Burley(in float3 baseColor, in float NdotL, in float NdotV, in float LdotH, in float roughness)
{
    float fd90 = 0.5 + 2.0 * roughness * LdotH * LdotH;
    return (baseColor / PI) * Fresnel_Shlick(1, fd90, NdotL).x * Fresnel_Shlick(1, fd90, NdotV).x;
}

// (D)GGXスペキュラ 法線分布
// https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.pdf
float Specular_D_GGX(in float alpha, in float NdotH)
{
    const float alpha2 = alpha * alpha;
    const float lower = (NdotH * NdotH * (alpha2 - 1)) + 1;
    return alpha2 / max(EPSILON, PI * lower * lower);
}

// (V) Height-Correlated Smith GGX (V項 = G項 / (4 * NdotL * NdotV))
float V_SmithGGXCorrelated(float NdotV, float NdotL, float alpha)
{
    float a2 = alpha * alpha;
    float lambdaV = NdotL * sqrt((NdotV - a2 * NdotV) * NdotV + a2);
    float lambdaL = NdotV * sqrt((NdotL - a2 * NdotL) * NdotL + a2);
    return 0.5f / max(lambdaV + lambdaL, EPSILON);
}

// マイクロファセットBRDF
float3 Specular_BRDF(in float alpha, in float3 specularColor, in float NdotV, in float NdotL, in float LdotH, in float NdotH)
{
	// Specular D (ファイロファセット法線分布)
    float specular_D = Specular_D_GGX(alpha, NdotH);

	// Specular Fresnel
    float3 specular_F = Fresnel_Shlick(specularColor, 1, LdotH);

	// Specular V (幾何減衰と分母の統合)
    float specular_V = V_SmithGGXCorrelated(NdotV, NdotL, alpha);

    return specular_D * specular_F * specular_V;
}

// ポワソンサンプリング用
static const float2 g_poissonDisk[4] = {
  float2(-0.94201624, -0.39906216),
  float2(0.94558609, -0.76890725),
  float2(-0.094184101, -0.92938870),
  float2(0.34495938, 0.29387760)
};
