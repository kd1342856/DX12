#pragma once
#include "../GPUResource/GPUTexture/GPUTexture.h"
#include "../GPUResource/CBufferData/CBufferData.h"
#include "../Shader/ComputeShader/ComputeShader.h"

class RenderTarget;

//=============================
//
// リファレンスのパストレーサー(開発用)
//
//   レイトレースの加速構造(RaytracingScene)を使い、拡散の反射を繰り返す光線を毎フレーム1ピクセル1本ずつ飛ばして、
//   カメラが止まっている間は結果を足し続ける。エディターのRendererのパネルで切り替え、
//   ラスタライズの描画と見比べて、DDGIや反射プローブ・露出の正しさを確かめるために使う。
//   有効な間は、シーンの色をこの結果で置き換える(SSR・霧・TAA・モーションブラーは掛けない)。
//
//=============================
class PathTracer
{
public:
    static PathTracer& Instance()
    {
        static PathTracer instance;
        return instance;
    }

    void Initialize(int width, int height);
    void Resize(int width, int height);

    // シーンの色を、パストレースの結果で置き換える。view・projectionはジッターを含まないカメラの行列
    void Render(RenderTarget* pSceneRT, const Math::Matrix& view, const Math::Matrix& projection, int maxBounces,
        const CBufferData::Light& lightData, const CBufferData::System& systemData);

    // 足し合わせた数(エディターの表示用)
    uint32_t GetSampleCount() const { return m_sampleIndex; }
    // 足し直す(設定を変えたとき)
    void ResetAccumulation() { m_sampleIndex = 0; }
    bool IsAvailable() const { return m_isInitialized; }

private:
    PathTracer() = default;

    GPUTexture m_accumulationTexture;
    GPUTexture m_outputTexture;
    ComputeShader m_shader;
    bool m_isInitialized = false;
    uint32_t m_sampleIndex = 0;
    int m_maxBounces = 0;
    Math::Matrix m_previousViewProjection;
};
