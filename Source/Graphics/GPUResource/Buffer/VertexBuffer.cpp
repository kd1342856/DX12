#include "../../../Pch.h"
#include "VertexBuffer.h"

void VertexBuffer::Create(GraphicsDevice* pDevice, UINT vertexSize, UINT vertexCount, const void* pData)
{
    m_device = pDevice;
    m_vertexSize = vertexSize;
    m_vertexCount = vertexCount;
    m_bufferSize = vertexSize * vertexCount;

    // TODO: Step 1 縺ｮ蠕悟濠縺ｧ ResourceUploader 繧剃ｽ懈・縺吶ｋ縺ｾ縺ｧ縺ｯ
    // 譌｢蟄倥・ UploadHeap 縺ｮ逶ｴ謗･逕滓・繧定｡後≧縺九∽ｸ譎ら噪縺ｫ D3D12_HEAP_TYPE_UPLOAD 縺ｧ菴懊ｋ
    // DX12Framework 縺ｧ縺ｯ螟壹￥縺ｮ蝣ｴ蜷医∵怙蛻昴・ Upload 繝舌ャ繝輔ぃ縺ｨ縺励※菴懊ｊ縲∝ｾ後〒 Default 繝舌ャ繝輔ぃ縺ｫ繧ｳ繝斐・縺吶ｋ縺ｮ縺檎炊諠ｳ縲・    // 莉雁屓縺ｯ縺ｾ縺壼虚縺上％縺ｨ繧呈怙蜆ｪ蜈医→縺励∝・縺ｮ Mesh.cpp 縺ｨ蜷檎ｭ峨・蜃ｦ逅・° UploadHeap 縺ｧ菴懈・縺吶ｋ縲・    
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
