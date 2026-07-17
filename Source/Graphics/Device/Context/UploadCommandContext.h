#pragma once
#include "CommandContext.h"
#include "../Queue/QueueManager.h"

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

private:
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_pCommandAllocator;
    
    UINT64 m_lastFenceValue = 0;
};

