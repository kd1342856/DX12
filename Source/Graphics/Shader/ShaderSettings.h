#pragma once

enum class DebugView : int
{
    Final = 0, Albedo, Normal, WorldNormal, Roughness, Metallic, AO, Depth, Position, Emissive
};

struct DebugViewItem
{
    DebugView Value;
    const char* Name;
};

constexpr DebugViewItem DebugViewNames[] = {
    { DebugView::Final, "Final" },
    { DebugView::Albedo, "Albedo" },
    { DebugView::Normal, "Normal" },
    { DebugView::WorldNormal, "World Normal" },
    { DebugView::Roughness, "Roughness" },
    { DebugView::Metallic, "Metallic" },
    { DebugView::AO, "AO" },
    { DebugView::Depth, "Depth" },
    { DebugView::Position, "Position" },
    { DebugView::Emissive, "Emissive" }
};

struct RendererSettings
{
    bool IsDirty = true;
    bool EnablePBR = true;

    // SSR (スクリーンスペース反射)。平面反射(専用の反射カメラ設定)が無いガラス等の
    // フォールバックとして、Opaqueパスの深度+カラーをレイマーチする。
    bool  EnableSSR = true;
    float SSRStepSize = 0.35f; // ビュー空間での1ステップの距離
};

struct LightingSettings
{
    bool IsDirty = true;
    DirectX::XMFLOAT3 DirectionalLightDir = { 0.5f, -1.0f, 0.3f };
    DirectX::XMFLOAT3 DirectionalLightColor = { 1.0f, 1.0f, 1.0f };
    float DirectionalLightIntensity = 1.0f;
    DirectX::XMFLOAT3 AmbientLight = { 0.35f, 0.35f, 0.35f };

    // 平行光(月明かり等)のシャドウマップで遮られている場所に残す間接光の下限。
    // 0 = 完全に光源が届かない場所は真っ黒、1 = 影の有無で環境光が変化しない(従来通り)。
    float IndirectShadowFloor = 0.05f;
};

struct ShadowSettings
{
    bool IsDirty = true;
    bool EnableShadows = true;
    float ShadowPower = 2.5f;
    float ShadowBias = 0.001f;
    DirectX::XMFLOAT4 CascadeSplits = { 0.1f, 0.3f, 0.6f, 1.0f };
};

struct IBLSettings
{
    bool IsDirty = true;
    bool EnableIBL = true;
    float IBLIntensity = 1.0f;
};

struct PostProcessSettings
{
    bool IsDirty = true;
    bool EnableHDR = true;
    float Gamma = 1.00f;
    float Exposure = 0.30f;

    // Bloom - 半径・反復回数を上げてはじめて「ふわっと広がる」見た目になる。
    // 閾値/強度だけいじっても変化が薄いのはこのため。
    float BloomThreshold = 1.0f;
    float BloomIntensity = 1.0f;
    float BloomRadius = 8.0f;     // 1回のBlurパスでのぼかし半径(テクセル)
    int   BloomIterations = 3;    // Blur(横+縦)を何回繰り返すか。CPU側のみで使用(GPUには送らない)

    // Vignette
    bool  EnableVignette = true;
    float VignetteIntensity = 1.4f;
    float VignetteSmoothness = 0.6f;

    // Film Grain
    bool  EnableFilmGrain = true;
    float FilmGrainIntensity = 0.035f;

    // Chromatic Aberration
    bool  EnableChromaticAberration = true;
    float ChromaticAberrationIntensity = 0.4f;

    // Depth of Field
    bool  EnableDOF = false;
    float DOFFocusDistance = 5.0f;
    float DOFFocusRange = 8.0f;

    // God Rays (平行光の光条。既存の平行光/シャドウマップは使わず、Bloom結果を
    // 光源のスクリーン座標からラジアルブラーする軽量な近似)
    bool  EnableGodRays = false;
    float GodRaysDensity = 0.9f;
    float GodRaysDecay = 0.96f;
    float GodRaysWeight = 0.4f;
    float GodRaysExposure = 1.0f;
    float GodRaysIntensity = 1.0f;
    int   GodRaysNumSamples = 48;
};

struct DebugSettings
{
    bool IsDirty = true;
    DebugView CurrentDebugView = DebugView::Final;
};

struct SSAOSettings
{
    bool  EnableSSAO = true;
    float Radius = 0.5f;
    float Bias = 0.03f;
    float Power = 1.5f;
    float Intensity = 1.0f;
};

struct FogSettings
{
    bool IsDirty = true;

    // Distance fog (always present at FogDensity from the start of a match; grows to
    // HuntFogDensity while a ghost is hunting)
    bool EnableFog = true;
    DirectX::XMFLOAT3 FogColor = { 0.5f, 0.5f, 0.55f };
    float FogDensity = 0.02f;      // Density under normal conditions
    float HuntFogDensity = 0.18f;  // Density while a ghost is hunting

    // Screen darkening while a ghost is hunting (scales ambient + directional light down)
    bool EnableHuntDarken = true;
    float HuntDarkenAmount = 0.6f; // 0 = no darkening, 1 = lights fully off at full hunt intensity

    // Shared normal<->hunt blend rate, in "fraction per second" (1.0 = full transition in ~1s).
    // Drives both the fog density interpolation and the darkening above, so they move together.
    float HuntTransitionSpeed = 0.8f;
};
