#include "../ResourceStateTracker.h"
#include <memory>
#pragma once

#include <d3d12.h>
#include <wrl.h>

class CommandContext
{
public:
    virtual ~CommandContext() = default;

    virtual void Begin() = 0;
    virtual void Close() = 0;
    virtual void ResetAllocator() = 0;

    ID3D12GraphicsCommandList6* GetCmdList() const { return m_pCmdList.Get(); }

    ResourceStateTracker* GetResourceStateTracker() { return &m_resourceStateTracker; }

protected:
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> m_pCmdList;
    ResourceStateTracker m_resourceStateTracker;
};



