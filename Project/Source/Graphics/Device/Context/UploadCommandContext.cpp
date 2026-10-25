#include "../../../Pch.h"
#include "UploadCommandContext.h"

UploadCommandContext::~UploadCommandContext()
{
    Shutdown();
}

bool UploadCommandContext::Init(ID3D12Device* pDevice)
{
    // Must use COPY type so it can be submitted to the Copy Queue.
    // Using DIRECT here would cause EXECUTECOMMANDLISTS_COMMANDLISTMISMATCH error.
    HRESULT hr = pDevice->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_COPY,
        IID_PPV_ARGS(&m_pCommandAllocator));
    if (FAILED(hr)) return false;

    // CommandList type must match the queue type it will be submitted to.
    hr = pDevice->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_COPY,
        m_pCommandAllocator.Get(),
        nullptr,
        IID_PPV_ARGS(&m_pCmdList));
    if (FAILED(hr)) return false;

    m_pCmdList->Close();

    return true;
}

void UploadCommandContext::Shutdown()
{
    m_pCmdList.Reset();
    m_pCommandAllocator.Reset();
}

void UploadCommandContext::Begin()
{
    m_pCommandAllocator->Reset();
    m_pCmdList->Reset(m_pCommandAllocator.Get(), nullptr);
}

void UploadCommandContext::Close()
{
    ResolvePendingStates();
    m_pCmdList->Close();
}

void UploadCommandContext::ResetAllocator()
{
    m_pCommandAllocator->Reset();
}

void UploadCommandContext::Execute(QueueManager* pQueueManager)
{
    Close();
    pQueueManager->ExecuteCopy(this);
    m_lastFenceValue = pQueueManager->SignalCopy();
}

void UploadCommandContext::WaitGraphics(QueueManager* pQueueManager)
{
    // Make the Graphics Queue wait for the Copy Queue to finish (GPU-side sync).
    pQueueManager->WaitQueueGraphics(pQueueManager->GetCopyQueue(), m_lastFenceValue);
}

void UploadCommandContext::AddPendingBarrier(const D3D12_RESOURCE_BARRIER& barrier)
{
    // Copy Queue does NOT support TRANSITION barriers.
    // Store them here and issue on the Graphics Queue in ResourceUploader::End().
    m_pendingBarriers.push_back(barrier);
}
