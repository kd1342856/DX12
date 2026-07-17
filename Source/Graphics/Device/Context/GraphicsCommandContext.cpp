#include "../../../Pch.h"
#include "GraphicsCommandContext.h"
#include <assert.h>

bool GraphicsCommandContext::Init(ID3D12Device* pDevice)
{
    // ダミーのアロケータを一旦作ってコマンドリストを初期化する（コマンドリスト作成時に必須なため）
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> tempAlloc;
    HRESULT hr = pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&tempAlloc));
    if (FAILED(hr)) return false;

    hr = pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, tempAlloc.Get(), nullptr, IID_PPV_ARGS(&m_pCmdList));
    if (FAILED(hr)) return false;

    // 初期状態でCloseしておく
    m_pCmdList->Close();

    return true;
}

void GraphicsCommandContext::Shutdown()
{
    m_pCmdList.Reset();
}

void GraphicsCommandContext::Begin(FrameResource& frameResource)
{
    m_pCurrentAllocator = frameResource.GetCommandAllocator();
    m_pCurrentAllocator->Reset();
    
    // CommandListのリセット
    m_pCmdList->Reset(m_pCurrentAllocator, nullptr);
    
    // 定数バッファアロケータのリセット
    frameResource.BeginFrame();
}

void GraphicsCommandContext::Begin()
{
    // 通常のBeginはGraphicsContextでは直接使わない
    assert(m_pCurrentAllocator != nullptr && "Call Begin(FrameResource&) instead.");
    m_pCurrentAllocator->Reset();
    m_pCmdList->Reset(m_pCurrentAllocator, nullptr);
}

void GraphicsCommandContext::Close()
{
    m_pCmdList->Close();
}

void GraphicsCommandContext::ResetAllocator()
{
    if (m_pCurrentAllocator)
    {
        m_pCurrentAllocator->Reset();
    }
}



