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
bool Editor::s_showEditor = true;
bool Editor::s_showProfilerOverlay = false;

std::shared_ptr<Scene> Editor::s_scene = nullptr;

bool Editor::s_showGameEditor = false;
bool Editor::s_showShaderEditor = false;
bool Editor::s_showNavMeshEditor = false;

void Editor::Init() {
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
