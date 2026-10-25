#pragma once
#include "../../../Graphics/Shader/ParticleShader/ParticleShader.h"

// 光の粒などの簡易パーティクル。スクリプトからEmit()で粒を出すと、寿命が尽きるまで
// CPU側で動かし(Update)、RenderSystem::RenderSceneの最後にまとめて描く(Render)。
// 実行中だけのもので、シーンには保存しない。
struct Particle
{
    Math::Vector3 m_position = {0, 0, 0};
    Math::Vector3 m_velocity = {0, 0, 0};
    Math::Vector3 m_color = {1, 1, 1}; // HDR(1を超えるとBloomで光る)
    float m_sizeStart = 0.05f;         // 半径(m)
    float m_sizeEnd = 0.0f;
    float m_life = 1.0f;          // 寿命(秒)
    float m_drag = 0.0f;          // 1秒あたりの速度の減衰率(0..)
    float m_swayAmplitude = 0.0f; // 横にゆらゆら揺れる幅(m)
    float m_swayFrequency = 1.0f; // 揺れの速さ(rad/s)
    float m_swayPhase = 0.0f;

    float m_age = 0.0f; // 内部用
};

class ParticleManager
{
public:
    static ParticleManager& Instance()
    {
        static ParticleManager instance;
        return instance;
    }

    void Emit(const Particle& particle);
    void Update(float deltaTime);
    void Render(RenderContext& context);
    void Clear()
    {
        m_particles.clear();
    }

private:
    ParticleManager() = default;

    static constexpr size_t kMaxParticles = 2048;
    std::vector<Particle> m_particles;
    std::vector<ParticleShader::Instance> m_drawList;
};
