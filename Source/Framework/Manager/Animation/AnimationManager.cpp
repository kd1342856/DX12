#include "AnimationManager.h"

void AnimationManager::PlayAnimation(Entity entity, int animIndex, bool isLoop, float speed)
{
    auto& ecs = GameManager::Instance().GetECS();
    if (ecs.HasComponent<AnimationDataComponent>(entity)) 
    {
        auto& anim = ecs.GetComponent<AnimationDataComponent>(entity);
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
    if (ecs.HasComponent<AnimationDataComponent>(entity) && ecs.HasComponent<ModelRenderData>(entity)) {
        auto& modelData = ecs.GetComponent<ModelRenderData>(entity);
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
    if (ecs.HasComponent<AnimationDataComponent>(entity)) {
        auto& anim = ecs.GetComponent<AnimationDataComponent>(entity);
        anim.currentAnim.IsPlaying = false;
        anim.currentAnim.ProgressTime = 0.0f;
    }
}

void AnimationManager::PauseAnimation(Entity entity)
{
    auto& ecs = GameManager::Instance().GetECS();
    if (ecs.HasComponent<AnimationDataComponent>(entity)) {
        auto& anim = ecs.GetComponent<AnimationDataComponent>(entity);
        anim.currentAnim.IsPlaying = false;
    }
}

void AnimationManager::ResumeAnimation(Entity entity)
{
    auto& ecs = GameManager::Instance().GetECS();
    if (ecs.HasComponent<AnimationDataComponent>(entity)) {
        auto& anim = ecs.GetComponent<AnimationDataComponent>(entity);
        anim.currentAnim.IsPlaying = true;
    }
}
