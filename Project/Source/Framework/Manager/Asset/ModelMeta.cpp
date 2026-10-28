#include "../../../Pch.h"
#include "ModelMeta.h"
#include "AssetMeta.h"
#include <algorithm>
#include <unordered_set>

namespace
{
    void LogWarning(const std::string& modelPath, const std::string& message)
    {
        Logger::Instance().AddLog(Logger::LogLevel::Warning, u8"[ModelMeta] " + modelPath + ".meta: " + message);
    }

    std::string DescribeEntry(const std::string& name, int index)
    {
        return u8"「" + name + u8"」(番号 " + std::to_string(index) + ")";
    }
}

ModelMetaManager& ModelMetaManager::Instance()
{
    static ModelMetaManager instance;
    return instance;
}

void ModelMetaManager::LoadForModel(const std::string& modelPath, const std::vector<std::string>& materialNames)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    Model& model = m_models[modelPath];
    if (model.isDirty)
    {
        // エディターで変えたまま保存していないので、ファイルは読み直さない
        model.materialNames = materialNames;
        Resolve(modelPath, model);
        return;
    }

    model = Model{};
    model.materialNames = materialNames;
    nlohmann::ordered_json json;
    switch (AssetMeta::Load(modelPath, json))
    {
        case AssetMeta::LoadResult::NotFound:
            break;
        case AssetMeta::LoadResult::Loaded:
            model.hasFile = true;
            ParseFile(modelPath, json, model);
            break;
        case AssetMeta::LoadResult::Broken:
        case AssetMeta::LoadResult::NewerVersion:
            model.hasFile = true;
            model.isReadOnly = true;
            break;
    }
    Resolve(modelPath, model);
    model.isLoaded = true;
}

void ModelMetaManager::ParseFile(const std::string& modelPath, const nlohmann::ordered_json& json, Model& model) const
{
    for (auto it = json.begin(); it != json.end(); ++it)
    {
        if (it.key() == "version" || it.key() == "materials")
        {
            continue;
        }
        LogWarning(modelPath, u8"知らないキー「" + it.key() + u8"」は読み飛ばします(保存では残します)");
        model.unknownTopLevel[it.key()] = it.value();
    }

    if (!json.contains("materials"))
    {
        return;
    }
    const nlohmann::ordered_json& materials = json["materials"];
    if (!materials.is_array())
    {
        LogWarning(modelPath, u8"materialsが配列ではないので読み飛ばします");
        return;
    }
    for (const nlohmann::ordered_json& item : materials)
    {
        if (!item.is_object())
        {
            LogWarning(modelPath, u8"materialsの中にオブジェクトでない要素があるので読み飛ばします");
            continue;
        }
        Entry entry;
        if (item.contains("name") && item["name"].is_string())
        {
            entry.name = item["name"].get<std::string>();
        }
        if (item.contains("index") && item["index"].is_number_integer())
        {
            entry.index = item["index"].get<int>();
        }
        if (item.contains("overrides") && item["overrides"].is_object())
        {
            const nlohmann::ordered_json& overrides = item["overrides"];
            for (auto it = overrides.begin(); it != overrides.end(); ++it)
            {
                MaterialField field;
                MaterialConstants value;
                if (!MaterialFields::FindByKey(it.key(), field))
                {
                    LogWarning(modelPath, DescribeEntry(entry.name, entry.index) + u8"の知らないキー「" + it.key() + u8"」は読み飛ばします(保存では残します)");
                    entry.unknownOverrides[it.key()] = it.value();
                    continue;
                }
                if (!MaterialFields::FromJson(it.value(), value, field))
                {
                    LogWarning(modelPath, DescribeEntry(entry.name, entry.index) + u8"の「" + it.key() + u8"」の値の形が違うので読み飛ばします");
                    continue;
                }
                entry.materialOverride.Set(field, value);
            }
        }
        model.entries.push_back(std::move(entry));
    }
}

