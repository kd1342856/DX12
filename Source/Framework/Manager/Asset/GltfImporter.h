#pragma once
#include <string>
#include "LoadModelOption.h"

#include "AssetImporter.h"
#include <filesystem>

struct AssetImportContext
{
    std::filesystem::path modelPath;
    std::filesystem::path rootDirectory;
    const struct aiScene* pScene = nullptr;
};

class ModelData;

class GltfImporter : public AssetImporter
{
public:
	bool ImportModel(const std::string& filepath, const LoadModelOption& option, ModelData* pOutModelData) override;
};
