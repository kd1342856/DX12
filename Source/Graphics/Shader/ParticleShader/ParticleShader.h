#pragma once
#include "../GraphicsShader/GraphicsShader.h"

// 加算合成のビルボードパーティクル。ParticleManagerが毎フレーム集めた粒をまとめて描く。
// シーンのHDRターゲット(R16G16B16A16_FLOAT)へ描くので、明るい色はそのままBloomに乗る。
class ParticleShader : public GraphicsShader
{
public:
    // HLSL側(ParticleShader_VS.hlslのParticleInstance)と同じ並び(32byte)
    struct Instance
    {
        Math::Vector3 Pos;
        float Size;
        Math::Vector4 Color; // rgb: HDRの色, a: 不透明度
    };
    static constexpr int kMaxPerBatch = 256; // HLSLのg_Particles[256]と合わせる

    void Create(GraphicsDevice* pGraphicsDevice) override;
    void Draw(RenderContext& context, const std::vector<Instance>& instances);
};
