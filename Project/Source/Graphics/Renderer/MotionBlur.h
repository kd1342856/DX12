#pragma once
#include "../GPUResource/GPUTexture/GPUTexture.h"
#include "../Shader/ComputeShader/ComputeShader.h"

class RenderTarget;

//=============================
//
// モーションブラー
//
//   速度(MotionVectors)に、シャッターが開いていた割合を掛けた長さだけ色を集めてぼかす。
//   カメラの設定(シャッター速度)を使うときはシャッター速度 ÷ フレームの時間、
//   使わないときはシャッター角(180度で半分)から割合を決める。
//
//=============================
class MotionBlur
{
public:
    static MotionBlur& Instance()
    {
        static MotionBlur instance;
        return instance;
    }

    void Initialize(int width, int height);
    void Resize(int width, int height);

    // pSceneRTの色をぼかした結果で置き換える。shutterFractionはシャッターが開いていた時間 ÷ フレームの時間
    void Apply(RenderTarget* pSceneRT, float shutterFraction, float maxBlurPixels);

    bool IsAvailable() const { return m_isInitialized; }

private:
    MotionBlur() = default;

    GPUTexture m_outputTexture;
    ComputeShader m_shader;
    bool m_isInitialized = false;
    uint32_t m_frameIndex = 0;
};
