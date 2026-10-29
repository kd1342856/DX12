#include "../../Pch.h"
#include "BenchmarkRunner.h"
#include "../../Framework/System/JobSystem/JobSystem.h"
#include "../../Framework/Manager/NavMesh/NavMeshManager.h"
#include "../../Graphics/Renderer/Renderer.h"
#include "../../Graphics/Shader/ShaderManager/ShaderManager.h"
#include "ImGui/imgui_stdlib.h"
#include <shellapi.h>
#include <ctime>

namespace
{
    constexpr float kTargetAvgFps = 100.0f;
    constexpr float kTargetOnePercentLowFps = 60.0f;

    // 読み込みが終わったとみなすまでに、ジョブが空のフレームが続く数と、待つ時間の上限
    constexpr int kLoadingIdleFrames = 30;
    constexpr float kLoadingMinSeconds = 1.0f;
    constexpr float kLoadingTimeoutSeconds = 120.0f;

    float ToFps(float ms)
    {
        return (ms > 0.0f) ? 1000.0f / ms : 0.0f;
    }

    const char* GetQualityName(QualityLevel level)
    {
        switch (level)
        {
        case QualityLevel::Low:
            return "Low";
        case QualityLevel::Medium:
            return "Medium";
        case QualityLevel::High:
            return "High";
        case QualityLevel::Ultra:
            return "Ultra";
        }
        return "Unknown";
    }

    // カンマや引用符を含む文字列を CSV の1つの欄として書けるようにする
    std::string EscapeCsv(const std::string& text)
    {
        if (text.find_first_of(",\"\n") == std::string::npos)
        {
            return text;
        }
        std::string escaped = "\"";
        for (char character : text)
        {
            if (character == '"')
            {
                escaped += '"';
            }
            escaped += character;
        }
        escaped += '"';
        return escaped;
    }

    // Excel で日本語が化けないよう、BOM 付きの UTF-8 で開く
    bool OpenCsv(std::ofstream& out, const std::filesystem::path& path)
    {
        out.open(path, std::ios::binary);
        if (!out)
        {
            return false;
        }
        out << "\xEF\xBB\xBF";
        return true;
    }
}

bool BenchmarkRunner::IsRequestedByCommandLine()
{
    int argCount = 0;
    LPWSTR* ppArgs = CommandLineToArgvW(GetCommandLineW(), &argCount);
    if (!ppArgs)
    {
        return false;
    }
    bool isRequested = false;
    for (int argIndex = 1; argIndex < argCount; ++argIndex)
    {
        if (_wcsicmp(ppArgs[argIndex], L"-benchmark") == 0)
        {
            isRequested = true;
        }
    }
    LocalFree(ppArgs);
    return isRequested;
}

void BenchmarkRunner::Start(bool quitWhenDone)
{
    if (IsActive())
    {
        return;
    }
    m_quitWhenDone = quitWhenDone;

    // 起動引数から始めるときは保存してある経路を使う。エディターからは編集中の経路をそのまま使う
    if (quitWhenDone && !m_path.Load(m_pathFile))
    {
        Logger::Instance().AddLog(Logger::LogLevel::Error, u8"[Benchmark] 経路のファイルが無い: %s", m_pathFile.c_str());
    }
    if (!m_path.IsValid())
    {
        Logger::Instance().AddLog(Logger::LogLevel::Error, u8"[Benchmark] 経路の点が2つ未満なので始められない");
        if (quitWhenDone)
        {
            PostQuitMessage(1);
        }
        return;
    }

    // 合否は画質 High・描画解像度の倍率 1.0・fpsの上限なしで決める
    GraphicsDevice& device = GraphicsDevice::Instance();
    m_savedQuality = QualitySettings::GetLevel();
    m_savedRenderScaleOverride = QualitySettings::GetRenderScaleOverride();
    m_savedFrameRateMode = device.GetFrameRateMode();
    QualitySettings::Request(QualityLevel::High);
    QualitySettings::RequestRenderScaleOverride(0.0f);
    device.SetFrameRateMode(GraphicsDevice::FrameRateMode::Unlimited);

    m_hasSavedCamera = false;
    m_phase = Phase::WaitForLoading;
    m_phaseTime = 0.0f;
    m_idleFrameCount = 0;
    Logger::Instance().AddLog(Logger::LogLevel::Info, u8"[Benchmark] 開始(点 %zu 個、1周 %.1f 秒)", m_path.GetPoints().size(), m_path.GetDuration());
}

