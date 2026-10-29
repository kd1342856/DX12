#pragma once
#include "../GPUResource/GPUTexture/GPUTexture.h"
#include "../Shader/ComputeShader/ComputeShader.h"

class RenderTarget;

//=============================
//
// GIのプローブの表示(エディターのデバッグビュー)
//
//   DDGIのプローブを、そのプローブの放射照度で照らした球としてシーンの色に描き込む(ProbeView_CS.hlsl)。
//   球はシーンの深度で隠れる。使えない(壁や物に埋まった)プローブは赤い。
//   シーンのRTにはUAVが無いので、別のテクスチャに書いてからシーンへ写す(TAAと同じ)。
//   エディターのカメラで、表示をオンにしているときだけ描く(保存しない)。
//
//=============================
class ProbeViewer
{
public:
    static ProbeViewer& Instance()
    {
        static ProbeViewer instance;
        return instance;
    }

    void SetEnabled(bool isEnabled) { m_isEnabled = isEnabled; }
    bool IsEnabled() const { return m_isEnabled; }
    void SetRadius(float radius) { m_radius = radius; }
    float GetRadius() const { return m_radius; }
    // 球の明るさを何段持ち上げるか(暗い部屋でも見えるように。見え方だけで、GIの値は変えない)
    void SetBrightnessEV(float brightnessEV) { m_brightnessEV = brightnessEV; }
    float GetBrightnessEV() const { return m_brightnessEV; }
    bool IsAvailable() const { return ComputeShader::IsSupported() && !m_hasFailed; }

    // シーンを描いた後(速度の後、ポストの前)に呼ぶ。view・projectionはそのフレームのカメラ
    void Render(RenderTarget* pSceneRT, const Math::Matrix& view, const Math::Matrix& projection);

private:
    ProbeViewer() = default;
    bool EnsureResources(UINT width, UINT height, DXGI_FORMAT colorFormat);

    ComputeShader m_clearShader;
    ComputeShader m_splatShader;
    ComputeShader m_shadeShader;
    GPUTexture m_probeDepth;
    GPUTexture m_output;
    bool m_isEnabled = false;
    float m_radius = 0.1f;
    float m_brightnessEV = 4.0f;
    bool m_isCreated = false;
    bool m_hasFailed = false;
};
