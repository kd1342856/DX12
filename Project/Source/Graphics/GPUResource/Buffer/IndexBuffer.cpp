#include "../../../Pch.h"
#include "IndexBuffer.h"

void IndexBuffer::Create(GraphicsDevice* pDevice, UINT indexCount, const void* pData)
{
    m_device = pDevice;
    m_indexCount = indexCount;
    m_bufferSize = indexCount * sizeof(uint32_t); // 通常は32bitインデックス

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
    m_view.Format = DXGI_FORMAT_R32_UINT;
}
