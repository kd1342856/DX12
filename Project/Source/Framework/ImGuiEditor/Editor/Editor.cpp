#include "../../../Pch.h"
#include "../../Manager/Scene/Scene.h"
#include "../../Manager/Scene/SceneManager.h"
#include "../../Manager/Collision/CollisionManager.h"
#include "../../DirectX/Utility/Profiler.h"
#include "../../System/JobSystem/JobSystem.h"
#include "../../../Graphics/GPUResource/RenderTarget/RenderTarget.h"
#include "../../DirectX/Utility/Input.h"
#include "Editor.h"
#include "../GameEditor/GameEditor.h"
#include "../ShaderEditor/ShaderEditor.h"
#include "../NavMeshEditor/NavMeshEditor.h"
#include "../EditorContext.h"
#include "../../../Graphics/Shader/ShaderManager/ShaderManager.h"

// Resolve the GetCurrentScene issue
static std::shared_ptr<Scene> GetCurrentScenePtr() {
    return Editor::GetScene();
}

std::shared_ptr<GameObject> Editor::s_selectedObject = nullptr;
std::string Editor::s_selectedAssetPath = "";
std::string Editor::s_currentAssetDir = "Asset";
bool Editor::s_editorMode = true;
bool Editor::s_showEditor = false; // 起動時はImGuiのデバッグ表示を出さない(F1で表示)
bool Editor::s_showProfilerOverlay = false;

std::shared_ptr<Scene> Editor::s_scene = nullptr;
std::string Editor::s_scenePath = "";

bool Editor::s_showGameEditor = false;
bool Editor::s_showShaderEditor = false;
bool Editor::s_showNavMeshEditor = false;
bool Editor::s_showPreviewCamera = false;
bool Editor::s_showSoundManager = false;
bool Editor::s_previewCameraHovered = false;

void Editor::Init() {
    s_selectedObject = nullptr;
    s_scenePath.clear(); // 保存先は各シーンのInit()で設定する(前シーンのファイルを引き継がない)
    LoadEditorData();    // パスが空なのでフォルダ構成も空に戻る(前シーンのフォルダを引き継がない)
    s_scene = std::make_shared<Scene>();
    s_scene->Init();
}

void Editor::Draw() 
{
    if (Input::Instance().IsKeyTrigger(DirectX::Keyboard::Keys::F1)) {
        s_showEditor = !s_showEditor;
    }

    if (!s_showEditor) return;
    if (!s_editorMode) return;

    DrawDockSpace();
    DrawMenuBar();
    DrawToolbar();
    DrawHierarchy();
    DrawInspector();
    DrawAssetBrowser();
    DrawStatistics();
    DrawConsole();
    if (s_showSoundManager) DrawSoundManager();
    if (s_showPreviewCamera) DrawPreviewCamera();
    else s_previewCameraHovered = false; // ウィンドウを閉じている間は右クリック操作を有効にしたままにしない

    static GameEditor gameEditor;
    static ShaderEditor shaderEditor;
    static NavMeshEditor navMeshEditor;
    static bool initialized = false;
    if (!initialized) {
        gameEditor.Initialize();
        shaderEditor.Initialize();
        navMeshEditor.Initialize();
        initialized = true;
    }

    EditorContext ctx;
    ctx.Renderer = &ShaderManager::Instance().GetRendererSettings();
    ctx.Lighting = &ShaderManager::Instance().GetLightingSettings();
    ctx.Shadow = &ShaderManager::Instance().GetShadowSettings();
    ctx.IBL = &ShaderManager::Instance().GetIBLSettings();
    ctx.PostProcess = &ShaderManager::Instance().GetPostProcessSettings();
    ctx.SSAO = &ShaderManager::Instance().GetSSAOSettings();
    ctx.Debug = &ShaderManager::Instance().GetDebugSettings();
    ctx.Fog = &ShaderManager::Instance().GetFogSettings();
    ctx.SelectedObject = s_selectedObject.get();

    if (s_showGameEditor) gameEditor.Draw(ctx);
    if (s_showShaderEditor) shaderEditor.Draw(ctx);
    if (s_showNavMeshEditor) navMeshEditor.Draw(ctx);
}

bool Editor::SaveCurrentScene()
{
    auto sceneObj = GetCurrentScenePtr();
    const std::string& path = GetScenePath();
    if (!sceneObj || path.empty())
    {
        Logger::Instance().AddLog(Logger::LogLevel::Warning, u8"このシーンには保存先がありません(保存しませんでした)");
        return false;
    }
    nlohmann::json j;
    sceneObj->Serialize(j);
    std::ofstream o(path);
    if (!o.is_open())
    {
        Logger::Instance().AddLog(Logger::LogLevel::Error, u8"シーンを書き込めませんでした: %s", path.c_str());
        return false;
    }
    o << std::setw(4) << j << std::endl;
    Logger::Instance().AddLog(Logger::LogLevel::Info, "Scene Saved: %s", path.c_str());
    SaveEditorData();
    return true;
}

void Editor::DrawProfilerOverlay()
{
    if (Input::Instance().IsKeyTrigger(DirectX::Keyboard::Keys::F3)) {
        s_showProfilerOverlay = !s_showProfilerOverlay;
    }

    if (!s_showProfilerOverlay) return;

    // 呼び出し元はフルスクリーン/プレイヤー視点のレンダーパス(GameScene::Render())からのみ
    // ここに到達し、同じフレームでフルエディタUI(上のDraw()、独自のStatisticsウィンドウ
    // 持ち)が同時に呼ばれることは無い - なのでここでは二重描画のリスクをガードする必要がない。
    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
    DrawStatistics();
}
