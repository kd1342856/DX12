#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <functional>
#include "LoadModelOption.h"
#include "MeshManager.h"
#include "TextureManager.h"
#include "MaterialManager.h"
#include "AnimationAssetManager.h"

// Forward declaration
class GraphicsDevice;
class ModelData;

class AssetManager
{
public:
	static AssetManager& Instance() { static AssetManager inst; return inst; }

	void Initialize(GraphicsDevice* pDevice);
	void Shutdown();

	// Load model synchronously (returns pointer to a model structure, or handles. 
	// For now, let's keep it returning bool and filling a ModelData for transitional compatibility, 
	// or return a ModelHandle. We'll use a transitional approach if needed.)
	bool LoadModel(const std::string& filepath, const LoadModelOption& option, ModelData* pOutModelData);

	// Async load
	void LoadModelAsync(const std::string& filepath, const LoadModelOption& option, std::function<void(bool, ModelData*)> onComplete);

private:
	GraphicsDevice* m_pDevice = nullptr;
};
