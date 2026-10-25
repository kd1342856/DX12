#include "../../../Pch.h"
#include "QueueManager.h"

bool QueueManager::Init(ID3D12Device* pDevice)
{
    m_upGraphicsQueue = std::make_unique<CommandQueue>();
    if (!m_upGraphicsQueue->Init(pDevice, D3D12_COMMAND_LIST_TYPE_DIRECT))
    {
        return false;
    }

    OutputDebugStringA("Initializing CopyQueue...\n");
    m_upCopyQueue = std::make_unique<CommandQueue>();
    if (m_upCopyQueue) OutputDebugStringA("CopyQueue allocated successfully!\n");
    if (!m_upCopyQueue->Init(pDevice, D3D12_COMMAND_LIST_TYPE_COPY))
    {
        return false;
    }

    return true;
}

void QueueManager::Shutdown()
{
    if (m_upCopyQueue)
    {
        m_upCopyQueue->Shutdown();
        m_upCopyQueue.reset();
    }
    if (m_upGraphicsQueue)
    {
        m_upGraphicsQueue->Shutdown();
        m_upGraphicsQueue.reset();
    }
}




void QueueManager::ExecuteGraphics(CommandContext* pContext)
{
    m_upGraphicsQueue->Execute(pContext);
}

void QueueManager::ExecuteCopy(CommandContext* pContext)
{
    m_upCopyQueue->Execute(pContext);
}

uint64_t QueueManager::SignalGraphics()
{
    return m_upGraphicsQueue->Signal();
}

uint64_t QueueManager::SignalCopy()
{
    return m_upCopyQueue->Signal();
}

void QueueManager::WaitGraphics(uint64_t fenceValue)
{
    m_upGraphicsQueue->WaitForFence(fenceValue);
}

void QueueManager::WaitCopy(uint64_t fenceValue)
{
    m_upCopyQueue->WaitForFence(fenceValue);
}

void QueueManager::WaitQueueGraphics(CommandQueue* pWaitOnQueue, uint64_t fenceValue)
{
    m_upGraphicsQueue->WaitQueue(pWaitOnQueue, fenceValue);
}


