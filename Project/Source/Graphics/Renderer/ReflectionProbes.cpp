#include "../../Pch.h"
#include "ReflectionProbes.h"
#include "../GPUResource/RenderTarget/RenderTarget.h"
#include "../../Framework/DirectX/Utility/Profiler.h"

namespace
{
    // ReflectionProbeFilter_CS.hlslのg_constants(b0)と同じ並び(Downsampleは先頭の3つだけを使う)
    struct FilterConstants
    {
        uint32_t sourceIndex;
        uint32_t outputIndex;
        uint32_t outputSize;
        uint32_t firstSlice;
        float roughness;
        uint32_t sampleCount;
        float sourceSize;
    };

    constexpr UINT kThreadGroupSize = 8;
    constexpr UINT kFilterSampleCount = 64;
    constexpr DXGI_FORMAT kFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
    constexpr float kCaptureNear = 0.05f;
    constexpr float kCaptureFar = 60.0f;
    // 箱の大きさや位置がこれ以上変わったら撮り直す
    constexpr float kVolumeEpsilon = 0.01f;

    // D3Dのキューブマップの面の順(+X,-X,+Y,-Y,+Z,-Z)と、それぞれの上方向(LocalShadowManagerと同じ)
    const Math::Vector3 kFaceDirections[6] = {
        { 1.0f, 0.0f, 0.0f }, { -1.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f }, { 0.0f, -1.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f },
    };
    const Math::Vector3 kFaceUps[6] = {
        { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f },
        { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f, 1.0f },
        { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f },
    };

    bool IsSameVolume(const ReflectionProbes::Volume& a, const ReflectionProbes::Volume& b)
    {
        return Math::Vector3::DistanceSquared(a.min, b.min) < kVolumeEpsilon * kVolumeEpsilon &&
            Math::Vector3::DistanceSquared(a.max, b.max) < kVolumeEpsilon * kVolumeEpsilon;
    }
}

void ReflectionProbes::Initialize()
{
    if (!ComputeShader::IsSupported())
    {
        return;
    }
    GPUTextureDesc desc;
    desc.dimension = TextureDimension::TextureCube;
    desc.width = kCaptureSize;
    desc.height = kCaptureSize;
    desc.depthOrArraySize = kMaxProbes;
    desc.mipLevels = kMipCount;
    desc.format = kFormat;
    desc.allowUAV = true;
    desc.pName = L"ReflectionProbes";
    if (!m_probeTexture.Create(desc))
    {
        return;
    }

    desc.depthOrArraySize = 1;
    desc.createMipSRVs = true;
    desc.pName = L"ReflectionProbeStaging";
    if (!m_stagingTexture.Create(desc))
    {
        return;
    }

    m_upCaptureTarget = std::make_unique<RenderTarget>();
    if (!m_upCaptureTarget->Create(kCaptureSize, kCaptureSize, kFormat))
    {
        return;
    }

    if (!m_downsampleShader.Create(L"Asset/Shader/Lighting/ReflectionProbeFilter_CS.hlsl", L"Downsample") ||
        !m_filterShader.Create(L"Asset/Shader/Lighting/ReflectionProbeFilter_CS.hlsl", L"main"))
    {
        return;
    }
    m_isInitialized = true;
}

Math::Vector3 ReflectionProbes::GetCapturePosition(const Volume& volume)
{
    return (volume.min + volume.max) * 0.5f;
}

void ReflectionProbes::SetVolumes(const std::vector<Volume>& volumes)
{
    const size_t count = (std::min)(volumes.size(), static_cast<size_t>(kMaxProbes));
    if (m_probes.size() != count)
    {
        m_probes.assign(count, Probe());
        m_captureProbe = 0;
        m_captureFace = 0;
    }
    for (size_t i = 0; i < count; ++i)
    {
        if (!IsSameVolume(m_probes[i].volume, volumes[i]))
        {
            // 撮り直すまでは古い絵を使い続けると位置がずれるので、使えないものとして扱う
            m_probes[i].volume = volumes[i];
            m_probes[i].captureExposure = 0.0f;
        }
    }
}

bool ReflectionProbes::BeginFaceCapture(Math::Matrix& outView, Math::Matrix& outProjection, RenderTarget*& outTarget)
{
    if (!m_isInitialized || m_probes.empty())
    {
        return false;
    }
    m_captureProbe %= static_cast<int>(m_probes.size());
    const Math::Vector3 position = GetCapturePosition(m_probes[m_captureProbe].volume);
    outView = DirectX::XMMatrixLookToLH(position, kFaceDirections[m_captureFace], kFaceUps[m_captureFace]);
    outProjection = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV2, 1.0f, kCaptureNear, kCaptureFar);
    outTarget = m_upCaptureTarget.get();

    auto& device = GraphicsDevice::Instance();
    device.SetRenderTarget(outTarget);
    outTarget->Clear(0.0f, 0.0f, 0.0f, 1.0f);
    return true;
}

