#include "../../Pch.h"
#include "DescriptorHeapManager.h"

bool DescriptorHeapManager::Init(ID3D12Device* pDevice, UINT numRTV, UINT numDSV, UINT numCBVSRVUAV, UINT numSampler)
{
    m_upRTVAllocator = std::make_unique<DescriptorAllocator>();
    if (!m_upRTVAllocator->Init(pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, numRTV, false)) return false;

    m_upDSVAllocator = std::make_unique<DescriptorAllocator>();
    if (!m_upDSVAllocator->Init(pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, numDSV, false)) return false;

    m_upCBVSRVUAVAllocator = std::make_unique<DescriptorAllocator>();
    if (!m_upCBVSRVUAVAllocator->Init(pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, numCBVSRVUAV, true)) return false;

    m_upSamplerAllocator = std::make_unique<DescriptorAllocator>();
    if (!m_upSamplerAllocator->Init(pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, numSampler, true)) return false;

    return true;
}

void DescriptorHeapManager::Release()
{
    if (m_upRTVAllocator) m_upRTVAllocator->Release();
    if (m_upDSVAllocator) m_upDSVAllocator->Release();
    if (m_upCBVSRVUAVAllocator) m_upCBVSRVUAVAllocator->Release();
    if (m_upSamplerAllocator) m_upSamplerAllocator->Release();
}