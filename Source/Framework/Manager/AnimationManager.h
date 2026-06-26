#pragma once

class AnimationManager
{
public:
    static AnimationManager& Instance()
    {
        static AnimationManager instance;
        return instance;
    }

    // Index-based playback
    void PlayAnimation(Entity entity, int animIndex, bool isLoop = true, float speed = 1.0f);
    
    // Name-based playback (requires ModelRenderData component)
    void PlayAnimationByName(Entity entity, const std::string& animName, bool isLoop = true, float speed = 1.0f);
    
    void StopAnimation(Entity entity);
    void PauseAnimation(Entity entity);
    void ResumeAnimation(Entity entity);

private:
    AnimationManager() = default;
    ~AnimationManager() = default;
    AnimationManager(const AnimationManager&) = delete;
    AnimationManager& operator=(const AnimationManager&) = delete;
};
