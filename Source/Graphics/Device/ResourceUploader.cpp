#include "../../Pch.h"
#include "ResourceUploader.h"
#include "GraphicsDevice.h"
#include "../GPUResource/Buffer/VertexBuffer.h"
#include "../GPUResource/Buffer/IndexBuffer.h"
#include "../GPUResource/Texture/Texture.h"

void ResourceUploader::Init(GraphicsDevice* pDevice)
{
    m_pDevice = pDevice;

    
    
}

void ResourceUploader::Release()
{
    m_uploadBuffers.clear();
    m_uploadContext.reset();
    
}

void ResourceUploader::Begin()
{
    m_mutex.lock();
    m_uploadContext = m_pDevice->GetContextManager()->AcquireUploadContext();
    m_uploadBuffers.clear();
}

void ResourceUploader::End()
{
    auto queueManager = m_pDevice->GetQueueManager();
    m_uploadContext.value()->Execute(queueManager);
    m_uploadContext.value()->WaitGraphics(queueManager);
    m_uploadContext.reset();

    // Fence“¯ŠúŠ®—¹Œã‚ÉUploadBuffer‚ğˆêÄ‚É‰ğ•ú
    m_uploadBuffers.clear();

    m_mutex.unlock();
}

Microsoft::WRL::ComPtr<ID3D12Resource> ResourceUploader::CreateUploadBuffer(UINT64 size)
{
    Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer;

    D3D12_HEAP_PROPERTIES prop = {};
    prop.Type = D3D12_HEAP_TYPE_UPLOAD;
    prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = size;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    m_pDevice->GetDevice()->CreateCommittedResource(
        &prop,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&uploadBuffer));

    m_uploadBuffers.push_back(uploadBuffer);
    return uploadBuffer;
}

void ResourceUploader::UploadVertexBuffer(VertexBuffer* pBuffer, const void* pData, UINT dataSize)
{
    OutputDebugStringA("UploadVertexBuffer\n");
    auto uploadBuffer = CreateUploadBuffer(dataSize);

    void* pMappedData = nullptr;
    uploadBuffer->Map(0, nullptr, &pMappedData);
    memcpy(pMappedData, pData, dataSize);
    uploadBuffer->Unmap(0, nullptr);

    m_uploadContext.value()->GetCmdList()->CopyBufferRegion(pBuffer->GetResource(), 0, uploadBuffer.Get(), 0, dataSize);

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = pBuffer->GetResource();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_uploadContext.value()->GetCmdList()->ResourceBarrier(1, &barrier);
}

void ResourceUploader::UploadIndexBuffer(IndexBuffer* pBuffer, const void* pData, UINT dataSize)
{
    OutputDebugStringA("UploadIndexBuffer\n");
    auto uploadBuffer = CreateUploadBuffer(dataSize);

    void* pMappedData = nullptr;
    uploadBuffer->Map(0, nullptr, &pMappedData);
    memcpy(pMappedData, pData, dataSize);
    uploadBuffer->Unmap(0, nullptr);

    m_uploadContext.value()->GetCmdList()->CopyBufferRegion(pBuffer->GetResource(), 0, uploadBuffer.Get(), 0, dataSize);

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = pBuffer->GetResource();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_INDEX_BUFFER;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_uploadContext.value()->GetCmdList()->ResourceBarrier(1, &barrier);
}

void ResourceUploader::UploadTexture(Texture* pTexture, const void* pData, UINT width, UINT height, UINT rowPitch, UINT slicePitch, DXGI_FORMAT format)
{
    // D3D12_TEXTURE_DATA_PITCH_ALIGNMENT (256)
    UINT alignedRowPitch = (rowPitch + 255) & ~255;
    UINT64 uploadBufferSize = (UINT64)alignedRowPitch * height;

    auto uploadBuffer = CreateUploadBuffer(uploadBufferSize);

    void* pMappedData = nullptr;
    uploadBuffer->Map(0, nullptr, &pMappedData);
    for (UINT y = 0; y < height; ++y)
    {
        memcpy((uint8_t*)pMappedData + y * alignedRowPitch, (uint8_t*)pData + y * rowPitch, rowPitch);
    }
    uploadBuffer->Unmap(0, nullptr);

    D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
    srcLocation.pResource = uploadBuffer.Get();
    srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    srcLocation.PlacedFootprint.Offset = 0;
    srcLocation.PlacedFootprint.Footprint.Format = format;
    srcLocation.PlacedFootprint.Footprint.Width = width;
    srcLocation.PlacedFootprint.Footprint.Height = height;
    srcLocation.PlacedFootprint.Footprint.Depth = 1;
    srcLocation.PlacedFootprint.Footprint.RowPitch = alignedRowPitch;

    D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
    dstLocation.pResource = pTexture->GetResource();
    dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLocation.SubresourceIndex = 0;

    m_uploadContext.value()->GetCmdList()->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = pTexture->GetResource();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_uploadContext.value()->GetCmdList()->ResourceBarrier(1, &barrier);
}






