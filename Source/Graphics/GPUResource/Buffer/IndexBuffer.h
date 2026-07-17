#pragma once
#include "GPUBuffer.h"

class IndexBuffer : public GPUBuffer
{
public:
    IndexBuffer() {}
    virtual ~IndexBuffer() {}

    void Create(GraphicsDevice* pDevice, UINT indexCount, const void* pData = nullptr);
    const D3D12_INDEX_BUFFER_VIEW& GetView() const { return m_view; }

private:
    D3D12_INDEX_BUFFER_VIEW m_view = {};
    UINT m_indexCount = 0;
};
