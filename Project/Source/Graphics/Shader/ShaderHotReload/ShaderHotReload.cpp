#include "../../../Pch.h"
#include "ShaderHotReload.h"

namespace
{
    // 保存の途中(一時ファイルへの書き込みと置き換えなど)で読まないよう、最後の変更からこれだけ待つ
    constexpr auto kSettleTime = std::chrono::milliseconds(250);
    // 変更の通知を受け取れなかったときに、更新日時を見て回る間隔
    constexpr auto kFallbackPollInterval = std::chrono::seconds(1);

    bool GetWriteTime(const std::wstring& path, std::filesystem::file_time_type& outTime)
    {
        std::error_code errorCode;
        outTime = std::filesystem::last_write_time(path, errorCode);
        return !errorCode;
    }
}

ShaderHotReload& ShaderHotReload::Instance()
{
    // ComputeShaderを持つシングルトンは、終了時にこれより後で破棄されることがあり、そのデストラクタからUnregisterを呼ぶ。
    // 破棄の順に左右されないよう、わざと解放しない
    static ShaderHotReload* s_pInstance = new ShaderHotReload();
    return *s_pInstance;
}

void ShaderHotReload::Initialize(const std::filesystem::path& watchDir)
{
    Terminate();
    std::error_code errorCode;
    m_watchDir = std::filesystem::absolute(watchDir, errorCode);
    m_hChangeNotification = FindFirstChangeNotificationW(m_watchDir.c_str(), TRUE, FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME);
    if (m_hChangeNotification == INVALID_HANDLE_VALUE)
    {
        Logger::Instance().AddLog(Logger::LogLevel::Warning, u8"[ShaderHotReload] フォルダの変更を監視できないため、1秒ごとに更新日時を調べます: %s", AppPaths::ToUtf8(m_watchDir).c_str());
    }
    m_lastPollTime = std::chrono::steady_clock::now();
}

void ShaderHotReload::Terminate()
{
    if (m_hChangeNotification != INVALID_HANDLE_VALUE)
    {
        FindCloseChangeNotification(m_hChangeNotification);
        m_hChangeNotification = INVALID_HANDLE_VALUE;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    m_registrations.clear();
    m_fileTimes.clear();
}

void ShaderHotReload::Register(const void* pOwner, const std::string& name, const std::vector<std::wstring>& dependencies, ReloadFunction reload)
{
    Registration registration;
    registration.name = name;
    for (const std::wstring& path : dependencies)
    {
        registration.dependencies.insert(ShaderCompiler::NormalizePath(path));
    }
    registration.reload = std::move(reload);

    std::lock_guard<std::mutex> lock(m_mutex);
    TrackFiles(registration.dependencies);
    m_registrations[pOwner] = std::move(registration);
}

void ShaderHotReload::UpdateDependencies(const void* pOwner, const std::vector<std::wstring>& dependencies)
{
    std::unordered_set<std::wstring> normalized;
    for (const std::wstring& path : dependencies)
    {
        normalized.insert(ShaderCompiler::NormalizePath(path));
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_registrations.find(pOwner);
    if (it == m_registrations.end())
    {
        return;
    }
    TrackFiles(normalized);
    it->second.dependencies = std::move(normalized);
}

void ShaderHotReload::Unregister(const void* pOwner)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_registrations.erase(pOwner);
}

void ShaderHotReload::ReportResult(const std::string& shaderName, const ShaderCompileResult& result, bool isSuccess)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (isSuccess)
    {
        m_errors.erase(shaderName);
        return;
    }
    ErrorEntry& entry = m_errors[shaderName];
    entry.shaderName = shaderName;
    entry.diagnostics.clear();
    for (const ShaderDiagnostic& diagnostic : result.diagnostics)
    {
        // 失敗したときは警告まで並べると読みにくいので、エラーだけにする
        if (diagnostic.isError)
        {
            entry.diagnostics.push_back(diagnostic);
        }
    }
    if (entry.diagnostics.empty())
    {
        entry.diagnostics = result.diagnostics;
    }
    entry.time = std::chrono::system_clock::now();
}

std::vector<ShaderHotReload::ErrorEntry> ShaderHotReload::GetErrors() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ErrorEntry> errors;
    errors.reserve(m_errors.size());
    for (const auto& [name, entry] : m_errors)
    {
        errors.push_back(entry);
    }
    std::sort(errors.begin(), errors.end(), [](const ErrorEntry& lhs, const ErrorEntry& rhs)
    {
        return lhs.shaderName < rhs.shaderName;
    });
    return errors;
}

size_t ShaderHotReload::GetWatchedFileCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_fileTimes.size();
}

size_t ShaderHotReload::GetRegisteredCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_registrations.size();
}

