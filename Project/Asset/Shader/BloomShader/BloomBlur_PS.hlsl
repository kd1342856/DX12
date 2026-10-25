#include "../PostProcessShader/PostProcessShader.hlsli"

// 1方向のガウスぼかし(横と縦の2回で2次元になる)。
// g_BlurRadiusはガウスの標準偏差(テクセル)。cbPostProcessで可変(ShaderEditorから調整可能)。
// 以前は9タップをg_BlurRadiusテクセルおきに読んでいたため、間のテクセルが抜けて、
// 明るい点(電球など)が飛び飛びに複製された格子の模様になっていた。
// 隣り合う2テクセルをバイリニアの1回の読み込みでまとめて読み、隙間なく重みを付ける
float4 main(PSInput In) : SV_Target0
{
    float2 texSize;
    g_tex.GetDimensions(texSize.x, texSize.y);
    const float2 texelStep = float2(g_BlurDirectionX, g_BlurDirectionY) / texSize;

    const float sigma = max(g_BlurRadius, 0.5);
    // 標準偏差の2.5倍まで読む(それより外の重みは1%未満)。重くなりすぎないよう32テクセルで打ち切る
    const int radius = min((int)ceil(sigma * 2.5), 32);
    const float invTwoSigmaSq = 1.0 / (2.0 * sigma * sigma);

    float3 color = g_tex.Sample(g_ss, In.UV).rgb;
    float weightSum = 1.0;

    for (int i = 1; i <= radius; i += 2)
    {
        // テクセルiとi+1の重みを足し、その重心の位置をバイリニアで読む
        const float weight0 = exp(-float(i * i) * invTwoSigmaSq);
        const float weight1 = exp(-float((i + 1) * (i + 1)) * invTwoSigmaSq);
        const float weight = weight0 + weight1;
        const float offset = (float(i) * weight0 + float(i + 1) * weight1) / weight;

        color += g_tex.Sample(g_ss, In.UV + texelStep * offset).rgb * weight;
        color += g_tex.Sample(g_ss, In.UV - texelStep * offset).rgb * weight;
        weightSum += 2.0 * weight;
    }

    return float4(color / weightSum, 1.0);
}
