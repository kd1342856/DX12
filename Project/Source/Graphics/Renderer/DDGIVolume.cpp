#include "../../Pch.h"
#include "DDGIVolume.h"
#include "RaytracingScene.h"
#include "../../Framework/DirectX/Utility/Profiler.h"

namespace
{
    // DDGI_CS.hlslのg_constants(b0)と同じ並び
    struct DDGIConstants
    {
        uint32_t tlasIndex;
        uint32_t geometryIndex;
        uint32_t rayDataIndex;
        uint32_t irradianceUAVIndex;
        uint32_t distanceUAVIndex;
        uint32_t irradianceSRVIndex;
        uint32_t distanceSRVIndex;
        uint32_t raysPerProbe;
        uint32_t probeCount;
        float hysteresis;
        uint32_t resetHistory;
        uint32_t borderTargetIndex;
        uint32_t borderInterior;
        float maxRayDistance;
        float skyAverage;
    };

    // DDGI_CS.hlslのcbDDGIFrame(b1)
    struct DDGIFrame
    {
        Math::Matrix rayRotation;
    };

    // 空のテクスチャの平均の明るさ(空の輝度の何倍か)。窓から入る空の光の量に効く
    constexpr float kSkyAverage = 0.2f;
    // 壁の中のプローブを拾わないよう、読む位置を法線とカメラの向きへずらす量(間隔に対する割合)
    constexpr float kNormalBiasRatio = 0.2f;
    constexpr float kViewBiasRatio = 0.2f;
    constexpr D3D12_RESOURCE_STATES kShaderReadState = GPUTexture::kInitialState;
}

void DDGIVolume::Initialize()
{
    if (!RaytracingScene::Instance().IsSupported())
    {
        return;
    }
    // 大きさはSetBoundsで決まるので、ここでは最小の大きさで作っておく
    GPUTextureDesc desc;
    desc.allowUAV = true;
    desc.width = kRaysPerProbe;
    desc.height = 1;
    desc.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    desc.pName = L"DDGIRayData";
    if (!m_rayDataTexture.Create(desc))
    {
        return;
    }
    desc.width = kTilesPerRow * (kIrradianceTexels + 2);
    desc.height = kIrradianceTexels + 2;
    desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    desc.pName = L"DDGIIrradiance";
    if (!m_irradianceTexture.Create(desc))
    {
        return;
    }
    desc.width = kTilesPerRow * (kDistanceTexels + 2);
    desc.height = kDistanceTexels + 2;
    desc.format = DXGI_FORMAT_R16G16_FLOAT;
    desc.pName = L"DDGIDistance";
    if (!m_distanceTexture.Create(desc))
    {
        return;
    }

    const wchar_t* pPath = L"Asset/Shader/Lighting/DDGI_CS.hlsl";
    if (!m_traceShader.Create(pPath, L"TraceRays") || !m_irradianceShader.Create(pPath, L"UpdateIrradiance") ||
        !m_distanceShader.Create(pPath, L"UpdateDistance") || !m_borderShader.Create(pPath, L"UpdateBorder"))
    {
        return;
    }
    m_isInitialized = true;
}

