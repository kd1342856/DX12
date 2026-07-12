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

void Editor::DrawStatistics()
{
    if (ImGui::Begin("Statistics"))
    {
        ImGui::Text("FPS: %.1f (%.3f ms/frame)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
        ImGui::Separator();

        ImGui::Text("--- Memory ---");
        ImGui::Text("System RAM: %.2f MB", Profiler::Instance().GetSystemRAMUsageMB());
        ImGui::Text("VRAM Usage: %.2f MB", GraphicsDevice::Instance().GetVRAMUsageMB());
        ImGui::Separator();

        // 履歴用バッファ（毎フレーム記録）
        static float drawCallHistory[120] = {};
        static float instanceHistory[120] = {};
        static float activeJobHistory[120] = {};
        static int historyIdx = 0;

        uint32_t drawCalls = Profiler::Instance().GetDrawCallCount();
        uint32_t instances = Profiler::Instance().GetInstanceCount();
        int activeJobs = JobSystem::Instance().GetActiveJobCount();
        size_t workerCount = JobSystem::Instance().GetWorkerCount();

        drawCallHistory[historyIdx] = static_cast<float>(drawCalls);
        instanceHistory[historyIdx] = static_cast<float>(instances);
        activeJobHistory[historyIdx] = static_cast<float>(activeJobs);
        historyIdx = (historyIdx + 1) % 120;

        ImGui::Text("--- Rendering ---");
        ImGui::Text("Draw Calls: %u", drawCalls);
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("--- Draw Call Breakdown ---");
            for (const auto& pair : Profiler::Instance().GetDrawCallBreakdown())
            {
                ImGui::Text("%s : %u", pair.first.c_str(), pair.second);
            }
            ImGui::EndTooltip();
        }

        ImGui::Text("Instances: %u", instances);
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("--- Instance Breakdown ---");
            for (const auto& pair : Profiler::Instance().GetInstanceBreakdown())
            {
                ImGui::Text("%s : %u", pair.first.c_str(), pair.second);
            }
            ImGui::EndTooltip();
        }
        ImGui::Separator();

        ImGui::Text("--- Job System ---");
        ImGui::Text("Workers: %zu", workerCount);
        ImGui::Text("Active Jobs: %d", activeJobs);
        ImGui::Text("Queued Jobs: %zu", JobSystem::Instance().GetQueuedJobCount());

        std::vector<bool> workerStatuses = JobSystem::Instance().GetWorkerStatuses();
        static std::vector<std::vector<float>> workerHistories;
        static int wHistoryIdx = 0;

        if (workerHistories.size() != workerCount)
        {
            workerHistories.resize(workerCount, std::vector<float>(120, 0.0f));
        }

        for (size_t i = 0; i < workerCount; ++i)
        {
            workerHistories[i][wHistoryIdx] = workerStatuses[i] ? 1.0f : 0.0f;
        }
        wHistoryIdx = (wHistoryIdx + 1) % 120;

        if (ImGui::BeginTable("WorkerGraphs", 4))
        {
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
            for (size_t i = 0; i < workerCount; ++i)
            {
                ImGui::TableNextColumn();
                std::string label = "##Worker" + std::to_string(i);
                ImGui::PlotLines(label.c_str(), workerHistories[i].data(), 120, wHistoryIdx, nullptr, 0.0f, 1.0f, ImVec2(-FLT_MIN, 40));
            }
            ImGui::PopStyleColor();
            ImGui::EndTable();
        }
    }
    ImGui::End();
}

