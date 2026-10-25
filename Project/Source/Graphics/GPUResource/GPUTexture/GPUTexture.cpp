#include "../../../Pch.h"
#include "GPUTexture.h"
#include "../../Device/ResourceStateTracker.h"

bool GPUTexture::Create(const GPUTextureDesc& desc)
{
    m_device = &GraphicsDevice::Instance();
    m_desc = desc;
    m_desc.mipLevels = (std::max)(m_desc.mipLevels, 1u);
    m_desc.depthOrArraySize = (std::max)(m_desc.depthOrArraySize, 1u);
    // RTVは2Dのミップ0だけに作る
    assert((!m_desc.allowRenderTarget || m_desc.dimension == TextureDimension::Texture2D) && "RTVを作れるのは2Dのテクスチャだけです");
    if (!CreateResource())
    {
        return false;
    }

    // ディスクリプタはここで一度だけ確保し、Resize()では同じ番号に書き直す
    auto* pHeapManager = m_device->GetDescriptorHeapManager();
    m_srvIndex = static_cast<int>(pHeapManager->GetCBVSRVUAVAllocator()->Allocate());
    if (m_desc.allowUAV)
    {
        m_uavIndices.resize(m_desc.mipLevels);
        for (int& uavIndex : m_uavIndices)
        {
            uavIndex = static_cast<int>(pHeapManager->GetCBVSRVUAVAllocator()->Allocate());
        }
    }
    if (m_desc.createMipSRVs)
    {
        m_mipSrvIndices.resize(m_desc.mipLevels);
        for (int& srvIndex : m_mipSrvIndices)
        {
            srvIndex = static_cast<int>(pHeapManager->GetCBVSRVUAVAllocator()->Allocate());
        }
    }
    if (m_desc.allowRenderTarget)
    {
        m_rtvIndex = static_cast<int>(pHeapManager->GetRTVAllocator()->Allocate());
    }
    WriteDescriptors();
    return true;
}

bool GPUTexture::Resize(UINT width, UINT height)
{
    if (width == m_desc.width && height == m_desc.height)
    {
        return true;
    }
    // 呼び出し側でGPUの処理が終わるのを待ってから呼ぶので、古いリソースはすぐに解放してよい
    m_device->GetResourceStateTracker()->RemoveResourceState(m_resource.Get());
    m_resource.Reset();
    m_desc.width = width;
    m_desc.height = height;
    if (!CreateResource())
    {
        return false;
    }
    WriteDescriptors();
    return true;
}

int GPUTexture::GetUAVIndex(UINT mip) const
{
    if (mip >= m_uavIndices.size())
    {
        return -1;
    }
    return m_uavIndices[mip];
}

int GPUTexture::GetMipSRVIndex(UINT mip) const
{
    if (mip >= m_mipSrvIndices.size())
    {
        return -1;
    }
    return m_mipSrvIndices[mip];
}

UINT GPUTexture::GetArraySize() const
{
    if (m_desc.dimension == TextureDimension::TextureCube)
    {
        return m_desc.depthOrArraySize * 6;
    }
    return m_desc.depthOrArraySize;
}

