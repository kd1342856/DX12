#pragma once
#include <string>
#include "LoadModelOption.h"

// Forward declaration for transitional compatibility
class ModelData;

class AssetImporter
{
public:
	virtual ~AssetImporter() = default;

	// Imports a model file (e.g. .fbx, .glb) into the engine format.
	virtual bool ImportModel(const std::string& filepath, const LoadModelOption& option, ModelData* pOutModelData) = 0;
};
