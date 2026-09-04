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
    // Bloom/DOF/ToneMapping等の画面全体エフェクト一式。RenderGame/RenderEditor共通処理として
    // まとめてある(2箇所に同じロジックをコピペしていると、片方だけ直して食い違う事故が起きるため)。
    // 結果は常にバックバッファへ合成する。clearR/G/B: 合成前にバックバッファをクリアする色。
    void DoPostProcess(Entity cameraEntity, float clearR, float clearG, float clearB);
    // ポイントライト(フリッカー含む)をカメラ位置基準で毎フレーム更新する。RenderScene呼び出し前に
    // 呼ぶ必要がある(LitShaderが読むcbLightのPL[]をここで確定させるため)。
    void UpdateLights(Entity cameraEntity);
    // 最も近いポイントライトの簡易シャドウ(単一パースペクティブ)を描画する。UpdateLights()の後、
    // RenderScene()より前に呼ぶ必要がある(LitShaderが読むシャドウマップ/行列をここで確定させるため)。
    void UpdatePointLightShadow(Entity cameraEntity);
    // 法線プリパス+SSAOの計算。RenderScene呼び出し前に呼ぶ必要がある
    // (LitShaderが読むg_ssaoMapをここで確定させるため)。
    void RenderSSAO(Entity cameraEntity);

    float m_exposure = 1.0f;
    bool m_fullscreenGame = true; // 起動時からPlayerのFPS視点でスタートする（F5でデバッグ用フリーカメラに切替）
    bool m_isCameraDragging = false;
    bool m_flashlightOn = false;

    Entity m_editorCameraEntity = INVALID_ENTITY;
    Entity m_gameCameraEntity = INVALID_ENTITY;
};