void ModelMetaManager::Resolve(const std::string& modelPath, Model& model) const
{
    const std::vector<std::string>& names = model.materialNames;
    const int materialCount = static_cast<int>(names.size());
    std::unordered_map<std::string, int> nameCounts;
    std::unordered_map<std::string, int> nameIndices;
    for (int materialIndex = 0; materialIndex < materialCount; ++materialIndex)
    {
        if (++nameCounts[names[materialIndex]] == 1)
        {
            nameIndices[names[materialIndex]] = materialIndex;
        }
    }

    std::unordered_set<int> usedIndices;
    for (Entry& entry : model.entries)
    {
        int found = -1;
        const auto countIt = nameCounts.find(entry.name);
        const bool isUniqueName = !entry.name.empty() && countIt != nameCounts.end() && countIt->second == 1;
        if (isUniqueName)
        {
            // 名前が一意ならそれで引く
            found = nameIndices[entry.name];
            if (entry.index >= 0 && entry.index != found)
            {
                LogWarning(modelPath, DescribeEntry(entry.name, entry.index) + u8"は番号が " + std::to_string(found) + u8" に変わっていたので、名前で当てました");
            }
        }
        else if (entry.index >= 0 && entry.index < materialCount)
        {
            // 名前が無いか、同じ名前が複数あるときだけ番号で引く。名前が違えば別のマテリアルなので当てない
            if (entry.name.empty() || names[entry.index] == entry.name)
            {
                found = entry.index;
            }
            else
            {
                LogWarning(modelPath, DescribeEntry(entry.name, entry.index) + u8"は、その番号のマテリアルの名前が「" + names[entry.index] + u8"」で食い違うので使いません(保存では残します)");
            }
        }
        else
        {
            LogWarning(modelPath, DescribeEntry(entry.name, entry.index) + u8"に当たるマテリアルが無いので使いません(保存では残します)");
        }

        if (found >= 0 && !usedIndices.insert(found).second)
        {
            LogWarning(modelPath, DescribeEntry(entry.name, entry.index) + u8"は、ほかのエントリと同じマテリアルに当たるので使いません(保存では残します)");
            found = -1;
        }
        entry.resolvedIndex = found;
    }
}

ModelMetaManager::Entry* ModelMetaManager::FindEntry(Model& model, int sourceIndex)
{
    for (Entry& entry : model.entries)
    {
        if (entry.resolvedIndex == sourceIndex)
        {
            return &entry;
        }
    }
    return nullptr;
}

const ModelMetaManager::Entry* ModelMetaManager::FindEntry(const Model& model, int sourceIndex) const
{
    for (const Entry& entry : model.entries)
    {
        if (entry.resolvedIndex == sourceIndex)
        {
            return &entry;
        }
    }
    return nullptr;
}

MaterialOverride ModelMetaManager::GetOverride(const std::string& modelPath, int sourceIndex) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_models.find(modelPath);
    if (sourceIndex < 0 || it == m_models.end())
    {
        return {};
    }
    const Entry* pEntry = FindEntry(it->second, sourceIndex);
    return pEntry ? pEntry->materialOverride : MaterialOverride{};
}

void ModelMetaManager::SetOverride(const std::string& modelPath, int sourceIndex, const MaterialOverride& materialOverride)
{
    if (sourceIndex < 0)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    Model& model = m_models[modelPath];
    Entry* pEntry = FindEntry(model, sourceIndex);
    if (!pEntry)
    {
        Entry entry;
        entry.index = sourceIndex;
        entry.resolvedIndex = sourceIndex;
        if (sourceIndex < static_cast<int>(model.materialNames.size()))
        {
            entry.name = model.materialNames[sourceIndex];
        }
        model.entries.push_back(std::move(entry));
        pEntry = &model.entries.back();
    }
    pEntry->materialOverride = materialOverride;
    model.isDirty = true;
}

std::string ModelMetaManager::GetMaterialName(const std::string& modelPath, int sourceIndex) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_models.find(modelPath);
    if (it == m_models.end() || sourceIndex < 0 || sourceIndex >= static_cast<int>(it->second.materialNames.size()))
    {
        return {};
    }
    return it->second.materialNames[sourceIndex];
}

int ModelMetaManager::CountUnmatched(const std::string& modelPath) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_models.find(modelPath);
    if (it == m_models.end())
    {
        return 0;
    }
    return static_cast<int>(std::count_if(it->second.entries.begin(), it->second.entries.end(), [](const Entry& entry)
    {
        return entry.resolvedIndex < 0;
    }));
}

