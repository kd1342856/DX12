#pragma once
#include <d3d12.h>
#include <wrl.h>
class GraphicsDevice;
class FrameConstantBufferAllocator;

class FrameResource
{
public:
    bool Init(GraphicsDevice* pDevice);

    void BeginFrame();

    ID3D12CommandAllocator* GetCommandAllocator() const { return m_pCommandAllocator.Get(); }

    FrameConstantBufferAllocator* GetConstantBufferAllocator() { return m_pConstantBufferAllocator.get(); }

    const FrameConstantBufferAllocator* GetConstantBufferAllocator() const { return m_pConstantBufferAllocator.get();}

    UINT64 GetFenceValue() const { return m_fenceValue; }
    void SetFenceValue(UINT64 value) { m_fenceValue = value; }

private:

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_pCommandAllocator;

    std::unique_ptr<FrameConstantBufferAllocator> m_pConstantBufferAllocator;

    UINT64 m_fenceValue = 0;
};