void BenchmarkRunner::Cancel(TransformData* pCamera)
{
    if (!IsActive())
    {
        return;
    }
    Logger::Instance().AddLog(Logger::LogLevel::Warning, u8"[Benchmark] 中止した(結果は出していない)");
    RestoreSettings(pCamera);
    if (m_quitWhenDone)
    {
        PostQuitMessage(1);
    }
}

void BenchmarkRunner::Update(float deltaTime, TransformData* pCamera)
{
    if (!IsActive())
    {
        return;
    }
    if (Input::Instance().IsKeyTrigger(DirectX::Keyboard::Keys::Escape))
    {
        Cancel(pCamera);
        return;
    }
    if (pCamera && !m_hasSavedCamera)
    {
        m_savedCameraPosition = pCamera->m_position;
        m_savedCameraRotation = pCamera->m_rotation;
        m_hasSavedCamera = true;
    }

    m_phaseTime += deltaTime;
    BenchmarkPose pose = m_path.Evaluate(0.0f);

    switch (m_phase)
    {
    case Phase::WaitForLoading:
    {
        const bool isIdle = JobSystem::Instance().GetQueuedJobCount() == 0 && JobSystem::Instance().GetActiveJobCount() == 0
            && NavMeshManager::Instance().IsBuilt();
        m_idleFrameCount = isIdle ? m_idleFrameCount + 1 : 0;
        const bool isTimedOut = m_phaseTime >= kLoadingTimeoutSeconds;
        if (isTimedOut)
        {
            Logger::Instance().AddLog(Logger::LogLevel::Warning, u8"[Benchmark] 読み込みが %.0f 秒で終わらなかったので、そのまま始める", kLoadingTimeoutSeconds);
        }
        if ((m_idleFrameCount >= kLoadingIdleFrames && m_phaseTime >= kLoadingMinSeconds) || isTimedOut)
        {
            m_phase = Phase::Warmup;
            m_phaseTime = 0.0f;
        }
        break;
    }
    case Phase::Warmup:
        SetHunt(m_path.GetPoints()[0].hunt);
        if (m_phaseTime >= m_path.GetWarmupSeconds())
        {
            BeginRun();
        }
        break;
    case Phase::Running:
        pose = m_path.Evaluate(m_phaseTime);
        SetHunt(m_path.GetPoints()[pose.segmentIndex].hunt);
        RecordFrame(pose.segmentIndex);
        if (m_phaseTime >= m_path.GetDuration())
        {
            FinishRun(pCamera);
            return;
        }
        break;
    case Phase::Idle:
        break;
    }

    if (pCamera)
    {
        pCamera->m_position = pose.position;
        pCamera->m_rotation = Math::Vector3(pose.pitch, pose.yaw, 0.0f);
    }
}

void BenchmarkRunner::BeginRun()
{
    m_phase = Phase::Running;
    m_phaseTime = 0.0f;
    m_frames.clear();
    m_passes.clear();
    // 1周ぶんを先に確保しておき、計測中にフレームの記録で確保が起きないようにする(500fps想定)
    m_frames.reserve(static_cast<size_t>(m_path.GetDuration() * 500.0f) + 1024);
    m_passes.reserve(128);
}

void BenchmarkRunner::RecordFrame(int segmentIndex)
{
    const Profiler& profiler = Profiler::Instance();
    FrameRecord record;
    record.time = m_phaseTime;
    record.frameMs = profiler.GetLastFrameMs();
    record.gpuFrameMs = profiler.GetGPUFrameTiming().ms;
    record.segmentIndex = segmentIndex;
    record.hunt = m_isHuntActive;
    m_frames.push_back(record);

    for (const Profiler::ScopeTiming& timing : profiler.GetGPUTimings())
    {
        PassAccumulator& pass = FindOrAddPass(timing.pName);
        pass.gpuSumMs += timing.ms;
        pass.gpuMaxMs = (std::max)(pass.gpuMaxMs, timing.ms);
        pass.gpuFrames++;
    }
    for (const Profiler::ScopeTiming& timing : profiler.GetCPUTimings())
    {
        PassAccumulator& pass = FindOrAddPass(timing.pName);
        pass.cpuSumMs += timing.ms;
        pass.cpuMaxMs = (std::max)(pass.cpuMaxMs, timing.ms);
        pass.cpuFrames++;
    }
}

