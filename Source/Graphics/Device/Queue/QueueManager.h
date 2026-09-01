#pragma once
#include "CommandQueue.h"
#include "../Context/CommandContext.h"
#include <memory>

class QueueManager
{
public:
    QueueManager() = default;
    ~QueueManager() = default;

    bool Init(ID3D12Device* pDevice);
    void Shutdown();

    CommandQueue* GetGraphicsQueue() const { return m_upGraphicsQueue.get(); }
    CommandQueue* GetCopyQueue() const { return m_upCopyQueue.get(); }

    void ExecuteGraphics(CommandContext* pContext);
    void ExecuteCopy(CommandContext* pContext);
    uint64_t SignalGraphics();
    uint64_t SignalCopy();
    void WaitGraphics(uint64_t fenceValue);
    void WaitCopy(uint64_t fenceValue);
    void WaitQueueGraphics(CommandQueue* pWaitOnQueue, uint64_t fenceValue);

private:
    std::unique_ptr<CommandQueue> m_upGraphicsQueue = nullptr;
    std::unique_ptr<CommandQueue> m_upCopyQueue = nullptr;
};
