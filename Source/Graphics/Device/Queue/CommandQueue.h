#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <mutex>
#include <cstdint>

class CommandContext;

class CommandQueue
{
public:
    CommandQueue();
    ~CommandQueue();

    bool Init(ID3D12Device* pDevice, D3D12_COMMAND_LIST_TYPE type);
    void Shutdown();

    void Execute(CommandContext* pContext);
	void Execute(ID3D12CommandList* pCmdList);
	void ExecuteAndSignal(ID3D12CommandList* pCmdList, ID3D12Fence* pFence, uint64_t fenceValue);
    
    // フェンスを進めて、その値を返す
    uint64_t Signal();
	void Signal(ID3D12Fence* pFence, uint64_t value);

    // 持E??したフェンス値までCPUを征E??させる
    void WaitForFence(uint64_t fenceValue);
    void WaitQueue(CommandQueue* pWaitOnQueue, uint64_t fenceValue);

    uint64_t GetFenceValue() const { return m_fenceValue; }
    ID3D12Fence* GetFence() const { return m_pFence.Get(); }

    void Flush();

    ID3D12CommandQueue* GetQueue() const { return m_pCommandQueue.Get(); }
    D3D12_COMMAND_LIST_TYPE GetType() const { return m_type; }

private:
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_pCommandQueue;
    Microsoft::WRL::ComPtr<ID3D12Fence> m_pFence;
    
    D3D12_COMMAND_LIST_TYPE m_type;
    uint64_t m_fenceValue;
    HANDLE m_fenceEvent;
    
    // D3D12チE??チE??レイヤーのクラチE??ュを防ぐため、ExecuteとSignalを保護する
    std::recursive_mutex m_queueMutex;
};








