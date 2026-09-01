#include "../../../Pch.h"
#include "NavMeshEditor.h"
#include "../../../../Library/ImGui/imgui.h"
#include "../../Manager/NavMesh/NavMeshManager.h"
#include "../Editor/Editor.h"
#include "../../Manager/Scene/Scene.h"
#include "../../Manager/GameManager.h"
#include "../../ECS/Components/Data/CameraData.h"
#include "../../Manager/Collision/CollisionManager.h"
#include "../../DirectX/Utility/Input.h"
#include "../../../Application/Object/Script/System/GameSequence.h"

void NavMeshEditor::Initialize()
{
}

void NavMeshEditor::Draw(EditorContext& ctx)
{
    ImGui::Begin("NavMesh Editor");

    auto& navMeshManager = NavMeshManager::Instance();
    auto settings = navMeshManager.GetBakeSettings();
    bool settingsChanged = false;

    if (ImGui::CollapsingHeader("Bake Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::TextWrapped(
            "Note: these settings only affect the legacy mesh-voxelization build (Recast auto), "
            "which is currently unused. \"Bake NavMesh\" below builds the room-based NavMesh "
            "instead, which ignores all of these.");
        ImGui::Separator();
        if (ImGui::SliderFloat("Cell Size", &settings.cellSize, 0.05f, 1.0f, "%.2f")) settingsChanged = true;
        if (ImGui::SliderFloat("Cell Height", &settings.cellHeight, 0.05f, 1.0f, "%.2f")) settingsChanged = true;
        ImGui::Separator();
        if (ImGui::SliderFloat("Agent Radius", &settings.agentRadius, 0.1f, 2.0f, "%.2f")) settingsChanged = true;
        if (ImGui::SliderFloat("Agent Height", &settings.agentHeight, 0.5f, 5.0f, "%.2f")) settingsChanged = true;
        if (ImGui::SliderFloat("Max Climb", &settings.agentMaxClimb, 0.0f, 2.0f, "%.2f")) settingsChanged = true;
        if (ImGui::SliderFloat("Max Slope", &settings.agentMaxSlope, 0.0f, 90.0f, "%.1f")) settingsChanged = true;
        
        ImGui::Separator();
        
        if (ImGui::Checkbox("Use Seed Point", &settings.useSeedPoint)) settingsChanged = true;
        if (settings.useSeedPoint)
        {
            ImGui::Text("Seed Point: (%.2f, %.2f, %.2f)", settings.seedPoint.x, settings.seedPoint.y, settings.seedPoint.z);
            ImGui::Checkbox("Placement Mode (Click on Scene to place)", &m_placementMode);
        }
    }

    if (settingsChanged)
    {
        settings.isDirty = true;
        navMeshManager.SetBakeSettings(settings);
    }

    ImGui::Separator();

    if (ImGui::Button("Bake NavMesh"))
    {
        // Builds the room-based NavMesh (one walkable rect per RoomArea plus adjacency
        // connectors), not the legacy mesh-voxelization path (NavMeshManager::BuildNavMesh) -
        // that path has a known, unresolved doorway-connectivity bug on this house model. See
        // BuildManualNavMesh's declaration comment in NavMeshManager.h for the full story.
        auto gs = GameSequence::GetInstance();
        if (!gs)
        {
            Logger::Instance().AddLog(Logger::LogLevel::Error,
                "NavMesh Editor: Bake failed - GameSequence not initialized yet (rooms not loaded).");
        }
        else if (navMeshManager.BuildManualNavMesh(gs->GetRooms()))
        {
            navMeshManager.SetDebugDrawEnabled(true);
        }

        settings.isDirty = false;
        navMeshManager.SetBakeSettings(settings);
    }

    ImGui::Separator();

    if (m_placementMode && settings.useSeedPoint)
    {
        if (Input::Instance().IsMouseLeftTrigger() && !ImGui::GetIO().WantCaptureMouse)
        {
            auto scene = Editor::GetScene();
            if (scene)
            {
                CameraData* camData = nullptr;
                for (auto& obj : scene->GetGameObjects())
                {
                    camData = GameManager::Instance().GetECS().TryGetComponent<CameraData>(obj->GetEntityID());
                    if (camData) break;
                }

                if (camData)
                {
                    int mouseX = Input::Instance().GetMouseX();
                    int mouseY = Input::Instance().GetMouseY();
                    float screenW = ImGui::GetIO().DisplaySize.x;
                    float screenH = ImGui::GetIO().DisplaySize.y;

                    Math::Vector3 nearPoint = Math::Viewport(0, 0, screenW, screenH).Unproject(
                        Math::Vector3((float)mouseX, (float)mouseY, 0.0f),
                        camData->m_projMatrix, camData->m_viewMatrix, Math::Matrix::Identity);
                    Math::Vector3 farPoint = Math::Viewport(0, 0, screenW, screenH).Unproject(
                        Math::Vector3((float)mouseX, (float)mouseY, 1.0f),
                        camData->m_projMatrix, camData->m_viewMatrix, Math::Matrix::Identity);
                    
                    Math::Vector3 dir = farPoint - nearPoint;
                    dir.Normalize();

                    RaycastHit hit = CollisionManager::Instance().RaycastAgainstMesh(nearPoint, dir, 1000.0f, "Stage");
                    if (hit.hit)
                    {
                        settings.seedPoint = hit.point;
                        settingsChanged = true; // trigger update
                        m_placementMode = false; // exit placement mode after clicking
                    }
                }
            }
        }
    }

    // Apply settings changes after mouse click if needed
    if (settingsChanged)
    {
        settings.isDirty = true;
        navMeshManager.SetBakeSettings(settings);
    }

    if (ImGui::CollapsingHeader("Debug", ImGuiTreeNodeFlags_DefaultOpen))
    {
        bool showNavMesh = navMeshManager.IsDebugDrawEnabled();
        if (ImGui::Checkbox("Show NavMesh", &showNavMesh))
        {
            navMeshManager.SetDebugDrawEnabled(showNavMesh);
        }
        ImGui::SameLine();
        ImGui::TextDisabled(navMeshManager.IsBuilt() ? "(built)" : "(not built)");
        // TODO: Wireframe and other debug settings
    }

    ImGui::End();
}
