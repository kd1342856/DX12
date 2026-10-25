//================================
//
// 自動露出のための輝度のヒストグラム
//
//   シーンの色は前露出(露出を掛けた値)なので、今の露出で割り戻して絶対の輝度(nit)にし、
//   そのlog2をkHistogramBinCount個の区間に数える。結果はCPUへ読み戻し、平均の明るさから露出を決める。
//   重くならないよう、2x2ピクセルにつき1点だけ数える。
//
//================================

static const uint kHistogramBinCount = 64;

struct Constants
{
    uint ColorIndex;     // シーンの色(SRV)
    uint HistogramIndex; // ヒストグラム(RWByteAddressBuffer、UAV)
    uint Width;
    uint Height;
    float MinLog2Luminance;   // 最初の区間の下限(log2 nit)
    float Log2LuminanceRange; // 区間全体の幅(log2)
    float Log2Exposure;       // 今のフレームの露出のlog2。前露出を割り戻すのに使う
};
ConstantBuffer<Constants> g_constants : register(b0);

groupshared uint s_bins[kHistogramBinCount];

[numthreads(kHistogramBinCount, 1, 1)]
void ClearHistogram(uint3 dispatchId : SV_DispatchThreadID)
{
    RWByteAddressBuffer histogram = ResourceDescriptorHeap[g_constants.HistogramIndex];
    histogram.Store(dispatchId.x * 4, 0);
}

[numthreads(16, 16, 1)]
void BuildHistogram(uint3 dispatchId : SV_DispatchThreadID, uint groupIndex : SV_GroupIndex)
{
    if (groupIndex < kHistogramBinCount)
    {
        s_bins[groupIndex] = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    const uint2 pixel = dispatchId.xy * 2;
    if (pixel.x < g_constants.Width && pixel.y < g_constants.Height)
    {
        Texture2D<float4> colorTex = ResourceDescriptorHeap[g_constants.ColorIndex];
        const float3 color = colorTex.Load(int3(pixel, 0)).rgb;
        const float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));
        // 真っ黒(空の外など)は数えない。暗い場面で露出が上がり続けないよう、下限より暗いものは最初の区間に入れる
        if (luminance > 1e-7)
        {
            const float log2Luminance = log2(luminance) - g_constants.Log2Exposure;
            const float normalized = saturate((log2Luminance - g_constants.MinLog2Luminance) / g_constants.Log2LuminanceRange);
            const uint bin = min((uint)(normalized * kHistogramBinCount), kHistogramBinCount - 1);
            InterlockedAdd(s_bins[bin], 1);
        }
    }
    GroupMemoryBarrierWithGroupSync();

    if (groupIndex < kHistogramBinCount && s_bins[groupIndex] > 0)
    {
        RWByteAddressBuffer histogram = ResourceDescriptorHeap[g_constants.HistogramIndex];
        histogram.InterlockedAdd(groupIndex * 4, s_bins[groupIndex]);
    }
}
