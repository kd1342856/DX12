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
    float SSROpaqueIntensity = 0.5f; // Opaque材質向けSSR(床・タイル等)の強さ。ガラスのSSRとは別パラメータ

    // コンタクトシャドウ: 光源の方向へ画面の深度を短くたどり、シャドウマップでは拾えない接地の細かい影を足す
    bool  EnableContactShadows = true;
    float ContactShadowLength = 0.2f; // たどる長さ(m)
};

struct LightingSettings
{
    bool IsDirty = true;
    DirectX::XMFLOAT3 DirectionalLightDir = { 0.5f, -1.0f, 0.3f };
    // 平行光(月)。色は色温度(K。0以下なら使わない)×色味を輝度1に正規化したもので、明るさは照度(lx)で持つ
    DirectX::XMFLOAT3 DirectionalLightColor = { 1.0f, 1.0f, 1.0f };
    float DirectionalLightTemperature = 4100.0f;
    float DirectionalLightIntensity = 0.1f; // 照度 lx(満月の夜で0.1～0.3)
    // 環境光の輝度 nit(cd/m2)。Step 5のGIで置き換えるまでの仮の値
    DirectX::XMFLOAT3 AmbientLight = { 0.02f, 0.02f, 0.02f };
    // 空(スカイスフィアのテクスチャ)の明るさ。テクスチャの値1.0が何nitになるか
    float SkyLuminance = 0.05f;

    // 平行光(月明かり等)のシャドウマップで遮られている場所に残す間接光の下限。
    // 0 = 完全に光源が届かない場所は真っ黒、1 = 影の有無で環境光が変化しない(従来通り)。
    float IndirectShadowFloor = 0.05f;

    // Indoor mask: pixels inside any RoomArea AABB never receive outside light (moon/ambient/sky),
    // treated as fully in the moon's shadow regardless of whether the shadow map actually covers them.
    bool EnableIndoorMask = true;
    float IndoorMargin = 0.3f; // grows each RoomArea AABB by this much (m) so walls/floors on the boundary count as indoor
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
    // ダミーIBL(空と地面の2色)の明るさ。2色の値1.0が何nitになるか
    float IBLIntensity = 0.02f;
};

struct PostProcessSettings
{
    bool IsDirty = true;
    bool EnableHDR = true;
    float Gamma = 1.00f;
    float BrightnessGamma = 1.00f;

    // 露出。EV100で持ち、ライトの値に前もって掛けてからシェーダーへ送る(前露出)。
    // UseCameraSettingsがtrueなら、絞り・シャッター速度・ISO感度からEV100を求める
    float EV100 = 6.0f;
    bool  UseCameraSettings = false;
    float Aperture = 2.8f;            // F値
    float ShutterSpeed = 1.0f / 30.0f; // 秒
    float ISO = 800.0f;

    // TAA(テンポラル・アンチエイリアス)。シェーダーモデル6.6に対応していないGPUでは使えない
    bool  EnableTAA = true;
    float TAACurrentWeight = 0.1f; // 今のフレームを混ぜる割合。小さいほど滑らかだが、動きに残像が出やすい

    // 自動露出。オンのときは、EV100(とカメラの設定)の代わりに画面の明るさから露出を決める
    bool  EnableAutoExposure = false;
    // 暗い場面でもこれより明るくはしない(EV100が小さいほど明るく写る)。夜の暗さが残るよう、手動の既定値(6)から2段まで
    float AutoExposureMinEV100 = 4.0f;
    float AutoExposureMaxEV100 = 10.0f;
    float ExposureCompensation = 0.0f;  // 露出の補正(段)。+で明るく
    float AutoExposureLowPercent = 50.0f;  // 平均に使う範囲(暗い方から数えた割合%)。真っ暗な影に引っ張られないように
    float AutoExposureHighPercent = 95.0f; // 光源そのものに引っ張られないように、明るい側も除く
    float AutoExposureSpeedUp = 3.0f;   // 明るい場面へ順応する速さ(1/秒)
    float AutoExposureSpeedDown = 1.0f; // 暗い場面へ順応する速さ(1/秒)。目と同じく暗さへの順応は遅い

    // モーションブラー。ぼけの長さはシャッターが開いていた時間に連動する。
    // UseCameraSettingsがtrueならシャッター速度とフレームの時間から、falseならシャッター角から決める
    bool  EnableMotionBlur = false;
    float MotionBlurShutterAngle = 180.0f; // 度。360で1フレームの間ずっと開いている
    float MotionBlurMaxPixels = 32.0f;     // ぼけの長さの上限(ピクセル)

    // Bloom - 半径・反復回数を上げてはじめて「ふわっと広がる」見た目になる。
    // 閾値/強度だけいじっても変化が薄いのはこのため。
    float BloomThreshold = 1.0f;
    float BloomIntensity = 1.0f;
    float BloomRadius = 8.0f;     // 1回のBlurパスでのぼかし半径(テクセル)
    int   BloomIterations = 3;    // Blur(横+縦)を何回繰り返すか。CPU側のみで使用(GPUには送らない)

    // Vignette
    bool  EnableVignette = true;
    // 自然な周辺減光(cos^4則)の効き。1で画角どおり、0で無し
    float VignetteIntensity = 1.0f;
    float VignetteSmoothness = 0.6f;

    // Film Grain
    bool  EnableFilmGrain = false;
    float FilmGrainIntensity = 0.035f; // ISO 800での強さ。ISO感度の平方根に比例して強くなる

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

    // ボリュメトリックフォグ(フロクセル)。オンのときは、上の濃さを消散係数(1/m)として使い、
    // FogColorを散乱の割合(色)として使う。霧の明るさはライト・影・DDGIの環境光から求まる
    bool  EnableVolumetricFog = true;
    float VolumetricFogAnisotropy = 0.3f;    // 正で前方散乱(光の方を向くと霧が明るく光る)
    float VolumetricFogFar = 40.0f;          // フロクセルの奥の端(m)。家の大きさに合わせる
    float VolumetricFogHistoryWeight = 0.9f; // 前のフレームの結果を残す割合
};
