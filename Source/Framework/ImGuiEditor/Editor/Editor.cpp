#include "../../../Pch.h"
#include "../../Manager/Scene/Scene.h"
#include "../../Manager/Scene/SceneManager.h"
#include "../../Manager/Collision/CollisionManager.h"
#include "../../DirectX/Utility/Profiler.h"
#include "../../System/JobSystem/JobSystem.h"
#include "../../../Graphics/GPUResource/RenderTarget/RenderTarget.h"
#include "../../DirectX/Utility/Input.h"

// Resolve the GetCurrentScene issue
static std::shared_ptr<Scene> GetCurrentScenePtr() {
    return Editor::GetScene();
}


std::shared_ptr<GameObject> Editor::s_selectedObject = nullptr;
std::string Editor::s_selectedAssetPath = "";
std::string Editor::s_currentAssetDir = "Asset";
bool Editor::s_editorMode = true;
bool Editor::s_showEditor = true;

std::shared_ptr<Scene> Editor::s_scene = nullptr;

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
}