bool ModelMetaManager::IsReadOnly(const std::string& modelPath) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_models.find(modelPath);
    return it != m_models.end() && it->second.isReadOnly;
}

bool ModelMetaManager::IsDirty(const std::string& modelPath) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_models.find(modelPath);
    return it != m_models.end() && it->second.isDirty;
}

int ModelMetaManager::CountDirtyModels() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<int>(std::count_if(m_models.begin(), m_models.end(), [](const auto& pair)
    {
        return pair.second.isDirty;
    }));
}

bool ModelMetaManager::Save(const std::string& modelPath)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_models.find(modelPath);
    if (it == m_models.end())
    {
        return false;
    }
    return SaveLocked(modelPath, it->second);
}

bool ModelMetaManager::SaveAll()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    bool isAllSaved = true;
    for (auto& [modelPath, model] : m_models)
    {
        if (model.isDirty)
        {
            isAllSaved &= SaveLocked(modelPath, model);
        }
    }
    return isAllSaved;
}

bool ModelMetaManager::SaveLocked(const std::string& modelPath, Model& model)
{
    if (model.isReadOnly)
    {
        Logger::Instance().AddLog(Logger::LogLevel::Error, u8"[ModelMeta] 読めなかった(か新しい版の).metaなので、上書きしません: " + modelPath + ".meta");
        return false;
    }

    // 当たったエントリはglTFの順に、今の名前と番号で書く。当たらなかったエントリは元のまま後ろに残す
    std::vector<const Entry*> ordered;
    for (const Entry& entry : model.entries)
    {
        if (!entry.materialOverride.IsEmpty() || !entry.unknownOverrides.empty())
        {
            ordered.push_back(&entry);
        }
    }
    std::stable_sort(ordered.begin(), ordered.end(), [](const Entry* pLeft, const Entry* pRight)
    {
        const bool isLeftMatched = pLeft->resolvedIndex >= 0;
        const bool isRightMatched = pRight->resolvedIndex >= 0;
        if (isLeftMatched != isRightMatched)
        {
            return isLeftMatched;
        }
        return isLeftMatched && pLeft->resolvedIndex < pRight->resolvedIndex;
    });

    nlohmann::ordered_json materials = nlohmann::ordered_json::array();
    for (const Entry* pEntry : ordered)
    {
        const bool isMatched = pEntry->resolvedIndex >= 0 && pEntry->resolvedIndex < static_cast<int>(model.materialNames.size());
        nlohmann::ordered_json item = nlohmann::ordered_json::object();
        item["name"] = isMatched ? model.materialNames[pEntry->resolvedIndex] : pEntry->name;
        item["index"] = isMatched ? pEntry->resolvedIndex : pEntry->index;
        nlohmann::ordered_json overrides = nlohmann::ordered_json::object();
        for (int fieldIndex = 0; fieldIndex < MaterialFields::kCount; ++fieldIndex)
        {
            const auto field = static_cast<MaterialField>(fieldIndex);
            if (pEntry->materialOverride.Has(field))
            {
                overrides[MaterialFields::GetKey(field)] = MaterialFields::ToJson(pEntry->materialOverride.values, field);
            }
        }
        for (auto it = pEntry->unknownOverrides.begin(); it != pEntry->unknownOverrides.end(); ++it)
        {
            overrides[it.key()] = it.value();
        }
        item["overrides"] = std::move(overrides);
        materials.push_back(std::move(item));
    }

    // 上書きが1つも無くなったら、.metaを消す
    if (materials.empty() && model.unknownTopLevel.empty())
    {
        if (model.hasFile && !AssetMeta::Remove(modelPath))
        {
            return false;
        }
        model.hasFile = false;
        model.isDirty = false;
        return true;
    }

    nlohmann::ordered_json json = nlohmann::ordered_json::object();
    json["materials"] = std::move(materials);
    for (auto it = model.unknownTopLevel.begin(); it != model.unknownTopLevel.end(); ++it)
    {
        json[it.key()] = it.value();
    }
    if (!AssetMeta::Save(modelPath, json))
    {
        return false;
    }
    model.hasFile = true;
    model.isDirty = false;
    Logger::Instance().AddLog(Logger::LogLevel::Info, u8"[ModelMeta] 保存しました: " + modelPath + ".meta");
    return true;
}
