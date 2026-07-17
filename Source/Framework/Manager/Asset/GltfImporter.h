#pragma once
#include <string>
#include "LoadModelOption.h"

#include "AssetImporter.h"

class ModelData;

class GltfImporter : public AssetImporter
{
public:
	bool ImportModel(const std::string& filepath, const LoadModelOption& option, ModelData* pOutModelData) override;
};
