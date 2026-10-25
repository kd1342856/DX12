#include "../../Pch.h"
#include "EnvironmentBRDF.h"
#include "../../Framework/DirectX/Utility/Profiler.h"

namespace
{
    // EnvironmentBRDF_CS.hlsl‚Ìg_constants(b0)‚Æ“¯‚¶•À‚Ñ
    struct GenerateConstants
    {
        uint32_t outputIndex;
        uint32_t size;
        uint32_t sampleCount;
    };

    constexpr UINT kThreadGroupSize = 8;
}

void EnvironmentBRDF::Initialize()
{
    if (!ComputeShader::IsSupported())
    {
        return;
    }
    GPUTextureDesc desc;
    desc.width = kSize;
    desc.height = kSize;
    desc.format = DXGI_FORMAT_R16G16_FLOAT;
    desc.allowUAV = true;
    desc.pName = L"EnvironmentBRDF";
    if (!m_texture.Create(desc))
    {
        return;
    }
    if (!m_shader.Create(L"Asset/Shader/Lighting/EnvironmentBRDF_CS.hlsl"))
    {
        return;
    }
    m_isInitialized = true;
}

void EnvironmentBRDF::GenerateIfNeeded()
{
    if (!m_isInitialized || m_isGenerated)
    {
        return;
    }
    auto& device = GraphicsDevice::Instance();
    auto* pContext = device.GetContextManager()->GetGraphicsContext();
    PROFILE_GPU_SCOPE(device.GetCmdList(), "EnvironmentBRDF");

    pContext->TransitionResource(m_texture.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    GenerateConstants constants = {};
    constants.outputIndex = static_cast<uint32_t>(m_texture.GetUAVIndex());
    constants.size = kSize;
    constants.sampleCount = kSampleCount;
    m_shader.Begin();
    m_shader.SetConstants(constants);
    m_shader.Dispatch(ComputeShader::DivideRoundUp(kSize, kThreadGroupSize), ComputeShader::DivideRoundUp(kSize, kThreadGroupSize));

    pContext->TransitionResource(m_texture.GetResource(), GPUTexture::kInitialState);
    pContext->FlushResourceBarriers();
    m_isGenerated = true;
}
