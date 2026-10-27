#include "../../Pch.h"
#include "PixelProbe.h"
#include "../GPUResource/RenderTarget/RenderTarget.h"
#include <DirectXPackedVector.h>

namespace
{
    // 1画素ぶんでも、テクスチャから写すときの行の幅は256バイトにそろえる
    constexpr UINT64 kReadbackSize = D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
}

void PixelProbe::Request(float u, float v)
{
    m_isRequested = true;
    m_requestU = u;
    m_requestV = v;
}

bool PixelProbe::EnsureBuffers()
{
    if (m_cpReadbackBuffers[0])
    {
        return true;
    }
    auto& device = GraphicsDevice::Instance();
    D3D12_HEAP_PROPERTIES readbackHeap = {};
    readbackHeap.Type = D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = kReadbackSize;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    for (auto& cpBuffer : m_cpReadbackBuffers)
    {
        if (FAILED(device.GetDevice()->CreateCommittedResource(&readbackHeap, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&cpBuffer))))
        {
            Logger::Instance().AddLog(Logger::LogLevel::Error, u8"[PixelProbe] 読み戻しのバッファを作れませんでした");
            for (auto& cpCreated : m_cpReadbackBuffers)
            {
                cpCreated.Reset();
            }
            return false;
        }
        cpBuffer->SetName(L"PixelProbeReadback");
    }
    return true;
}

void PixelProbe::Dispatch(RenderTarget* pSceneRT, float exposure)
{
    if (!m_isRequested || !pSceneRT || !pSceneRT->GetResource() || !EnsureBuffers())
    {
        m_isRequested = false;
        return;
    }
    m_isRequested = false;
    // 一番古いスロットは、GPUが書き終えている(この後で上書きするので先に読む)
    if (m_dispatchCount >= kSlotCount)
    {
        ReadOldest();
    }

    // シーンのRTは画面と大きさが違うことがある(描画の解像度の倍率)ので、割合から画素の位置にする
    const int x = std::clamp(static_cast<int>(m_requestU * pSceneRT->GetWidth()), 0, pSceneRT->GetWidth() - 1);
    const int y = std::clamp(static_cast<int>(m_requestV * pSceneRT->GetHeight()), 0, pSceneRT->GetHeight() - 1);
    const int slot = static_cast<int>(m_dispatchCount % kSlotCount);
    ID3D12Resource* pResource = pSceneRT->GetResource();
    const D3D12_RESOURCE_DESC resourceDesc = pResource->GetDesc();

    auto& device = GraphicsDevice::Instance();
    auto* pContext = device.GetContextManager()->GetGraphicsContext();
    pContext->TransitionResource(pResource, D3D12_RESOURCE_STATE_COPY_SOURCE);
    pContext->FlushResourceBarriers();

    D3D12_TEXTURE_COPY_LOCATION destination = {};
    destination.pResource = m_cpReadbackBuffers[slot].Get();
    destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    destination.PlacedFootprint.Offset = 0;
    destination.PlacedFootprint.Footprint.Format = resourceDesc.Format;
    destination.PlacedFootprint.Footprint.Width = 1;
    destination.PlacedFootprint.Footprint.Height = 1;
    destination.PlacedFootprint.Footprint.Depth = 1;
    destination.PlacedFootprint.Footprint.RowPitch = D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;

    D3D12_TEXTURE_COPY_LOCATION source = {};
    source.pResource = pResource;
    source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    source.SubresourceIndex = 0;

    const D3D12_BOX box = { static_cast<UINT>(x), static_cast<UINT>(y), 0, static_cast<UINT>(x + 1), static_cast<UINT>(y + 1), 1 };
    device.GetCmdList()->CopyTextureRegion(&destination, 0, 0, 0, &source, &box);

    Result& request = m_slotRequests[slot];
    request.x = x;
    request.y = y;
    request.exposure = exposure;
    m_slotFormats[slot] = resourceDesc.Format;
    ++m_dispatchCount;
}

void PixelProbe::ReadOldest()
{
    const int slot = static_cast<int>(m_dispatchCount % kSlotCount);
    void* pMapped = nullptr;
    const D3D12_RANGE readRange = { 0, static_cast<SIZE_T>(kReadbackSize) };
    if (FAILED(m_cpReadbackBuffers[slot]->Map(0, &readRange, &pMapped)))
    {
        return;
    }
    Result result = m_slotRequests[slot];
    result.isValid = true;
    switch (m_slotFormats[slot])
    {
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
        {
            const auto* pHalf = static_cast<const DirectX::PackedVector::HALF*>(pMapped);
            result.red = DirectX::PackedVector::XMConvertHalfToFloat(pHalf[0]);
            result.green = DirectX::PackedVector::XMConvertHalfToFloat(pHalf[1]);
            result.blue = DirectX::PackedVector::XMConvertHalfToFloat(pHalf[2]);
            break;
        }
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
        {
            const auto* pFloat = static_cast<const float*>(pMapped);
            result.red = pFloat[0];
            result.green = pFloat[1];
            result.blue = pFloat[2];
            break;
        }
        case DXGI_FORMAT_R11G11B10_FLOAT:
        {
            DirectX::PackedVector::XMFLOAT3PK packed;
            std::memcpy(&packed, pMapped, sizeof(packed));
            const DirectX::XMVECTOR unpacked = DirectX::PackedVector::XMLoadFloat3PK(&packed);
            result.red = DirectX::XMVectorGetX(unpacked);
            result.green = DirectX::XMVectorGetY(unpacked);
            result.blue = DirectX::XMVectorGetZ(unpacked);
            break;
        }
        default:
            // 知らない形式は読まない
            result.isValid = false;
            break;
    }
    const D3D12_RANGE writtenRange = { 0, 0 };
    m_cpReadbackBuffers[slot]->Unmap(0, &writtenRange);
    if (result.isValid)
    {
        m_result = result;
    }
}
