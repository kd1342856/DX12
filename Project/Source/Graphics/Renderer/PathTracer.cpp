#include "../../Pch.h"
#include "PathTracer.h"
#include "RaytracingScene.h"
#include "../GPUResource/RenderTarget/RenderTarget.h"
#include "../../Framework/DirectX/Utility/Profiler.h"

namespace
{
    // PathTrace_CS.hlslのg_constants(b0)と同じ並び
    struct PathTraceConstants
    {
        uint32_t tlasIndex;
        uint32_t geometryIndex;
        uint32_t accumulationUAVIndex;
        uint32_t outputUAVIndex;
        uint32_t width;
        uint32_t height;
        uint32_t sampleIndex;
        uint32_t maxBounces;
        float skyAverage;
    };

    // PathTrace_CS.hlslのcbPathTrace(b1)と同じ並び
    struct PathTraceFrame
    {
        Math::Matrix inverseViewProjection;
        Math::Vector3 cameraPosition;
        float pad;
    };

    constexpr UINT kThreadGroupSize = 8;
    // DDGIVolumeと同じ(空のテクスチャの平均の明るさ)
    constexpr float kSkyAverage = 0.2f;
    constexpr D3D12_RESOURCE_STATES kShaderReadState = GPUTexture::kInitialState;

    bool IsSameMatrix(const Math::Matrix& a, const Math::Matrix& b)
    {
        return std::memcmp(&a, &b, sizeof(Math::Matrix)) == 0;
    }
}

void PathTracer::Initialize(int width, int height)
{
    if (!RaytracingScene::Instance().IsSupported())
    {
        return;
    }
    GPUTextureDesc desc;
    desc.width = static_cast<UINT>(width);
    desc.height = static_cast<UINT>(height);
    desc.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    desc.allowUAV = true;
    desc.pName = L"PathTraceAccumulation";
    if (!m_accumulationTexture.Create(desc))
    {
        return;
    }
    // シーンの色へそのままコピーで戻すので、シーンのRTと同じ形式にする
    desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    desc.pName = L"PathTraceOutput";
    if (!m_outputTexture.Create(desc))
    {
        return;
    }
    if (!m_shader.Create(L"Asset/Shader/Lighting/PathTrace_CS.hlsl"))
    {
        return;
    }
    m_isInitialized = true;
}

void PathTracer::Resize(int width, int height)
{
    if (!m_isInitialized)
    {
        return;
    }
    m_accumulationTexture.Resize(static_cast<UINT>(width), static_cast<UINT>(height));
    m_outputTexture.Resize(static_cast<UINT>(width), static_cast<UINT>(height));
    m_sampleIndex = 0;
}

void PathTracer::Render(RenderTarget* pSceneRT, const Math::Matrix& view, const Math::Matrix& projection, int maxBounces,
    const CBufferData::Light& lightData, const CBufferData::System& systemData)
{
    const RaytracingScene& scene = RaytracingScene::Instance();
    if (!m_isInitialized || !pSceneRT || scene.GetTLASIndex() == -1)
    {
        return;
    }
    auto& device = GraphicsDevice::Instance();
    auto* pCmdList = device.GetCmdList();
    auto* pContext = device.GetContextManager()->GetGraphicsContext();
    PROFILE_GPU_SCOPE(pCmdList, "PathTrace");

    // カメラが動いた・設定が変わったら足し直す
    const Math::Matrix viewProjection = view * projection;
    if (!IsSameMatrix(viewProjection, m_previousViewProjection) || maxBounces != m_maxBounces)
    {
        m_sampleIndex = 0;
    }
    m_previousViewProjection = viewProjection;
    m_maxBounces = maxBounces;

    PathTraceConstants constants = {};
    constants.tlasIndex = static_cast<uint32_t>(scene.GetTLASIndex());
    constants.geometryIndex = static_cast<uint32_t>(scene.GetGeometryInfoIndex());
    constants.accumulationUAVIndex = static_cast<uint32_t>(m_accumulationTexture.GetUAVIndex());
    constants.outputUAVIndex = static_cast<uint32_t>(m_outputTexture.GetUAVIndex());
    constants.width = m_outputTexture.GetWidth();
    constants.height = m_outputTexture.GetHeight();
    constants.sampleIndex = m_sampleIndex;
    constants.maxBounces = static_cast<uint32_t>((std::max)(maxBounces, 0));
    constants.skyAverage = kSkyAverage;

    PathTraceFrame frame = {};
    frame.inverseViewProjection = viewProjection.Invert();
    frame.cameraPosition = view.Invert().Translation();

    pContext->TransitionResource(m_accumulationTexture.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    pContext->TransitionResource(m_outputTexture.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    m_shader.Begin();
    m_shader.SetConstants(constants);
    m_shader.SetConstantBuffer(1, frame);
    m_shader.SetConstantBuffer(3, lightData);
    m_shader.SetConstantBuffer(4, systemData);
    m_shader.Dispatch(ComputeShader::DivideRoundUp(constants.width, kThreadGroupSize), ComputeShader::DivideRoundUp(constants.height, kThreadGroupSize));

    pContext->TransitionResource(m_accumulationTexture.GetResource(), kShaderReadState);
    pContext->TransitionResource(m_outputTexture.GetResource(), D3D12_RESOURCE_STATE_COPY_SOURCE);
    pContext->TransitionResource(pSceneRT->GetResource(), D3D12_RESOURCE_STATE_COPY_DEST);
    pContext->FlushResourceBarriers();
    pCmdList->CopyResource(pSceneRT->GetResource(), m_outputTexture.GetResource());
    pContext->TransitionResource(pSceneRT->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET);
    pContext->FlushResourceBarriers();

    ++m_sampleIndex;
}
