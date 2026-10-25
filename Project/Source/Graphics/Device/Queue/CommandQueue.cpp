#include "../../../Pch.h"
#include "CommandQueue.h"
#include "../Context/CommandContext.h"
#include <assert.h>

CommandQueue::CommandQueue()
    : m_type(D3D12_COMMAND_LIST_TYPE_DIRECT)
    , m_fenceValue(0)
    , m_fenceEvent(nullptr)
{
}

CommandQueue::~CommandQueue()
{
    Shutdown();
}

bool CommandQueue::Init(ID3D12Device* pDevice, D3D12_COMMAND_LIST_TYPE type)
{
    m_type = type;

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = m_type;
    queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.NodeMask = 0;

    HRESULT hr = pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_pCommandQueue));
    if (FAILED(hr))
        return false;

    hr = pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_pFence));
    if (FAILED(hr))
        return false;

    m_fenceValue = 0;
    m_fenceEvent = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
    if (!m_fenceEvent)
        return false;

    return true;
}

void CommandQueue::Shutdown()
{
	if (!m_pCommandQueue) return;

	Flush();

    if (m_fenceEvent)
    {
        CloseHandle(m_fenceEvent);
        m_fenceEvent = nullptr;
    }

    m_pFence.Reset();
    m_pCommandQueue.Reset();
}

void CommandQueue::Execute(CommandContext* pContext)
{
	Execute(pContext->GetCmdList());
}

void CommandQueue::Execute(ID3D12CommandList* pCmdList)
{
	std::lock_guard<std::recursive_mutex> lock(m_queueMutex);
	ID3D12CommandList* ppCommandLists[] = { pCmdList };
	m_pCommandQueue->ExecuteCommandLists(1, ppCommandLists);
}

void CommandQueue::ExecuteAndSignal(ID3D12CommandList* pCmdList, ID3D12Fence* pFence, uint64_t fenceValue)
{
	std::lock_guard<std::recursive_mutex> lock(m_queueMutex);
	ID3D12CommandList* ppCommandLists[] = { pCmdList };
	m_pCommandQueue->ExecuteCommandLists(1, ppCommandLists);
	m_pCommandQueue->Signal(pFence, fenceValue);
}

uint64_t CommandQueue::Signal()
{
	std::lock_guard<std::recursive_mutex> lock(m_queueMutex);
	m_fenceValue++;
	m_pCommandQueue->Signal(m_pFence.Get(), m_fenceValue);
	return m_fenceValue;
}

void CommandQueue::Signal(ID3D12Fence* pFence, uint64_t value)
{
	std::lock_guard<std::recursive_mutex> lock(m_queueMutex);
	m_pCommandQueue->Signal(pFence, value);
}

void CommandQueue::WaitForFence(uint64_t fenceValue)
{
    if (m_pFence->GetCompletedValue() < fenceValue)
    {
        m_pFence->SetEventOnCompletion(fenceValue, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
}

void CommandQueue::Flush()
{
    WaitForFence(Signal());
}







void CommandQueue::WaitQueue(CommandQueue* pWaitOnQueue, uint64_t fenceValue)
{
    m_pCommandQueue->Wait(pWaitOnQueue->GetFence(), fenceValue);
}
