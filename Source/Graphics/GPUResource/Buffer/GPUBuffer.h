#pragma once
#include "../GPUResource.h"

class GPUBuffer : public GPUResource
{
public:
    GPUBuffer() {}
    virtual ~GPUBuffer() {}

    D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const
    {
        if (m_resource) return m_resource->GetGPUVirtualAddress();
        return 0;
    }

    UINT GetBufferSize() const { return m_bufferSize; }

protected:
    UINT m_bufferSize = 0;
};
