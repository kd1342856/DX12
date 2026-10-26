#pragma once
#include <chrono>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "../ShaderCompiler/ShaderCompiler.h"

//=============================
//
// シェーダーのホットリロード
//
//   Asset/Shader 以下のファイルの更新を監視し、そのファイルを使っているシェーダー(ソースかインクルード)を
//   コンパイルし直してPSOを作り直す。コンパイルに失敗したら前のシェーダーを使い続け、エラーをエディターに表示する。
//   コンパイルはShaderCompiler::TryCompileShaderを通すので、DXILのディスクキャッシュのキーと照合を共有する
//   (変更を元に戻したときはキャッシュから読む)。
//
//   シェーダーを持つ側(ShaderManager・ComputeShader)は、作ったときにRegisterで依存するファイルと
//   作り直す関数を登録する。作り直す関数はGPUが止まっている間に呼ばれるので、古いPSOをすぐに解放してよい。
//
//=============================
class ShaderHotReload
{
public:
    // 作り直す関数。成功したらtrue(falseなら前のシェーダーを使い続けている)
    using ReloadFunction = std::function<bool()>;

    struct ErrorEntry
    {
        std::string shaderName; // UTF-8。ファイル名とエントリポイント
        std::vector<ShaderDiagnostic> diagnostics;
        std::chrono::system_clock::time_point time;
    };

    static ShaderHotReload& Instance();

    // watchDirの下(サブフォルダを含む)を監視する
    void Initialize(const std::filesystem::path& watchDir);
    void Terminate();

    // pOwnerは登録を区別するキー(呼び出し側のオブジェクトのアドレス)。同じpOwnerで呼ぶと置き換える
    void Register(const void* pOwner, const std::string& name, const std::vector<std::wstring>& dependencies, ReloadFunction reload);
    void UpdateDependencies(const void* pOwner, const std::vector<std::wstring>& dependencies);
    void Unregister(const void* pOwner);

    // コンパイルの結果を記録する。失敗ならエラーの一覧に載せ、成功なら同じシェーダーのエラーを消す
    void ReportResult(const std::string& shaderName, const ShaderCompileResult& result, bool isSuccess);

    // メインスレッドで、フレームのコマンドを積み始める前に毎フレーム呼ぶ。
    // 変更があれば、GPUの完了を待ってから該当するシェーダーを作り直す
    void Update();

    // 登録しているシェーダーをすべて作り直す(エディターのボタン用。次のUpdateで行う)
    void RequestReloadAll() { m_isReloadAllRequested = true; }

    bool IsEnabled() const { return m_isEnabled; }
    void SetEnabled(bool isEnabled) { m_isEnabled = isEnabled; }

    // エディターの表示用。メインスレッドから読む
    std::vector<ErrorEntry> GetErrors() const;
    int GetLastReloadCount() const { return m_lastReloadCount; }
    int GetLastFailedCount() const { return m_lastFailedCount; }
    double GetLastReloadMilliseconds() const { return m_lastReloadMilliseconds; }
    std::chrono::system_clock::time_point GetLastReloadTime() const { return m_lastReloadTime; }
    size_t GetWatchedFileCount() const;
    size_t GetRegisteredCount() const;

private:
    struct Registration
    {
        std::string name;
        std::unordered_set<std::wstring> dependencies; // ShaderCompiler::NormalizePathで正規化したパス
        ReloadFunction reload;
    };

    ShaderHotReload() = default;

    void TrackFiles(const std::unordered_set<std::wstring>& paths);
    std::unordered_set<std::wstring> CollectChangedFiles();
    void Reload(const std::vector<const void*>& owners);

    std::filesystem::path m_watchDir;
    HANDLE m_hChangeNotification = INVALID_HANDLE_VALUE;
    bool m_isEnabled = true;
    bool m_hasPendingChange = false;
    bool m_isReloadAllRequested = false;
    std::chrono::steady_clock::time_point m_lastChangeTime;
    std::chrono::steady_clock::time_point m_lastPollTime;

    mutable std::mutex m_mutex;
    std::unordered_map<const void*, Registration> m_registrations;
    // 監視しているファイルと、最後に見た更新日時
    std::unordered_map<std::wstring, std::filesystem::file_time_type> m_fileTimes;
    std::unordered_map<std::string, ErrorEntry> m_errors;

    int m_lastReloadCount = 0;
    int m_lastFailedCount = 0;
    double m_lastReloadMilliseconds = 0.0;
    std::chrono::system_clock::time_point m_lastReloadTime;
};
