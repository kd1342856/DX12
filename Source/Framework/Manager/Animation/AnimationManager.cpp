#include "../../../Pch.h"
#include "AnimationManager.h"

void AnimationManager::PlayAnimation(Entity entity, int animIndex, bool isLoop, float speed)
{
    auto& ecs = GameManager::Instance().GetECS();
    if (auto* pAnim = ecs.TryGetComponent<AnimationDataComponent>(entity)) {
        auto& anim = *pAnim;
        bool needsRestart = (anim.currentAnim.AnimationIndex != animIndex || !anim.currentAnim.IsPlaying);
        if (needsRestart)
        {
            anim.currentAnim.AnimationIndex = animIndex;
            anim.currentAnim.ProgressTime = 0.0f;
            anim.currentAnim.IsPlaying = true;
        }
        // 同じアニメーションを継続再生中でも、ループ/速度指定は毎回反映する。
        // (FinishLoopTowardPose()でIsLoop=falseにした直後にもう一度呼ばれた場合、
        //  ループへ戻せるようにするため)
        anim.currentAnim.IsLoop = isLoop;
        anim.currentAnim.Speed = speed;
        anim.currentAnim.StopAtTime = -1.0f; // FinishLoopTowardPose()で設定した目標をクリア
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

void AnimationManager::FinishLoopTowardPose(Entity entity, float poseRatio, float speed)
{
    auto& ecs = GameManager::Instance().GetECS();
    auto* pAnim = ecs.TryGetComponent<AnimationDataComponent>(entity);
    auto* pModel = ecs.TryGetComponent<ModelRenderData>(entity);
    if (!pAnim || !pModel || !pModel->m_spModelData) return;

    auto& anim = pAnim->currentAnim;
    if (!anim.IsPlaying) return;
    if (anim.StopAtTime >= 0.0f) return; // 既に目標を計算済み(毎フレーム呼ばれても再計算しない)

    const auto& anims = pModel->m_spModelData->GetAnimations();
    if (anim.AnimationIndex < 0 || anim.AnimationIndex >= (int)anims.size()) return;

    float duration = anims[anim.AnimationIndex].duration;
    if (duration <= 0.0f) return;

    poseRatio = std::clamp(poseRatio, 0.0f, 1.0f);

    // poseRatio(アニメーション長に対する割合)の位置だけを目標にする。
    // 候補を1箇所に固定することで、リリースするタイミングによらず
    // 常に同じキーフレーム位置で止まるようにする
    // (以前はposeRatio+0.5も候補にしていたが、片方が良いポーズでも
    //  タイミング次第でもう片方(足が前に出た位置等)で止まってしまっていたため廃止)。
    float targetTime = duration * poseRatio;
    while (targetTime <= anim.ProgressTime) targetTime += duration;

    anim.IsLoop = false;
    anim.StopAtTime = targetTime;
    anim.Speed = speed; // 余韻フェーズだけの再生速度(通常再生時のSpeedとは別に指定できる)
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

// --- 複数アニメーション同時管理 ---

void AnimationManager::PlayMultiAnimation(Entity entity, int animIndex, bool isLoop, float speed)
{
    auto& ecs = GameManager::Instance().GetECS();
    auto* pAnim = ecs.TryGetComponent<AnimationDataComponent>(entity);
    if (!pAnim) return;

    auto& animData = pAnim->multiAnims[animIndex];
    animData.AnimationIndex = animIndex;
    animData.IsPlaying = true;
    animData.IsLoop = isLoop;
    animData.Speed = speed;
    // 逆再生（閉める）場合は末尾から開始、正再生（開ける）場合は先頭から開始
    if (speed < 0.0f) {
        // 末尾から始めるためにモデルのアニメーション時間を取得する
        auto* pModel = ecs.TryGetComponent<ModelRenderData>(entity);
        if (pModel && pModel->m_spModelData) {
            const auto& anims = pModel->m_spModelData->GetAnimations();
            if (animIndex < (int)anims.size()) {
                animData.ProgressTime = anims[animIndex].duration - 0.001f;
            }
        }
    } else {
        animData.ProgressTime = 0.0f;
    }
}

void AnimationManager::StopMultiAnimation(Entity entity, int animIndex)
{
    auto& ecs = GameManager::Instance().GetECS();
    if (auto* pAnim = ecs.TryGetComponent<AnimationDataComponent>(entity)) {
        pAnim->multiAnims.erase(animIndex);
    }
}

bool AnimationManager::IsMultiAnimPlaying(Entity entity, int animIndex) const
{
    auto& ecs = GameManager::Instance().GetECS();
    if (auto* pAnim = ecs.TryGetComponent<AnimationDataComponent>(entity)) {
        auto it = pAnim->multiAnims.find(animIndex);
        if (it != pAnim->multiAnims.end()) {
            return it->second.IsPlaying;
        }
    }
    return false;
}

float AnimationManager::GetMultiAnimTime(Entity entity, int animIndex) const
{
    auto& ecs = GameManager::Instance().GetECS();
    if (auto* pAnim = ecs.TryGetComponent<AnimationDataComponent>(entity)) {
        auto it = pAnim->multiAnims.find(animIndex);
        if (it != pAnim->multiAnims.end()) {
            return it->second.ProgressTime;
        }
    }
    return -1.0f;
}
