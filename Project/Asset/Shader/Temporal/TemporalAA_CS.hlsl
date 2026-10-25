//================================
//
// TAA(テンポラル・アンチエイリアス)
//
//   毎フレーム投影をピクセル以下ずらして(ジッター)描いた絵を、速度で前のフレームの位置を
//   たどった履歴と混ぜ、ネイティブ解像度のままジャギーとちらつきを抑える。
//   - 速度は3x3の中で一番手前のピクセルのものを使う(輪郭が背景の速度で引きずられないように)
//   - 履歴はCatmull-Romで読む(バイリニアより、混ぜ続けてもぼけにくい)
//   - 履歴は今の3x3の色の分布(YCoCgの平均±標準偏差)の範囲へ寄せる(ゴーストを抑える)
//   - 明るいピクセルほど重みを下げて混ぜる(HDRの明るい点がちらつかないように)
//
//================================

struct Constants
{
    uint ColorIndex;    // 今のフレームのシーンの色(SRV)
    uint DepthIndex;    // シーンの深度(SRV)
    uint VelocityIndex; // 速度(SRV)
    uint HistoryIndex;  // 前のフレームの結果(SRV)
    uint OutputIndex;   // 今のフレームの結果(UAV)。次のフレームの履歴になる
    uint Width;
    uint Height;
    uint ResetHistory;  // 1なら履歴を使わない(最初のフレーム・解像度を変えた後など)
    float CurrentWeight; // 今のフレームを混ぜる割合(小さいほど滑らかだが、動きに遅れて残像が出やすい)
};
ConstantBuffer<Constants> g_constants : register(b0);

SamplerState g_linearClamp : register(s1);

float3 RGBToYCoCg(float3 c)
{
    return float3(
        0.25 * c.r + 0.5 * c.g + 0.25 * c.b,
        0.5 * c.r - 0.5 * c.b,
        -0.25 * c.r + 0.5 * c.g - 0.25 * c.b);
}

float3 YCoCgToRGB(float3 c)
{
    return float3(c.x + c.y - c.z, c.x + c.z, c.x - c.y - c.z);
}

// 明るさで重みを付ける(Karis)。YCoCgのYを輝度として使う
float LumaWeight(float3 yCoCg)
{
    return 1.0 / (1.0 + max(yCoCg.x, 0.0));
}

// 9タップのCatmull-Romを、バイリニアの5回の読み込みで近似する
float3 SampleHistoryCatmullRom(Texture2D<float4> historyTex, float2 uv, float2 size)
{
    const float2 samplePos = uv * size;
    const float2 texPos1 = floor(samplePos - 0.5) + 0.5;
    const float2 f = samplePos - texPos1;

    const float2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f));
    const float2 w1 = 1.0 + f * f * (-2.5 + 1.5 * f);
    const float2 w2 = f * (0.5 + f * (2.0 - 1.5 * f));
    const float2 w3 = f * f * (-0.5 + 0.5 * f);

    const float2 w12 = w1 + w2;
    const float2 offset12 = w2 / w12;

    const float2 invSize = 1.0 / size;
    const float2 texPos0 = (texPos1 - 1.0) * invSize;
    const float2 texPos3 = (texPos1 + 2.0) * invSize;
    const float2 texPos12 = (texPos1 + offset12) * invSize;

    float3 result = 0.0;
    float weightSum = 0.0;
    float w;
    w = w12.x * w0.y;  result += historyTex.SampleLevel(g_linearClamp, float2(texPos12.x, texPos0.y), 0).rgb * w;  weightSum += w;
    w = w0.x * w12.y;  result += historyTex.SampleLevel(g_linearClamp, float2(texPos0.x, texPos12.y), 0).rgb * w;  weightSum += w;
    w = w12.x * w12.y; result += historyTex.SampleLevel(g_linearClamp, float2(texPos12.x, texPos12.y), 0).rgb * w; weightSum += w;
    w = w3.x * w12.y;  result += historyTex.SampleLevel(g_linearClamp, float2(texPos3.x, texPos12.y), 0).rgb * w;  weightSum += w;
    w = w12.x * w3.y;  result += historyTex.SampleLevel(g_linearClamp, float2(texPos12.x, texPos3.y), 0).rgb * w;  weightSum += w;
    return max(result / max(weightSum, 1e-4), 0.0);
}

