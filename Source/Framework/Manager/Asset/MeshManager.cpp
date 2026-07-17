#include "../../../Pch.h"
#include "MeshManager.h"
#include "../../../Graphics/Device/GraphicsDevice.h"

AssetHandle<Mesh> MeshManager::CreateMesh(const AssetMeshData& data)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	auto spMesh = std::make_unique<Mesh>();
	
	// Create GPU Resources via existing Mesh::Create method.
	// Currently Mesh::Create expects vertices and faces, but AssetMeshData uses indices directly.
	// We might need to adapt Mesh::Create or convert indices to MeshFace.
	std::vector<MeshFace> faces;
	faces.resize(data.indices.size() / 3);
	for (size_t i = 0; i < faces.size(); ++i) {
		faces[i].Idx[0] = data.indices[i * 3 + 0];
		faces[i].Idx[1] = data.indices[i * 3 + 1];
		faces[i].Idx[2] = data.indices[i * 3 + 2];
	}

	// Material passing is a bit tricky since Mesh::Create previously took a Material by value.
	// We will pass an empty Material for now, and rely on the RenderItem to bind the correct MaterialInstance.
	Material dummyMaterial;
	spMesh->Create(&GraphicsDevice::Instance(), data.vertices, faces, dummyMaterial);

	// Find an empty slot
	uint32_t slotIndex = m_nextIndex;
	for (uint32_t i = 0; i < m_slots.size(); ++i) {
		if (!m_slots[i].active) {
			slotIndex = i;
			break;
		}
	}

	if (slotIndex == m_slots.size()) {
		m_slots.push_back({});
		m_nextIndex++;
	}

	auto& slot = m_slots[slotIndex];
	slot.resource = std::move(spMesh);
	slot.generation++;
	slot.active = true;

	return AssetHandle<Mesh>(slotIndex, slot.generation);
}

Mesh* MeshManager::Get(AssetHandle<Mesh> handle)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!handle.IsValid() || handle.index >= m_slots.size()) return nullptr;
	
	auto& slot = m_slots[handle.index];
	if (slot.active && slot.generation == handle.generation) {
		return slot.resource.get();
	}
	return nullptr;
}

void MeshManager::Release(AssetHandle<Mesh> handle)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!handle.IsValid() || handle.index >= m_slots.size()) return;

	auto& slot = m_slots[handle.index];
	if (slot.active && slot.generation == handle.generation) {
		slot.resource.reset();
		slot.active = false;
	}
}

