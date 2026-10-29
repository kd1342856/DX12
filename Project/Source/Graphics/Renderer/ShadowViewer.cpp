#include "../../Pch.h"
#include "ShadowViewer.h"
#include "../GPUResource/ShadowMap/ShadowMapArray.h"
#include "../../Framework/DirectX/Utility/Profiler.h"

namespace
{
    // ShadowView_CS.hlslのg_constants(b0)と同じ並び
    struct ShadowViewConstants
    {
        uint32_t sourceIndex;
        uint32_t destinationIndex;
        uint32_t destinationWidth;
        uint32_t destinationHeight;
        uint32_t mode;
        uint32_t layer;
        float nearZ;
        float farZ;
    };

    constexpr uint32_t kModeDirectional = 0;
    constexpr uint32_t kModeSpot = 1;
    constexpr uint32_t kModeCube = 2;
    constexpr UINT kThreadGroupSize = 8;
    constexpr D3D12_RESOURCE_STATES kShaderReadState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
}

bool ShadowViewer::CreateThumbnail(Thumbnail& thumbnail, UINT width, UINT height, const wchar_t* pName)
{
    GPUTextureDesc desc;
    desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.allowUAV = true;
    desc.width = width;
    desc.height = height;
    desc.pName = pName;
    if (!thumbnail.texture.Create(desc))
    {
        return false;
    }
    // ImGuiのヒープの番号は返せないので、作るのは一度だけにする
    thumbnail.imGuiSrvIndex = GraphicsDevice::Instance().AllocateImGuiSRV(thumbnail.texture.GetResource());
    return true;
}

bool ShadowViewer::EnsureResources()
{
    if (m_isCreated)
    {
        return true;
    }
    if (m_hasFailed || !ComputeShader::IsSupported())
    {
        return false;
    }
    // 失敗しても、シェーダーを直せばホットリロードで使えるようになるので、作り直しはしない
    m_shader.Create(L"Asset/Shader/Debug/ShadowView_CS.hlsl");

    bool isCreated = CreateThumbnail(m_directional, kDirectionalSize, kDirectionalSize, L"ShadowViewDirectional");
    for (Thumbnail& spot : m_spots)
    {
        isCreated = isCreated && CreateThumbnail(spot, kSpotSize, kSpotSize, L"ShadowViewSpot");
    }
    for (Thumbnail& cube : m_cubes)
    {
        isCreated = isCreated && CreateThumbnail(cube, kCubeFaceSize * 3, kCubeFaceSize * 2, L"ShadowViewCube");
    }
    if (!isCreated)
    {
        m_hasFailed = true;
        return false;
    }
    m_isCreated = true;
    return true;
}

void ShadowViewer::Dispatch()
{
    if (!m_isRequested)
    {
        return;
    }
    m_isRequested = false;
    if (!EnsureResources() || !m_shader.IsValid())
    {
        return;
    }

    auto& device = GraphicsDevice::Instance();
    auto* pContext = device.GetContextManager()->GetGraphicsContext();
    PROFILE_GPU_SCOPE(device.GetCmdList(), "ShadowView");

    // 影の深度をコンピュートから読める状態にする(ボリュメトリックフォグと同じ。次に影を描くときに書き込みへ戻る)
    pContext->TransitionResource(device.GetShadowMap()->GetResource(), kShaderReadState);
    pContext->TransitionResource(device.GetSpotShadowMaps()->GetResource(), kShaderReadState);
    pContext->TransitionResource(device.GetCubeShadowMaps()->GetResource(), kShaderReadState);
    pContext->TransitionResource(m_directional.texture.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    for (Thumbnail& spot : m_spots)
    {
        pContext->TransitionResource(spot.texture.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    }
    for (Thumbnail& cube : m_cubes)
    {
        pContext->TransitionResource(cube.texture.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    }
    pContext->FlushResourceBarriers();

    m_shader.Begin();
    Convert(m_directional, device.GetShadowMap()->GetSRVNumber(), kModeDirectional, 0, 0.0f, 1.0f);

    // 枠を割り当てたライトの届く範囲で、深度を距離に戻す(影の投影と同じ手前と奥の面)
    auto& shadowManager = LocalShadowManager::Instance();
    const auto& spotSlots = shadowManager.GetSpotSlots();
    for (int slotIndex = 0; slotIndex < kSpotCount && slotIndex < static_cast<int>(spotSlots.size()); ++slotIndex)
    {
        const float farZ = (std::max)(spotSlots[slotIndex].range, LocalShadowManager::kShadowNear * 2.0f);
        Convert(m_spots[slotIndex], device.GetSpotShadowMaps()->GetSRVNumber(), kModeSpot, static_cast<uint32_t>(slotIndex), LocalShadowManager::kShadowNear, farZ);
    }
    const auto& cubeSlots = shadowManager.GetCubeSlots();
    for (int slotIndex = 0; slotIndex < kCubeCount && slotIndex < static_cast<int>(cubeSlots.size()); ++slotIndex)
    {
        const float farZ = LocalShadowManager::GetCubeFar(cubeSlots[slotIndex].range);
        Convert(m_cubes[slotIndex], device.GetCubeShadowMaps()->GetSRVNumber(), kModeCube, static_cast<uint32_t>(slotIndex), LocalShadowManager::kShadowNear, farZ);
    }

    // ImGuiが読めるように戻す
    pContext->TransitionResource(m_directional.texture.GetResource(), GPUTexture::kInitialState);
    for (Thumbnail& spot : m_spots)
    {
        pContext->TransitionResource(spot.texture.GetResource(), GPUTexture::kInitialState);
    }
    for (Thumbnail& cube : m_cubes)
    {
        pContext->TransitionResource(cube.texture.GetResource(), GPUTexture::kInitialState);
    }
    pContext->FlushResourceBarriers();
    m_hasContent = true;
}

void ShadowViewer::Convert(Thumbnail& thumbnail, int sourceIndex, uint32_t mode, uint32_t layer, float nearZ, float farZ)
{
    if (sourceIndex < 0)
    {
        return;
    }
    ShadowViewConstants constants = {};
    constants.sourceIndex = static_cast<uint32_t>(sourceIndex);
    constants.destinationIndex = static_cast<uint32_t>(thumbnail.texture.GetUAVIndex());
    constants.destinationWidth = thumbnail.texture.GetWidth();
    constants.destinationHeight = thumbnail.texture.GetHeight();
    constants.mode = mode;
    constants.layer = layer;
    constants.nearZ = nearZ;
    constants.farZ = farZ;
    m_shader.SetConstants(constants);
    m_shader.Dispatch(ComputeShader::DivideRoundUp(constants.destinationWidth, kThreadGroupSize), ComputeShader::DivideRoundUp(constants.destinationHeight, kThreadGroupSize));
}
