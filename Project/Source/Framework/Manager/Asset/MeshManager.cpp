#include "../../../Pch.h"
#include "MeshManager.h"
#include "MaterialManager.h"
#include "../../../Graphics/Device/GraphicsDevice.h"
#include "../../../Graphics/GPUUploadQueue/GPUUploadQueue.h"

// CreateMesh
// Can be called from a worker thread.
// CPU-side data conversion only.
// GPU upload is deferred to main thread via GPUUploadQueue.
AssetHandle<Mesh> MeshManager::CreateMesh(const AssetMeshData& data)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // Convert index list to MeshFace (CPU processing only)
    std::vector<MeshFace> faces;
    faces.resize(data.indices.size() / 3);
    for (size_t i = 0; i < faces.size(); ++i) {
        faces[i].Idx[0] = data.indices[i * 3 + 0];
        faces[i].Idx[1] = data.indices[i * 3 + 1];
        faces[i].Idx[2] = data.indices[i * 3 + 2];
    }

    if (faces.empty() || data.vertices.empty()) {
        Logger::Instance().AddLog(Logger::LogLevel::Warning, "MeshManager::CreateMesh: vertices or faces are empty, skipping mesh creation.");
        return AssetHandle<Mesh>();
    }


    // Resolve material on CPU side
    Material mat;
    if (data.materialHandle.IsValid()) {
        if (auto* pMat = MaterialManager::Instance().Get(data.materialHandle)) mat = *pMat;
    }

    // Allocate slot (GPU resource not created yet)
    auto spMesh = std::make_unique<Mesh>();

    uint32_t slotIndex = m_nextIndex;
    for (uint32_t i = 0; i < m_slots.size(); ++i) {
        if (!m_slots[i].active) { slotIndex = i; break; }
    }
    if (slotIndex == (uint32_t)m_slots.size()) {
        m_slots.push_back({});
        m_nextIndex++;
    }

    auto& slot = m_slots[slotIndex];
    slot.resource = std::move(spMesh);
    slot.generation++;
    slot.active = true;

    AssetHandle<Mesh> handle(slotIndex, slot.generation);

    // Defer GPU upload to main thread (Handle only, not shared_ptr)
    MeshUploadData uploadData;
    uploadData.vertices     = data.vertices;
    uploadData.faces        = std::move(faces);
    uploadData.material     = mat;       // Material included
    uploadData.targetHandle = handle;    // Handle only (lifetime managed by MeshManager)

    UploadRequest req;
    req.data = std::move(uploadData);
    GPUUploadQueue::Instance().Submit(std::move(req));

    return handle;
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

// GetRaw - No lock version for main thread use (GPUUploadQueue::Process)
Mesh* MeshManager::GetRaw(AssetHandle<Mesh> handle)
{
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