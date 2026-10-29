#pragma once
#include "../SceneBase.h"
#include "../../../Framework/Manager/Scene/Scene.h"
#include "../../../Graphics/GPUResource/RenderTarget/RenderTarget.h"
#include "../../../Graphics/Renderer/RenderGraph.h"

class GameScene : public SceneBase
{
public:
    void Init() override;
    void Update(float deltaTime) override;
    void Unload() override;

private:
    void HandleModeSwitch();
    // ESCでオプション画面を開閉して描く。開いている間はゲームをポーズする(戻り値: ポーズ中か)
    bool UpdateOptionMenu();
    // ポーズの開始/終了(音の一時停止とマウスモードの切り替え)
    void SetPaused(bool paused);
    void UpdateInput();
    void UpdateCamera();
    // エディターのカメラ(MainCamera)とゲームのカメラを探して m_editorCameraEntity・m_gameCameraEntity を決める
    std::shared_ptr<GameObject> FindCameras();
    // 自由カメラかプレイヤーの視点を、入力で動かす
    void UpdateCameraControl(const std::shared_ptr<GameObject>& pEditorCameraObj);
    TransformData* GetEditorCameraTransform();
    // ベンチマーク中の1フレーム(ゲームを止め、カメラを経路で動かして描く)
    void UpdateBenchmark(float deltaTime);
    void TryBuildNavMesh();
    void Render();
    void RenderGame();
    void RenderEditor();
    // 1フレームの描画をレンダーグラフに組み立てて実行する(ゲームとエディターで共通。違いはisEditorの条件だけ)
    void RenderFrame(Entity cameraEntity, bool isEditor);
    // Bloom/DOF/ToneMapping等の画面全体エフェクト一式。RenderGame/RenderEditor共通処理として
    // まとめてある(2箇所に同じロジックをコピペしていると、片方だけ直して食い違う事故が起きるため)。
    // 結果は常にバックバッファへ合成する。clearR/G/B: 合成前にバックバッファをクリアする色。
    void DoPostProcess(Entity cameraEntity, float clearR, float clearG, float clearB);
    // ポイントライト(フリッカー含む)をカメラ位置基準で毎フレーム更新する。RenderScene呼び出し前に
    // 呼ぶ必要がある(LitShaderが読むcbLightのPL[]をここで確定させるため)。
    void UpdateLights(Entity cameraEntity);
    // 最も近いポイントライトの簡易シャドウ(単一パースペクティブ)を描画する。UpdateLights()の後、
    // RenderScene()より前に呼ぶ必要がある(LitShaderが読むシャドウマップ/行列をここで確定させるため)。
    // 局所光源(スポットライト・点光源)の影を描く(UpdateLightsの後に呼ぶ)
    void RenderLocalLightShadows();
    // 法線プリパス+SSAOの計算。RenderScene呼び出し前に呼ぶ必要がある
    // (LitShaderが読むg_ssaoMapをここで確定させるため)。
    void RenderSSAO(Entity cameraEntity);
    // TAAのジッターと、速度に使うカメラの行列を、そのフレームのメインのカメラで決める。
    // 深度プリパスより前に呼ぶ。描き終わったらEndTemporalFrame()でジッターを0に戻す
    void BeginTemporalFrame(Entity cameraEntity);
    // 反射プローブを1面撮る(局所光源の影を描いた後、メインの描画の前に呼ぶ)
    void CaptureReflectionProbe();
    // レイトレースの加速構造を作り、DDGIのプローブを更新する(局所光源の影の後、反射プローブとメインの描画の前に呼ぶ)
    void UpdateGlobalIllumination();
    void EndTemporalFrame();
    // CameraData.m_isDebugPreviewを立てたカメラ(あれば1つ目)のTransformから見た絵を専用RTに
    // 描画する。RenderGame/RenderEditorどちらの後にもRender()から呼ぶ(反射カメラ等の他の
    // システムとは完全に独立、既存のRenderScene()をそのまま流用するだけ)。
    void RenderDebugPreviewCamera();

    float m_exposure = 1.0f;
    bool m_fullscreenGame = true; // 起動時からPlayerのFPS視点でスタートする（F5でデバッグ用フリーカメラに切替）
    bool m_isCameraDragging = false;
    bool m_isPreviewCameraDragging = false; // Debug Preview Cameraを右クリックドラッグで操作中か
    bool m_flashlightOn = false;

    float m_navMeshWaitTime = 0.0f; // NavMesh構築前に家のモデルの読み込みを待った時間

    Entity m_editorCameraEntity = INVALID_ENTITY;
    Entity m_gameCameraEntity = INVALID_ENTITY;

    // TAAの履歴を捨てるかの判定用(カメラの切り替え・瞬間移動)
    Entity m_temporalCameraEntity = INVALID_ENTITY;
    // 1フレームの描画の組み立て(毎フレーム組み立て直す)
    RenderGraph m_renderGraph;
    Math::Vector3 m_temporalCameraPosition = Math::Vector3::Zero;
};
