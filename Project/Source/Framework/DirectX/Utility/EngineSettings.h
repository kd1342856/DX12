#pragma once

// プロセス起動時にしか反映できないオプション（例: D3D12デバッグレイヤーはデバイス作成前
// にしか有効化できず、ライブ切り替えができない）のための小さな永続設定ヘルパー。ImGuiの
// チェックボックスで切り替えられるようにはできる - 値はディスクに保存され、*次回*起動時に
// 反映される。
//
// ファイル形式はあえて単純にしていて、exeと同じフォルダのEngineSettings.iniに設定ごとに
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
    // プロセスのカレントディレクトリが毎回同じ場合しか同じファイルを見つけられない。
    // Visual Studioからの起動(CWD=プロジェクトルート)、exeを直接ダブルクリック
    // (CWD=exe自身のフォルダ)、「Apply & Restart」ボタン、それぞれで別々のファイルを
    // 読み書きしてしまい、最後にどれを使ったかによって設定が黙って既定値に戻ってしまう
    // （まさにD3D12デバッグレイヤーのチェックボックスが反映されていないように見えた原因）。
    // exe自身のフォルダは、どの起動方法でも共通する唯一の場所。
    static std::string GetSettingsFilePath()
    {
        wchar_t exePath[MAX_PATH];
        DWORD len = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        if (len == 0 || len >= MAX_PATH) return "EngineSettings.ini"; // フォールバック: CWD相対

        std::wstring path(exePath, len);
        size_t slash = path.find_last_of(L"\\/");
        std::wstring dir = (slash == std::wstring::npos) ? L"" : path.substr(0, slash + 1);
        std::wstring fullW = dir + L"EngineSettings.ini";

        int utf8Len = WideCharToMultiByte(CP_UTF8, 0, fullW.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string full(utf8Len > 0 ? utf8Len - 1 : 0, '\0');
        if (utf8Len > 0) WideCharToMultiByte(CP_UTF8, 0, fullW.c_str(), -1, full.data(), utf8Len, nullptr, nullptr);
        return full;
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
