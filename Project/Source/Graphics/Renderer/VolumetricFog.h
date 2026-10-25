#pragma once
#include "../GPUResource/GPUTexture/GPUTexture.h"
#include "../GPUResource/CBufferData/CBufferData.h"
#include "../Shader/ComputeShader/ComputeShader.h"

class RenderTarget;
struct FogSettings;

//=============================
//
// ボリュメトリックフォグ(フロクセル)
//
//   視錐台を、画面のkTileSizeピクセル四方 × 奥行きkSliceCount枚(手前ほど薄い)の箱に分け、
//   箱ごとに霧が散乱する光(ライト・影・DDGIの環境光)を求めて、手前から奥へ積み上げる。
//   箱の中で調べる奥行きの位置をフレームごとにずらし、前のフレームの結果と混ぜて滑らかにする。
//   画面には、ピクセルの深さまで積み上げた透過率と散乱光を掛ける(VolumetricFog_CS.hlsl)。
//   今までの距離フォグ(LitShaderの中の指数の2乗のフォグ)の代わりになる。
//
//=============================
class VolumetricFog
{
public:
    static VolumetricFog& Instance()
    {
        static VolumetricFog instance;
        return instance;
    }

    static constexpr UINT kTileSize = 16;
    static constexpr UINT kSliceCount = 64;

    void Initialize(int width, int height);
    void Resize(int width, int height);

    // 霧を計算してpSceneRTの色に掛ける。view・projectionはジッターを含まないカメラの行列、extinctionは消散係数(1/m)
    void Render(RenderTarget* pSceneRT, const Math::Matrix& view, const Math::Matrix& projection,
        const FogSettings& settings, float extinction, const CBufferData::Light& lightData, const CBufferData::System& systemData);

    // 次のフレームで前の結果を使わない(カメラの切り替え・瞬間移動のとき)
    void InvalidateHistory() { m_hasHistory = false; }
    bool IsAvailable() const { return m_isInitialized; }

private:
    VolumetricFog() = default;

    static UINT GridWidth(int width) { return (static_cast<UINT>(width) + kTileSize - 1) / kTileSize; }
    static UINT GridHeight(int height) { return (static_cast<UINT>(height) + kTileSize - 1) / kTileSize; }

    GPUTexture m_scatteringTextures[2]; // 今と前のフレームの散乱(交互に使う)
    GPUTexture m_integratedTexture;
    GPUTexture m_outputTexture;         // 霧を掛けた色(シーンの色へコピーで戻す)
    ComputeShader m_injectShader;
    ComputeShader m_integrateShader;
    ComputeShader m_applyShader;
    bool m_isInitialized = false;
    bool m_hasHistory = false;
    int m_writeIndex = 0;
    uint32_t m_frameIndex = 0;
    Math::Matrix m_previousViewProjection;
};
