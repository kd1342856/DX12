#include "../../../Pch.h"
#include "ShaderSettingsIO.h"
#include "../ShaderManager/ShaderManager.h"

namespace
{
    const std::filesystem::path kPresetDir = L"Asset/Data/ShaderPreset";

    // --- 値とJSONの変換。設定の構造体に出てくる型だけ ---
    nlohmann::json ToJson(bool value)
    {
        return value;
    }

    nlohmann::json ToJson(int value)
    {
        return value;
    }

    nlohmann::json ToJson(float value)
    {
        return value;
    }

    nlohmann::json ToJson(const DirectX::XMFLOAT3& value)
    {
        return nlohmann::json::array({ value.x, value.y, value.z });
    }

    nlohmann::json ToJson(const DirectX::XMFLOAT4& value)
    {
        return nlohmann::json::array({ value.x, value.y, value.z, value.w });
    }

    // 型が合わない値は読まずに今の値のままにする(古いファイルや手で書き換えたファイルでも止まらないように)
    void FromJson(const nlohmann::json& json, bool& outValue)
    {
        if (json.is_boolean())
        {
            outValue = json.get<bool>();
        }
    }

    void FromJson(const nlohmann::json& json, int& outValue)
    {
        if (json.is_number())
        {
            outValue = json.get<int>();
        }
    }

    void FromJson(const nlohmann::json& json, float& outValue)
    {
        if (json.is_number())
        {
            outValue = json.get<float>();
        }
    }

    void FromJson(const nlohmann::json& json, DirectX::XMFLOAT3& outValue)
    {
        if (json.is_array() && json.size() >= 3 && json[0].is_number() && json[1].is_number() && json[2].is_number())
        {
            outValue = { json[0].get<float>(), json[1].get<float>(), json[2].get<float>() };
        }
    }

    void FromJson(const nlohmann::json& json, DirectX::XMFLOAT4& outValue)
    {
        if (json.is_array() && json.size() >= 4 && json[0].is_number() && json[1].is_number() && json[2].is_number() && json[3].is_number())
        {
            outValue = { json[0].get<float>(), json[1].get<float>(), json[2].get<float>(), json[3].get<float>() };
        }
    }

    template<typename TSettings>
    void SetDirty(TSettings& settings, decltype(settings.IsDirty)*)
    {
        settings.IsDirty = true;
    }

    // IsDirtyを持たない設定(SSAOSettings)は毎フレーム読まれるので何もしない
    template<typename TSettings>
    void SetDirty(TSettings&, ...)
    {
    }

    template<typename TSettings>
    nlohmann::json WriteGroup(TSettings& settings)
    {
        nlohmann::json group = nlohmann::json::object();
        settings.VisitFields([&](const FieldMeta& meta, auto& field)
        {
            // 画質設定・オプション画面・開発用が決める値は保存しない(シーンを開いたときにプレイヤーの設定を上書きしないため)
            if ((meta.flags & FieldFlags::kNotSaved) == 0)
            {
                group[meta.key] = ToJson(field);
            }
        });
        return group;
    }

    template<typename TSettings>
    void ReadGroup(const nlohmann::json& group, TSettings& settings)
    {
        if (!group.is_object())
        {
            return;
        }
        settings.VisitFields([&](const FieldMeta& meta, auto& field)
        {
            if ((meta.flags & FieldFlags::kNotSaved) != 0)
            {
                return;
            }
            auto it = group.find(meta.key);
            if (it != group.end())
            {
                FromJson(*it, field);
            }
        });
        SetDirty(settings, nullptr);
    }

    // 設定のグループをJSONのキーと組にして順に渡す
    template<typename TFunction>
    void ForEachGroup(RendererSettings& renderer, LightingSettings& lighting, ShadowSettings& shadow,
        PostProcessSettings& postProcess, SSAOSettings& ssao, FogSettings& fog, TFunction&& function)
    {
        function("Renderer", renderer);
        function("Lighting", lighting);
        function("Shadow", shadow);
        function("PostProcess", postProcess);
        function("SSAO", ssao);
        function("Fog", fog);
    }

    std::filesystem::path GetPresetPath(const std::string& name)
    {
        return kPresetDir / (std::filesystem::u8path(name).wstring() + L".json");
    }

    // ファイル名に使えない文字が無いか
    bool IsValidPresetName(const std::string& name)
    {
        if (name.empty() || name.size() > 64)
        {
            return false;
        }
        return name.find_first_of("\\/:*?\"<>|") == std::string::npos && name != "." && name != "..";
    }
}

