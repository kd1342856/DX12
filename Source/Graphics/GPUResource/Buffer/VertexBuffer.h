#pragma once
#include "GPUBuffer.h"

class VertexBuffer : public GPUBuffer
{
public:
    VertexBuffer() {}
    virtual ~VertexBuffer() {}

    void Create(GraphicsDevice* pDevice, UINT vertexSize, UINT vertexCount, const void* pData = nullptr);
    const D3D12_VERTEX_BUFFER_VIEW& GetView() const { return m_view; }

private:
    D3D12_VERTEX_BUFFER_VIEW m_view = {};
    UINT m_vertexCount = 0;
    UINT m_vertexSize = 0;
};
