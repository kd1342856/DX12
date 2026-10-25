#include "PostProcessShader.hlsli"

// ACES Filmic ToneMapping
float3 ACESFilmicToneMapping(float3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

// 簡易ハッシュ関数(フィルムグレイン・ディザ用)
float Hash(float2 p)
{
    return frac(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453123);
}

// リニアの値をsRGBの伝達関数(IEC 61966-2-1)でエンコードする。
// スワップチェーンのRTVはUNORMのまま(後から描くImGui・SpriteBatchの色を変えないため)なので、ここで行う
float3 LinearToSrgb(float3 linearColor)
{
    float3 low = linearColor * 12.92;
    float3 high = 1.055 * pow(max(linearColor, 0.0), 1.0 / 2.4) - 0.055;
    return select(linearColor <= 0.0031308, low, high);
}

// 8bitに量子化したときの縞(バンディング)を消すディザ。三角分布で±1段階、毎フレーム模様を変える。
// 暗いグラデーションが多いホラーでは、縞が特に目立つ
float DitherNoise(float2 pixelPos)
{
    float2 seed = pixelPos + frac(g_Time * 7.31) * 512.0;
    float noise1 = Hash(seed);
    float noise2 = Hash(seed + float2(37.17, 11.83));
    return (noise1 + noise2 - 1.0) / 255.0;
}

// 深度バッファ(NDC 0..1)からビュー空間の線形距離を求める。
// XMMatrixPerspectiveFovLH相当の投影を前提とした標準的な変換式。
float LinearizeDepth(float depthNdc, float nearZ, float farZ)
{
    return (nearZ * farZ) / (max(farZ - depthNdc * (farZ - nearZ), 0.0001));
}

float4 main(PSInput In) : SV_Target0
{
    //------------------------------------------
    // 色収差 (レンズの色ズレ)。中心から離れるほどRGBのサンプル位置をずらす。
    //------------------------------------------
    float3 sceneColor;
    if (g_ChromaticAberrationIntensity > 0.0001)
    {
        float2 dir = In.UV - 0.5;
        float2 offset = dir * g_ChromaticAberrationIntensity * 0.02;
        // ずらした先が画面外(UV<0 / >1)になると、g_ssがWrapのため反対側の端の色を拾ってしまい、
        // 画面の左右端に赤/水色の縦線が出ていた(しゃがんで手が右端にかかった時など)。画面内にクランプする。
        float r = g_tex.Sample(g_ss, saturate(In.UV + offset)).r;
        float g = g_tex.Sample(g_ss, In.UV).g;
        float b = g_tex.Sample(g_ss, saturate(In.UV - offset)).b;
        sceneColor = float3(r, g, b);
    }
    else
    {
        sceneColor = g_tex.Sample(g_ss, In.UV).rgb;
    }

    //------------------------------------------
    // 被写界深度(DOF)。ピント距離から外れるほど、事前にぼかし済みの
    // g_texDOFへブレンドする(実際のボケ量ではなく簡易近似)。
    //------------------------------------------
    if (g_EnableDOF != 0)
    {
        float depthNdc = g_texDepth.Load(int3(In.Pos.xy, 0)).r;
        float linearDepth = LinearizeDepth(depthNdc, g_CameraNear, g_CameraFar);
        float coc = saturate(abs(linearDepth - g_DOFFocusDistance) / max(g_DOFFocusRange, 0.001));

        float3 blurredColor = g_texDOF.Sample(g_ss, In.UV).rgb;
        sceneColor = lerp(sceneColor, blurredColor, coc);
    }

    //------------------------------------------
    // Bloom加算
    //------------------------------------------
    float3 bloomColor = g_texBloom.Sample(g_ss, In.UV).rgb;
    sceneColor += bloomColor * g_BloomIntensity;

    //------------------------------------------
    // God Rays(光条)加算
    //------------------------------------------
    if (g_EnableGodRays != 0)
    {
        float3 godRaysColor = g_texGodRays.Sample(g_ss, In.UV).rgb;
        sceneColor += godRaysColor * g_GodRaysIntensity;
    }

    //------------------------------------------
    // 周辺減光(ビネット)。レンズの自然な口径食で、像の明るさは光線とレンズの軸のなす角θのcos^4に比例する。
    // 画角からθを求めるので、広角ほど強く効く。光の量の話なので、トーンマップの前のリニアで掛ける
    //------------------------------------------
    if (g_VignetteIntensity > 0.0001)
    {
        const float2 tanAngle = (In.UV * 2.0 - 1.0) * float2(g_TanHalfFovX, g_TanHalfFovY);
        const float tanSq = dot(tanAngle, tanAngle);
        const float cos4 = 1.0 / ((1.0 + tanSq) * (1.0 + tanSq));
        sceneColor *= pow(cos4, g_VignetteIntensity);
    }

    //------------------------------------------
    // フィルムグレイン(センサーのノイズ)。光の粒の数のゆらぎ(ショットノイズ)なので、ゆらぎの大きさは
    // 明るさの平方根に比例する(暗いところほど粒の量は小さいが、明るさに対しては目立つ)。
    // 強さはISO感度に連動する(C++で求める)。トーンマップの前のリニアで足す
    //------------------------------------------
    if (g_FilmGrainIntensity > 0.0001)
    {
        const float2 seed = In.UV * float2(1920.0, 1080.0) + frac(g_Time) * 173.0;
        // 一様な乱数を3つ足して、平均0の山形の分布にする
        const float noise = (Hash(seed) + Hash(seed + 17.31) + Hash(seed + 41.77) - 1.5) * 2.0;
        const float luminance = dot(sceneColor, float3(0.2126, 0.7152, 0.0722));
        sceneColor = max(sceneColor + noise * g_FilmGrainIntensity * sqrt(max(luminance, 0.0)), 0.0);
    }

    //------------------------------------------
    // ToneMapping
    //------------------------------------------
    if (g_EnableHDR != 0)
    {
        // 露出はライトの値に前もって掛けてある(前露出)ので、ここでは掛けない
        sceneColor = ACESFilmicToneMapping(sceneColor);
    }

    // ここまではリニア。ここから先(明るさ)は表示用のsRGBの値で扱う
    sceneColor = LinearToSrgb(saturate(sceneColor));
    // 表示ガンマの微調整(既定の1.0では何もしない)。sRGBへのエンコードは上で済んでいる
    sceneColor = pow(sceneColor, 1.0f / g_Gamma);

    sceneColor = pow(saturate(sceneColor), 1.0 / max(g_BrightnessGamma, 0.01));

    sceneColor += DitherNoise(In.Pos.xy);

    return float4(saturate(sceneColor), 1.0);
}
