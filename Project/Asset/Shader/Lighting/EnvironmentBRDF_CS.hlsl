//================================
//
// 環境BRDFのLUT(スプリットサム近似)
//
//   鏡面反射を環境光(反射プローブなど)で照らしたときの量 ∫ f(l,v) cosθl dl を、
//   F0 × A + B の形に分けて前もって数値積分しておく(Karis 2013)。
//   横軸はNdotV、縦軸は知覚的なラフネス(GGXのαの平方根)。
//   A + B は1回の反射で返るエネルギー(F0=1のとき)で、マルチスキャッタのエネルギー補償にも使う。
//   BRDFはinc_PBRCommon.hlsliと同じGGXと高さ相関のSmith(V項)。
//
//================================

struct Constants
{
    uint OutputIndex; // 結果(RWTexture2D<float2>、UAV)
    uint Size;        // LUTの1辺のテクセル数
    uint SampleCount; // 1テクセルあたりのサンプル数
};
ConstantBuffer<Constants> g_constants : register(b0);

static const float kPi = 3.14159265;

// Hammersley点列
float2 Hammersley(uint index, uint count)
{
    return float2(float(index) / float(count), float(reversebits(index)) * 2.3283064365386963e-10);
}

// GGXの法線分布で、接空間(Nが+Z)のハーフベクトルを選ぶ
float3 ImportanceSampleGGX(float2 xi, float alpha)
{
    const float phi = 2.0 * kPi * xi.x;
    const float cosTheta = sqrt((1.0 - xi.y) / (1.0 + (alpha * alpha - 1.0) * xi.y));
    const float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    return float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

// 高さ相関のSmith(G / (4 NdotL NdotV))。inc_PBRCommon.hlsliのV_SmithGGXCorrelatedと同じ式
float VisibilitySmithGGXCorrelated(float NdotV, float NdotL, float alpha)
{
    const float a2 = alpha * alpha;
    const float lambdaV = NdotL * sqrt((NdotV - a2 * NdotV) * NdotV + a2);
    const float lambdaL = NdotV * sqrt((NdotL - a2 * NdotL) * NdotL + a2);
    return 0.5 / max(lambdaV + lambdaL, 1e-6);
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_constants.Size || dispatchId.y >= g_constants.Size)
    {
        return;
    }
    RWTexture2D<float2> outputTex = ResourceDescriptorHeap[g_constants.OutputIndex];

    // テクセルの中心がちょうど0と1になるように並べる(読む側も同じ対応で補間する)
    const float NdotV = max(float(dispatchId.x) / float(g_constants.Size - 1), 1e-3);
    const float perceptualRoughness = float(dispatchId.y) / float(g_constants.Size - 1);
    const float alpha = max(perceptualRoughness * perceptualRoughness, 1e-3);

    const float3 V = float3(sqrt(1.0 - NdotV * NdotV), 0.0, NdotV);
    float A = 0.0;
    float B = 0.0;
    for (uint i = 0; i < g_constants.SampleCount; ++i)
    {
        const float3 H = ImportanceSampleGGX(Hammersley(i, g_constants.SampleCount), alpha);
        const float3 L = 2.0 * dot(V, H) * H - V;
        const float NdotL = saturate(L.z);
        if (NdotL > 0.0)
        {
            const float NdotH = saturate(H.z);
            const float VdotH = saturate(dot(V, H));
            // f cosθ / pdf = F × V × 4 × NdotL × VdotH / NdotH (pdf = D × NdotH / (4 VdotH))
            const float weight = VisibilitySmithGGXCorrelated(NdotV, NdotL, alpha) * 4.0 * NdotL * VdotH / max(NdotH, 1e-6);
            const float fresnel = pow(1.0 - VdotH, 5.0);
            A += (1.0 - fresnel) * weight;
            B += fresnel * weight;
        }
    }
    outputTex[dispatchId.xy] = float2(A, B) / float(g_constants.SampleCount);
}
