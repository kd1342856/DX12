#pragma once

class AnimationSystem : public SystemBase
{
public:
    void Update(float deltaTime) override
    {

        for (auto const& entity : m_entities)
        {
            auto& animComp = m_pCoordinator->GetComponent<AnimationDataComponent>(entity);
            auto& modelData = m_pCoordinator->GetComponent<ModelRenderData>(entity);

            auto& data = animComp.currentAnim;
            if (!data.IsPlaying || data.AnimationIndex < 0) continue;
            if (!modelData.m_spModelData || !modelData.m_spModelData->IsLoaded()) continue;

            const auto& anims = modelData.m_spModelData->GetAnimations();
            if (data.AnimationIndex >= (int)anims.size()) continue;

            const auto& animInfo = anims[data.AnimationIndex];

            // Update time
            data.ProgressTime += (animInfo.ticksPerSecond * deltaTime * data.Speed);

            if (data.ProgressTime >= animInfo.duration)
            {
                if (data.IsLoop) {
                    data.ProgressTime = fmod(data.ProgressTime, animInfo.duration);
                } else {
                    data.ProgressTime = animInfo.duration - 0.001f;
                    data.IsPlaying = false;
                }
            }

            modelData.m_spModelData->UpdateAnimation(data.AnimationIndex, data.ProgressTime);
        }
    }
};