//================================
//
// バッファの可視化(シェーダーエディターの「バッファの一覧」)
//
//   途中のバッファを、ImGuiでそのまま出せる色(sRGBのRGBA8)に直して書く。
//   書き込み先の大きさに合わせて元のバッファを読むので、縮小と拡大のどちらにも使う。
//
//================================

struct Constants
{
    uint SourceIndex;       // 元のバッファ(SRV)
    uint DestinationIndex;  // 書き込み先(UAV、RGBA8)
    uint DestinationWidth;
    uint DestinationHeight;
    uint Mode;              // BufferViewer::Bufferと同じ番号
    uint SourceWidth;
    uint SourceHeight;
    uint SecondSourceIndex; // 2つのバッファを比べるときの、もう1つ(SRV)
};
ConstantBuffer<Constants> g_constants : register(b0);

cbuffer cbFrame : register(b1)
{
    row_major float4x4 g_mInvProj; // カメラの投影の逆行列(深度から距離を戻す)
};

SamplerState g_pointClamp : register(s0);
SamplerState g_linearClamp : register(s1);

static const uint kModeSceneColor = 0;
static const uint kModeDepth = 1;
static const uint kModeNormal = 2;
static const uint kModeRoughness = 3;
static const uint kModeSSAO = 4;
static const uint kModeVelocity = 5;
static const uint kModeBloom = 6;
static const uint kModeTAAResult = 7;
static const uint kModeTAAChange = 8;

// この距離(m)で黒になる
static const float kFarDistance = 50.0;
// この速さ(ピクセル/フレーム)で一番濃くなる
static const float kFullVelocityPixels = 16.0;
// 前のフレームとの差を何倍に強調するか
static const float kChangeScale = 4.0;

float3 LinearToSrgb(float3 linearColor)
{
    const float3 low = linearColor * 12.92;
    const float3 high = 1.055 * pow(linearColor, 1.0 / 2.4) - 0.055;
    return select(linearColor <= 0.0031308, low, high);
}

// HDRの明るさを0〜1へ丸めてからsRGBにする(画面のトーンマッピングとは別の、見るためだけの丸め)
float3 HDRToDisplay(float3 hdrColor)
{
    const float3 color = max(hdrColor, 0.0);
    return LinearToSrgb(color / (1.0 + color));
}

// 色相(0〜1)から鮮やかな色へ
float3 HueToRgb(float hue)
{
    return saturate(abs(frac(hue + float3(0.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0) - 1.0);
}

float DepthToBrightness(float depth, float2 uv)
{
    const float2 ndc = uv * float2(2.0, -2.0) + float2(-1.0, 1.0);
    const float4 viewPos = mul(float4(ndc, depth, 1.0), g_mInvProj);
    if (abs(viewPos.w) < 1e-6)
    {
        return 0.0;
    }
    const float distance = length(viewPos.xyz / viewPos.w);
    return 1.0 - saturate(log2(1.0 + distance) / log2(1.0 + kFarDistance));
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_constants.DestinationWidth || dispatchId.y >= g_constants.DestinationHeight)
    {
        return;
    }
    RWTexture2D<float4> destination = ResourceDescriptorHeap[g_constants.DestinationIndex];
    const float2 uv = (float2(dispatchId.xy) + 0.5) / float2(g_constants.DestinationWidth, g_constants.DestinationHeight);

    float3 color = 0.0;
    if (g_constants.Mode == kModeDepth)
    {
        Texture2D<float> depthTex = ResourceDescriptorHeap[g_constants.SourceIndex];
        color = DepthToBrightness(depthTex.SampleLevel(g_pointClamp, uv, 0), uv);
    }
    else
    {
        Texture2D<float4> source = ResourceDescriptorHeap[g_constants.SourceIndex];
        if (g_constants.Mode == kModeSceneColor || g_constants.Mode == kModeBloom || g_constants.Mode == kModeTAAResult)
        {
            color = HDRToDisplay(source.SampleLevel(g_linearClamp, uv, 0).rgb);
        }
        else if (g_constants.Mode == kModeNormal)
        {
            // 法線は *0.5+0.5 で入っているので、そのまま色として出す
            color = source.SampleLevel(g_pointClamp, uv, 0).rgb;
        }
        else if (g_constants.Mode == kModeRoughness)
        {
            color = source.SampleLevel(g_pointClamp, uv, 0).aaa;
        }
        else if (g_constants.Mode == kModeSSAO)
        {
            color = source.SampleLevel(g_linearClamp, uv, 0).rrr;
        }
        else if (g_constants.Mode == kModeVelocity)
        {
            // UVの差をピクセルに直し、向きを色相、速さを濃さにする
            const float2 velocityPixels = source.SampleLevel(g_pointClamp, uv, 0).xy * float2(g_constants.SourceWidth, g_constants.SourceHeight);
            const float speed = length(velocityPixels);
            const float hue = atan2(velocityPixels.y, velocityPixels.x) / (2.0 * 3.14159265) + 0.5;
            color = HueToRgb(hue) * saturate(speed / kFullVelocityPixels);
        }
        else if (g_constants.Mode == kModeTAAChange)
        {
            // 画面の色(丸めてsRGBにした後)で比べるので、暗い所の小さな揺れも見える
            Texture2D<float4> previous = ResourceDescriptorHeap[g_constants.SecondSourceIndex];
            const float3 latestColor = HDRToDisplay(source.SampleLevel(g_pointClamp, uv, 0).rgb);
            const float3 previousColor = HDRToDisplay(previous.SampleLevel(g_pointClamp, uv, 0).rgb);
            const float change = saturate(length(latestColor - previousColor) * kChangeScale);
            // 黒 → 赤 → 黄 → 白
            color = saturate(float3(change * 3.0, change * 3.0 - 1.0, change * 3.0 - 2.0));
        }
    }
    destination[dispatchId.xy] = float4(color, 1.0);
}
