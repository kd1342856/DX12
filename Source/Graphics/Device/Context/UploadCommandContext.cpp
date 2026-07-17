#include "../../../Pch.h"
#include "UploadCommandContext.h"

UploadCommandContext::~UploadCommandContext()
{
    Shutdown();
}

bool UploadCommandContext::Init(ID3D12Device* pDevice)
{
    HRESULT hr = pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_pCommandAllocator));
    if (FAILED(hr)) return false;

    hr = pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_pCommandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_pCmdList));
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
    pQueueManager->WaitQueueGraphics(pQueueManager->GetCopyQueue(), m_lastFenceValue);
}



