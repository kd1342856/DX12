#pragma once
#include <cmath>

class AnimationSystem : public SystemBase
{
public:
    void Update(float deltaTime) override
    {
        for (auto const& entity : m_entities)
        {
            auto& animComp = m_pCoordinator->GetComponent<AnimationDataComponent>(entity);
            auto& modelData = m_pCoordinator->GetComponent<ModelRenderData>(entity);

            if (!modelData.m_spModelData || !modelData.m_spModelData->IsLoaded()) continue;

            const auto& anims = modelData.m_spModelData->GetAnimations();
            bool anyUpdated = false;

            // ---- 通常の1つのアニメーション再生 ----
            {
                auto& data = animComp.currentAnim;
                if (data.IsPlaying && data.AnimationIndex >= 0 && data.AnimationIndex < (int)anims.size())
                {
                    const auto& animInfo = anims[data.AnimationIndex];

                    // 時間を進める（Speed < 0 で逆再生）
                    data.ProgressTime += (animInfo.ticksPerSecond * deltaTime * data.Speed);

                    if (data.Speed >= 0.0f)
                    {
                        // 正再生: 終端処理
                        // StopAtTimeが指定されていれば(非ループ時のみ)、durationの代わりにそこで止める。
                        // (例: 歩行を「足がそろう位置」で止めたい場合に使う)
                        float stopLimit = (!data.IsLoop && data.StopAtTime >= 0.0f) ? data.StopAtTime : animInfo.duration;
                        if (data.ProgressTime >= stopLimit)
                        {
                            if (data.IsLoop) {
                                while (data.ProgressTime >= animInfo.duration) {
                                    data.ProgressTime -= animInfo.duration;
                                }
                            } else {
                                data.ProgressTime = stopLimit - 0.001f;
                                data.IsPlaying = false;
                                data.StopAtTime = -1.0f;
                            }
                        }
                    }
                    else
                    {
                        // 逆再生: 先頭に達したら停止
                        if (data.ProgressTime <= 0.0f)
                        {
                            if (data.IsLoop) {
                                data.ProgressTime = animInfo.duration;
                            } else {
                                data.ProgressTime = 0.0f;
                                data.IsPlaying = false;
                            }
                        }
                    }

                    // UpdateAnimation()に渡す時刻は必ずクリップ自身の長さ(duration)でラップする。
                    // StopAtTimeがdurationを跨いだ位置(まだ1周以上先)を指している間、
                    // IsLoop=falseだとProgressTimeがラップされず伸び続けるが、
                    // UpdateAnimation()側は範囲外の時刻を最後のキーフレームにクランプしてしまうため、
                    // ラップせずに渡すと「狙ったポーズ」ではなく「クリップの最終ポーズ(片足立ち等)」
                    // が表示されてしまっていた(目標まで何周必要かで見た目が変わっていた不具合の原因)。
                    float sampleTime = std::fmod(data.ProgressTime, animInfo.duration);
                    if (sampleTime < 0.0f) sampleTime += animInfo.duration;
                    modelData.m_spModelData->UpdateAnimation(data.AnimationIndex, sampleTime);
                    anyUpdated = true;
                }
            }

            // ---- 複数アニメーション同時再生（ドアなど）----
            for (auto& pair : animComp.multiAnims)
            {
                int animIdx = pair.first;
                auto& data = pair.second;

                if (!data.IsPlaying || animIdx < 0 || animIdx >= (int)anims.size()) continue;

                const auto& animInfo = anims[animIdx];

                float safeDelta = deltaTime;
                if (safeDelta > 0.1f || safeDelta <= 0.0001f) {
                    safeDelta = 0.016f;
                }

                float addTime = animInfo.ticksPerSecond * safeDelta * data.Speed;
                if (fabsf(addTime) < 0.001f) {
                    addTime = 16.0f * data.Speed;
                }
                data.ProgressTime += addTime;

                float targetTime = animInfo.duration * 0.5f;

                if (data.Speed >= 0.0f)
                {
                    if (data.ProgressTime >= targetTime)
                    {
                        if (data.IsLoop) {
                            while (data.ProgressTime >= targetTime) {
                                data.ProgressTime -= targetTime;
                            }
                        } else {
                            data.ProgressTime = targetTime - 0.001f;
                            data.IsPlaying = false;
                        }
                    }
                }
                else
                {
                    if (data.ProgressTime <= 0.0f)
                    {
                        if (data.IsLoop) {
                            data.ProgressTime = targetTime;
                        } else {
                            data.ProgressTime = 0.0f;
                            data.IsPlaying = false;
                        }
                    }
                }

                char msg[256];
                sprintf_s(msg, "[AnimSys] Entity=%d AnimIdx=%d Progress=%.2f/%.2f Speed=%.2f IsPlaying=%d",
                          entity, animIdx, data.ProgressTime, animInfo.duration, data.Speed, data.IsPlaying);
                Logger::Instance().AddLog(Logger::LogLevel::Info, msg);

                modelData.m_spModelData->UpdateAnimation(animIdx, data.ProgressTime);
                anyUpdated = true;
            }
        }
    }
};