#include "../../Pch.h"
#include "ResourceUploader.h"
#include "GraphicsDevice.h"
#include "../GPUResource/Buffer/VertexBuffer.h"
#include "../GPUResource/Buffer/IndexBuffer.h"
#include "../../Framework/DirectX/Utility/Thread.h"
#include "../GPUResource/Texture/Texture.h"

void ResourceUploader::Init(GraphicsDevice* pDevice)
{
    m_pDevice = pDevice;

    // Create a dedicated Graphics CommandList for issuing TRANSITION barriers after copy.
    // This is needed because Copy Queue does NOT support TRANSITION barriers.
    InitBarrierCommandList();
}

bool ResourceUploader::InitBarrierCommandList()
{
    // DIRECT type allocator + command list, used only for post-copy barrier flush
    HRESULT hr = m_pDevice->GetDevice()->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&m_pBarrierAllocator));

    if (FAILED(hr))
    {
        OutputDebugStringA("ResourceUploader: Failed to create barrier CommandAllocator\n");
        return false;
    }

    hr = m_pDevice->GetDevice()->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_pBarrierAllocator.Get(),
        nullptr,
        IID_PPV_ARGS(&m_pBarrierCmdList));

    if (FAILED(hr))
    {
        OutputDebugStringA("ResourceUploader: Failed to create barrier CommandList\n");
        return false;
    }

    // Start in closed state
    m_pBarrierCmdList->Close();
    return true;
}

void ResourceUploader::ReleaseBarrierCommandList()
{
    m_pBarrierCmdList.Reset();
    m_pBarrierAllocator.Reset();
}

void ResourceUploader::Release()
{
    m_uploadBuffers.clear();
    m_uploadContext.reset();
    ReleaseBarrierCommandList();
}

void ResourceUploader::Begin()
{
    // Must only be called from the main thread
    assert(Thread::IsMainThread() && "ResourceUploader::Begin must be called on the main thread!");

    m_mutex.lock();
    // Acquire a COPY-type upload context and reset it
    m_uploadContext = m_pDevice->GetContextManager()->AcquireUploadContext();
    m_uploadBuffers.clear();
    // Clear any pending barriers from previous upload
    m_uploadContext.value()->ClearPendingBarriers();
}

void ResourceUploader::End()
{
    auto queueManager = m_pDevice->GetQueueManager();

    // Step 1: Execute copy commands on Copy Queue and signal
    m_uploadContext.value()->Execute(queueManager);

    // Step 2: GPU-side: make Graphics Queue wait for Copy Queue to finish
    m_uploadContext.value()->WaitGraphics(queueManager);

    // Step 3: Issue pending TRANSITION barriers on the Graphics Queue.
    // Copy Queue does NOT allow TRANSITION barriers, so we batch them here.
    const auto& pendingBarriers = m_uploadContext.value()->GetPendingBarriers();
    if (!pendingBarriers.empty() && m_pBarrierCmdList)
    {
        // Reset the barrier-only allocator and command list
        m_pBarrierAllocator->Reset();
        m_pBarrierCmdList->Reset(m_pBarrierAllocator.Get(), nullptr);

        // Record all pending barriers
        m_pBarrierCmdList->ResourceBarrier(
            static_cast<UINT>(pendingBarriers.size()),
            pendingBarriers.data());

        // Close -> submit to Graphics Queue -> signal -> CPU wait
        m_pBarrierCmdList->Close();
        queueManager->GetGraphicsQueue()->Execute(m_pBarrierCmdList.Get());
        uint64_t fenceVal = queueManager->SignalGraphics();
        queueManager->WaitGraphics(fenceVal);
    }

    // Release the upload context back to the pool and free staging buffers
    m_uploadContext.reset();
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

    HRESULT hr = m_pDevice->GetDevice()->CreateCommittedResource(
        &prop,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&uploadBuffer));

    if (FAILED(hr))
    {
        OutputDebugStringA("CreateUploadBuffer failed\n");
        return nullptr;
    }

    m_uploadBuffers.push_back(uploadBuffer);
    return uploadBuffer;
}

