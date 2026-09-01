#pragma once
#include "../SceneBase.h"
#include "../../../Framework/Manager/Scene/Scene.h"
#include "../../../Graphics/GPUResource/RenderTarget/RenderTarget.h"

class GameScene : public SceneBase
{
public:
    void Init() override;
    void Update(float deltaTime) override;

private:
    void HandleModeSwitch();
    void UpdateInput();
    void UpdateCamera();
    void TryBuildNavMesh();
    void Render();
    void RenderGame();
    void RenderEditor();

    float m_exposure = 1.0f;
    bool m_fullscreenGame = true; // 起動時からPlayerのFPS視点でスタートする（F5でデバッグ用フリーカメラに切替）
    bool m_isCameraDragging = false;
    bool m_flashlightOn = false;

    Entity m_editorCameraEntity = INVALID_ENTITY;
    Entity m_gameCameraEntity = INVALID_ENTITY;
};
