#pragma once
#include "DescriptorAllocator.h"

class DescriptorHeapManager
{
public:
    DescriptorHeapManager() = default;
    ~DescriptorHeapManager() = default;

    bool Init(ID3D12Device* pDevice, UINT numRTV, UINT numDSV, UINT numCBVSRVUAV, UINT numSampler);
    void Release();

    DescriptorAllocator* GetRTVAllocator() const { return m_upRTVAllocator.get(); }
    DescriptorAllocator* GetDSVAllocator() const { return m_upDSVAllocator.get(); }
    DescriptorAllocator* GetCBVSRVUAVAllocator() const { return m_upCBVSRVUAVAllocator.get(); }
    DescriptorAllocator* GetSamplerAllocator() const { return m_upSamplerAllocator.get(); }

private:
    std::unique_ptr<DescriptorAllocator> m_upRTVAllocator;
    std::unique_ptr<DescriptorAllocator> m_upDSVAllocator;
    std::unique_ptr<DescriptorAllocator> m_upCBVSRVUAVAllocator;
    std::unique_ptr<DescriptorAllocator> m_upSamplerAllocator;
};