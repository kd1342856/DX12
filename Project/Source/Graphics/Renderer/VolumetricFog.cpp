#include "../../Pch.h"
#include "VolumetricFog.h"
#include "DDGIVolume.h"
#include "../GPUResource/RenderTarget/RenderTarget.h"
#include "../Shader/ShaderSettings.h"
#include "../../Framework/DirectX/Utility/Profiler.h"

namespace
{
    // VolumetricFog_CS.hlslのg_constants(b0)と同じ並び
    struct FogConstants
    {
        uint32_t scatteringUAVIndex;
        uint32_t historySRVIndex;
        uint32_t integratedUAVIndex;
        uint32_t integratedSRVIndex;
        uint32_t sceneColorSRVIndex;
        uint32_t depthSRVIndex;
        uint32_t outputUAVIndex;
        uint32_t directionalShadowSRVIndex;
        uint32_t spotShadowSRVIndex;
        uint32_t cubeShadowSRVIndex;
        uint32_t ddgiIrradianceSRVIndex;
        uint32_t ddgiDistanceSRVIndex;
        uint32_t hasHistory;
    };

    // VolumetricFog_CS.hlslのcbFog(b1)と同じ並び
    struct FogFrame
    {
        Math::Matrix inverseViewProjection;
        Math::Matrix previousViewProjection;
        Math::Vector3 cameraPosition;
        float fogNear;
        Math::Vector3 cameraForward;
        float fogFar;
        Math::Vector3 albedo;
        float extinction;
        uint32_t gridSize[3];
        float phaseG;
        float screenSize[2];
        float jitterZ;
        float historyWeight;
    };

    constexpr UINT kThreadGroupSize = 8;
    // フロクセルの最初の面(m)。カメラのすぐ前は霧が薄いので、分割をもったいなく使わない
    constexpr float kFogNear = 0.1f;
    constexpr D3D12_RESOURCE_STATES kShaderReadState = GPUTexture::kInitialState;

    // Haltonの基数2(0～1)
    float Halton2(uint32_t index)
    {
        float result = 0.0f;
        float fraction = 0.5f;
        while (index > 0)
        {
            result += fraction * static_cast<float>(index & 1u);
            index >>= 1;
            fraction *= 0.5f;
        }
        return result;
    }
}

void VolumetricFog::Initialize(int width, int height)
{
    if (!ComputeShader::IsSupported())
    {
        return;
    }
    GPUTextureDesc desc;
    desc.dimension = TextureDimension::Texture3D;
    desc.width = GridWidth(width);
    desc.height = GridHeight(height);
    desc.depthOrArraySize = kSliceCount;
    desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    desc.allowUAV = true;
    static const wchar_t* const kNames[2] = { L"FogScattering0", L"FogScattering1" };
    for (int i = 0; i < 2; ++i)
    {
        desc.pName = kNames[i];
        if (!m_scatteringTextures[i].Create(desc))
        {
            return;
        }
    }
    desc.pName = L"FogIntegrated";
    if (!m_integratedTexture.Create(desc))
    {
        return;
    }

    GPUTextureDesc outputDesc;
    outputDesc.width = static_cast<UINT>(width);
    outputDesc.height = static_cast<UINT>(height);
    // シーンの色へそのままコピーで戻すので、シーンのRTと同じ形式にする
    outputDesc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    outputDesc.allowUAV = true;
    outputDesc.pName = L"FogOutput";
    if (!m_outputTexture.Create(outputDesc))
    {
        return;
    }

    const wchar_t* pPath = L"Asset/Shader/Lighting/VolumetricFog_CS.hlsl";
    if (!m_injectShader.Create(pPath, L"InjectScattering") || !m_integrateShader.Create(pPath, L"IntegrateScattering") ||
        !m_applyShader.Create(pPath, L"ApplyFog"))
    {
        return;
    }
    m_isInitialized = true;
}

