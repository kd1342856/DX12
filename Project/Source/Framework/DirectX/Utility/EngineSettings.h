#pragma once

// プロセス起動時にしか反映できないオプション（例: D3D12デバッグレイヤーはデバイス作成前
// にしか有効化できず、ライブ切り替えができない）のための小さな永続設定ヘルパー。ImGuiの
// チェックボックスで切り替えられるようにはできる - 値はディスクに保存され、*次回*起動時に
// 反映される。
//
// ファイル形式はあえて単純にしていて、<root>/Saved/EngineSettings.iniに設定ごとに
// "Key=0"/"Key=1" の行が1つ。今後起動時専用のトグルを増やす場合は、下のEnableD3D12DebugLayer
// と同じやり方でキーを追加すること。
class EngineSettings
{
public:
    static bool GetBool(const std::string& key, bool defaultValue)
    {
        auto& map = Instance().m_values;
        auto it = map.find(key);
        return it != map.end() ? it->second : defaultValue;
    }

    static void SetBool(const std::string& key, bool value)
    {
        Instance().m_values[key] = value;
        Instance().Save();
    }

private:
    static EngineSettings& Instance()
    {
        static EngineSettings instance;
        return instance;
    }

    EngineSettings() { Load(); }

    // 単純な相対パス "EngineSettings.ini" ではなく、一度だけ解決して使い回す - 相対パスだと、
    // プロセスのカレントディレクトリが毎回同じ場合しか同じファイルを見つけられない
    // （D3D12デバッグレイヤーのチェックボックスが反映されていないように見えた原因）。
    // AppPathsはexeの場所から<root>を見つけるので、Visual Studioからの起動、exeの直接起動、
    // 「Apply & Restart」のどれでも<root>/Saved/EngineSettings.iniを指す。
    static std::filesystem::path GetSettingsFilePath()
    {
        return AppPaths::GetSavedFilePath(L"EngineSettings.ini");
    }

    void Load()
    {
        std::ifstream ifs(GetSettingsFilePath());
        if (!ifs.is_open()) return;

        std::string line;
        while (std::getline(ifs, line))
        {
            size_t eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string key = line.substr(0, eq);
            std::string val = line.substr(eq + 1);
            if (!val.empty() && (val.back() == '\r')) val.pop_back();
            m_values[key] = (val == "1");
        }
    }

    void Save() const
    {
        std::ofstream ofs(GetSettingsFilePath(), std::ios::trunc);
        if (!ofs.is_open()) return;
        for (const auto& pair : m_values)
        {
            ofs << pair.first << "=" << (pair.second ? "1" : "0") << "\n";
        }
    }

    std::unordered_map<std::string, bool> m_values;
};
