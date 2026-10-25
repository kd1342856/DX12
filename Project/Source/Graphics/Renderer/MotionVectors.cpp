#include "../../Pch.h"
#include "MotionVectors.h"
#include "../GPUResource/RenderTarget/RenderTarget.h"
#include "../../Framework/DirectX/Utility/Profiler.h"

namespace
{
    // CameraVelocity_CS.hlslのg_constants(b0)と同じ並び
    struct CameraVelocityConstants
    {
        uint32_t depthIndex;
        uint32_t velocityIndex;
        uint32_t width;
        uint32_t height;
    };

    // CameraVelocity_CS.hlslのcbFrame(b1)と同じ並び
    struct CameraVelocityFrame
    {
        Math::Matrix inverseJitteredViewProjection;
        Math::Matrix currentViewProjection;
        Math::Matrix previousViewProjection;
    };

    constexpr UINT kThreadGroupSize = 8;
    constexpr D3D12_RESOURCE_STATES kShaderReadState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
}

void MotionVectors::Initialize(int width, int height)
{
    if (!ComputeShader::IsSupported())
    {
        return;
    }
    GPUTextureDesc desc;
    desc.width = static_cast<UINT>(width);
    desc.height = static_cast<UINT>(height);
    desc.format = kFormat;
    desc.allowUAV = true;
    desc.allowRenderTarget = true;
    desc.pName = L"Velocity";
    if (!m_velocityTexture.Create(desc))
    {
        return;
    }
    if (!m_cameraVelocityShader.Create(L"Asset/Shader/Temporal/CameraVelocity_CS.hlsl"))
    {
        return;
    }
    m_isInitialized = true;
}

void MotionVectors::Resize(int width, int height)
{
    if (!m_isInitialized)
    {
        return;
    }
    m_velocityTexture.Resize(static_cast<UINT>(width), static_cast<UINT>(height));
}

void MotionVectors::BeginFrame(const Math::Matrix& view, const Math::Matrix& projection, const Math::Matrix& jitteredProjection)
{
    const Math::Matrix viewProjection = view * projection;
    // 最初のフレームと、履歴を捨てた直後は、前の行列を今と同じにする(速度0)
    m_previousViewProjection = m_hasPrevious ? m_currentViewProjection : viewProjection;
    m_currentViewProjection = viewProjection;
    m_inverseJitteredViewProjection = (view * jitteredProjection).Invert();
    m_hasPrevious = true;
}

void MotionVectors::RenderCameraVelocity(RenderTarget* pSceneRT)
{
    if (!m_isInitialized || !pSceneRT)
    {
        return;
    }
    auto& device = GraphicsDevice::Instance();
    auto* pContext = device.GetContextManager()->GetGraphicsContext();
    PROFILE_GPU_SCOPE(device.GetCmdList(), "CameraVelocity");

    pContext->TransitionResource(pSceneRT->GetDepthResource(), kShaderReadState);
    pContext->TransitionResource(m_velocityTexture.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    CameraVelocityConstants constants = {};
    constants.depthIndex = static_cast<uint32_t>(pSceneRT->GetDepthSRVIndex());
    constants.velocityIndex = static_cast<uint32_t>(m_velocityTexture.GetUAVIndex());
    constants.width = m_velocityTexture.GetWidth();
    constants.height = m_velocityTexture.GetHeight();

    CameraVelocityFrame frame = {};
    frame.inverseJitteredViewProjection = m_inverseJitteredViewProjection;
    frame.currentViewProjection = m_currentViewProjection;
    frame.previousViewProjection = m_previousViewProjection;

    m_cameraVelocityShader.Begin();
    m_cameraVelocityShader.SetConstants(constants);
    m_cameraVelocityShader.SetConstantBuffer(frame);
    m_cameraVelocityShader.Dispatch(ComputeShader::DivideRoundUp(constants.width, kThreadGroupSize), ComputeShader::DivideRoundUp(constants.height, kThreadGroupSize));

    // 続けて動く物を、シーンの深度でテストしながら上に描く
    pContext->TransitionResource(pSceneRT->GetDepthResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
    pContext->TransitionResource(m_velocityTexture.GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET);
    pContext->FlushResourceBarriers();
}
