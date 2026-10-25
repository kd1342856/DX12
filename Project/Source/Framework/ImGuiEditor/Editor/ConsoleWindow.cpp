#include "../../../Pch.h"
#include "../../Manager/Scene/Scene.h"
#include "../../Manager/Scene/SceneManager.h"
#include "../../Manager/Collision/CollisionManager.h"
#include "../../DirectX/Utility/Profiler.h"
#include "../../System/JobSystem/JobSystem.h"

// Resolve the GetCurrentScene issue
static std::shared_ptr<Scene> GetCurrentScenePtr() {
    return Editor::GetScene();
}


void Editor::DrawConsole() {
    Logger::Instance().DrawImGuiWindow();
}
