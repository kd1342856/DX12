#include "../../../Pch.h"
#include "ShadowMapArray.h"

bool ShadowMapArray::Create(GraphicsDevice* pGraphicsDevice, UINT resolution, UINT layerCount, bool isCube)
{
    m_device = pGraphicsDevice;
    m_resolution = resolution;
    m_layerCount = layerCount;
    m_isCube = isCube;
    const UINT faceCount = layerCount * (isCube ? 6 : 1);

    D3D12_HEAP_PROPERTIES heapProp = {};
    heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;

    // SRV(R32_FLOAT)とDSV(D32_FLOAT)の両方で使うのでTYPELESSで作る
    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resDesc.Width = resolution;
    resDesc.Height = resolution;
    resDesc.DepthOrArraySize = static_cast<UINT16>(faceCount);
    resDesc.MipLevels = 1;
    resDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    resDesc.SampleDesc.Count = 1;
    resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_D32_FLOAT;
    clearValue.DepthStencil.Depth = 1.0f;

    HRESULT hr = m_device->GetDevice()->CreateCommittedResource(
        &heapProp, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clearValue, IID_PPV_ARGS(&m_resource));
    if (FAILED(hr))
    {
        assert(0 && "影の深度テクスチャ配列の生成に失敗しました");
        return false;
    }

    // 1面ずつ描き込むためのDSV
    DescriptorAllocator* pDsvAllocator = m_device->GetDescriptorHeapManager()->GetDSVAllocator();
    m_dsvNumbers.resize(faceCount);
    for (UINT faceIndex = 0; faceIndex < faceCount; ++faceIndex)
    {
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
        dsvDesc.Texture2DArray.FirstArraySlice = faceIndex;
        dsvDesc.Texture2DArray.ArraySize = 1;
        m_dsvNumbers[faceIndex] = pDsvAllocator->Allocate();
        m_device->GetDevice()->CreateDepthStencilView(m_resource.Get(), &dsvDesc, pDsvAllocator->GetCPUHandle(m_dsvNumbers[faceIndex]));
    }

    // シェーダーからは配列全体を1つのSRVで読む
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    if (isCube)
    {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
        srvDesc.TextureCubeArray.MipLevels = 1;
        srvDesc.TextureCubeArray.First2DArrayFace = 0;
        srvDesc.TextureCubeArray.NumCubes = layerCount;
    }
    else
    {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
        srvDesc.Texture2DArray.MipLevels = 1;
        srvDesc.Texture2DArray.FirstArraySlice = 0;
        srvDesc.Texture2DArray.ArraySize = faceCount;
    }
    DescriptorAllocator* pSrvAllocator = m_device->GetDescriptorHeapManager()->GetCBVSRVUAVAllocator();
    m_srvNumber = static_cast<int>(pSrvAllocator->Allocate());
    m_device->GetDevice()->CreateShaderResourceView(m_resource.Get(), &srvDesc, pSrvAllocator->GetCPUHandle(m_srvNumber));

    // 一度も描いていない面は読まない(LocalShadowManagerは描いた面だけをシェーダーに渡す)ので、ここではクリアしない
    m_device->GetResourceStateTracker()->AddResourceState(m_resource.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
    return true;
}

void ShadowMapArray::ClearFace(UINT faceIndex)
{
    m_device->GetCmdList()->ClearDepthStencilView(
        m_device->GetDescriptorHeapManager()->GetDSVAllocator()->GetCPUHandle(m_dsvNumbers[faceIndex]),
        D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
}