bool GPUTexture::CreateResource()
{
    D3D12_HEAP_PROPERTIES heapProperties = {};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = (m_desc.dimension == TextureDimension::Texture3D) ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Width = m_desc.width;
    resourceDesc.Height = m_desc.height;
    resourceDesc.DepthOrArraySize = static_cast<UINT16>(GetArraySize());
    resourceDesc.MipLevels = static_cast<UINT16>(m_desc.mipLevels);
    resourceDesc.Format = m_desc.format;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    if (m_desc.allowUAV)
    {
        resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    D3D12_CLEAR_VALUE clearValue = {};
    const D3D12_CLEAR_VALUE* pClearValue = nullptr;
    if (m_desc.allowRenderTarget)
    {
        resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        clearValue.Format = m_desc.format;
        pClearValue = &clearValue;
    }

    HRESULT hr = m_device->GetDevice()->CreateCommittedResource(
        &heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
        kInitialState, pClearValue, IID_PPV_ARGS(&m_resource));
    if (FAILED(hr))
    {
        Logger::Instance().AddLog(Logger::LogLevel::Error, u8"[GPUTexture] テクスチャを作れませんでした (%u x %u)", m_desc.width, m_desc.height);
        return false;
    }
    if (m_desc.pName)
    {
        m_resource->SetName(m_desc.pName);
    }
    m_device->GetResourceStateTracker()->AddResourceState(m_resource.Get(), kInitialState);
    return true;
}

void GPUTexture::WriteDescriptors()
{
    ID3D12Device* pDevice = m_device->GetDevice();
    auto* pHeapManager = m_device->GetDescriptorHeapManager();

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = m_desc.format;
    switch (m_desc.dimension)
    {
    case TextureDimension::Texture2D:
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = m_desc.mipLevels;
        break;
    case TextureDimension::Texture2DArray:
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
        srvDesc.Texture2DArray.MipLevels = m_desc.mipLevels;
        srvDesc.Texture2DArray.ArraySize = m_desc.depthOrArraySize;
        break;
    case TextureDimension::TextureCube:
        if (m_desc.depthOrArraySize > 1)
        {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
            srvDesc.TextureCubeArray.MipLevels = m_desc.mipLevels;
            srvDesc.TextureCubeArray.NumCubes = m_desc.depthOrArraySize;
        }
        else
        {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
            srvDesc.TextureCube.MipLevels = m_desc.mipLevels;
        }
        break;
    case TextureDimension::Texture3D:
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
        srvDesc.Texture3D.MipLevels = m_desc.mipLevels;
        break;
    }
    pDevice->CreateShaderResourceView(m_resource.Get(), &srvDesc, pHeapManager->GetCBVSRVUAVAllocator()->GetCPUHandle(m_srvIndex));

    // ミップ1枚だけのSRV
    for (UINT mip = 0; mip < static_cast<UINT>(m_mipSrvIndices.size()); ++mip)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC mipSrvDesc = {};
        mipSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        mipSrvDesc.Format = m_desc.format;
        switch (m_desc.dimension)
        {
        case TextureDimension::Texture2D:
            mipSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            mipSrvDesc.Texture2D.MostDetailedMip = mip;
            mipSrvDesc.Texture2D.MipLevels = 1;
            break;
        case TextureDimension::Texture2DArray:
        case TextureDimension::TextureCube:
            mipSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
            mipSrvDesc.Texture2DArray.MostDetailedMip = mip;
            mipSrvDesc.Texture2DArray.MipLevels = 1;
            mipSrvDesc.Texture2DArray.ArraySize = GetArraySize();
            break;
        case TextureDimension::Texture3D:
            mipSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
            mipSrvDesc.Texture3D.MostDetailedMip = mip;
            mipSrvDesc.Texture3D.MipLevels = 1;
            break;
        }
        pDevice->CreateShaderResourceView(m_resource.Get(), &mipSrvDesc, pHeapManager->GetCBVSRVUAVAllocator()->GetCPUHandle(m_mipSrvIndices[mip]));
    }

    // UAVはミップごとに作る。キューブは面を並べた2D配列として書き込む
    for (UINT mip = 0; mip < static_cast<UINT>(m_uavIndices.size()); ++mip)
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = m_desc.format;
        switch (m_desc.dimension)
        {
        case TextureDimension::Texture2D:
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            uavDesc.Texture2D.MipSlice = mip;
            break;
        case TextureDimension::Texture2DArray:
        case TextureDimension::TextureCube:
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
            uavDesc.Texture2DArray.MipSlice = mip;
            uavDesc.Texture2DArray.ArraySize = GetArraySize();
            break;
        case TextureDimension::Texture3D:
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
            uavDesc.Texture3D.MipSlice = mip;
            uavDesc.Texture3D.WSize = (std::max)(m_desc.depthOrArraySize >> mip, 1u);
            break;
        }
        pDevice->CreateUnorderedAccessView(m_resource.Get(), nullptr, &uavDesc, pHeapManager->GetCBVSRVUAVAllocator()->GetCPUHandle(m_uavIndices[mip]));
    }

    if (m_rtvIndex != -1)
    {
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = m_desc.format;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        pDevice->CreateRenderTargetView(m_resource.Get(), &rtvDesc, pHeapManager->GetRTVAllocator()->GetCPUHandle(m_rtvIndex));
    }
}

void GPUTexture::ClearRenderTarget(float r, float g, float b, float a)
{
    if (m_rtvIndex == -1)
    {
        return;
    }
    const float clearColor[] = { r, g, b, a };
    auto rtvHandle = m_device->GetDescriptorHeapManager()->GetRTVAllocator()->GetCPUHandle(m_rtvIndex);
    m_device->GetCmdList()->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
}
