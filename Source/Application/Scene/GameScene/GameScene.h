#pragma once
#include "../SceneBase.h"
#include "../../../Framework/Manager/Scene/Scene.h"
#include "../../../Graphics/Buffer/RenderTarget/RenderTarget.h"

class GameScene : public SceneBase
{
public:
    void Init() override;
    void Update() override;

public:
    std::shared_ptr<Scene> GetScene() const { return m_spScene; }

private:
    void UpdateInput();
    void UpdateCameras();
    void Render();

    std::shared_ptr<Scene> m_spScene;
    std::unique_ptr<RenderTarget> m_upRenderTarget;
    float m_exposure = 1.0f;
    bool m_showEditor = true;
    bool m_fullscreenGame = false;
    bool m_isCameraDragging = false;
    bool m_flashlightOn = false;

    Entity m_editorCameraEntity = INVALID_ENTITY;
    Entity m_gameCameraEntity = INVALID_ENTITY;
};