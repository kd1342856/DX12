#include "../Common/ShaderCore.hlsli"

Texture2D g_skyTex : register(t0);

struct PSInput
{
    float4 Pos : SV_Position;
    float2 UV : TEXCOORD0;
};

float4 main(PSInput In) : SV_Target0
{
    // 空のテクスチャはBaseColorのスロットなので_SRGBフォーマットで読み込まれ、サンプルした時点でリニアになっている。
    // (以前はここでpow(2.2)していたが、ハードウェアの変換と二重になるので外した)
    float4 color = g_skyTex.Sample(g_ss_linear_wrap, In.UV);

    // Can add color tinting, rotation, or twinkling here later.
    return color;
}
