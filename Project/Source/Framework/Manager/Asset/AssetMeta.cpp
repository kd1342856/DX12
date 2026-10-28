#include "../../../Pch.h"
#include "AssetMeta.h"
#include <fstream>
#include <sstream>

std::filesystem::path AssetMeta::GetPath(const std::string& assetPath)
{
    std::filesystem::path metaPath(assetPath);
    metaPath += ".meta";
    return metaPath;
}

AssetMeta::LoadResult AssetMeta::Load(const std::string& assetPath, nlohmann::ordered_json& outJson)
{
    outJson = nlohmann::ordered_json::object();
    const std::filesystem::path metaPath = GetPath(assetPath);
    std::error_code errorCode;
    if (!std::filesystem::exists(metaPath, errorCode))
    {
        return LoadResult::NotFound;
    }

    std::ifstream file(metaPath, std::ios::binary);
    std::stringstream stream;
    stream << file.rdbuf();
    nlohmann::ordered_json json = nlohmann::ordered_json::parse(stream.str(), nullptr, false);
    if (json.is_discarded() || !json.is_object())
    {
        Logger::Instance().AddLog(Logger::LogLevel::Error, u8"[AssetMeta] 読めないので使いません(上書きもしません): " + metaPath.string());
        return LoadResult::Broken;
    }

    int version = kVersion;
    if (json.contains("version") && json["version"].is_number_integer())
    {
        version = json["version"].get<int>();
    }
    else
    {
        Logger::Instance().AddLog(Logger::LogLevel::Warning, u8"[AssetMeta] versionが無いので、版1として読みます: " + metaPath.string());
    }
    if (version > kVersion)
    {
        Logger::Instance().AddLog(Logger::LogLevel::Warning, u8"[AssetMeta] 新しい版(" + std::to_string(version) + u8")の.metaなので使いません(上書きもしません): " + metaPath.string());
        return LoadResult::NewerVersion;
    }
    outJson = std::move(json);
    return LoadResult::Loaded;
}

bool AssetMeta::Save(const std::string& assetPath, const nlohmann::ordered_json& json)
{
    // "version"を先頭にし、残りは渡された順のまま
    nlohmann::ordered_json ordered = nlohmann::ordered_json::object();
    ordered["version"] = kVersion;
    for (auto it = json.begin(); it != json.end(); ++it)
    {
        if (it.key() != "version")
        {
            ordered[it.key()] = it.value();
        }
    }

    const std::filesystem::path metaPath = GetPath(assetPath);
    std::filesystem::path temporaryPath = metaPath;
    temporaryPath += ".tmp";
    {
        std::ofstream file(temporaryPath, std::ios::binary | std::ios::trunc);
        if (!file)
        {
            Logger::Instance().AddLog(Logger::LogLevel::Error, u8"[AssetMeta] 書けませんでした: " + metaPath.string());
            return false;
        }
        file << ordered.dump(2) << "\n";
    }
    std::error_code errorCode;
    std::filesystem::rename(temporaryPath, metaPath, errorCode);
    if (errorCode)
    {
        std::filesystem::remove(temporaryPath, errorCode);
        Logger::Instance().AddLog(Logger::LogLevel::Error, u8"[AssetMeta] 置き換えられませんでした: " + metaPath.string());
        return false;
    }
    return true;
}

bool AssetMeta::Remove(const std::string& assetPath)
{
    std::error_code errorCode;
    std::filesystem::remove(GetPath(assetPath), errorCode);
    return !errorCode;
}
