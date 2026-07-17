#include "../../Pch.h"
#include "FrameResource.h"
#include "Frame/FrameManager.h"
#include "../Graphics.h"

bool FrameResource::Init(GraphicsDevice* pDevice)
{
    if (!pDevice)
        return false;

    auto device = pDevice->GetDevice();

    HRESULT hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_pCommandAllocator));

    if (FAILED(hr))
        return false;

    m_pConstantBufferAllocator = std::make_unique<FrameConstantBufferAllocator>();

    if (!m_pConstantBufferAllocator->Create(pDevice, 10000, FrameManager::kFrameCount))
    {
        return false;
    }

    return true;
}

void FrameResource::BeginFrame()
{
    if (m_pConstantBufferAllocator)
    {
        m_pConstantBufferAllocator->Reset();
    }
}

