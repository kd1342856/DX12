#pragma once
#include "./SceneBase.h"
#include <memory>
#include <DirectXMath.h>

// フェード状態
enum class FadeState
{
    None,
    FadeOut,
    FadeIn
};

// =============================================
// SceneManager
// シーンの切替・フェード管理
// Scene の唯一の所有者（unique_ptr）
// =============================================
class SceneManager
{
public:
    static SceneManager& Instance();

    void Init();
    void Update();
    void DrawFade();

    // Scene を unique_ptr で受け取り所有権を移譲
    void ChangeScene(std::unique_ptr<SceneBase> nextScene, float fadeDuration = 1.0f);

    // 現在のシーンへの参照（所有権は渡さない）
    SceneBase* GetCurrentScene() const { return m_currentScene.get(); }

    // フェードなしで初期シーンを設定（起動時のみ使用）
    void SetCurrentSceneWithoutFade(std::unique_ptr<SceneBase> scene);

private:
    SceneManager() = default;
    ~SceneManager() = default;

    std::unique_ptr<SceneBase> m_currentScene;
    std::unique_ptr<SceneBase> m_nextScene;

    FadeState m_fadeState   = FadeState::None;
    float     m_fadeAlpha   = 0.0f;
    float     m_fadeDuration = 1.0f;

    // フェード用テクスチャ（所有しない・借りるだけ）
    Texture* m_pFadeTexture = nullptr;
};
