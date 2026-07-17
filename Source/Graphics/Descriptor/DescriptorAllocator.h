#pragma once
#include <mutex>

class DescriptorAllocator
{
public:
    DescriptorAllocator() = default;
    ~DescriptorAllocator() = default;

    bool Init(ID3D12Device* pDevice, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT numDescriptors, bool isShaderVisible);
    void Release();

    ID3D12DescriptorHeap* GetHeap() const { return m_heap.Get(); }
    UINT GetDescriptorSize() const { return m_descriptorSize; }
    UINT GetCapacity() const { return m_capacity; }

    UINT Allocate();
    void Free(UINT index);

    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(UINT index) const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(UINT index) const;

private:
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_heap;
    UINT m_descriptorSize = 0;
    UINT m_capacity = 0;

    std::vector<UINT> m_freeIndices;
    UINT m_nextFreeIndex = 0;
	std::mutex m_mutex;
};
