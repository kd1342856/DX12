#include "../../Pch.h"
#include "ProbeViewer.h"
#include "DDGIVolume.h"
#include "../GPUResource/RenderTarget/RenderTarget.h"
#include "../Shader/ShaderManager/ShaderManager.h"
#include "../../Framework/DirectX/Utility/Profiler.h"

namespace
{
    // ProbeView_CS.hlslのg_constants(b0)と同じ並び
    struct ProbeViewConstants
    {
        uint32_t sceneDepthIndex;
        uint32_t probeDepthIndex;
        uint32_t sceneColorIndex;
        uint32_t outputIndex;
        uint32_t irradianceIndex;
        uint32_t width;
        uint32_t height;
        uint32_t probeCount;
        float radius;
        float exposure;
        float maxDistance;
        float projectionScaleY;
        float brightnessScale;
    };

    // ProbeView_CS.hlslのcbFrame(b1)と同じ並び
    struct ProbeViewFrame
    {
        Math::Matrix viewProjection;
        Math::Matrix inverseViewProjection;
        Math::Vector4 cameraPosition;
        Math::Vector4 ddgiOrigin;
        DirectX::XMINT4 ddgiCounts;
    };

    constexpr UINT kThreadGroupSize = 8;
    // プローブの番号は15ビットにまとめる(DDGIVolumeの1軸の上限32の3乗まで)
    constexpr uint32_t kMaxProbeCount = 1u << 15;
    // 距離をまとめるときの上限(m)。これより遠い球は前後が正しくないことがある
    constexpr float kMaxDistance = 64.0f;
    constexpr D3D12_RESOURCE_STATES kShaderReadState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
}

bool ProbeViewer::EnsureResources(UINT width, UINT height, DXGI_FORMAT colorFormat)
{
    if (m_hasFailed || !ComputeShader::IsSupported())
    {
        return false;
    }
    if (!m_isCreated)
    {
        // 失敗しても、シェーダーを直せばホットリロードで使えるようになるので、作り直しはしない
        m_clearShader.Create(L"Asset/Shader/Debug/ProbeView_CS.hlsl", L"ClearMain");
        m_splatShader.Create(L"Asset/Shader/Debug/ProbeView_CS.hlsl", L"SplatMain");
        m_shadeShader.Create(L"Asset/Shader/Debug/ProbeView_CS.hlsl", L"ShadeMain");

        GPUTextureDesc desc;
        desc.width = width;
        desc.height = height;
        desc.allowUAV = true;
        desc.format = DXGI_FORMAT_R32_UINT;
        desc.pName = L"ProbeViewDepth";
        if (!m_probeDepth.Create(desc))
        {
            m_hasFailed = true;
            return false;
        }
        desc.format = colorFormat;
        desc.pName = L"ProbeViewOutput";
        if (!m_output.Create(desc))
        {
            m_hasFailed = true;
            return false;
        }
        m_isCreated = true;
        return true;
    }
    if (m_output.GetWidth() != width || m_output.GetHeight() != height)
    {
        // 描画解像度が変わった(まれ)。GPUが古いテクスチャを使い終わるまで待ってから作り直す
        GraphicsDevice::Instance().GetQueueManager()->GetGraphicsQueue()->Flush();
        m_probeDepth.Resize(width, height);
        m_output.Resize(width, height);
    }
    return true;
}