BenchmarkRunner::PassAccumulator& BenchmarkRunner::FindOrAddPass(const char* pName)
{
    // 区間名は Profiler に1つずつ登録されているので、ポインタで比べられる
    for (PassAccumulator& pass : m_passes)
    {
        if (pass.pName == pName)
        {
            return pass;
        }
    }
    PassAccumulator& pass = m_passes.emplace_back();
    pass.pName = pName;
    return pass;
}

void BenchmarkRunner::FinishRun(TransformData* pCamera)
{
    // 出力先は Saved/Benchmark/<日時>/
    std::time_t now = std::time(nullptr);
    std::tm localTime = {};
    localtime_s(&localTime, &now);
    char stamp[32] = {};
    std::strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", &localTime);
    const std::filesystem::path outputDir = AppPaths::GetSavedDir() / "Benchmark" / stamp;

    m_lastResult = Result();
    if (WriteResults(AppPaths::ToUtf8(outputDir)))
    {
        const Profiler::FrameStats& stats = m_lastResult.stats;
        const bool isPassed = ToFps(stats.avgMs) >= kTargetAvgFps && ToFps(stats.onePercentLowMs) >= kTargetOnePercentLowFps;
        Logger::Instance().AddLog(isPassed ? Logger::LogLevel::Info : Logger::LogLevel::Warning,
            u8"[Benchmark] %s 平均 %.1ffps、1%% low %.1ffps、最悪 %.2fms(%d フレーム) → %s",
            isPassed ? u8"合格" : u8"不合格", ToFps(stats.avgMs), ToFps(stats.onePercentLowMs), stats.maxMs, stats.frameCount,
            m_lastResult.outputDir.c_str());
    }
    RestoreSettings(pCamera);
    if (m_quitWhenDone)
    {
        PostQuitMessage(0);
    }
}

void BenchmarkRunner::RestoreSettings(TransformData* pCamera)
{
    SetHunt(false);
    QualitySettings::Request(m_savedQuality);
    QualitySettings::RequestRenderScaleOverride(m_savedRenderScaleOverride);
    GraphicsDevice::Instance().SetFrameRateMode(m_savedFrameRateMode);
    if (pCamera && m_hasSavedCamera)
    {
        pCamera->m_position = m_savedCameraPosition;
        pCamera->m_rotation = m_savedCameraRotation;
    }
    m_phase = Phase::Idle;
}

void BenchmarkRunner::SetHunt(bool hunt)
{
    // ゴーストのハントと同じ数え方なので、ゴースト側の状態とは足し合わせになる(ゲームは止めているので変わらない)
    if (hunt != m_isHuntActive)
    {
        ShaderManager::Instance().SetHuntActive(hunt);
        m_isHuntActive = hunt;
    }
}

