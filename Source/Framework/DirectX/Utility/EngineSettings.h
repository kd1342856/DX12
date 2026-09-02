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

    void Load()
    {
        std::ifstream ifs("EngineSettings.ini");
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
        std::ofstream ofs("EngineSettings.ini", std::ios::trunc);
        if (!ofs.is_open()) return;
        for (const auto& pair : m_values)
        {
            ofs << pair.first << "=" << (pair.second ? "1" : "0") << "\n";
        }
    }

    std::unordered_map<std::string, bool> m_values;
};