void DDGIVolume::SetBounds(const Math::Vector3& boundsMin, const Math::Vector3& boundsMax)
{
    if (!m_isInitialized)
    {
        return;
    }
    const Math::Vector3 size = Math::Vector3::Max(boundsMax - boundsMin, Math::Vector3(0.1f, 0.1f, 0.1f));
    // 1軸のプローブの数が上限を超えないよう、間隔を広げる
    float spacing = kTargetSpacing;
    const float longest = (std::max)(size.x, (std::max)(size.y, size.z));
    spacing = (std::max)(spacing, longest / static_cast<float>(kMaxProbesPerAxis - 1));

    int counts[3];
    const float sizes[3] = { size.x, size.y, size.z };
    for (int axis = 0; axis < 3; ++axis)
    {
        counts[axis] = (std::max)(2, static_cast<int>(std::ceil(sizes[axis] / spacing)) + 1);
    }
    const bool isChanged = counts[0] != m_counts[0] || counts[1] != m_counts[1] || counts[2] != m_counts[2] ||
        std::abs(spacing - m_spacing) > 0.001f || Math::Vector3::DistanceSquared(boundsMin, m_origin) > 0.0001f || m_probeCount == 0;
    if (!isChanged)
    {
        return;
    }
    m_origin = boundsMin;
    m_spacing = spacing;
    m_counts[0] = counts[0];
    m_counts[1] = counts[1];
    m_counts[2] = counts[2];
    m_probeCount = static_cast<UINT>(counts[0] * counts[1] * counts[2]);
    ResizeTextures();
    m_isHistoryValid = false;
    m_hasData = false;
    Logger::Instance().AddLog(Logger::LogLevel::Info, u8"[DDGI] プローブ %d x %d x %d (%u個)、間隔 %.2f m",
        counts[0], counts[1], counts[2], m_probeCount, spacing);
}

void DDGIVolume::ResizeTextures()
{
    // 作り直すテクスチャを、前のフレームのGPUが使い終わるまで待つ(範囲が変わるのは読み込みのときだけ)
    GraphicsDevice::Instance().GetQueueManager()->GetGraphicsQueue()->Flush();
    const UINT rows = (m_probeCount + kTilesPerRow - 1) / kTilesPerRow;
    m_rayDataTexture.Resize(kRaysPerProbe, m_probeCount);
    m_irradianceTexture.Resize(kTilesPerRow * (kIrradianceTexels + 2), rows * (kIrradianceTexels + 2));
    m_distanceTexture.Resize(kTilesPerRow * (kDistanceTexels + 2), rows * (kDistanceTexels + 2));
}

Math::Matrix DDGIVolume::MakeRandomRotation()
{
    // 一様な乱数の四元数(Shoemake)
    std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
    const float u1 = distribution(m_random);
    const float u2 = distribution(m_random) * DirectX::XM_2PI;
    const float u3 = distribution(m_random) * DirectX::XM_2PI;
    const float a = std::sqrt(1.0f - u1);
    const float b = std::sqrt(u1);
    const Math::Quaternion rotation(a * std::sin(u2), a * std::cos(u2), b * std::sin(u3), b * std::cos(u3));
    return Math::Matrix::CreateFromQuaternion(rotation);
}

