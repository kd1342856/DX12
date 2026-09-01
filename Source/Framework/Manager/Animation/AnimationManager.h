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

    // 即座に止めず、ループを解除して「次に来る指定ポーズ」まで再生してから自然に停止させる。
    // (キーを離した瞬間に途中のポーズで固まらないようにするためのもの)
    // poseRatio: アニメーション長に対する目標ポーズの位置(0.0～1.0)。
    //            例えば歩行モーションで足がそろって見える時刻をアニメーションエディタ等で調べて指定する。
    //            周期的な動作なら poseRatio と poseRatio+0.5 の2箇所を候補にして、近い方まで再生する。
    // 既に目標を計算済み、または対象が再生中でなければ何もしない。
    // speed: 余韻(足がそろうまで再生する)フェーズの再生速度倍率。通常再生時のSpeedとは別に指定できる。
    void FinishLoopTowardPose(Entity entity, float poseRatio, float speed = 1.0f);

    // --- 複数アニメーション同時管理（ドア等） ---
    // 指定インデックスのアニメーションをmultiAnimsに追加して再生する
    void PlayMultiAnimation(Entity entity, int animIndex, bool isLoop = false, float speed = 1.0f);
    // 指定インデックスのmultiAnimを停止する
    void StopMultiAnimation(Entity entity, int animIndex);
    // 指定インデックスのmultiAnimが再生中かどうかを返す
    bool IsMultiAnimPlaying(Entity entity, int animIndex) const;
    // 指定インデックスのmultiAnimの現在時間を返す（-1.0fなら未登録）
    float GetMultiAnimTime(Entity entity, int animIndex) const;

private:
    AnimationManager() = default;
    ~AnimationManager() = default;
    AnimationManager(const AnimationManager&) = delete;
    AnimationManager& operator=(const AnimationManager&) = delete;
};
