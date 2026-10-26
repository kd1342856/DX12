#pragma once
#include "../GraphicsShader/GraphicsShader.h"

class RenderTarget;

//=============================
//
// HDR出力の最後の合成
//
//   ポストの結果(線形)とUI(SDR)を重ね、BT.2020とPQに変換してスワップチェーン(R10G10B10A2)へ書く。
//   GraphicsDevice::EndFrameが、HDR出力のときだけ呼ぶ。
//
//=============================
class HDRCompositeShader : public GraphicsShader
{
public:
    virtual void Create(GraphicsDevice* pGraphicsDevice) override;

    // 呼ぶ前に、スワップチェーンのバッファを描き込み先にし、2つのRTをシェーダーから読める状態にしておく
    void Draw(RenderTarget* pSceneRT, RenderTarget* pUIRT, float paperWhiteNits);
};
