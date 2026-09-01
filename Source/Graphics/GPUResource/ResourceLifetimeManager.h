#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <mutex>
#include <cstdint>

// Manages safe deferred deletion of GPU resources.
// GPU resources cannot be freed immediately if the GPU is still referencing them.
// Enqueue a resource with DeferDelete() along with the fence value at time of deletion.
// Call ProcessDeleteQueue() each frame (after AcquireFrame) to safely release
// any resources whose associated fence value has been completed by the GPU.
class ResourceLifetimeManager
{
public:
    ResourceLifetimeManager() = default;
    ~ResourceLifetimeManager() = default;

    // Submit a GPU resource for deferred deletion.
    // 'fenceValue' is the Graphics Queue fence value at the time this resource
    // was last used. The resource will be released once the GPU completes that work.
    void DeferDelete(Microsoft::WRL::ComPtr<ID3D12Resource> resource, uint64_t fenceValue);

    // Call this once per frame at BeginFrame, passing the most recently
    // completed Graphics Queue fence value. Releases any resources that
    // the GPU has finished using.
    void ProcessDeleteQueue(uint64_t completedFenceValue);

    // Force-release all pending resources (call on Shutdown, after GPU idle).
    void ForceReleaseAll();

private:
    struct PendingDelete
    {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        uint64_t fenceValue = 0;
    };

    std::vector<PendingDelete> m_pendingDeletes;
    std::mutex m_mutex;
};