void ProbeViewer::Render(RenderTarget* pSceneRT, const Math::Matrix& view, const Math::Matrix& projection)
{
    auto& ddgi = DDGIVolume::Instance();
    if (!m_isEnabled || !pSceneRT || !ddgi.IsAvailable() || !ddgi.HasData())
    {
        return;
    }
    const UINT width = static_cast<UINT>(pSceneRT->GetWidth());
    const UINT height = static_cast<UINT>(pSceneRT->GetHeight());
    if (!EnsureResources(width, height, pSceneRT->GetResource()->GetDesc().Format)
        || !m_clearShader.IsValid() || !m_splatShader.IsValid() || !m_shadeShader.IsValid())
    {
        return;
    }

    // プローブの並びは、DDGIVolumeがライトの定数バッファに書いたものを使う
    const auto& lightData = ShaderManager::Instance().GetLightData();
    const uint32_t probeCount = (std::min)(static_cast<uint32_t>(lightData.DDGICounts.x * lightData.DDGICounts.y * lightData.DDGICounts.z), kMaxProbeCount);
    if (probeCount == 0)
    {
        return;
    }

    auto& device = GraphicsDevice::Instance();
    auto* pCmdList = device.GetCmdList();
    auto* pContext = device.GetContextManager()->GetGraphicsContext();
    PROFILE_GPU_SCOPE(pCmdList, "ProbeView");

    ProbeViewFrame frame = {};
    frame.viewProjection = view * projection;
    frame.inverseViewProjection = frame.viewProjection.Invert();
    const Math::Vector3 cameraPosition = view.Invert().Translation();
    frame.cameraPosition = Math::Vector4(cameraPosition.x, cameraPosition.y, cameraPosition.z, 1.0f);
    frame.ddgiOrigin = lightData.DDGIOrigin;
    frame.ddgiCounts = lightData.DDGICounts;

    ProbeViewConstants constants = {};
    constants.sceneDepthIndex = static_cast<uint32_t>(pSceneRT->GetDepthSRVIndex());
    constants.probeDepthIndex = static_cast<uint32_t>(m_probeDepth.GetUAVIndex());
    constants.sceneColorIndex = static_cast<uint32_t>(pSceneRT->GetSRVIndex());
    constants.outputIndex = static_cast<uint32_t>(m_output.GetUAVIndex());
    constants.irradianceIndex = static_cast<uint32_t>(ddgi.GetIrradianceSRVIndex());
    constants.width = width;
    constants.height = height;
    constants.probeCount = probeCount;
    constants.radius = m_radius;
    constants.exposure = ShaderManager::Instance().GetExposure();
    constants.maxDistance = kMaxDistance;
    constants.projectionScaleY = projection._22;
    constants.brightnessScale = std::exp2(m_brightnessEV);

    pContext->TransitionResource(pSceneRT->GetResource(), kShaderReadState);
    pContext->TransitionResource(pSceneRT->GetDepthResource(), kShaderReadState);
    pContext->TransitionResource(m_probeDepth.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    pContext->TransitionResource(m_output.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    pContext->FlushResourceBarriers();

    // 1. 画素ごとの「一番手前の球」を空にする
    const UINT groupCountX = ComputeShader::DivideRoundUp(width, kThreadGroupSize);
    const UINT groupCountY = ComputeShader::DivideRoundUp(height, kThreadGroupSize);
    m_clearShader.Begin();
    m_clearShader.SetConstants(constants);
    m_clearShader.SetConstantBuffer(frame);
    m_clearShader.Dispatch(groupCountX, groupCountY);

    D3D12_RESOURCE_BARRIER uavBarrier = {};
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.UAV.pResource = m_probeDepth.GetResource();
    pCmdList->ResourceBarrier(1, &uavBarrier);

    // 2. プローブごとに、画面の上の範囲で一番手前の球を残す
    m_splatShader.Begin();
    m_splatShader.SetConstants(constants);
    m_splatShader.SetConstantBuffer(frame);
    m_splatShader.Dispatch(probeCount, 1);

    // 3. 残った球を照らして、シーンの色に重ねる
    pContext->TransitionResource(m_probeDepth.GetResource(), kShaderReadState);
    pContext->FlushResourceBarriers();
    constants.probeDepthIndex = static_cast<uint32_t>(m_probeDepth.GetSRVIndex());
    m_shadeShader.Begin();
    m_shadeShader.SetConstants(constants);
    m_shadeShader.SetConstantBuffer(frame);
    m_shadeShader.Dispatch(groupCountX, groupCountY);

    // 結果をシーンの色へ写す(TAAと同じ)。深度は次の描画のために書き込みへ戻す
    pContext->TransitionResource(m_output.GetResource(), D3D12_RESOURCE_STATE_COPY_SOURCE);
    pContext->TransitionResource(pSceneRT->GetResource(), D3D12_RESOURCE_STATE_COPY_DEST);
    pContext->FlushResourceBarriers();
    pCmdList->CopyResource(pSceneRT->GetResource(), m_output.GetResource());
    pContext->TransitionResource(pSceneRT->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET);
    pContext->TransitionResource(pSceneRT->GetDepthResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
    pContext->FlushResourceBarriers();
}