void ReflectionProbes::EndFaceCapture(float exposure)
{
    if (!m_isInitialized || m_probes.empty())
    {
        return;
    }
    auto& device = GraphicsDevice::Instance();
    auto* pCmdList = device.GetCmdList();
    auto* pContext = device.GetContextManager()->GetGraphicsContext();

    // 撮った面を、ステージングのキューブのミップ0のその面へ写す
    pContext->TransitionResource(m_upCaptureTarget->GetResource(), D3D12_RESOURCE_STATE_COPY_SOURCE);
    pContext->TransitionResource(m_stagingTexture.GetResource(), D3D12_RESOURCE_STATE_COPY_DEST);
    pContext->FlushResourceBarriers();
    D3D12_TEXTURE_COPY_LOCATION destination = {};
    destination.pResource = m_stagingTexture.GetResource();
    destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    destination.SubresourceIndex = m_stagingTexture.GetSubresourceIndex(0, static_cast<UINT>(m_captureFace));
    D3D12_TEXTURE_COPY_LOCATION source = {};
    source.pResource = m_upCaptureTarget->GetResource();
    source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    source.SubresourceIndex = 0;
    pCmdList->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
    pContext->TransitionResource(m_upCaptureTarget->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    pContext->TransitionResource(m_stagingTexture.GetResource(), GPUTexture::kInitialState);
    pContext->FlushResourceBarriers();

    ++m_captureFace;
    if (m_captureFace < 6)
    {
        return;
    }
    m_captureFace = 0;
    FilterProbe(m_captureProbe);
    m_probes[m_captureProbe].captureExposure = exposure;
    m_captureProbe = (m_captureProbe + 1) % static_cast<int>(m_probes.size());
}

void ReflectionProbes::FilterProbe(int probeIndex)
{
    auto& device = GraphicsDevice::Instance();
    auto* pCmdList = device.GetCmdList();
    auto* pContext = device.GetContextManager()->GetGraphicsContext();
    PROFILE_GPU_SCOPE(pCmdList, "ReflectionProbeFilter");

    // 1. ステージングのミップを作る。ミップごとに、そのミップの6面だけをUAVにして、1つ上のミップから縮める。
    //    状態の管理(ResourceStateTracker)はリソース全体の単位なので、ミップごとの遷移はここで直接行い、
    //    終わったら全体の状態(kInitialState)に戻しておく
    ID3D12Resource* pStaging = m_stagingTexture.GetResource();
    for (UINT mip = 1; mip < kMipCount; ++mip)
    {
        D3D12_RESOURCE_BARRIER barriers[6] = {};
        for (UINT face = 0; face < 6; ++face)
        {
            barriers[face].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barriers[face].Transition.pResource = pStaging;
            barriers[face].Transition.Subresource = m_stagingTexture.GetSubresourceIndex(mip, face);
            barriers[face].Transition.StateBefore = GPUTexture::kInitialState;
            barriers[face].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        }
        pCmdList->ResourceBarrier(6, barriers);

        FilterConstants constants = {};
        constants.sourceIndex = static_cast<uint32_t>(m_stagingTexture.GetMipSRVIndex(mip - 1));
        constants.outputIndex = static_cast<uint32_t>(m_stagingTexture.GetUAVIndex(mip));
        constants.outputSize = kCaptureSize >> mip;
        m_downsampleShader.Begin();
        m_downsampleShader.SetConstants(constants);
        m_downsampleShader.Dispatch(ComputeShader::DivideRoundUp(constants.outputSize, kThreadGroupSize), ComputeShader::DivideRoundUp(constants.outputSize, kThreadGroupSize), 6);

        for (D3D12_RESOURCE_BARRIER& barrier : barriers)
        {
            std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
        }
        pCmdList->ResourceBarrier(6, barriers);
    }

    // 2. ラフネスごとにぼかして、プローブのキューブ配列のこのプローブの6面へ書く
    pContext->TransitionResource(m_probeTexture.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    m_filterShader.Begin();
    for (UINT mip = 0; mip < kMipCount; ++mip)
    {
        FilterConstants constants = {};
        constants.sourceIndex = static_cast<uint32_t>(m_stagingTexture.GetSRVIndex());
        constants.outputIndex = static_cast<uint32_t>(m_probeTexture.GetUAVIndex(mip));
        constants.outputSize = kCaptureSize >> mip;
        constants.firstSlice = static_cast<uint32_t>(probeIndex * 6);
        constants.roughness = static_cast<float>(mip) / static_cast<float>(kMipCount - 1);
        constants.sampleCount = kFilterSampleCount;
        constants.sourceSize = static_cast<float>(kCaptureSize);
        m_filterShader.SetConstants(constants);
        m_filterShader.Dispatch(ComputeShader::DivideRoundUp(constants.outputSize, kThreadGroupSize), ComputeShader::DivideRoundUp(constants.outputSize, kThreadGroupSize), 6);
    }
    pContext->TransitionResource(m_probeTexture.GetResource(), GPUTexture::kInitialState);
    pContext->FlushResourceBarriers();
}

void ReflectionProbes::FillShaderData(CBufferData::Light& lightData, float exposure) const
{
    int count = 0;
    if (m_isInitialized)
    {
        for (size_t i = 0; i < m_probes.size(); ++i)
        {
            const Probe& probe = m_probes[i];
            const Math::Vector3 position = GetCapturePosition(probe.volume);
            // まだ撮り終えていないプローブは、露出の比を0にして使わない
            const float exposureRatio = (probe.captureExposure > 0.0f) ? exposure / probe.captureExposure : 0.0f;
            lightData.ProbeBoxMin[i] = { probe.volume.min.x, probe.volume.min.y, probe.volume.min.z, 0.0f };
            lightData.ProbeBoxMax[i] = { probe.volume.max.x, probe.volume.max.y, probe.volume.max.z, 0.0f };
            lightData.ProbeCapturePosition[i] = { position.x, position.y, position.z, exposureRatio };
            ++count;
        }
    }
    lightData.ProbeCount = count;
    lightData.ProbeMipCount = static_cast<float>(kMipCount);
}
