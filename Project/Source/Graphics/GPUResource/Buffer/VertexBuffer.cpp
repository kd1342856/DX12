#include "../../../Pch.h"
#include "VertexBuffer.h"

void VertexBuffer::Create(GraphicsDevice* pDevice, UINT vertexSize, UINT vertexCount, const void* pData)
{
    m_device = pDevice;
    m_vertexSize = vertexSize;
    m_vertexCount = vertexCount;
    m_bufferSize = vertexSize * vertexCount;

    // TODO: Step 1 の後半で ResourceUploader を作成するまでは
    // 既存の UploadHeap の直接生成を行うか、一時的に D3D12_HEAP_TYPE_UPLOAD で作る
    // DX12Framework では多くの場合、最初は Upload バッファとして作り、後で Default バッファにコピーするのが理想。
    // 今回はまず動くことを最優先とし、他の Mesh.cpp と同等の処理で UploadHeap で作成する。
    D3D12_HEAP_PROPERTIES prop = {};
    prop.Type = D3D12_HEAP_TYPE_DEFAULT;
    prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = m_bufferSize;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    auto hr = m_device->GetDevice()->CreateCommittedResource(
        &prop,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&m_resource));

    if (FAILED(hr)) return;

    m_view.BufferLocation = m_resource->GetGPUVirtualAddress();
    m_view.SizeInBytes = m_bufferSize;
    m_view.StrideInBytes = m_vertexSize;
}
