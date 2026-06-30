#pragma once
#include "../../../../Library/nlohmann/json.hpp"

class PrefabManager {
public:
    static PrefabManager& Instance();

    nlohmann::json GetPrefab(const std::string& filepath);

    void ClearCache();

private:
    PrefabManager() = default;
    ~PrefabManager() = default;

    std::unordered_map<std::string, nlohmann::json> m_prefabCache;
};

