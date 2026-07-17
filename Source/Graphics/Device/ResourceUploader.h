#include "Context/ContextManager.h"
#include <optional>
#pragma once
#include <mutex>

class GraphicsDevice;
class Texture;
class VertexBuffer;
class IndexBuffer;

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

private:
    GraphicsDevice* m_pDevice = nullptr;

    
    std::optional<ScopedCommandContext<UploadCommandContext>> m_uploadContext;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_uploadBuffers;
    std::mutex m_mutex;
};


