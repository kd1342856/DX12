#include "../../../Pch.h"
#include "ConstantBuffer.h"

void ConstantBuffer::Create(GraphicsDevice* pDevice, UINT size)
{
    m_device = pDevice;
    m_bufferSize = (size + 255) & ~255; // 256バイトアライメント

    D3D12_HEAP_PROPERTIES prop = {};
    prop.Type = D3D12_HEAP_TYPE_UPLOAD;
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
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_resource));

    if (FAILED(hr)) return;

    m_resource->Map(0, nullptr, &m_pMappedData);
}

void ConstantBuffer::UpdateData(const void* pData, UINT size)
{
    if (m_pMappedData && pData)
    {
        memcpy(m_pMappedData, pData, size);
    }
}