void VolumetricFog::Resize(int width, int height)
{
    if (!m_isInitialized)
    {
        return;
    }
    for (GPUTexture& texture : m_scatteringTextures)
    {
        texture.Resize(GridWidth(width), GridHeight(height));
    }
    m_integratedTexture.Resize(GridWidth(width), GridHeight(height));
    m_outputTexture.Resize(static_cast<UINT>(width), static_cast<UINT>(height));
    m_hasHistory = false;
}

void VolumetricFog::Render(RenderTarget* pSceneRT, const Math::Matrix& view, const Math::Matrix& projection,
    const FogSettings& settings, float extinction, const CBufferData::Light& lightData, const CBufferData::System& systemData)
{
    if (!m_isInitialized || !pSceneRT)
    {
        return;
    }
    auto& device = GraphicsDevice::Instance();
    auto* pCmdList = device.GetCmdList();
    auto* pContext = device.GetContextManager()->GetGraphicsContext();
    PROFILE_GPU_SCOPE(pCmdList, "VolumetricFog");

    GPUTexture& scattering = m_scatteringTextures[m_writeIndex];
    GPUTexture& history = m_scatteringTextures[1 - m_writeIndex];
    const DDGIVolume& ddgi = DDGIVolume::Instance();

    FogConstants constants = {};
    constants.scatteringUAVIndex = static_cast<uint32_t>(scattering.GetUAVIndex());
    constants.historySRVIndex = static_cast<uint32_t>(history.GetSRVIndex());
    constants.integratedUAVIndex = static_cast<uint32_t>(m_integratedTexture.GetUAVIndex());
    constants.integratedSRVIndex = static_cast<uint32_t>(m_integratedTexture.GetSRVIndex());
    constants.sceneColorSRVIndex = static_cast<uint32_t>(pSceneRT->GetSRVIndex());
    constants.depthSRVIndex = static_cast<uint32_t>(pSceneRT->GetDepthSRVIndex());
    constants.outputUAVIndex = static_cast<uint32_t>(m_outputTexture.GetUAVIndex());
    constants.directionalShadowSRVIndex = static_cast<uint32_t>(device.GetShadowMap()->GetSRVNumber());
    constants.spotShadowSRVIndex = static_cast<uint32_t>(device.GetSpotShadowMaps()->GetSRVNumber());
    constants.cubeShadowSRVIndex = static_cast<uint32_t>(device.GetCubeShadowMaps()->GetSRVNumber());
    constants.ddgiIrradianceSRVIndex = static_cast<uint32_t>(ddgi.IsAvailable() ? ddgi.GetIrradianceSRVIndex() : device.GetBlackTex()->GetSRVNumber());
    constants.ddgiDistanceSRVIndex = static_cast<uint32_t>(ddgi.IsAvailable() ? ddgi.GetDistanceSRVIndex() : device.GetBlackTex()->GetSRVNumber());
    constants.hasHistory = m_hasHistory ? 1u : 0u;

    const Math::Matrix viewProjection = view * projection;
    const Math::Matrix cameraWorld = view.Invert();
    FogFrame frame = {};
    frame.inverseViewProjection = viewProjection.Invert();
    frame.previousViewProjection = m_hasHistory ? m_previousViewProjection : viewProjection;
    frame.cameraPosition = cameraWorld.Translation();
    frame.cameraForward = Math::Vector3::TransformNormal(Math::Vector3(0.0f, 0.0f, 1.0f), cameraWorld);
    frame.cameraForward.Normalize();
    frame.fogNear = kFogNear;
    frame.fogFar = (std::max)(settings.VolumetricFogFar, kFogNear * 2.0f);
    frame.albedo = Math::Vector3(settings.FogColor.x, settings.FogColor.y, settings.FogColor.z);
    frame.extinction = extinction;
    frame.gridSize[0] = scattering.GetWidth();
    frame.gridSize[1] = scattering.GetHeight();
    frame.gridSize[2] = kSliceCount;
    frame.phaseG = settings.VolumetricFogAnisotropy;
    frame.screenSize[0] = static_cast<float>(pSceneRT->GetWidth());
    frame.screenSize[1] = static_cast<float>(pSceneRT->GetHeight());
    frame.jitterZ = Halton2((m_frameIndex++ % 16) + 1);
    frame.historyWeight = settings.VolumetricFogHistoryWeight;

    // 1. フロクセルごとの散乱(前のフレームの結果を読む)。影はコンピュートからも読める状態にする
    pContext->TransitionResource(device.GetShadowMap()->GetResource(), kShaderReadState);
    pContext->TransitionResource(device.GetSpotShadowMaps()->GetResource(), kShaderReadState);
    pContext->TransitionResource(device.GetCubeShadowMaps()->GetResource(), kShaderReadState);
    pContext->TransitionResource(scattering.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    pContext->TransitionResource(history.GetResource(), kShaderReadState);
    m_injectShader.Begin();
    m_injectShader.SetConstants(constants);
    m_injectShader.SetConstantBuffer(1, frame);
    m_injectShader.SetConstantBuffer(3, lightData);
    m_injectShader.SetConstantBuffer(4, systemData);
    m_injectShader.Dispatch(ComputeShader::DivideRoundUp(frame.gridSize[0], kThreadGroupSize), ComputeShader::DivideRoundUp(frame.gridSize[1], kThreadGroupSize), kSliceCount);

    // 2. 積み上げる(今のフレームの散乱を読む。HistorySRVIndexの欄を、読む散乱のテクスチャに使う)
    pContext->TransitionResource(scattering.GetResource(), kShaderReadState);
    pContext->TransitionResource(m_integratedTexture.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    FogConstants integrateConstants = constants;
    integrateConstants.historySRVIndex = static_cast<uint32_t>(scattering.GetSRVIndex());
    m_integrateShader.Begin();
    m_integrateShader.SetConstants(integrateConstants);
    m_integrateShader.SetConstantBuffer(1, frame);
    m_integrateShader.SetConstantBuffer(3, lightData);
    m_integrateShader.Dispatch(ComputeShader::DivideRoundUp(frame.gridSize[0], kThreadGroupSize), ComputeShader::DivideRoundUp(frame.gridSize[1], kThreadGroupSize), 1);

    // 3. 画面に掛けて、シーンの色へ戻す
    pContext->TransitionResource(m_integratedTexture.GetResource(), kShaderReadState);
    pContext->TransitionResource(pSceneRT->GetResource(), kShaderReadState);
    pContext->TransitionResource(pSceneRT->GetDepthResource(), kShaderReadState);
    pContext->TransitionResource(m_outputTexture.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    m_applyShader.Begin();
    m_applyShader.SetConstants(constants);
    m_applyShader.SetConstantBuffer(1, frame);
    m_applyShader.SetConstantBuffer(3, lightData);
    m_applyShader.Dispatch(ComputeShader::DivideRoundUp(pSceneRT->GetWidth(), kThreadGroupSize), ComputeShader::DivideRoundUp(pSceneRT->GetHeight(), kThreadGroupSize));

    pContext->TransitionResource(m_outputTexture.GetResource(), D3D12_RESOURCE_STATE_COPY_SOURCE);
    pContext->TransitionResource(pSceneRT->GetResource(), D3D12_RESOURCE_STATE_COPY_DEST);
    pContext->FlushResourceBarriers();
    pCmdList->CopyResource(pSceneRT->GetResource(), m_outputTexture.GetResource());
    pContext->TransitionResource(pSceneRT->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET);
    pContext->TransitionResource(pSceneRT->GetDepthResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
    pContext->FlushResourceBarriers();

    m_previousViewProjection = viewProjection;
    m_hasHistory = true;
    m_writeIndex = 1 - m_writeIndex;
}
