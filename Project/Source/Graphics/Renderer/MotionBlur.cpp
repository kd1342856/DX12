#include "../../Pch.h"
#include "MotionBlur.h"
#include "MotionVectors.h"
#include "../GPUResource/RenderTarget/RenderTarget.h"
#include "../../Framework/DirectX/Utility/Profiler.h"

namespace
{
    // MotionBlur_CS.hlslのg_constants(b0)と同じ並び
    struct MotionBlurConstants
    {
        uint32_t colorIndex;
        uint32_t velocityIndex;
        uint32_t outputIndex;
        uint32_t width;
        uint32_t height;
        float shutterFraction;
        float maxBlurPixels;
        uint32_t frameIndex;
    };

    constexpr UINT kThreadGroupSize = 8;
    constexpr D3D12_RESOURCE_STATES kShaderReadState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
}

void MotionBlur::Initialize(int width, int height)
{
    if (!ComputeShader::IsSupported() || !MotionVectors::Instance().IsValid())
    {
        return;
    }
    GPUTextureDesc desc;
    desc.width = static_cast<UINT>(width);
    desc.height = static_cast<UINT>(height);
    // シーンの色へそのままコピーで戻すので、シーンのRTと同じ形式にする
    desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    desc.allowUAV = true;
    desc.pName = L"MotionBlurOutput";
    if (!m_outputTexture.Create(desc))
    {
        return;
    }
    if (!m_shader.Create(L"Asset/Shader/Temporal/MotionBlur_CS.hlsl"))
    {
        return;
    }
    m_isInitialized = true;
}

void MotionBlur::Resize(int width, int height)
{
    if (!m_isInitialized)
    {
        return;
    }
    m_outputTexture.Resize(static_cast<UINT>(width), static_cast<UINT>(height));
}

void MotionBlur::Apply(RenderTarget* pSceneRT, float shutterFraction, float maxBlurPixels)
{
    if (!m_isInitialized || !pSceneRT || shutterFraction <= 0.0f)
    {
        return;
    }
    auto& device = GraphicsDevice::Instance();
    auto* pCmdList = device.GetCmdList();
    auto* pContext = device.GetContextManager()->GetGraphicsContext();
    PROFILE_GPU_SCOPE(pCmdList, "MotionBlur");

    GPUTexture* pVelocity = MotionVectors::Instance().GetVelocityTexture();
    pContext->TransitionResource(pSceneRT->GetResource(), kShaderReadState);
    pContext->TransitionResource(pVelocity->GetResource(), kShaderReadState);
    pContext->TransitionResource(m_outputTexture.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    MotionBlurConstants constants = {};
    constants.colorIndex = static_cast<uint32_t>(pSceneRT->GetSRVIndex());
    constants.velocityIndex = static_cast<uint32_t>(pVelocity->GetSRVIndex());
    constants.outputIndex = static_cast<uint32_t>(m_outputTexture.GetUAVIndex());
    constants.width = m_outputTexture.GetWidth();
    constants.height = m_outputTexture.GetHeight();
    constants.shutterFraction = (std::min)(shutterFraction, 1.0f);
    constants.maxBlurPixels = maxBlurPixels;
    constants.frameIndex = m_frameIndex++;

    m_shader.Begin();
    m_shader.SetConstants(constants);
    m_shader.Dispatch(ComputeShader::DivideRoundUp(constants.width, kThreadGroupSize), ComputeShader::DivideRoundUp(constants.height, kThreadGroupSize));

    pContext->TransitionResource(m_outputTexture.GetResource(), D3D12_RESOURCE_STATE_COPY_SOURCE);
    pContext->TransitionResource(pSceneRT->GetResource(), D3D12_RESOURCE_STATE_COPY_DEST);
    pContext->FlushResourceBarriers();
    pCmdList->CopyResource(pSceneRT->GetResource(), m_outputTexture.GetResource());
    pContext->TransitionResource(pSceneRT->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET);
    pContext->FlushResourceBarriers();
}