bool BenchmarkRunner::WriteResults(const std::string& outputDir)
{
    if (m_frames.empty())
    {
        Logger::Instance().AddLog(Logger::LogLevel::Error, u8"[Benchmark] 記録したフレームが無い");
        return false;
    }
    std::error_code errorCode;
    const std::filesystem::path dirPath = std::filesystem::u8path(outputDir);
    std::filesystem::create_directories(dirPath, errorCode);

    // 全体と区間ごとの統計
    std::vector<float> frameMs(m_frames.size());
    std::vector<float> scratch(m_frames.size());
    double gpuFrameSumMs = 0.0;
    for (size_t frameIndex = 0; frameIndex < m_frames.size(); ++frameIndex)
    {
        frameMs[frameIndex] = m_frames[frameIndex].frameMs;
        gpuFrameSumMs += m_frames[frameIndex].gpuFrameMs;
    }
    const Profiler::FrameStats stats = Profiler::ComputeFrameStats(frameMs.data(), static_cast<int>(frameMs.size()), scratch.data());
    const float gpuFrameAvgMs = static_cast<float>(gpuFrameSumMs / m_frames.size());
    const bool isPassed = ToFps(stats.avgMs) >= kTargetAvgFps && ToFps(stats.onePercentLowMs) >= kTargetOnePercentLowFps;

    // summary.csv: 1行に1項目
    {
        std::ofstream out;
        if (!OpenCsv(out, dirPath / "summary.csv"))
        {
            Logger::Instance().AddLog(Logger::LogLevel::Error, u8"[Benchmark] CSVを書けなかった: %s", outputDir.c_str());
            return false;
        }
        GraphicsDevice& device = GraphicsDevice::Instance();
        out << "item,value\n";
        out << "path_file," << EscapeCsv(m_pathFile) << "\n";
        out << "quality," << GetQualityName(QualitySettings::GetLevel()) << "\n";
        out << "render_scale," << QualitySettings::GetRenderScale() << "\n";
        out << "window_resolution," << device.GetBackBufferWidth() << "x" << device.GetBackBufferHeight() << "\n";
        out << "render_resolution," << Renderer::GetRenderWidth() << "x" << Renderer::GetRenderHeight() << "\n";
        out << "frames," << stats.frameCount << "\n";
        out << "duration_s," << m_frames.back().time << "\n";
        out << "avg_fps," << ToFps(stats.avgMs) << "\n";
        out << "avg_ms," << stats.avgMs << "\n";
        out << "one_percent_low_fps," << ToFps(stats.onePercentLowMs) << "\n";
        out << "one_percent_low_ms," << stats.onePercentLowMs << "\n";
        out << "p99_ms," << stats.p99Ms << "\n";
        out << "max_ms," << stats.maxMs << "\n";
        out << "stddev_ms," << stats.stdDevMs << "\n";
        out << "gpu_frame_avg_ms," << gpuFrameAvgMs << "\n";
        out << "target_avg_fps," << kTargetAvgFps << "\n";
        out << "target_one_percent_low_fps," << kTargetOnePercentLowFps << "\n";
        out << "result," << (isPassed ? "PASS" : "FAIL") << "\n";
    }

    // segments.csv: 経路の区間(出発した点)ごと
    {
        std::ofstream out;
        if (OpenCsv(out, dirPath / "segments.csv"))
        {
            out << "segment,label,hunt,frames,avg_fps,avg_ms,one_percent_low_fps,p99_ms,max_ms,gpu_frame_avg_ms\n";
            const std::vector<BenchmarkPoint>& points = m_path.GetPoints();
            for (int segmentIndex = 0; segmentIndex < static_cast<int>(points.size()); ++segmentIndex)
            {
                int segmentFrameCount = 0;
                double segmentGpuSumMs = 0.0;
                for (const FrameRecord& record : m_frames)
                {
                    if (record.segmentIndex == segmentIndex)
                    {
                        frameMs[segmentFrameCount++] = record.frameMs;
                        segmentGpuSumMs += record.gpuFrameMs;
                    }
                }
                if (segmentFrameCount == 0)
                {
                    continue;
                }
                const Profiler::FrameStats segmentStats = Profiler::ComputeFrameStats(frameMs.data(), segmentFrameCount, scratch.data());
                out << segmentIndex << "," << EscapeCsv(points[segmentIndex].label) << "," << (points[segmentIndex].hunt ? 1 : 0) << ","
                    << segmentFrameCount << "," << ToFps(segmentStats.avgMs) << "," << segmentStats.avgMs << ","
                    << ToFps(segmentStats.onePercentLowMs) << "," << segmentStats.p99Ms << "," << segmentStats.maxMs << ","
                    << segmentGpuSumMs / segmentFrameCount << "\n";
            }
        }
    }

    // passes.csv: 区間名ごとのGPU・CPU時間(その区間が出てきたフレームでの平均)。GPUの平均が重い順
    {
        std::vector<PassAccumulator> sortedPasses = m_passes;
        std::sort(sortedPasses.begin(), sortedPasses.end(), [](const PassAccumulator& lhs, const PassAccumulator& rhs)
        {
            const double lhsAvg = lhs.gpuFrames ? lhs.gpuSumMs / lhs.gpuFrames : -1.0;
            const double rhsAvg = rhs.gpuFrames ? rhs.gpuSumMs / rhs.gpuFrames : -1.0;
            return lhsAvg > rhsAvg;
        });
        std::ofstream out;
        if (OpenCsv(out, dirPath / "passes.csv"))
        {
            out << "pass,gpu_avg_ms,gpu_max_ms,gpu_frames,cpu_avg_ms,cpu_max_ms,cpu_frames\n";
            for (const PassAccumulator& pass : sortedPasses)
            {
                out << EscapeCsv(pass.pName) << ",";
                if (pass.gpuFrames > 0)
                {
                    out << pass.gpuSumMs / pass.gpuFrames << "," << pass.gpuMaxMs << "," << pass.gpuFrames << ",";
                }
                else
                {
                    out << ",,0,";
                }
                if (pass.cpuFrames > 0)
                {
                    out << pass.cpuSumMs / pass.cpuFrames << "," << pass.cpuMaxMs << "," << pass.cpuFrames << "\n";
                }
                else
                {
                    out << ",,0\n";
                }
            }
        }
    }

    // frames.csv: 1フレーム1行(グラフにする用)
    {
        std::ofstream out;
        if (OpenCsv(out, dirPath / "frames.csv"))
        {
            out << "frame,time_s,frame_ms,gpu_frame_ms,segment,hunt\n";
            for (size_t frameIndex = 0; frameIndex < m_frames.size(); ++frameIndex)
            {
                const FrameRecord& record = m_frames[frameIndex];
                out << frameIndex << "," << record.time << "," << record.frameMs << "," << record.gpuFrameMs << ","
                    << record.segmentIndex << "," << (record.hunt ? 1 : 0) << "\n";
            }
        }
    }

    m_lastResult.isValid = true;
    m_lastResult.stats = stats;
    m_lastResult.gpuFrameAvgMs = gpuFrameAvgMs;
    m_lastResult.outputDir = outputDir;
    return true;
}

