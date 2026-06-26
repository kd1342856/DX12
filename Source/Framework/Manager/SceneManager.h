#pragma once

#include <memory>
#include "../../Application/Scene/SceneBase.h"
#include "../../Graphics/Buffer/Texture/Texture.h"
#include <DirectXMath.h>

enum class FadeState
{
    None,
    FadeOut,
    FadeIn
};

class SceneManager
{
public:
    static SceneManager& Instance();

    void Init();
    void Update();
    void DrawFade();

    void ChangeScene(std::shared_ptr<SceneBase> nextScene, float fadeDuration = 1.0f);

    std::shared_ptr<SceneBase> GetCurrentScene() const { return m_spCurrentScene; }
    void SetCurrentSceneWithoutFade(std::shared_ptr<SceneBase> scene) { m_spCurrentScene = scene; }

private:
    SceneManager() = default;
    ~SceneManager() = default;

    std::shared_ptr<SceneBase> m_spCurrentScene;
    std::shared_ptr<SceneBase> m_spNextScene;

    FadeState m_fadeState = FadeState::None;
    float m_fadeAlpha = 0.0f;
    float m_fadeDuration = 1.0f;

    // 黒画面フェード用テクスチャ
    Texture* m_pFadeTexture = nullptr;
};
