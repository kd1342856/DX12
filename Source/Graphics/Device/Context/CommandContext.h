#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <unordered_map>
#include <vector>

class GPUResource;

struct PendingTransition
{
    ID3D12Resource* resource;
    D3D12_RESOURCE_STATES before;
    D3D12_RESOURCE_STATES after;
};

class CommandContext
{
public:
    virtual ~CommandContext() = default;

    virtual void Begin() = 0;
    virtual void Close() = 0;
    virtual void ResetAllocator() = 0;

    ID3D12GraphicsCommandList6* GetCmdList() const { return m_pCmdList.Get(); }

    void TransitionResource(ID3D12Resource* pResource, D3D12_RESOURCE_STATES stateAfter);
    void TransitionResource(GPUResource* pResource, D3D12_RESOURCE_STATES stateAfter);
    void FlushResourceBarriers();
    
    // CommandContext がクローズ/サブミットされる際にグローバルな Tracker を更新する
    void ResolvePendingStates();

protected:
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> m_pCmdList;
    std::vector<PendingTransition> m_pendingStates;
    std::vector<D3D12_RESOURCE_BARRIER> m_pendingBarriers;
};
