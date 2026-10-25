#include "../../Pch.h"
#include "ResourceLifetimeManager.h"

void ResourceLifetimeManager::DeferDelete(
    Microsoft::WRL::ComPtr<ID3D12Resource> resource,
    uint64_t fenceValue)
{
    if (!resource) return;

    std::lock_guard<std::mutex> lock(m_mutex);

    PendingDelete entry;
    entry.resource   = std::move(resource);
    entry.fenceValue = fenceValue;
    m_pendingDeletes.push_back(std::move(entry));
}

void ResourceLifetimeManager::ProcessDeleteQueue(uint64_t completedFenceValue)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // Remove (and thereby Release) all entries whose fence value the GPU has passed.
    // Use erase-remove idiom to batch the deletions efficiently.
    auto it = m_pendingDeletes.begin();
    while (it != m_pendingDeletes.end())
    {
        if (it->fenceValue <= completedFenceValue)
        {
            // ComPtr goes out of scope here -> Release() called automatically
            it = m_pendingDeletes.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void ResourceLifetimeManager::ForceReleaseAll()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    // Reset all ComPtrs -> GPU must already be idle when this is called
    m_pendingDeletes.clear();
}
