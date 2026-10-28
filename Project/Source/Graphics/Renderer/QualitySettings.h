#pragma once

//=============================
//
// 画質設定(スケーラビリティ)
//
//   低・中・高・最高のプリセットで、描画の重さに効く設定をまとめて切り替える。
//   基準は「高」(RTX 3060・1080pで目標の性能を満たす設定)。
//   解像度やテクスチャを作り直す必要があるので、切り替えはフレームの先頭(GDF::BeginFrame)で
//   GPUの処理を待ってから反映する。
//
//=============================
enum class QualityLevel
{
    Low,
    Medium,
    High,
    Ultra,
};

struct QualityPreset
{
    float renderScale;                 // 描画解像度 = ウィンドウ解像度 × この値
    UINT directionalShadowResolution;  // 平行光(月)の影
    UINT spotShadowResolution;         // スポットライトの影(2D配列の1枚)
    UINT cubeShadowResolution;         // 点光源と広いスポットの影(キューブの1面)
    bool enableSSAO;
    bool enableSSR;
    int maxPlanarReflections;          // 同時に描く平面反射(窓)の数(0～3)
    UINT planarReflectionResolution;
    int bloomIterations;
    bool raytracedReflections;         // レイトレースの反射(滑らかな面だけ)
};

class QualitySettings
{
public:
    static const QualityPreset& GetPreset(QualityLevel level);

    // 画質を変える。次のフレームの先頭で反映される
    static void Request(QualityLevel level);
    static QualityLevel GetLevel() { return s_level; }
    static const QualityPreset& GetCurrent() { return GetPreset(s_level); }

    // 変更が要求されていれば、GPUの処理を待って影・平面反射の解像度と各設定を反映する。
    // 反映したらtrue(呼び出し側で描画解像度のRTを作り直す)。フレームの先頭(コマンドを記録する前)で呼ぶ
    static bool ApplyPending();

    // ウィンドウ解像度から描画解像度を求める
    static int ToRenderSize(UINT windowSize);

    // 描画解像度の倍率。エディターで上書きしていればその値、していなければ画質のプリセットの値
    static float GetRenderScale() { return (s_renderScaleOverride > 0.0f) ? s_renderScaleOverride : GetCurrent().renderScale; }
    // 描画解像度の倍率をエディターから上書きする(0でプリセットの値に戻す)。保存はしない(開発用)。次のフレームの先頭で反映される
    static void RequestRenderScaleOverride(float scale);
    static float GetRenderScaleOverride() { return s_renderScaleOverride; }

private:
    static inline QualityLevel s_level = QualityLevel::High;
    static inline QualityLevel s_requestedLevel = QualityLevel::High;
    static inline bool s_isRequested = false;
    static inline float s_renderScaleOverride = 0.0f;
    static inline float s_requestedRenderScaleOverride = 0.0f;
    static inline bool s_isRenderScaleRequested = false;
};
