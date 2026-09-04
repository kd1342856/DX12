#pragma once
#include <algorithm>
#include <vector>
#include "../../../../Graphics/Shader/ShaderManager/ShaderManager.h"

// PointLightDataを持つエンティティを毎フレーム集めてShaderManagerへ送るシステム。
// - フリッカー(明滅)はここでCPU側に計算し、送るColorに反映させる(シェーダー側は
//   ただの点光源として扱うだけで済む)。
// - シェーダー側の配列(g_PL[8])には上限があるので、カメラに近い順で上位8個だけを送る。
class LightSystem : public SystemBase
{
public:
    // RenderScene直前に、実際に描画へ使うカメラの位置を渡して呼ぶ想定(近い順のソートに使う)。
    void Update(float deltaTime, const Math::Vector3& viewerPos)
    {
        m_elapsedTime += deltaTime;

        struct Candidate { Math::Vector3 pos; float rangeSq; CBufferData::PointLight data; };
        std::vector<Candidate> candidates;
        candidates.reserve(m_entities.size());

        for (auto const& entity : m_entities)
        {
            auto& trans = m_pCoordinator->GetComponent<TransformData>(entity);
            auto& light = m_pCoordinator->GetComponent<PointLightData>(entity);
            if (!light.m_enabled) continue;

            Math::Vector3 pos = trans.m_worldMatrix.Translation();

            float intensityMul = 1.0f;
            if (light.m_flickerEnabled)
            {
                // 複数の高さの違う正弦波+ノイズ寄りのjitterを足して、機械的すぎない揺らぎにする。
                float t = (m_elapsedTime + light.m_flickerSeed) * light.m_flickerSpeed;
                float wave = sinf(t) * 0.6f + sinf(t * 2.7f + 1.3f) * 0.4f; // -1..1
                float flicker = 0.5f + 0.5f * wave; // 0..1
                intensityMul = 1.0f - light.m_flickerIntensity * (1.0f - flicker);
                if (intensityMul < 0.0f) intensityMul = 0.0f;
            }

            CBufferData::PointLight pl;
            pl.Pos = pos;
            pl.Range = light.m_range;
            pl.Color = { light.m_color.x * light.m_intensity * intensityMul,
                         light.m_color.y * light.m_intensity * intensityMul,
                         light.m_color.z * light.m_intensity * intensityMul };

            float distSq = Math::Vector3::DistanceSquared(pos, viewerPos);
            candidates.push_back({ pos, distSq, pl });
        }

        std::sort(candidates.begin(), candidates.end(),
            [](const Candidate& a, const Candidate& b) { return a.rangeSq < b.rangeSq; });

        constexpr int kMaxPointLights = 8;
        auto& lightData = ShaderManager::Instance().GetMutableLightData();
        int count = (std::min)((int)candidates.size(), kMaxPointLights);
        for (int i = 0; i < count; ++i)
        {
            lightData.PL[i] = candidates[i].data;
        }
        lightData.PL_Count = count;
    }

private:
    float m_elapsedTime = 0.0f;
};
