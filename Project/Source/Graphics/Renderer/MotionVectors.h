#pragma once
#include "../GPUResource/GPUTexture/GPUTexture.h"
#include "../Shader/ComputeShader/ComputeShader.h"

class RenderTarget;

//=============================
//
// 速度(モーションベクトル)のバッファ
//
//   画面の各ピクセルが、前のフレームから画面の上をどれだけ動いたか(UVの差、R16G16_FLOAT)。
//   TAAの履歴の位置と、モーションブラーの方向に使う。
//   1. カメラの動きによる速度を、シーンの深度からコンピュートで全画面に書く
//   2. 前のフレームから動いた物(スキニング・扉など)だけをRenderSystem::RenderVelocityで描き直して上書きする
//   どちらもジッターを含まない行列で求めるので、ジッターの揺れは速度に入らない。
//
//=============================
class MotionVectors
{
public:
    static MotionVectors& Instance()
    {
        static MotionVectors instance;
        return instance;
    }

    static constexpr DXGI_FORMAT kFormat = DXGI_FORMAT_R16G16_FLOAT;

    void Initialize(int width, int height);
    void Resize(int width, int height);

    // シーンを描く前に、そのフレームのカメラの行列を渡す(projectionはジッターを含まないもの)。
    // 前のフレームの行列はここで入れ替わる
    void BeginFrame(const Math::Matrix& view, const Math::Matrix& projection, const Math::Matrix& jitteredProjection);
    // 次のフレームで前の行列を使わないようにする(カメラの切り替え・瞬間移動のとき)
    void InvalidateHistory() { m_hasPrevious = false; }

    // カメラの動きによる速度を全画面に書く。pSceneRTの深度(メインの描画の後)を読む。
    // 書いた後は速度のテクスチャをRENDER_TARGETの状態にして返す(続けて動く物を描くため)
    void RenderCameraVelocity(RenderTarget* pSceneRT);

    GPUTexture* GetVelocityTexture() { return &m_velocityTexture; }
    const Math::Matrix& GetCurrentViewProjection() const { return m_currentViewProjection; }
    const Math::Matrix& GetPreviousViewProjection() const { return m_previousViewProjection; }
    bool IsValid() const { return m_isInitialized; }

private:
    MotionVectors() = default;

    GPUTexture m_velocityTexture;
    ComputeShader m_cameraVelocityShader;
    bool m_isInitialized = false;

    Math::Matrix m_currentViewProjection;
    Math::Matrix m_previousViewProjection;
    Math::Matrix m_inverseJitteredViewProjection;
    bool m_hasPrevious = false;
};