void BenchmarkRunner::DrawOverlay() const
{
    ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.5f);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs
        | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    if (ImGui::Begin("##BenchmarkOverlay", nullptr, flags))
    {
        switch (m_phase)
        {
        case Phase::WaitForLoading:
            ImGui::TextUnformatted(u8"ベンチマーク: 読み込みを待っています");
            break;
        case Phase::Warmup:
            ImGui::TextUnformatted(u8"ベンチマーク: ウォームアップ中");
            break;
        case Phase::Running:
        {
            const float duration = (std::max)(m_path.GetDuration(), 0.001f);
            const BenchmarkPose pose = m_path.Evaluate(m_phaseTime);
            ImGui::Text(u8"ベンチマーク: %3.0f%%  %s%s", (std::min)(m_phaseTime / duration, 1.0f) * 100.0f,
                m_path.GetPoints()[pose.segmentIndex].label.c_str(), m_isHuntActive ? u8"(ハント)" : "");
            break;
        }
        case Phase::Idle:
            break;
        }
        ImGui::TextUnformatted(u8"Esc で中止");
    }
    ImGui::End();
}

void BenchmarkRunner::DrawEditorWindow(TransformData* pEditorCamera)
{
    if (!ImGui::Begin("Benchmark"))
    {
        ImGui::End();
        return;
    }

    if (IsActive())
    {
        ImGui::TextUnformatted(u8"計測中");
        if (ImGui::Button(u8"中止"))
        {
            Cancel(pEditorCamera);
        }
        ImGui::End();
        return;
    }

    // 経路のファイル
    ImGui::InputText(u8"ファイル", &m_pathFile);
    if (ImGui::Button(u8"読み込む"))
    {
        if (!m_path.Load(m_pathFile))
        {
            Logger::Instance().AddLog(Logger::LogLevel::Error, u8"[Benchmark] 経路を読み込めなかった: %s", m_pathFile.c_str());
        }
        m_selectedPointIndex = -1;
    }
    ImGui::SameLine();
    if (ImGui::Button(u8"保存"))
    {
        if (m_path.Save(m_pathFile))
        {
            Logger::Instance().AddLog(Logger::LogLevel::Info, u8"[Benchmark] 経路を保存した: %s", m_pathFile.c_str());
        }
    }

    float speed = m_path.GetSpeed();
    if (ImGui::DragFloat(u8"速さ (m/s)", &speed, 0.05f, 0.1f, 20.0f, "%.2f"))
    {
        m_path.SetSpeed(speed);
    }
    float warmupSeconds = m_path.GetWarmupSeconds();
    if (ImGui::DragFloat(u8"ウォームアップ (秒)", &warmupSeconds, 0.1f, 0.0f, 30.0f, "%.1f"))
    {
        m_path.SetWarmupSeconds(warmupSeconds);
    }

    std::vector<BenchmarkPoint>& points = m_path.GetPoints();
    ImGui::Text(u8"点 %zu 個、1周 %.1f 秒", points.size(), m_path.GetDuration());

    // 自由カメラ(F5)で飛んで、今の位置と向きを点にする。選んでいる点の後ろに入れる
    ImGui::BeginDisabled(pEditorCamera == nullptr);
    if (ImGui::Button(u8"カメラの位置に点を足す") && pEditorCamera)
    {
        BenchmarkPoint point;
        point.position = pEditorCamera->m_position;
        point.pitch = pEditorCamera->m_rotation.x;
        point.yaw = pEditorCamera->m_rotation.y;
        const int insertIndex = (m_selectedPointIndex >= 0) ? m_selectedPointIndex + 1 : static_cast<int>(points.size());
        point.label = "Point " + std::to_string(insertIndex);
        points.insert(points.begin() + insertIndex, point);
        m_selectedPointIndex = insertIndex;
    }
    ImGui::EndDisabled();

    // 点の一覧。並べ替えや削除はループの後でまとめて行う
    enum class PointAction
    {
        None,
        MoveUp,
        MoveDown,
        Delete,
    };
    PointAction action = PointAction::None;
    int actionIndex = -1;
    if (ImGui::BeginTable("BenchmarkPoints", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn(u8"名前");
        ImGui::TableSetupColumn(u8"ハント", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn(u8"止まる秒", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn(u8"操作", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();

        for (int pointIndex = 0; pointIndex < static_cast<int>(points.size()); ++pointIndex)
        {
            BenchmarkPoint& point = points[pointIndex];
            ImGui::PushID(pointIndex);
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            char indexLabel[16];
            snprintf(indexLabel, sizeof(indexLabel), "%d", pointIndex);
            if (ImGui::Selectable(indexLabel, m_selectedPointIndex == pointIndex))
            {
                m_selectedPointIndex = (m_selectedPointIndex == pointIndex) ? -1 : pointIndex;
            }

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputText("##Label", &point.label);

            ImGui::TableNextColumn();
            ImGui::Checkbox("##Hunt", &point.hunt);
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip(u8"この点から次の点に着くまで、ハント中の状態(フォグと暗さ)にする");
            }

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(60.0f);
            ImGui::DragFloat("##Wait", &point.waitSeconds, 0.1f, 0.0f, 60.0f, "%.1f");

            ImGui::TableNextColumn();
            ImGui::BeginDisabled(pEditorCamera == nullptr);
            if (ImGui::SmallButton(u8"見る") && pEditorCamera)
            {
                pEditorCamera->m_position = point.position;
                pEditorCamera->m_rotation = Math::Vector3(point.pitch, point.yaw, 0.0f);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip(u8"自由カメラをこの点に移す");
            }
            ImGui::SameLine();
            if (ImGui::SmallButton(u8"上書き") && pEditorCamera)
            {
                point.position = pEditorCamera->m_position;
                point.pitch = pEditorCamera->m_rotation.x;
                point.yaw = pEditorCamera->m_rotation.y;
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip(u8"この点を自由カメラの今の位置と向きにする");
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::SmallButton(u8"上"))
            {
                action = PointAction::MoveUp;
                actionIndex = pointIndex;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton(u8"下"))
            {
                action = PointAction::MoveDown;
                actionIndex = pointIndex;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton(u8"削除"))
            {
                action = PointAction::Delete;
                actionIndex = pointIndex;
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    const int pointCount = static_cast<int>(points.size());
    if (action == PointAction::MoveUp && actionIndex > 0)
    {
        std::swap(points[actionIndex], points[actionIndex - 1]);
        m_selectedPointIndex = actionIndex - 1;
    }
    else if (action == PointAction::MoveDown && actionIndex + 1 < pointCount)
    {
        std::swap(points[actionIndex], points[actionIndex + 1]);
        m_selectedPointIndex = actionIndex + 1;
    }
    else if (action == PointAction::Delete && actionIndex >= 0)
    {
        points.erase(points.begin() + actionIndex);
        m_selectedPointIndex = -1;
    }

    ImGui::Separator();
    ImGui::BeginDisabled(!m_path.IsValid());
    if (ImGui::Button(u8"ベンチマークを開始"))
    {
        Start(false);
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
        ImGui::SetTooltip(u8"画質を High、描画解像度の倍率を 1.0、fpsの上限をなしにして回る。\n"
            u8"変更の前後を比べるときは、起動引数 -benchmark で起動直後から測る");
    }

    if (m_lastResult.isValid)
    {
        const Profiler::FrameStats& stats = m_lastResult.stats;
        const float avgFps = ToFps(stats.avgMs);
        const float onePercentLowFps = ToFps(stats.onePercentLowMs);
        const bool isPassed = avgFps >= kTargetAvgFps && onePercentLowFps >= kTargetOnePercentLowFps;
        ImGui::TextColored(isPassed ? ImVec4(0.4f, 0.9f, 0.4f, 1.0f) : ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
            u8"前回: %s  平均 %.1f fps、1%% low %.1f fps、最悪 %.2f ms、GPU %.2f ms", isPassed ? u8"合格" : u8"不合格",
            avgFps, onePercentLowFps, stats.maxMs, m_lastResult.gpuFrameAvgMs);
        ImGui::TextWrapped("%s", m_lastResult.outputDir.c_str());
    }

    ImGui::End();
}
