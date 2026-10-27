#include "EditorPreferences.h"
#include "../../Graphics/Shader/ShaderManager/ShaderManager.h"

namespace
{
    // 表示ガンマの範囲。外れた値が書かれていても画面が壊れないようにする
    constexpr float kMinDisplayGamma = 1.0f;
    constexpr float kMaxDisplayGamma = 3.0f;

    std::filesystem::path GetPath()
    {
        return AppPaths::GetSavedFilePath(L"EditorSettings.json");
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
}

void EditorPreferences::Save()
{
    nlohmann::json json = nlohmann::json::object();
    json["displayGamma"] = ShaderManager::Instance().GetEditorDisplayGamma();
    std::ofstream ofs(GetPath());
    if (!ofs.is_open())
    {
        Logger::Instance().AddLog(Logger::LogLevel::Error, u8"[EditorPreferences] EditorSettings.json を書き込めませんでした");
        return;
    }
    ofs << std::setw(4) << json << std::endl;
}
