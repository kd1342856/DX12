#include "../../Pch.h"
#include "DescriptorAllocator.h"

bool DescriptorAllocator::Init(ID3D12Device* pDevice, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT numDescriptors, bool isShaderVisible)
{
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = type;
    desc.NumDescriptors = numDescriptors;
    desc.Flags = isShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    desc.NodeMask = 0;

    if (FAILED(pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_heap))))
    {
        return false;
    }

    m_descriptorSize = pDevice->GetDescriptorHandleIncrementSize(type);
    m_capacity = numDescriptors;
    m_nextFreeIndex = 0;
    m_freeIndices.clear();

    return true;
}

void DescriptorAllocator::Release()
{
    m_heap.Reset();
}

UINT DescriptorAllocator::Allocate()
{
	std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_freeIndices.empty())
    {
        UINT index = m_freeIndices.back();
        m_freeIndices.pop_back();
        return index;
    }

    if (m_nextFreeIndex < m_capacity)
    {
        return m_nextFreeIndex++;
    }

    assert(false && "Descriptor Heap is full!");
    return -1;
}

void DescriptorAllocator::Free(UINT index)
{
	std::lock_guard<std::mutex> lock(m_mutex);
    m_freeIndices.push_back(index);
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllocator::GetCPUHandle(UINT index) const
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_heap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(index) * m_descriptorSize;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorAllocator::GetGPUHandle(UINT index) const
{
    D3D12_GPU_DESCRIPTOR_HANDLE handle = m_heap->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<UINT64>(index) * m_descriptorSize;
    return handle;
}
