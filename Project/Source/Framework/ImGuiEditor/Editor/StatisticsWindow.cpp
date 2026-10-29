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

        // 直近のフレーム時間の統計と、目標(平均100fps以上、1% low 60fps以上)との比較
        {
            constexpr float kTargetAvgFps = 100.0f;
            constexpr float kTargetOnePercentLowFps = 60.0f;
            const ImVec4 kPassColor(0.4f, 0.9f, 0.4f, 1.0f);
            const ImVec4 kFailColor(1.0f, 0.4f, 0.4f, 1.0f);

            const Profiler::FrameStats& stats = Profiler::Instance().GetRecentFrameStats();
            auto toFps = [](float ms)
            {
                return (ms > 0.0f) ? 1000.0f / ms : 0.0f;
            };
            const float avgFps = toFps(stats.avgMs);
            const float onePercentLowFps = toFps(stats.onePercentLowMs);

            ImGui::Text("--- Frame Stats (last %d frames) ---", stats.frameCount);
            ImGui::TextColored(avgFps >= kTargetAvgFps ? kPassColor : kFailColor,
                "Avg:     %6.1f fps (%6.2f ms)  target %.0f", avgFps, stats.avgMs, kTargetAvgFps);
            ImGui::TextColored(onePercentLowFps >= kTargetOnePercentLowFps ? kPassColor : kFailColor,
                "1%% low: %6.1f fps (%6.2f ms)  target %.0f", onePercentLowFps, stats.onePercentLowMs, kTargetOnePercentLowFps);
            ImGui::Text("P99: %.2f ms  Max: %.2f ms  StdDev: %.2f ms", stats.p99Ms, stats.maxMs, stats.stdDevMs);
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("1%% low = slowest 1%% of frames (average), P99 = 99th percentile frame time");
            }
            const Profiler::ScopeTiming& gpuFrame = Profiler::Instance().GetGPUFrameTiming();
            ImGui::Text("GPU Frame: %.2f ms (avg %.2f ms)", gpuFrame.ms, gpuFrame.avgMs);
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
        // avg は指数移動平均(およそ直近50回ぶん)。GPUの平均が重い順に並べる
        if (ImGui::BeginTable("PassTiming", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Pass");
            ImGui::TableSetupColumn("CPU");
            ImGui::TableSetupColumn("CPU avg");
            ImGui::TableSetupColumn("GPU");
            ImGui::TableSetupColumn("GPU avg");
            ImGui::TableHeadersRow();

            const auto& cpuTimings = Profiler::Instance().GetCPUTimings();
            const auto& gpuTimings = Profiler::Instance().GetGPUTimings();

            // 区間名はProfilerに1つずつ登録されているので、同じ名前ならポインタも同じ
            auto findTiming = [](const std::vector<Profiler::ScopeTiming>& timings, const char* pName) -> const Profiler::ScopeTiming*
            {
                for (const Profiler::ScopeTiming& timing : timings)
                {
                    if (timing.pName == pName)
                    {
                        return &timing;
                    }
                }
                return nullptr;
            };

            // 両方に出てきた名前の和集合。片方にしか無いパスも表示に出るようにする。
            // 毎フレーム確保しないよう、配列は使い回す
            static std::vector<const char*> s_names;
            s_names.clear();
            for (const Profiler::ScopeTiming& timing : cpuTimings)
            {
                s_names.push_back(timing.pName);
            }
            for (const Profiler::ScopeTiming& timing : gpuTimings)
            {
                if (!findTiming(cpuTimings, timing.pName))
                {
                    s_names.push_back(timing.pName);
                }
            }
            // GPUの平均が重い順。GPUで測っていない区間はその後ろに、CPUの平均が重い順
            std::sort(s_names.begin(), s_names.end(), [&](const char* pLhs, const char* pRhs)
            {
                const Profiler::ScopeTiming* pLhsGpu = findTiming(gpuTimings, pLhs);
                const Profiler::ScopeTiming* pRhsGpu = findTiming(gpuTimings, pRhs);
                if (pLhsGpu && pRhsGpu)
                {
                    return pLhsGpu->avgMs > pRhsGpu->avgMs;
                }
                if (pLhsGpu || pRhsGpu)
                {
                    return pLhsGpu != nullptr;
                }
                const Profiler::ScopeTiming* pLhsCpu = findTiming(cpuTimings, pLhs);
                const Profiler::ScopeTiming* pRhsCpu = findTiming(cpuTimings, pRhs);
                const float lhsCpuMs = pLhsCpu ? pLhsCpu->avgMs : 0.0f;
                const float rhsCpuMs = pRhsCpu ? pRhsCpu->avgMs : 0.0f;
                return lhsCpuMs > rhsCpuMs;
            });

            auto drawTiming = [](const Profiler::ScopeTiming* pTiming)
            {
                ImGui::TableNextColumn();
                if (pTiming)
                {
                    ImGui::Text("%.3f", pTiming->ms);
                }
                else
                {
                    ImGui::TextUnformatted("-");
                }
                ImGui::TableNextColumn();
                if (pTiming)
                {
                    ImGui::Text("%.3f", pTiming->avgMs);
                }
                else
                {
                    ImGui::TextUnformatted("-");
                }
            };

            for (const char* pName : s_names)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(pName);
                drawTiming(findTiming(cpuTimings, pName));
                drawTiming(findTiming(gpuTimings, pName));
            }
            ImGui::EndTable();
        }
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

