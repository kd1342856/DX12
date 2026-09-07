#pragma once
#include <algorithm>
#include <vector>
#include "../../../../Graphics/Shader/ShaderManager/ShaderManager.h"
#include "../../../Manager/Collision/CollisionManager.h"

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

            // エディタでの配置確認用ギズモ(常に描画しておいても、実際に画面に出るのは
            // RenderEditorのDebugDraw経由でエディタモードの時だけ)。
            DrawGizmo(pos, light.m_color, light.m_range);
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

    static void DrawGizmo(const Math::Vector3& pos, const Math::Vector3& color, float range)
    {
        ImU32 col = IM_COL32(
            (int)(saturate(color.x) * 255), (int)(saturate(color.y) * 255), (int)(saturate(color.z) * 255), 255);

        // 中心の十字
        const float s = 0.15f;
        auto& cm = CollisionManager::Instance();
        cm.AddDebugLine(pos - Math::Vector3(s, 0, 0), pos + Math::Vector3(s, 0, 0), col);
        cm.AddDebugLine(pos - Math::Vector3(0, s, 0), pos + Math::Vector3(0, s, 0), col);
        cm.AddDebugLine(pos - Math::Vector3(0, 0, s), pos + Math::Vector3(0, 0, s), col);

        // Range(減衰距離)を示す水平の円(XZ平面、簡易12角形)
        constexpr int kSegments = 12;
        for (int i = 0; i < kSegments; ++i)
        {
            float a0 = (float)i / kSegments * 6.28318530718f;
            float a1 = (float)(i + 1) / kSegments * 6.28318530718f;
            Math::Vector3 p0 = pos + Math::Vector3(cosf(a0), 0, sinf(a0)) * range;
            Math::Vector3 p1 = pos + Math::Vector3(cosf(a1), 0, sinf(a1)) * range;
            cm.AddDebugLine(p0, p1, col);
        }
    }

    static float saturate(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
};