void ResourceUploader::UploadVertexBuffer(VertexBuffer* pBuffer, const void* pData, UINT dataSize)
{
    OutputDebugStringA("UploadVertexBuffer\n");

    auto uploadBuffer = CreateUploadBuffer(dataSize);
    if (!uploadBuffer)
    {
        OutputDebugStringA("UploadVertexBuffer: CreateUploadBuffer failed, skipping.\n");
        return;
    }

    // Write CPU data into the upload (staging) buffer
    void* pMappedData = nullptr;
    uploadBuffer->Map(0, nullptr, &pMappedData);
    memcpy(pMappedData, pData, dataSize);
    uploadBuffer->Unmap(0, nullptr);

    // Copy staging buffer -> GPU DEFAULT buffer on Copy Queue
    m_uploadContext.value()->GetCmdList()->CopyBufferRegion(
        pBuffer->GetResource(), 0, uploadBuffer.Get(), 0, dataSize);

    // Queue up the TRANSITION barrier (cannot issue on Copy Queue)
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = pBuffer->GetResource();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_uploadContext.value()->AddPendingBarrier(barrier);
}

void ResourceUploader::UploadIndexBuffer(IndexBuffer* pBuffer, const void* pData, UINT dataSize)
{
    OutputDebugStringA("UploadIndexBuffer\n");

    auto uploadBuffer = CreateUploadBuffer(dataSize);
    if (!uploadBuffer)
    {
        OutputDebugStringA("UploadIndexBuffer: CreateUploadBuffer failed, skipping.\n");
        return;
    }

    // Write CPU data into the upload (staging) buffer
    void* pMappedData = nullptr;
    uploadBuffer->Map(0, nullptr, &pMappedData);
    memcpy(pMappedData, pData, dataSize);
    uploadBuffer->Unmap(0, nullptr);

    // Copy staging buffer -> GPU DEFAULT buffer on Copy Queue
    m_uploadContext.value()->GetCmdList()->CopyBufferRegion(
        pBuffer->GetResource(), 0, uploadBuffer.Get(), 0, dataSize);

    // Queue up the TRANSITION barrier (cannot issue on Copy Queue)
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = pBuffer->GetResource();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_INDEX_BUFFER;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_uploadContext.value()->AddPendingBarrier(barrier);
}

void ResourceUploader::UploadTexture(Texture* pTexture, const void* pData, UINT width, UINT height, UINT rowPitch, UINT slicePitch, DXGI_FORMAT format)
{
    // Align row pitch to D3D12_TEXTURE_DATA_PITCH_ALIGNMENT (256 bytes)
    UINT alignedRowPitch = (rowPitch + 255) & ~255;
    UINT64 uploadBufferSize = (UINT64)alignedRowPitch * height;

    auto uploadBuffer = CreateUploadBuffer(uploadBufferSize);
    if (!uploadBuffer)
    {
        OutputDebugStringA("UploadTexture: CreateUploadBuffer failed, skipping.\n");
        return;
    }

    // Write CPU texture data row-by-row with pitch alignment
    void* pMappedData = nullptr;
    uploadBuffer->Map(0, nullptr, &pMappedData);
    for (UINT y = 0; y < height; ++y)
    {
        memcpy(
            (uint8_t*)pMappedData + y * alignedRowPitch,
            (uint8_t*)pData + y * rowPitch,
            rowPitch);
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

    // Copy staging buffer -> GPU texture on Copy Queue
    m_uploadContext.value()->GetCmdList()->CopyTextureRegion(
        &dstLocation, 0, 0, 0, &srcLocation, nullptr);

    // Queue up the TRANSITION barrier (cannot issue on Copy Queue)
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = pTexture->GetResource();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_uploadContext.value()->AddPendingBarrier(barrier);
}
