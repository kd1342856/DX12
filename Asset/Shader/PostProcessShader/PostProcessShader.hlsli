Texture2D g_tex : register(t0);      // シーンHDR (Bloom/GodRays系パスでは共通のsrcテクスチャとしても使う)
Texture2D g_texBloom : register(t1); // Bloom合成結果 (最終合成パスのみ使用)
Texture2D g_texDOF : register(t2);   // 被写界深度用: あらかじめぼかしたシーンのコピー (最終合成パスのみ使用)
Texture2D g_texDepth : register(t3); // シーン深度 (最終合成パスのみ使用。DOFのCoC計算用)
Texture2D g_texGodRays : register(t4); // God Rays(光条)の結果 (最終合成パスのみ使用)
SamplerState g_ss : register(s0);

// PostProcess関連の全パス(Extract/Blur/最終合成)で共有するcbuffer。
// 新しいポストエフェクトを追加する時は、ここにパラメータを足して
// PostProcessShader_PS.hlsl側で使うだけでよい(拡張性のため)。
cbuffer cbPostProcess : register(b0)
{
    //---------------------------------
    // Tonemap / Exposure
    //---------------------------------
    float g_Exposure;
    float g_Gamma;
    uint  g_EnableHDR;
    float g_Time; // 経過時間(秒) - フィルムグレイン等アニメーションするエフェクト用

    //---------------------------------
    // Bloom
    //---------------------------------
    float g_BloomThreshold;
    float g_BloomIntensity;
    float g_BlurDirectionX; // Blurパス専用(ExtractOverride等では未使用)
    float g_BlurDirectionY;

    float g_BlurRadius; // Blurパスのぼかし半径(テクセル単位、可変)
    //---------------------------------
    // Vignette
    //---------------------------------
    float g_VignetteIntensity;
    float g_VignetteSmoothness;
    //---------------------------------
    // Film Grain
    //---------------------------------
    float g_FilmGrainIntensity;

    //---------------------------------
    // Chromatic Aberration
    //---------------------------------
    float g_ChromaticAberrationIntensity;
    //---------------------------------
    // Depth of Field
    //---------------------------------
    uint  g_EnableDOF;
    float g_DOFFocusDistance;
    float g_DOFFocusRange;

    // カメラのNear/Far平面 (深度バッファのリニア化用)
    float g_CameraNear;
    float g_CameraFar;
    // God Rays(平行光の光条)用: 光源をスクリーン空間に投影したUV(画面外の値もありうる)
    float g_GodRaysLightU;
    float g_GodRaysLightV;

    //---------------------------------
    // God Rays
    //---------------------------------
    float g_GodRaysDensity;   // サンプル間隔の広さ
    float g_GodRaysDecay;     // サンプルごとの減衰率
    float g_GodRaysWeight;    // 各サンプルの寄与
    float g_GodRaysExposure;  // 最終的な強さ

    uint  g_GodRaysNumSamples; // ラジアルブラーのサンプル数
    uint  g_EnableGodRays;     // 0/1 (光源がカメラの後ろにある等、無効な時も0)
    float g_GodRaysIntensity;  // 最終合成時にシーンへ加算する強さ
    float g_PadPostProcess;
};

struct PSInput
{
    float4 Pos : SV_Position;
    float2 UV : TEXCOORD0;
};
