#include "../../../Pch.h"
#include "AssetManager.h"
#include "GltfImporter.h"
#include "AssetImporter.h"
#include "../../System/JobSystem/JobSystem.h" // For async execution
#include "../../../Graphics/GPUUploadQueue/GPUUploadQueue.h"

void AssetManager::Initialize(GraphicsDevice* pDevice)
{
	m_pDevice = pDevice;
}

void AssetManager::Shutdown()
{
}

bool AssetManager::LoadModel(const std::string& filepath, const LoadModelOption& option, ModelData* pOutModelData)
{
	// Synchronous load
	GltfImporter importer;
	bool success = importer.ImportModel(filepath, option, pOutModelData);
	
	// Execute any uploads immediately since this is synchronous
	GPUUploadQueue::Instance().Process();
	
	return success;
}

void AssetManager::LoadModelAsync(const std::string& filepath, const LoadModelOption& option, std::function<void(bool, ModelData*)> onComplete)
{
	// ModelData needs to be allocated somewhere safe. 
	// For now, we allocate dynamically and pass it to the callback.
	ModelData* pModelData = new ModelData(); // Needs to be managed by the caller

	JobSystem::Instance().Execute([this, filepath, option, pModelData, onComplete]() {
		GltfImporter importer;
		bool success = importer.ImportModel(filepath, option, pModelData);

		GPUUploadQueue::Instance().Submit([success, pModelData, onComplete]() {
			if (onComplete) {
				onComplete(success, pModelData);
			}
		});
	});
}
