#pragma once
#include "Context/ContextManager.h"
#include <optional>
#include <mutex>
#include <vector>

class GraphicsDevice;
class Texture;
class VertexBuffer;
class IndexBuffer;

// Handles GPU resource uploads using a dedicated Copy Queue.
// TRANSITION barriers are NOT allowed on the Copy Queue, so they are batched
// and flushed on the Graphics Queue after the copy completes.
class ResourceUploader
{
public:
    ResourceUploader() {}
    ~ResourceUploader() {}

    void Init(GraphicsDevice* pDevice);
    void Release();

    void Begin();
    void End();

    void UploadTexture(Texture* pTexture, const void* pData, UINT width, UINT height, UINT rowPitch, UINT slicePitch, DXGI_FORMAT format);
    void UploadVertexBuffer(VertexBuffer* pBuffer, const void* pData, UINT dataSize);
    void UploadIndexBuffer(IndexBuffer* pBuffer, const void* pData, UINT dataSize);

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateUploadBuffer(UINT64 size);

    // Create/release the dedicated Graphics CommandList used for post-copy barrier flush
    bool InitBarrierCommandList();
    void ReleaseBarrierCommandList();

private:
    GraphicsDevice* m_pDevice = nullptr;

    std::optional<ScopedCommandContext<UploadCommandContext>> m_uploadContext;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_uploadBuffers;
    std::mutex m_mutex;

    // Dedicated DIRECT CommandAllocator + CommandList for flushing TRANSITION barriers.
    // These barriers cannot be issued on the Copy Queue, so they are submitted
    // on the Graphics Queue inside End() after copy completes.
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_pBarrierAllocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_pBarrierCmdList;
};
