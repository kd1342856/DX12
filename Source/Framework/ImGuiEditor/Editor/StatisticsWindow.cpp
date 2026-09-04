#include "../../../Pch.h"
#include "../../Manager/Scene/Scene.h"
#include "../../Manager/Scene/SceneManager.h"
#include "../../Manager/Collision/CollisionManager.h"
#include "../../DirectX/Utility/Profiler.h"
#include "../../System/JobSystem/JobSystem.h"
#include "../../Manager/NavMesh/NavMeshManager.h"
#include "../../../Graphics/Device/GraphicsDevice.h"

// Resolve the GetCurrentScene issue
static std::shared_ptr<Scene> GetCurrentScenePtr() {
    return Editor::GetScene();
}

void Editor::DrawStatistics()
{
    if (ImGui::Begin("Statistics"))
    {
        ImGui::Text("FPS: %.1f (%.3f ms/frame)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
#ifdef _DEBUG
        // このセッションでの実測値。GraphicsDevice::Init()で一度だけ決まる - 永続化された
        // リクエスト(次回起動時に反映)についてはRendererPanelのチェックボックスを参照。
        // 再起動するまではこの値と食い違っていて正常。
        ImGui::Text("D3D12 Debug Layer: %s", GraphicsDevice::IsDebugLayerActive() ? "ON" : "OFF");
#endif

        // Present～Present間のフレーム時間の履歴(上のImGui自身の平滑化されたFramerateとは
        // 違い、これがプレイヤーの体感そのもの)。
        {
            const auto& frameHistory = Profiler::Instance().GetFrameTimeHistory();
            float frameTimes[Profiler::kFrameHistorySize];
            std::copy(frameHistory.begin(), frameHistory.end(), frameTimes);
            float maxMs = 0.0f;
            for (float v : frameTimes) maxMs = std::max(maxMs, v);
            ImGui::PlotLines("##FrameTimeMs", frameTimes, Profiler::kFrameHistorySize,
                Profiler::Instance().GetFrameTimeHistoryIndex(), nullptr, 0.0f,
                std::max(maxMs, 1.0f) * 1.1f, ImVec2(-FLT_MIN, 60));
        }
        ImGui::Separator();

        ImGui::Text("--- Memory ---");
        ImGui::Text("System RAM: %.2f MB", Profiler::Instance().GetSystemRAMUsageMB());
        ImGui::Text("VRAM Usage: %.2f MB", GraphicsDevice::Instance().GetVRAMUsageMB());
        ImGui::Separator();

        ImGui::Text("--- Pass Timing (ms) ---");
        // GPU計測はCPU計測より数フレーム遅れる(記録先のフレームインフライトスロットが
        // 再利用しても安全になった時点で結果を読み戻すため)が、パス名で対応が取れるので、
        // 単純に横並びのテーブルにするだけで重いパスを見つけるには十分。
        if (ImGui::BeginTable("PassTiming", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Pass");
            ImGui::TableSetupColumn("CPU");
            ImGui::TableSetupColumn("GPU");
            ImGui::TableHeadersRow();

            const auto& cpuTimings = Profiler::Instance().GetCPUTimings();
            const auto& gpuTimings = Profiler::Instance().GetGPUTimings();

            // 両方に出てきた名前の和集合。片方にしか無いパスも表示に出るようにする。
            std::vector<std::string> names;
            names.reserve(cpuTimings.size() + gpuTimings.size());
            for (const auto& pair : cpuTimings) names.push_back(pair.first);
            for (const auto& pair : gpuTimings)
            {
                if (std::find(names.begin(), names.end(), pair.first) == names.end())
                    names.push_back(pair.first);
            }
            std::sort(names.begin(), names.end());

            for (const auto& name : names)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%s", name.c_str());
                ImGui::TableNextColumn();
                auto cpuIt = cpuTimings.find(name);
                if (cpuIt != cpuTimings.end()) ImGui::Text("%.3f", cpuIt->second);
                else ImGui::TextUnformatted("-");
                ImGui::TableNextColumn();
                auto gpuIt = gpuTimings.find(name);
                if (gpuIt != gpuTimings.end()) ImGui::Text("%.3f", gpuIt->second);
                else ImGui::TextUnformatted("-");
            }
            ImGui::EndTable();
        }
        ImGui::Separator();

        // ����p�o�b�t�@�i���t���[���L�^�j
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
        ImGui::Text("Entities: %u visible / %u culled", Profiler::Instance().GetEntitiesVisible(), Profiler::Instance().GetEntitiesCulled());
        ImGui::Text("Meshes:   %u visible / %u culled", Profiler::Instance().GetMeshesVisible(), Profiler::Instance().GetMeshesCulled());
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
        // 小さいNavMeshだとfindPathは1フレームより十分速く終わるので、「Active Jobs」が
        // 1になる瞬間を目で追うのは難しい。この合計値は、非同期の経路再計算
        // (NavMeshManager::MoveToward)が実際に起きている限り増え続ける。
        ImGui::Text("Async Path Recomputes: %d", NavMeshManager::Instance().GetAsyncRecomputeCount());

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

