#include "../../../Pch.h"
#include "Texture.h"
#include "../../Device/GraphicsDevice.h"
#include "../../Device/ResourceUploader.h"
#include "../../GPUUploadQueue/GPUUploadQueue.h"
#include "../../../Framework/DirectX/Utility/Thread.h"

bool Texture::LoadCPU(const std::string& filePath, TextureUploadData& outData)
{
    wchar_t wFilePath[128];
    MultiByteToWideChar(CP_ACP, 0, filePath.c_str(), -1, wFilePath, _countof(wFilePath));

    DirectX::TexMetadata metadata = {};
    DirectX::ScratchImage scratchImage = {};

    HRESULT hr = DirectX::LoadFromWICFile(wFilePath, DirectX::WIC_FLAGS_NONE, &metadata, scratchImage);
    if (FAILED(hr))
    {
        OutputDebugStringA(("Texture::LoadCPU failed: " + filePath + "\n").c_str());
        m_state = AssetState::Failed;
        return false;
    }

    const DirectX::Image* pImage = scratchImage.GetImage(0, 0, 0);

    outData.width    = static_cast<UINT>(pImage->width);
    outData.height   = static_cast<UINT>(pImage->height);
    outData.rowPitch = static_cast<UINT>(pImage->rowPitch);
    outData.format   = pImage->format;
    outData.pixels.assign(pImage->pixels, pImage->pixels + pImage->slicePitch);

    m_state = AssetState::WaitingGPU;
    return true;
}

bool Texture::LoadCPUFromMemory(const void* pData, size_t size, TextureUploadData& outData)
{
    DirectX::TexMetadata metadata = {};
    DirectX::ScratchImage scratchImage = {};

    HRESULT hr = DirectX::LoadFromWICMemory((const uint8_t*)pData, size, DirectX::WIC_FLAGS_NONE, &metadata, scratchImage);
    if (FAILED(hr))
    {
        OutputDebugStringA("Texture::LoadCPUFromMemory failed\n");
        m_state = AssetState::Failed;
        return false;
    }

    const DirectX::Image* pImage = scratchImage.GetImage(0, 0, 0);

    outData.width    = static_cast<UINT>(pImage->width);
    outData.height   = static_cast<UINT>(pImage->height);
    outData.rowPitch = static_cast<UINT>(pImage->rowPitch);
    outData.format   = pImage->format;
    outData.pixels.assign(pImage->pixels, pImage->pixels + pImage->slicePitch);

    m_state = AssetState::WaitingGPU;
    return true;
}

bool Texture::CreateGPU(GraphicsDevice* pDevice, const TextureUploadData& data)
{
    assert(Thread::IsMainThread() && "Texture::CreateGPU must be called on the main thread!");

    m_device = pDevice;

    D3D12_HEAP_PROPERTIES heapProp = {};
    heapProp.Type                 = D3D12_HEAP_TYPE_DEFAULT;
    heapProp.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC recDesc = {};
    recDesc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    recDesc.Format           = data.format;
    recDesc.Width            = static_cast<UINT64>(data.width);
    recDesc.Height           = data.height;
    recDesc.DepthOrArraySize = 1;
    recDesc.MipLevels        = 1;
    recDesc.SampleDesc.Count = 1;

    // CopyキューはCOMMON状態のリソースを自動的にCOPY_DESTへプロモートするためCOMMONで作成する
    HRESULT hr = m_device->GetDevice()->CreateCommittedResource(
        &heapProp, D3D12_HEAP_FLAG_NONE, &recDesc,
        D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_resource));

    if (FAILED(hr))
    {
        OutputDebugStringA("Texture::CreateGPU: CreateCommittedResource failed\n");
        m_state = AssetState::Failed;
        return false;
    }

    m_device->GetResourceUploader()->UploadTexture(
        this,
        data.pixels.data(),
        data.width,
        data.height,
        data.rowPitch,
        static_cast<UINT>(data.pixels.size()),
        data.format);

    m_srvNumber = m_device->CreateSRV(m_resource.Get());

    return true;
}

bool Texture::CreateFromMemory(const void* data, int width, int height, DXGI_FORMAT format)
{
    assert(Thread::IsMainThread() && "Texture::CreateFromMemory must be called on the main thread!");

    m_device = &GraphicsDevice::Instance();

    D3D12_HEAP_PROPERTIES heapProp = {};
    heapProp.Type                 = D3D12_HEAP_TYPE_DEFAULT;
    heapProp.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC recDesc = {};
    recDesc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    recDesc.Format           = format;
    recDesc.Width            = static_cast<UINT64>(width);
    recDesc.Height           = static_cast<UINT>(height);
    recDesc.DepthOrArraySize = 1;
    recDesc.MipLevels        = 1;
    recDesc.SampleDesc.Count = 1;

    // CopyキューはCOMMON状態のリソースを自動的にCOPY_DESTへプロモートするためCOMMONで作成する
    HRESULT hr = m_device->GetDevice()->CreateCommittedResource(
        &heapProp, D3D12_HEAP_FLAG_NONE, &recDesc,
        D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_resource));

    if (FAILED(hr))
    {
        OutputDebugStringA("Texture::CreateFromMemory: CreateCommittedResource failed\n");
        m_state = AssetState::Failed;
        return false;
    }

    UINT rowPitch   = width * 4;
    UINT slicePitch = rowPitch * height;

    m_device->GetResourceUploader()->Begin();
    m_device->GetResourceUploader()->UploadTexture(
        this, data, width, height, rowPitch, slicePitch, format);
    m_device->GetResourceUploader()->End();

    m_srvNumber = m_device->CreateSRV(m_resource.Get());
    m_state     = AssetState::Ready;
    return true;
}

void Texture::Set(int index)
{
    m_device->GetCmdList()->SetGraphicsRootDescriptorTable(
        index,
        m_device->GetDescriptorHeapManager()->GetCBVSRVUAVAllocator()->GetGPUHandle(m_srvNumber));
}
