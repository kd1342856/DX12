//================================
//
// HDR出力の最後の合成
//
//   ポストの結果(線形、BT.709、1.0が白)の上にUI(sRGBでエンコードされたSDR)を重ね、
//   白を何nitで出すか(g_PaperWhiteNits)を掛けてBT.2020へ変換し、PQ(ST.2084)でエンコードする。
//
//================================
// 頂点シェーダーはPostProcessShader_VS.hlslを使う(全画面の三角形)
struct PSInput
{
    float4 Pos : SV_Position;
    float2 UV : TEXCOORD0;
};

Texture2D g_sceneTex : register(t0); // ポストの結果
Texture2D g_uiTex : register(t1);    // UI(スプライト・ImGui)。透明から描いてあるので、色はアルファを掛けた後の値

cbuffer cbHDRComposite : register(b0)
{
    float g_PaperWhiteNits;
    float3 g_HDRCompositePad;
};

float3 SrgbToLinear(float3 srgb)
{
    const float3 low = srgb / 12.92;
    const float3 high = pow(max((srgb + 0.055) / 1.055, 0.0), 2.4);
    return select(srgb <= 0.04045, low, high);
}

// BT.709の原色からBT.2020の原色へ
float3 Rec709ToRec2020(float3 color)
{
    const float3x3 conversion = float3x3(
        0.627402, 0.329292, 0.043306,
        0.069095, 0.919544, 0.011360,
        0.016394, 0.088028, 0.895578);
    return mul(conversion, color);
}

// PQ(ST.2084)。入力は10000nitを1とした輝度
float3 LinearToPQ(float3 normalized)
{
    const float m1 = 2610.0 / 16384.0;
    const float m2 = 2523.0 / 4096.0 * 128.0;
    const float c1 = 3424.0 / 4096.0;
    const float c2 = 2413.0 / 4096.0 * 32.0;
    const float c3 = 2392.0 / 4096.0 * 32.0;
    const float3 p = pow(saturate(normalized), m1);
    return pow((c1 + c2 * p) / (1.0 + c3 * p), m2);
}

float4 main(PSInput In) : SV_Target0
{
    const int3 pixel = int3(In.Pos.xy, 0);
    const float3 scene = g_sceneTex.Load(pixel).rgb;
    const float4 ui = g_uiTex.Load(pixel);
    // UIは白(1.0)が白の明るさになるよう、sRGBを線形に戻して重ねる
    const float3 color = SrgbToLinear(ui.rgb) + scene * (1.0 - ui.a);
    const float3 nits = Rec709ToRec2020(max(color, 0.0)) * g_PaperWhiteNits;
    return float4(LinearToPQ(nits / 10000.0), 1.0);
}
