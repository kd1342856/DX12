//================================
//
// 反射プローブのプリフィルタ
//
//   撮ったキューブマップ(ステージング)から、ラフネスごとにぼかしたミップを作り、
//   全プローブを並べたキューブ配列の、そのプローブの6面に書く(スプリットサム近似の前半、Karis 2013)。
//   ミップmのラフネスは m / (ミップの数 - 1)(知覚的なラフネス。読む側もこの対応で選ぶ)。
//   GGXの法線分布で重点的に選んだ向きを集め、N = V = R と仮定する。
//
//================================

struct Constants
{
    uint SourceIndex;  // 撮ったキューブマップ(TextureCube、SRV)
    uint OutputIndex;  // このミップのUAV(RWTexture2DArray、全プローブの6面が並ぶ)
    uint OutputSize;   // このミップの1辺のテクセル数
    uint FirstSlice;   // このプローブの最初の面(プローブの番号 × 6)
    float Roughness;   // このミップの知覚的なラフネス
    uint SampleCount;
    float SourceSize;  // 撮ったキューブマップの1辺のテクセル数
};
ConstantBuffer<Constants> g_constants : register(b0);

SamplerState g_linearClamp : register(s1);

static const float kPi = 3.14159265;

float2 Hammersley(uint index, uint count)
{
    return float2(float(index) / float(count), float(reversebits(index)) * 2.3283064365386963e-10);
}

// キューブの面とその面のUVから向きを求める(D3Dのキューブの面の並び: +X, -X, +Y, -Y, +Z, -Z)
float3 CubeFaceDirection(uint face, float2 uv)
{
    const float2 p = uv * 2.0 - 1.0;
    float3 direction;
    switch (face)
    {
    case 0: direction = float3(1.0, -p.y, -p.x); break;
    case 1: direction = float3(-1.0, -p.y, p.x); break;
    case 2: direction = float3(p.x, 1.0, p.y); break;
    case 3: direction = float3(p.x, -1.0, -p.y); break;
    case 4: direction = float3(p.x, -p.y, 1.0); break;
    default: direction = float3(-p.x, -p.y, -1.0); break;
    }
    return normalize(direction);
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_constants.OutputSize || dispatchId.y >= g_constants.OutputSize)
    {
        return;
    }
    TextureCube<float4> sourceTex = ResourceDescriptorHeap[g_constants.SourceIndex];
    RWTexture2DArray<float4> outputTex = ResourceDescriptorHeap[g_constants.OutputIndex];

    const uint face = dispatchId.z;
    const float2 uv = (float2(dispatchId.xy) + 0.5) / float(g_constants.OutputSize);
    const float3 N = CubeFaceDirection(face, uv);
    const uint3 outputPosition = uint3(dispatchId.xy, g_constants.FirstSlice + face);

    // ラフネス0は鏡なので、そのまま写す
    if (g_constants.Roughness <= 0.0)
    {
        outputTex[outputPosition] = float4(sourceTex.SampleLevel(g_linearClamp, N, 0).rgb, 1.0);
        return;
    }

    const float alpha = g_constants.Roughness * g_constants.Roughness;
    const float3 up = abs(N.y) < 0.999 ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
    const float3 tangentX = normalize(cross(up, N));
    const float3 tangentY = cross(N, tangentX);

    // 1サンプルが覆う立体角に合わせて、撮ったキューブマップの粗いミップを読む(Filtered Importance Sampling)。
    // ステージングにミップが無いときは0を読み、サンプル数で補う
    const float texelSolidAngle = 4.0 * kPi / (6.0 * g_constants.SourceSize * g_constants.SourceSize);

    float3 color = 0.0;
    float weightSum = 0.0;
    for (uint i = 0; i < g_constants.SampleCount; ++i)
    {
        const float2 xi = Hammersley(i, g_constants.SampleCount);
        const float phi = 2.0 * kPi * xi.x;
        const float cosTheta = sqrt((1.0 - xi.y) / (1.0 + (alpha * alpha - 1.0) * xi.y));
        const float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
        const float3 H = tangentX * (sinTheta * cos(phi)) + tangentY * (sinTheta * sin(phi)) + N * cosTheta;
        const float3 L = 2.0 * dot(N, H) * H - N;
        const float NdotL = dot(N, L);
        if (NdotL > 0.0)
        {
            // N = V なので NdotH = VdotH。pdf(L) = D / 4
            const float NdotH = saturate(cosTheta);
            const float a2 = alpha * alpha;
            const float denominator = NdotH * NdotH * (a2 - 1.0) + 1.0;
            const float D = a2 / (kPi * denominator * denominator);
            const float sampleSolidAngle = 1.0 / (float(g_constants.SampleCount) * max(D * 0.25, 1e-6));
            const float mip = max(0.5 * log2(sampleSolidAngle / texelSolidAngle) + 1.0, 0.0);
            color += sourceTex.SampleLevel(g_linearClamp, L, mip).rgb * NdotL;
            weightSum += NdotL;
        }
    }
    outputTex[outputPosition] = float4(color / max(weightSum, 1e-6), 1.0);
}

//--------------------------------
// 撮ったキューブマップのミップを作る(2x2の平均)。プリフィルタで粗いミップを読むために使う
//--------------------------------
// 定数はプリフィルタと同じ並び(SourceIndex: 1つ上のミップだけのSRV(Texture2DArray、6面)、
// OutputIndex: このミップのUAV、OutputSize)
[numthreads(8, 8, 1)]
void Downsample(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_constants.OutputSize || dispatchId.y >= g_constants.OutputSize)
    {
        return;
    }
    Texture2DArray<float4> sourceTex = ResourceDescriptorHeap[g_constants.SourceIndex];
    RWTexture2DArray<float4> outputTex = ResourceDescriptorHeap[g_constants.OutputIndex];
    const int3 base = int3(dispatchId.xy * 2, dispatchId.z);
    const float4 color = sourceTex.Load(int4(base, 0)) + sourceTex.Load(int4(base + int3(1, 0, 0), 0)) +
        sourceTex.Load(int4(base + int3(0, 1, 0), 0)) + sourceTex.Load(int4(base + int3(1, 1, 0), 0));
    outputTex[dispatchId] = color * 0.25;
}
