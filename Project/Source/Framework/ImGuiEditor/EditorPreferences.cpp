#include "EditorPreferences.h"
#include "../../Graphics/Shader/ShaderManager/ShaderManager.h"

namespace
{
    // 表示ガンマの範囲。外れた値が書かれていても画面が壊れないようにする
    constexpr float kMinDisplayGamma = 1.0f;
    constexpr float kMaxDisplayGamma = 3.0f;

    std::vector<EditorPreferences::Pin> s_pins;

    std::filesystem::path GetPath()
    {
        return AppPaths::GetSavedFilePath(L"EditorSettings.json");
    }

    std::vector<EditorPreferences::Pin>::iterator FindPin(const std::string& target, const std::string& key)
    {
        return std::find_if(s_pins.begin(), s_pins.end(), [&](const EditorPreferences::Pin& pin)
        {
            return pin.target == target && pin.key == key;
        });
    }
}

void EditorPreferences::Load()
{
    std::ifstream ifs(GetPath());
    if (!ifs.is_open())
    {
        return;
    }
    const nlohmann::json json = nlohmann::json::parse(ifs, nullptr, false);
    if (json.is_discarded() || !json.is_object())
    {
        Logger::Instance().AddLog(Logger::LogLevel::Warning, u8"[EditorPreferences] EditorSettings.json を読めないので既定値を使います");
        return;
    }
    auto it = json.find("displayGamma");
    if (it != json.end() && it->is_number())
    {
        ShaderManager::Instance().SetEditorDisplayGamma(std::clamp(it->get<float>(), kMinDisplayGamma, kMaxDisplayGamma));
    }
    s_pins.clear();
    auto pinsIt = json.find("pins");
    if (pinsIt != json.end() && pinsIt->is_array())
    {
        for (const auto& pin : *pinsIt)
        {
            if (pin.is_object() && pin.contains("target") && pin.contains("key") && pin["target"].is_string() && pin["key"].is_string())
            {
                s_pins.push_back({ pin["target"].get<std::string>(), pin["key"].get<std::string>() });
            }
        }
    }
}

void EditorPreferences::Save()
{
    nlohmann::json json = nlohmann::json::object();
    json["displayGamma"] = ShaderManager::Instance().GetEditorDisplayGamma();
    nlohmann::json pins = nlohmann::json::array();
    for (const Pin& pin : s_pins)
    {
        pins.push_back({ { "target", pin.target }, { "key", pin.key } });
    }
    json["pins"] = pins;
    std::ofstream ofs(GetPath());
    if (!ofs.is_open())
    {
        Logger::Instance().AddLog(Logger::LogLevel::Error, u8"[EditorPreferences] EditorSettings.json を書き込めませんでした");
        return;
    }
    ofs << std::setw(4) << json << std::endl;
}

const std::vector<EditorPreferences::Pin>& EditorPreferences::GetPins()
{
    return s_pins;
}

bool EditorPreferences::IsPinned(const std::string& target, const std::string& key)
{
    return FindPin(target, key) != s_pins.end();
}

void EditorPreferences::TogglePin(const std::string& target, const std::string& key)
{
    auto it = FindPin(target, key);
    if (it != s_pins.end())
    {
        s_pins.erase(it);
    }
    else
    {
        s_pins.push_back({ target, key });
    }
    Save();
}

void EditorPreferences::MovePin(const std::string& target, const std::string& key, int offset)
{
    auto it = FindPin(target, key);
    if (it == s_pins.end())
    {
        return;
    }
    const ptrdiff_t index = it - s_pins.begin();
    const ptrdiff_t newIndex = index + offset;
    if (newIndex < 0 || newIndex >= static_cast<ptrdiff_t>(s_pins.size()))
    {
        return;
    }
    std::swap(s_pins[index], s_pins[newIndex]);
    Save();
}