void ShaderHotReload::TrackFiles(const std::unordered_set<std::wstring>& paths)
{
    // m_mutexを持った状態で呼ぶ
    for (const std::wstring& path : paths)
    {
        if (m_fileTimes.find(path) != m_fileTimes.end())
        {
            continue;
        }
        std::filesystem::file_time_type writeTime;
        if (GetWriteTime(path, writeTime))
        {
            m_fileTimes[path] = writeTime;
        }
        else
        {
            // まだ無いファイル(消えたインクルードなど)。できたときに変更として拾う
            m_fileTimes[path] = std::filesystem::file_time_type::min();
        }
    }
}

std::unordered_set<std::wstring> ShaderHotReload::CollectChangedFiles()
{
    std::unordered_set<std::wstring> changedFiles;
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& [path, lastTime] : m_fileTimes)
    {
        std::filesystem::file_time_type writeTime;
        if (!GetWriteTime(path, writeTime))
        {
            // 保存の途中で一時的に消えていることがある。次の変更で拾う
            continue;
        }
        if (writeTime != lastTime)
        {
            lastTime = writeTime;
            changedFiles.insert(path);
        }
    }
    return changedFiles;
}

void ShaderHotReload::Update()
{
    const auto now = std::chrono::steady_clock::now();

    if (m_hChangeNotification != INVALID_HANDLE_VALUE)
    {
        // 待たずに調べる。通知があれば次の通知を受け取れるようにしておく
        while (WaitForSingleObject(m_hChangeNotification, 0) == WAIT_OBJECT_0)
        {
            m_hasPendingChange = true;
            m_lastChangeTime = now;
            if (!FindNextChangeNotification(m_hChangeNotification))
            {
                FindCloseChangeNotification(m_hChangeNotification);
                m_hChangeNotification = INVALID_HANDLE_VALUE;
                break;
            }
        }
    }
    else if (now - m_lastPollTime >= kFallbackPollInterval)
    {
        m_lastPollTime = now;
        m_hasPendingChange = true;
        m_lastChangeTime = now - kSettleTime;
    }

    std::vector<const void*> owners;
    if (m_isReloadAllRequested)
    {
        m_isReloadAllRequested = false;
        // 更新日時も今の値にそろえ、この後の通知で二重に作り直さないようにする
        CollectChangedFiles();
        m_hasPendingChange = false;
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& [pOwner, registration] : m_registrations)
        {
            owners.push_back(pOwner);
        }
    }
    else
    {
        if (!m_isEnabled || !m_hasPendingChange || now - m_lastChangeTime < kSettleTime)
        {
            return;
        }
        m_hasPendingChange = false;

        const std::unordered_set<std::wstring> changedFiles = CollectChangedFiles();
        if (changedFiles.empty())
        {
            return;
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& [pOwner, registration] : m_registrations)
        {
            for (const std::wstring& path : changedFiles)
            {
                if (registration.dependencies.count(path) > 0)
                {
                    owners.push_back(pOwner);
                    break;
                }
            }
        }
    }

    if (!owners.empty())
    {
        Reload(owners);
    }
}

void ShaderHotReload::Reload(const std::vector<const void*>& owners)
{
    // 作り直す関数はShaderManagerなどのロックを取り、ReportResultでこちらのロックも取るので、ロックの外で呼ぶ
    std::vector<std::pair<std::string, ReloadFunction>> reloads;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const void* pOwner : owners)
        {
            auto it = m_registrations.find(pOwner);
            if (it != m_registrations.end())
            {
                reloads.emplace_back(it->second.name, it->second.reload);
            }
        }
    }
    if (reloads.empty())
    {
        return;
    }

    const auto startTime = std::chrono::steady_clock::now();

    // 前のフレームまでのコマンドが古いPSOとルートシグネチャを使い終わるのを待つ。
    // これでGPUが止まっているので、作り直す関数は古いものをすぐに解放してよい
    GraphicsDevice::Instance().GetQueueManager()->GetGraphicsQueue()->Flush();

    int failedCount = 0;
    for (const auto& [name, reload] : reloads)
    {
        if (!reload())
        {
            ++failedCount;
            Logger::Instance().AddLog(Logger::LogLevel::Error, u8"[ShaderHotReload] %s のコンパイルに失敗しました。前のシェーダーを使い続けます", name.c_str());
        }
    }

    const auto endTime = std::chrono::steady_clock::now();
    m_lastReloadCount = static_cast<int>(reloads.size());
    m_lastFailedCount = failedCount;
    m_lastReloadMilliseconds = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    m_lastReloadTime = std::chrono::system_clock::now();

    Logger::Instance().AddLog(failedCount > 0 ? Logger::LogLevel::Warning : Logger::LogLevel::Info,
        u8"[ShaderHotReload] %d 個のシェーダーを作り直しました(失敗 %d、%.0f ms)", m_lastReloadCount, failedCount, m_lastReloadMilliseconds);
}
