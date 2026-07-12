#include "../../../Pch.h"
#include "AnimationManager.h"

void AnimationManager::PlayAnimation(Entity entity, int animIndex, bool isLoop, float speed)
{
    auto& ecs = GameManager::Instance().GetECS();
    if (auto* pAnim = ecs.TryGetComponent<AnimationDataComponent>(entity)) {
        auto& anim = *pAnim;
        if (anim.currentAnim.AnimationIndex != animIndex || !anim.currentAnim.IsPlaying) 
        {
            anim.currentAnim.AnimationIndex = animIndex;
            anim.currentAnim.ProgressTime = 0.0f;
            anim.currentAnim.IsPlaying = true;
            anim.currentAnim.IsLoop = isLoop;
            anim.currentAnim.Speed = speed;
        }
    }
}

void AnimationManager::PlayAnimationByName(Entity entity, const std::string& animName, bool isLoop, float speed)
{
    auto& ecs = GameManager::Instance().GetECS();
    auto* pAnim = ecs.TryGetComponent<AnimationDataComponent>(entity);
    auto* pModel = ecs.TryGetComponent<ModelRenderData>(entity);
    if (pAnim && pModel) {
        auto& modelData = *pModel;
        if (modelData.m_spModelData) {
            const auto& animations = modelData.m_spModelData->GetAnimations();
            for (int i = 0; i < (int)animations.size(); ++i) {
                if (animations[i].name == animName) {
                    PlayAnimation(entity, i, isLoop, speed);
                    return;
                }
            }
        }
    }
}

void AnimationManager::StopAnimation(Entity entity)
{
    auto& ecs = GameManager::Instance().GetECS();
    if (auto* pAnim = ecs.TryGetComponent<AnimationDataComponent>(entity)) {
        auto& anim = *pAnim;
        anim.currentAnim.IsPlaying = false;
        anim.currentAnim.ProgressTime = 0.0f;
    }
}

void AnimationManager::PauseAnimation(Entity entity)
{
    auto& ecs = GameManager::Instance().GetECS();
    if (auto* pAnim = ecs.TryGetComponent<AnimationDataComponent>(entity)) {
        auto& anim = *pAnim;
        anim.currentAnim.IsPlaying = false;
    }
}

void AnimationManager::ResumeAnimation(Entity entity)
{
    auto& ecs = GameManager::Instance().GetECS();
    if (auto* pAnim = ecs.TryGetComponent<AnimationDataComponent>(entity)) {
        auto& anim = *pAnim;
        anim.currentAnim.IsPlaying = true;
    }
}
