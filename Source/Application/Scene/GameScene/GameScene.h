#pragma once
#include "../SceneBase.h"
#include "../../../Framework/Manager/Scene.h"
#include "../../../Graphics/Buffer/RenderTarget/RenderTarget.h"

class GameScene : public SceneBase
{
public:
    void Init() override;
    void Update() override;

public:
    std::shared_ptr<Scene> GetScene() const { return m_spScene; }

private:
    std::shared_ptr<Scene> m_spScene;
    std::unique_ptr<RenderTarget> m_upRenderTarget;
    float m_exposure = 1.0f;
    bool m_showEditor = true;
    bool m_fullscreenGame = false;
    bool m_isCameraDragging = false;
};