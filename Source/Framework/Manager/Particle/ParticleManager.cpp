#include "../../../Pch.h"
#include "ParticleManager.h"
#include "../../../Graphics/Shader/ShaderLibrary.h"

void ParticleManager::Emit(const Particle& particle)
{
    if (m_particles.size() >= kMaxParticles)
    {
        return;
    }
    m_particles.push_back(particle);
    m_particles.back().m_age = 0.0f;
}

void ParticleManager::Update(float deltaTime)
{
    for (auto& p : m_particles)
    {
        p.m_age += deltaTime;
        p.m_velocity *= (std::max)(0.0f, 1.0f - p.m_drag * deltaTime);
        p.m_position += p.m_velocity * deltaTime;
    }
    m_particles.erase(
        std::remove_if(m_particles.begin(), m_particles.end(),
                       [](const Particle& p)
    { return p.m_age >= p.m_life; }),
        m_particles.end());
}

void ParticleManager::Render(RenderContext& context)
{
    if (m_particles.empty())
    {
        return;
    }

    m_drawList.clear();
    m_drawList.reserve(m_particles.size());
    for (const auto& p : m_particles)
    {
        float t = (p.m_life > 0.0f) ? (p.m_age / p.m_life) : 1.0f;
        // 出た瞬間にパッと出ないよう少しだけフェードインし、後半でフェードアウトする
        float fadeIn = (std::min)(1.0f, t / 0.1f);
        float fadeOut = 1.0f - (std::max)(0.0f, (t - 0.5f) / 0.5f);
        float alpha = fadeIn * fadeOut;
        if (alpha <= 0.0f)
        {
            continue;
        }

        float sway = p.m_swayPhase + p.m_age * p.m_swayFrequency;
        ParticleShader::Instance inst;
        inst.Pos = p.m_position + Math::Vector3(sinf(sway), 0.0f, cosf(sway)) * p.m_swayAmplitude;
        inst.Size = p.m_sizeStart + (p.m_sizeEnd - p.m_sizeStart) * t;
        inst.Color = Math::Vector4(p.m_color.x, p.m_color.y, p.m_color.z, alpha);
        m_drawList.push_back(inst);
    }

    ShaderLibrary::Instance().Get<ParticleShader>().Draw(context, m_drawList);
}
