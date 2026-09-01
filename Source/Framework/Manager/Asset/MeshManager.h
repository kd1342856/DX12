#pragma once
#include <mutex>
#include "AssetHandle.h"
#include "AssetData.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>
#include "../../../Graphics/Geometry/Mesh/Mesh.h"

class MeshManager
{
public:
	static MeshManager& Instance() { static MeshManager inst; return inst; }

	AssetHandle<Mesh> CreateMesh(const AssetMeshData& data);

	// Handle -> Mesh* (with generation check)
	Mesh* Get(AssetHandle<Mesh> handle);

	// Handle -> Mesh* without lock (called from GPUUploadQueue::Process on main thread)
	Mesh* GetRaw(AssetHandle<Mesh> handle);

	void Release(AssetHandle<Mesh> handle);

private:
	struct MeshSlot {
		std::unique_ptr<Mesh> resource;
		uint32_t generation = 0;
		bool active = false;
	};

	std::vector<MeshSlot> m_slots;
	uint32_t m_nextIndex = 0;
	std::mutex m_mutex;
};