void DDGIVolume::Update(const CBufferData::Light& lightData, const CBufferData::System& systemData)
{
    const RaytracingScene& scene = RaytracingScene::Instance();
    if (!m_isInitialized || !m_isEnabled || m_probeCount == 0 || scene.GetTLASIndex() == -1)
    {
        return;
    }
    auto& device = GraphicsDevice::Instance();
    auto* pCmdList = device.GetCmdList();
    auto* pContext = device.GetContextManager()->GetGraphicsContext();
    PROFILE_GPU_SCOPE(pCmdList, "DDGI");

    DDGIConstants constants = {};
    constants.tlasIndex = static_cast<uint32_t>(scene.GetTLASIndex());
    constants.geometryIndex = static_cast<uint32_t>(scene.GetGeometryInfoIndex());
    constants.rayDataIndex = static_cast<uint32_t>(m_rayDataTexture.GetUAVIndex());
    constants.irradianceUAVIndex = static_cast<uint32_t>(m_irradianceTexture.GetUAVIndex());
    constants.distanceUAVIndex = static_cast<uint32_t>(m_distanceTexture.GetUAVIndex());
    constants.irradianceSRVIndex = static_cast<uint32_t>(m_irradianceTexture.GetSRVIndex());
    constants.distanceSRVIndex = static_cast<uint32_t>(m_distanceTexture.GetSRVIndex());
    constants.raysPerProbe = kRaysPerProbe;
    constants.probeCount = m_probeCount;
    constants.hysteresis = m_hysteresis;
    constants.resetHistory = m_isHistoryValid ? 0u : 1u;
    constants.maxRayDistance = m_spacing * 1.5f;
    constants.skyAverage = kSkyAverage;

    DDGIFrame frame;
    frame.rayRotation = MakeRandomRotation();

    // レイトレースで読むライト・システムの定数は、DDGIの値を書いた後のもの(跳ね返りで前のプローブを読むため)
    CBufferData::Light traceLightData = lightData;
    FillShaderData(traceLightData);
    // 最初の更新では前の値が無いので、跳ね返りは読まない
    if (!m_isHistoryValid)
    {
        traceLightData.DDGIParams.x = 0.0f;
    }

    // 1. レイを飛ばす
    pContext->TransitionResource(m_rayDataTexture.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    pContext->TransitionResource(m_irradianceTexture.GetResource(), kShaderReadState);
    pContext->TransitionResource(m_distanceTexture.GetResource(), kShaderReadState);
    m_traceShader.Begin();
    m_traceShader.SetConstants(constants);
    m_traceShader.SetConstantBuffer(1, frame);
    m_traceShader.SetConstantBuffer(3, traceLightData);
    m_traceShader.SetConstantBuffer(4, systemData);
    m_traceShader.Dispatch(m_probeCount, 1, 1);

    // 2. プローブのテクセルへ集める
    D3D12_RESOURCE_BARRIER uavBarrier = {};
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.UAV.pResource = m_rayDataTexture.GetResource();
    pCmdList->ResourceBarrier(1, &uavBarrier);
    pContext->TransitionResource(m_irradianceTexture.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    pContext->TransitionResource(m_distanceTexture.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    m_irradianceShader.Begin();
    m_irradianceShader.SetConstants(constants);
    m_irradianceShader.SetConstantBuffer(1, frame);
    m_irradianceShader.SetConstantBuffer(3, traceLightData);
    m_irradianceShader.Dispatch(m_probeCount, 1, 1);
    m_distanceShader.Begin();
    m_distanceShader.SetConstants(constants);
    m_distanceShader.SetConstantBuffer(1, frame);
    m_distanceShader.SetConstantBuffer(3, traceLightData);
    m_distanceShader.Dispatch(m_probeCount, 1, 1);

    // 3. タイルの縁を写す(内側を書き終えてから)
    uavBarrier.UAV.pResource = nullptr;
    pCmdList->ResourceBarrier(1, &uavBarrier);
    m_borderShader.Begin();
    m_borderShader.SetConstantBuffer(3, traceLightData);
    constants.borderTargetIndex = static_cast<uint32_t>(m_irradianceTexture.GetUAVIndex());
    constants.borderInterior = kIrradianceTexels;
    m_borderShader.SetConstants(constants);
    m_borderShader.Dispatch(m_probeCount, 1, 1);
    constants.borderTargetIndex = static_cast<uint32_t>(m_distanceTexture.GetUAVIndex());
    constants.borderInterior = kDistanceTexels;
    m_borderShader.SetConstants(constants);
    m_borderShader.Dispatch(m_probeCount, 1, 1);

    pContext->TransitionResource(m_irradianceTexture.GetResource(), kShaderReadState);
    pContext->TransitionResource(m_distanceTexture.GetResource(), kShaderReadState);
    pContext->FlushResourceBarriers();

    m_isHistoryValid = true;
    m_hasData = true;
}

void DDGIVolume::FillShaderData(CBufferData::Light& lightData) const
{
    const bool isActive = m_isInitialized && m_isEnabled && m_hasData;
    lightData.DDGIOrigin = Math::Vector4(m_origin.x, m_origin.y, m_origin.z, m_spacing);
    lightData.DDGICounts = DirectX::XMINT4(m_counts[0], m_counts[1], m_counts[2], static_cast<int32_t>(kTilesPerRow));
    lightData.DDGIParams = Math::Vector4(isActive ? 1.0f : 0.0f, m_spacing * kNormalBiasRatio, m_spacing * kViewBiasRatio, 0.0f);
}
