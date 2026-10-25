#pragma once
#include <random>
#include "../GPUResource/GPUTexture/GPUTexture.h"
#include "../GPUResource/CBufferData/CBufferData.h"
#include "../Shader/ComputeShader/ComputeShader.h"

//=============================
//
// DDGI(Dynamic Diffuse Global Illumination)のボリューム
//
//   家(部屋の箱をまとめた範囲)に、約1m間隔で格子状にプローブを並べる。
//   毎フレーム、全部のプローブからレイを飛ばし(インラインのレイトレース)、当たった所の明るさで
//   プローブの放射照度と、遮られているかを調べるための距離を少しずつ更新する。
//   懐中電灯で照らした壁の跳ね返りや、点けた照明の光が部屋に回る様子を、フレームを重ねて求める。
//   描画は、そのピクセルを囲む8つのプローブから拡散の環境光を読む(inc_DDGI.hlsli)。
//
//=============================
class DDGIVolume
{
public:
    static DDGIVolume& Instance()
    {
        static DDGIVolume instance;
        return instance;
    }

    // プローブ1つのテクセル数(内側)。inc_DDGI.hlsliと合わせる
    static constexpr UINT kIrradianceTexels = 6;
    static constexpr UINT kDistanceTexels = 14;
    // DDGI_CS.hlslのTraceRaysのスレッドグループの大きさと合わせる
    static constexpr UINT kRaysPerProbe = 64;
    static constexpr int kMaxProbesPerAxis = 32;
    // テクスチャにプローブのタイルを1行に並べる数
    static constexpr UINT kTilesPerRow = 64;
    // プローブの間隔の目安(m)。家が大きいときは、1軸のプローブの数が上限を超えないよう広げる
    static constexpr float kTargetSpacing = 1.0f;
    // シェーダーのレジスタ(放射照度t23、距離t24)
    static constexpr uint32_t kIrradianceRegister = 23;
    static constexpr uint32_t kDistanceRegister = 24;

    void Initialize();

    // プローブを並べる範囲。変わったらテクスチャを作り直す(フレームの描画を始める前に呼ぶ)
    void SetBounds(const Math::Vector3& boundsMin, const Math::Vector3& boundsMax);
    // プローブを更新する。加速構造(RaytracingScene::Build)を作った後、メインの描画の前に呼ぶ
    void Update(const CBufferData::Light& lightData, const CBufferData::System& systemData);
    // ライトの定数バッファに、プローブの並びと有効かどうかを書く
    void FillShaderData(CBufferData::Light& lightData) const;

    // 描画から読めるか(1回以上更新したか)
    bool HasData() const { return m_hasData; }
    bool IsAvailable() const { return m_isInitialized; }
    int GetIrradianceSRVIndex() const { return m_irradianceTexture.GetSRVIndex(); }
    int GetDistanceSRVIndex() const { return m_distanceTexture.GetSRVIndex(); }

    // 前の値を残す割合(大きいほどちらつかないが、照明の変化に遅れる)
    void SetHysteresis(float hysteresis) { m_hysteresis = hysteresis; }
    void SetEnabled(bool isEnabled) { m_isEnabled = isEnabled; }
    bool IsEnabled() const { return m_isEnabled; }
    UINT GetProbeCount() const { return m_probeCount; }

private:
    DDGIVolume() = default;

    void ResizeTextures();
    Math::Matrix MakeRandomRotation();

    GPUTexture m_rayDataTexture;
    GPUTexture m_irradianceTexture;
    GPUTexture m_distanceTexture;
    ComputeShader m_traceShader;
    ComputeShader m_irradianceShader;
    ComputeShader m_distanceShader;
    ComputeShader m_borderShader;
    bool m_isInitialized = false;
    bool m_isEnabled = true;
    bool m_hasData = false;
    bool m_isHistoryValid = false;

    Math::Vector3 m_origin = Math::Vector3::Zero;
    float m_spacing = kTargetSpacing;
    int m_counts[3] = { 2, 2, 2 };
    UINT m_probeCount = 0;
    float m_hysteresis = 0.97f;
    std::mt19937 m_random{ 12345u };
};
