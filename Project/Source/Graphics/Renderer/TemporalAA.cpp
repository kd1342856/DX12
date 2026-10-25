#include "../../Pch.h"
#include "TemporalAA.h"
#include "MotionVectors.h"
#include "../GPUResource/RenderTarget/RenderTarget.h"
#include "../../Framework/DirectX/Utility/Profiler.h"

namespace
{
    // TemporalAA_CS.hlslのg_constants(b0)と同じ並び
    struct ResolveConstants
    {
        uint32_t colorIndex;
        uint32_t depthIndex;
        uint32_t velocityIndex;
        uint32_t historyIndex;
        uint32_t outputIndex;
        uint32_t width;
        uint32_t height;
        uint32_t resetHistory;
        float currentWeight;
    };

    constexpr UINT kThreadGroupSize = 8;
    // ジッターの周期。Halton(2,3)の最初の8点で、ピクセルの中をむらなく埋める
    constexpr uint32_t kJitterPhaseCount = 8;
    constexpr DXGI_FORMAT kHistoryFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
    constexpr D3D12_RESOURCE_STATES kShaderReadState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
}

void TemporalAA::Initialize(int width, int height)
{
    if (!ComputeShader::IsSupported() || !MotionVectors::Instance().IsValid())
    {
        return;
    }
    static const wchar_t* const kHistoryNames[2] = { L"TAAHistory0", L"TAAHistory1" };
    for (int i = 0; i < 2; ++i)
    {
        GPUTextureDesc desc;
        desc.width = static_cast<UINT>(width);
        desc.height = static_cast<UINT>(height);
        // シーンの色へそのままコピーで戻すので、シーンのRTと同じ形式にする
        desc.format = kHistoryFormat;
        desc.allowUAV = true;
        desc.pName = kHistoryNames[i];
        if (!m_historyTextures[i].Create(desc))
        {
            return;
        }
    }
    if (!m_resolveShader.Create(L"Asset/Shader/Temporal/TemporalAA_CS.hlsl"))
    {
        return;
    }
    m_isInitialized = true;
}

void TemporalAA::Resize(int width, int height)
{
    if (!m_isInitialized)
    {
        return;
    }
    for (GPUTexture& history : m_historyTextures)
    {
        history.Resize(static_cast<UINT>(width), static_cast<UINT>(height));
    }
    m_isHistoryValid = false;
}

float TemporalAA::Halton(uint32_t index, uint32_t base)
{
    float result = 0.0f;
    float fraction = 1.0f;
    while (index > 0)
    {
        fraction /= static_cast<float>(base);
        result += fraction * static_cast<float>(index % base);
        index /= base;
    }
    return result;
}

Math::Vector2 TemporalAA::BeginFrame(bool isEnabled, int width, int height)
{
    const bool wasEnabled = m_isEnabledThisFrame;
    m_isEnabledThisFrame = isEnabled && m_isInitialized;
    if (!m_isEnabledThisFrame)
    {
        return Math::Vector2::Zero;
    }
    // 無効から有効に切り替えた直後は、古い履歴を使わない
    if (!wasEnabled)
    {
        m_isHistoryValid = false;
    }

    // Haltonの0番目は(0,0)なので1から数える
    const uint32_t phase = (m_frameIndex % kJitterPhaseCount) + 1;
    ++m_frameIndex;
    const float offsetX = Halton(phase, 2) - 0.5f;
    const float offsetY = Halton(phase, 3) - 0.5f;
    // ピクセルのずれをNDCへ(NDCのyは上向き、ピクセルのyは下向き)
    return Math::Vector2(2.0f * offsetX / static_cast<float>(width), -2.0f * offsetY / static_cast<float>(height));
}

Math::Matrix TemporalAA::ApplyJitter(const Math::Matrix& projection, const Math::Vector2& jitterNdc)
{
    // 行ベクトルの投影: clip.x = ... + z*_31 + w*_41、clip.w = z*_34 + w*_44。
    // _31に(ずれ×_34)、_41に(ずれ×_44)を足すと、clip.x/clip.wがちょうどずれの分だけ動く
    Math::Matrix jittered = projection;
    jittered._31 += jitterNdc.x * projection._34;
    jittered._41 += jitterNdc.x * projection._44;
    jittered._32 += jitterNdc.y * projection._34;
    jittered._42 += jitterNdc.y * projection._44;
    return jittered;
}

void TemporalAA::Resolve(RenderTarget* pSceneRT, float currentWeight)
{
    if (!m_isEnabledThisFrame || !pSceneRT)
    {
        return;
    }
    auto& device = GraphicsDevice::Instance();
    auto* pCmdList = device.GetCmdList();
    auto* pContext = device.GetContextManager()->GetGraphicsContext();
    PROFILE_GPU_SCOPE(pCmdList, "TAA");

    GPUTexture* pVelocity = MotionVectors::Instance().GetVelocityTexture();
    GPUTexture& history = m_historyTextures[1 - m_writeIndex];
    GPUTexture& output = m_historyTextures[m_writeIndex];

    pContext->TransitionResource(pSceneRT->GetResource(), kShaderReadState);
    pContext->TransitionResource(pSceneRT->GetDepthResource(), kShaderReadState);
    pContext->TransitionResource(pVelocity->GetResource(), kShaderReadState);
    pContext->TransitionResource(history.GetResource(), kShaderReadState);
    pContext->TransitionResource(output.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    ResolveConstants constants = {};
    constants.colorIndex = static_cast<uint32_t>(pSceneRT->GetSRVIndex());
    constants.depthIndex = static_cast<uint32_t>(pSceneRT->GetDepthSRVIndex());
    constants.velocityIndex = static_cast<uint32_t>(pVelocity->GetSRVIndex());
    constants.historyIndex = static_cast<uint32_t>(history.GetSRVIndex());
    constants.outputIndex = static_cast<uint32_t>(output.GetUAVIndex());
    constants.width = output.GetWidth();
    constants.height = output.GetHeight();
    constants.resetHistory = m_isHistoryValid ? 0u : 1u;
    constants.currentWeight = currentWeight;

    m_resolveShader.Begin();
    m_resolveShader.SetConstants(constants);
    m_resolveShader.Dispatch(ComputeShader::DivideRoundUp(constants.width, kThreadGroupSize), ComputeShader::DivideRoundUp(constants.height, kThreadGroupSize));

    // 結果をシーンの色へ戻す。結果のテクスチャはそのまま次のフレームの履歴になる
    pContext->TransitionResource(output.GetResource(), D3D12_RESOURCE_STATE_COPY_SOURCE);
    pContext->TransitionResource(pSceneRT->GetResource(), D3D12_RESOURCE_STATE_COPY_DEST);
    pContext->FlushResourceBarriers();
    pCmdList->CopyResource(pSceneRT->GetResource(), output.GetResource());

    pContext->TransitionResource(pSceneRT->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET);
    pContext->TransitionResource(pSceneRT->GetDepthResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
    pContext->FlushResourceBarriers();

    m_isHistoryValid = true;
    m_writeIndex = 1 - m_writeIndex;
}
