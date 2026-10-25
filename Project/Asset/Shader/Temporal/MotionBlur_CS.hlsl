//================================
//
// モーションブラー
//
//   各ピクセルの速度(前のフレームからの画面上の動き)に、シャッターが開いていた割合を掛けた長さだけ、
//   その方向に色を集めてぼかす。シャッターが開いている間の動きを写すので、ぼけの長さはシャッター速度に連動する。
//   サンプルの位置はピクセルごとにずらし、少ないサンプル数でも縞にならないようにする。
//   (速度は物ごとに描いてあるので、カメラだけが動いたときも、物だけが動いたときもぼける)
//
//================================

struct Constants
{
    uint ColorIndex;    // シーンの色(SRV)
    uint VelocityIndex; // 速度(SRV)
    uint OutputIndex;   // 結果(UAV)
    uint Width;
    uint Height;
    float ShutterFraction; // シャッターが開いていた時間 ÷ フレームの時間(0〜1)
    float MaxBlurPixels;   // ぼけの長さの上限(ピクセル)
    uint FrameIndex;       // サンプルのずらし方をフレームごとに変える
};
ConstantBuffer<Constants> g_constants : register(b0);

SamplerState g_pointClamp : register(s0);
SamplerState g_linearClamp : register(s1);

static const int kSampleCount = 8;

// ピクセルごとに散らばった0〜1の値(Interleaved Gradient Noise)
float InterleavedGradientNoise(float2 pixel, uint frameIndex)
{
    pixel += 5.588238 * float(frameIndex % 64);
    return frac(52.9829189 * frac(dot(pixel, float2(0.06711056, 0.00583715))));
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_constants.Width || dispatchId.y >= g_constants.Height)
    {
        return;
    }
    Texture2D<float4> colorTex = ResourceDescriptorHeap[g_constants.ColorIndex];
    Texture2D<float2> velocityTex = ResourceDescriptorHeap[g_constants.VelocityIndex];
    RWTexture2D<float4> outputTex = ResourceDescriptorHeap[g_constants.OutputIndex];

    const int2 pixel = int2(dispatchId.xy);
    const float2 size = float2(g_constants.Width, g_constants.Height);
    const float2 uv = (float2(pixel) + 0.5) / size;
    const float4 centerColor = colorTex.Load(int3(pixel, 0));

    // シャッターが開いている間の動き(UV)。長すぎる動きは上限で切る
    float2 blurVector = velocityTex.Load(int3(pixel, 0)) * g_constants.ShutterFraction;
    const float blurPixels = length(blurVector * size);
    if (blurPixels < 0.5)
    {
        outputTex[pixel] = centerColor;
        return;
    }
    if (blurPixels > g_constants.MaxBlurPixels)
    {
        blurVector *= g_constants.MaxBlurPixels / blurPixels;
    }

    // シャッターが開いた瞬間から閉じた瞬間まで(今の位置を中心に前後へ半分ずつ)を均等に集める
    const float noise = InterleavedGradientNoise(float2(pixel), g_constants.FrameIndex);
    float3 color = 0.0;
    [unroll]
    for (int i = 0; i < kSampleCount; ++i)
    {
        const float t = (float(i) + noise) / float(kSampleCount) - 0.5;
        color += colorTex.SampleLevel(g_linearClamp, uv + blurVector * t, 0).rgb;
    }
    outputTex[pixel] = float4(color / float(kSampleCount), centerColor.a);
}
