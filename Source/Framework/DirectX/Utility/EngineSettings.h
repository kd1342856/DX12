#pragma once

// Tiny persisted-settings helper for options that can only take effect at process
// startup (e.g. the D3D12 debug layer, which must be enabled before the device is
// created and can't be toggled live). An ImGui checkbox can still let the user flip
// these - the value is saved to disk and picked up on the *next* launch.
//
// File format is deliberately trivial: one "Key=0"/"Key=1" line per setting in
// EngineSettings.ini next to the executable. Add new keys the same way as
// EnableD3D12DebugLayer below if more startup-only toggles are needed later.
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

    // Resolved once and reused, rather than using a bare relative "EngineSettings.ini" -
    // that only ever finds the same file if the process's current working directory happens
    // to be identical every time it's launched. Launching via Visual Studio (CWD = project
    // dir) vs. double-clicking the exe (CWD = its own folder) vs. the "Apply & Restart"
    // button would each read/write a *different* file, silently defaulting settings back to
    // their fallback value depending on which one you happened to use last (exactly what
    // made the D3D12 debug layer checkbox look like it wasn't sticking). The exe's own
    // directory is the one thing every launch method has in common.
    static std::string GetSettingsFilePath()
    {
        wchar_t exePath[MAX_PATH];
        DWORD len = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        if (len == 0 || len >= MAX_PATH) return "EngineSettings.ini"; // fallback: CWD-relative

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