nlohmann::json ShaderSettingsIO::Capture()
{
    ShaderManager& manager = ShaderManager::Instance();
    nlohmann::json json = nlohmann::json::object();
    ForEachGroup(manager.GetRendererSettings(), manager.GetLightingSettings(), manager.GetShadowSettings(),
        manager.GetPostProcessSettings(), manager.GetSSAOSettings(), manager.GetFogSettings(), [&](const char* pGroupName, auto& settings)
    {
        json[pGroupName] = WriteGroup(settings);
    });
    return json;
}

nlohmann::json ShaderSettingsIO::CaptureDefaults()
{
    RendererSettings renderer;
    LightingSettings lighting;
    ShadowSettings shadow;
    PostProcessSettings postProcess;
    SSAOSettings ssao;
    FogSettings fog;
    nlohmann::json json = nlohmann::json::object();
    ForEachGroup(renderer, lighting, shadow, postProcess, ssao, fog, [&](const char* pGroupName, auto& settings)
    {
        json[pGroupName] = WriteGroup(settings);
    });
    return json;
}

void ShaderSettingsIO::Apply(const nlohmann::json& json)
{
    if (!json.is_object())
    {
        return;
    }
    ShaderManager& manager = ShaderManager::Instance();
    ForEachGroup(manager.GetRendererSettings(), manager.GetLightingSettings(), manager.GetShadowSettings(),
        manager.GetPostProcessSettings(), manager.GetSSAOSettings(), manager.GetFogSettings(), [&](const char* pGroupName, auto& settings)
    {
        auto it = json.find(pGroupName);
        if (it != json.end())
        {
            ReadGroup(*it, settings);
        }
    });
}

int ShaderSettingsIO::CountDifferences(const nlohmann::json& lhs, const nlohmann::json& rhs)
{
    int count = 0;
    for (auto groupIt = lhs.begin(); groupIt != lhs.end(); ++groupIt)
    {
        auto otherGroupIt = rhs.find(groupIt.key());
        for (auto fieldIt = groupIt->begin(); fieldIt != groupIt->end(); ++fieldIt)
        {
            if (otherGroupIt == rhs.end() || !otherGroupIt->contains(fieldIt.key()) || (*otherGroupIt)[fieldIt.key()] != *fieldIt)
            {
                ++count;
            }
        }
    }
    return count;
}

std::vector<std::string> ShaderSettingsIO::ListPresets()
{
    std::vector<std::string> names;
    std::error_code errorCode;
    for (const auto& entry : std::filesystem::directory_iterator(kPresetDir, errorCode))
    {
        if (entry.is_regular_file() && entry.path().extension() == L".json")
        {
            names.push_back(entry.path().stem().u8string());
        }
    }
    std::sort(names.begin(), names.end());
    return names;
}

bool ShaderSettingsIO::HasPreset(const std::string& name)
{
    std::error_code errorCode;
    return IsValidPresetName(name) && std::filesystem::exists(GetPresetPath(name), errorCode);
}

bool ShaderSettingsIO::SavePreset(const std::string& name, std::string& outError)
{
    if (!IsValidPresetName(name))
    {
        outError = u8"名前が空か、ファイル名に使えない文字(\\ / : * ? \" < > |)が入っています";
        return false;
    }
    std::error_code errorCode;
    std::filesystem::create_directories(kPresetDir, errorCode);
    std::ofstream ofs(GetPresetPath(name));
    if (!ofs.is_open())
    {
        outError = u8"ファイルを書き込めませんでした";
        return false;
    }
    ofs << std::setw(4) << Capture() << std::endl;
    Logger::Instance().AddLog(Logger::LogLevel::Info, u8"[ShaderPreset] 保存しました: %s", name.c_str());
    return true;
}

bool ShaderSettingsIO::LoadPreset(const std::string& name, std::string& outError)
{
    std::ifstream ifs(GetPresetPath(name));
    if (!IsValidPresetName(name) || !ifs.is_open())
    {
        outError = u8"プリセットのファイルが見つかりません";
        return false;
    }
    nlohmann::json json = nlohmann::json::parse(ifs, nullptr, false);
    if (json.is_discarded())
    {
        outError = u8"JSONとして読めませんでした";
        return false;
    }
    Apply(json);
    Logger::Instance().AddLog(Logger::LogLevel::Info, u8"[ShaderPreset] 読み込みました: %s", name.c_str());
    return true;
}