// 履歴(YCoCg)を、範囲[minColor, maxColor]の箱へ、箱の中心に向かって寄せる
float3 ClipToAABB(float3 history, float3 minColor, float3 maxColor)
{
    const float3 center = 0.5 * (maxColor + minColor);
    const float3 extents = 0.5 * (maxColor - minColor) + 1e-5;
    const float3 offset = history - center;
    const float3 unit = abs(offset / extents);
    const float maxUnit = max(unit.x, max(unit.y, unit.z));
    return (maxUnit > 1.0) ? center + offset / maxUnit : history;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_constants.Width || dispatchId.y >= g_constants.Height)
    {
        return;
    }
    Texture2D<float4> colorTex = ResourceDescriptorHeap[g_constants.ColorIndex];
    Texture2D<float> depthTex = ResourceDescriptorHeap[g_constants.DepthIndex];
    Texture2D<float2> velocityTex = ResourceDescriptorHeap[g_constants.VelocityIndex];
    Texture2D<float4> historyTex = ResourceDescriptorHeap[g_constants.HistoryIndex];
    RWTexture2D<float4> outputTex = ResourceDescriptorHeap[g_constants.OutputIndex];

    const int2 pixel = int2(dispatchId.xy);
    const int2 maxPixel = int2(g_constants.Width, g_constants.Height) - 1;
    const float2 size = float2(g_constants.Width, g_constants.Height);

    // 3x3の色の平均と標準偏差(YCoCg)、一番手前のピクセル
    float3 center = 0.0;
    float3 moment1 = 0.0;
    float3 moment2 = 0.0;
    float closestDepth = 1.0;
    int2 closestPixel = pixel;
    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            const int2 samplePixel = clamp(pixel + int2(x, y), 0, maxPixel);
            const float3 sampleColor = RGBToYCoCg(max(colorTex.Load(int3(samplePixel, 0)).rgb, 0.0));
            moment1 += sampleColor;
            moment2 += sampleColor * sampleColor;
            if (x == 0 && y == 0)
            {
                center = sampleColor;
            }
            const float sampleDepth = depthTex.Load(int3(samplePixel, 0));
            if (sampleDepth < closestDepth)
            {
                closestDepth = sampleDepth;
                closestPixel = samplePixel;
            }
        }
    }

    const float2 uv = (float2(pixel) + 0.5) / size;
    const float2 velocity = velocityTex.Load(int3(closestPixel, 0));
    const float2 prevUV = uv - velocity;

    const bool isOutside = any(prevUV < 0.0) || any(prevUV > 1.0);
    if (g_constants.ResetHistory != 0 || isOutside)
    {
        outputTex[pixel] = float4(YCoCgToRGB(center), 1.0);
        return;
    }

    float3 history = RGBToYCoCg(SampleHistoryCatmullRom(historyTex, prevUV, size));
    // 履歴が壊れていたら(NaNなど)今の色で置き換える
    if (any(isnan(history)) || any(isinf(history)))
    {
        history = center;
    }

    // 分散で決めた箱へ履歴を寄せる。箱の広さは標準偏差の1倍
    const float3 mean = moment1 / 9.0;
    const float3 sigma = sqrt(abs(moment2 / 9.0 - mean * mean));
    history = ClipToAABB(history, mean - sigma, mean + sigma);

    // 速く動いているところは今のフレームを多めに混ぜ、残像を短くする
    const float velocityPixels = length(velocity * size);
    const float currentWeight = lerp(g_constants.CurrentWeight, 0.5, saturate(velocityPixels / 40.0));

    const float currentLumaWeight = LumaWeight(center) * currentWeight;
    const float historyLumaWeight = LumaWeight(history) * (1.0 - currentWeight);
    const float3 result = (center * currentLumaWeight + history * historyLumaWeight) / (currentLumaWeight + historyLumaWeight);

    outputTex[pixel] = float4(max(YCoCgToRGB(result), 0.0), 1.0);
}
