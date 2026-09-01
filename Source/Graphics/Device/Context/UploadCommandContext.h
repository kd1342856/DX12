#pragma once
#include "CommandContext.h"
#include "../Queue/QueueManager.h"
#include <vector>
#include <d3d12.h>

// Upload context that uses D3D12_COMMAND_LIST_TYPE_COPY (Copy Queue)
// Note: ResourceBarrier(TRANSITION) is NOT allowed on Copy Queue.
// Pending barriers are stored here and flushed on the Graphics Queue in ResourceUploader::End().
class UploadCommandContext : public CommandContext
{
public:
    UploadCommandContext() = default;
    virtual ~UploadCommandContext();

    bool Init(ID3D12Device* pDevice);
    void Shutdown();

    virtual void Begin() override;
    virtual void Close() override;
    virtual void ResetAllocator() override;

    void Execute(QueueManager* pQueueManager);
    void WaitGraphics(QueueManager* pQueueManager);

    // Add a barrier to be issued on the Graphics Queue after copy is complete.
    // Copy Queue does NOT support TRANSITION barriers.
    void AddPendingBarrier(const D3D12_RESOURCE_BARRIER& barrier);
    const std::vector<D3D12_RESOURCE_BARRIER>& GetPendingBarriers() const { return m_pendingBarriers; }
    void ClearPendingBarriers() { m_pendingBarriers.clear(); }

private:
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_pCommandAllocator;
    
    UINT64 m_lastFenceValue = 0;

    // List of TRANSITION barriers to be issued on Graphics Queue after copy completes
    std::vector<D3D12_RESOURCE_BARRIER> m_pendingBarriers;
};
