#include "../../Pch.h"
#include "AutoExposure.h"
#include "../GPUResource/RenderTarget/RenderTarget.h"
#include "../Shader/ShaderSettings.h"
#include "../../Framework/DirectX/Utility/Profiler.h"

namespace
{
    // AutoExposure_CS.hlslのg_constants(b0)と同じ並び
    struct HistogramConstants
    {
        uint32_t colorIndex;
        uint32_t histogramIndex;
        uint32_t width;
        uint32_t height;
        float minLog2Luminance;
        float log2LuminanceRange;
        float log2Exposure;
    };

    constexpr UINT kThreadGroupSize = 16;
    constexpr UINT64 kHistogramSize = AutoExposure::kHistogramBinCount * sizeof(uint32_t);
    constexpr D3D12_RESOURCE_STATES kShaderReadState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

    D3D12_RESOURCE_DESC MakeBufferDesc(UINT64 size, D3D12_RESOURCE_FLAGS flags)
    {
        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = size;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.Flags = flags;
        return desc;
    }
}

void AutoExposure::Initialize()
{
    if (!ComputeShader::IsSupported())
    {
        return;
    }
    auto& device = GraphicsDevice::Instance();

    // ヒストグラム(GPUで数える)
    D3D12_HEAP_PROPERTIES defaultHeap = {};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
    const D3D12_RESOURCE_DESC histogramDesc = MakeBufferDesc(kHistogramSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    if (FAILED(device.GetDevice()->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &histogramDesc,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&m_cpHistogramBuffer))))
    {
        Logger::Instance().AddLog(Logger::LogLevel::Error, u8"[AutoExposure] ヒストグラムのバッファを作れませんでした");
        return;
    }
    m_cpHistogramBuffer->SetName(L"ExposureHistogram");
    device.GetResourceStateTracker()->AddResourceState(m_cpHistogramBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    // シェーダーからはRWByteAddressBufferとして読み書きする
    auto* pAllocator = device.GetDescriptorHeapManager()->GetCBVSRVUAVAllocator();
    m_histogramUAVIndex = static_cast<int>(pAllocator->Allocate());
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.NumElements = kHistogramBinCount;
    uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
    device.GetDevice()->CreateUnorderedAccessView(m_cpHistogramBuffer.Get(), nullptr, &uavDesc, pAllocator->GetCPUHandle(m_histogramUAVIndex));

    // CPUへの読み戻し(READBACKのヒープはCOPY_DESTのまま使う)
    D3D12_HEAP_PROPERTIES readbackHeap = {};
    readbackHeap.Type = D3D12_HEAP_TYPE_READBACK;
    const D3D12_RESOURCE_DESC readbackDesc = MakeBufferDesc(kHistogramSize, D3D12_RESOURCE_FLAG_NONE);
    for (auto& cpReadback : m_cpReadbackBuffers)
    {
        if (FAILED(device.GetDevice()->CreateCommittedResource(&readbackHeap, D3D12_HEAP_FLAG_NONE, &readbackDesc,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&cpReadback))))
        {
            Logger::Instance().AddLog(Logger::LogLevel::Error, u8"[AutoExposure] 読み戻しのバッファを作れませんでした");
            return;
        }
        cpReadback->SetName(L"ExposureHistogramReadback");
    }

    if (!m_clearShader.Create(L"Asset/Shader/Temporal/AutoExposure_CS.hlsl", L"ClearHistogram") ||
        !m_buildShader.Create(L"Asset/Shader/Temporal/AutoExposure_CS.hlsl", L"BuildHistogram"))
    {
        return;
    }
    m_isInitialized = true;
}

void AutoExposure::Dispatch(RenderTarget* pSceneRT, float exposure)
{
    if (!m_isInitialized || !pSceneRT)
    {
        return;
    }
    auto& device = GraphicsDevice::Instance();
    auto* pCmdList = device.GetCmdList();
    auto* pContext = device.GetContextManager()->GetGraphicsContext();
    PROFILE_GPU_SCOPE(pCmdList, "AutoExposure");

    HistogramConstants constants = {};
    constants.colorIndex = static_cast<uint32_t>(pSceneRT->GetSRVIndex());
    constants.histogramIndex = static_cast<uint32_t>(m_histogramUAVIndex);
    constants.width = static_cast<uint32_t>(pSceneRT->GetWidth());
    constants.height = static_cast<uint32_t>(pSceneRT->GetHeight());
    constants.minLog2Luminance = kMinLog2Luminance;
    constants.log2LuminanceRange = kMaxLog2Luminance - kMinLog2Luminance;
    constants.log2Exposure = std::log2((std::max)(exposure, 1e-20f));

    pContext->TransitionResource(pSceneRT->GetResource(), kShaderReadState);
    pContext->TransitionResource(m_cpHistogramBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    m_clearShader.Begin();
    m_clearShader.SetConstants(constants);
    m_clearShader.Dispatch(1, 1);

    // 0にしたのを待ってから数える
    D3D12_RESOURCE_BARRIER uavBarrier = {};
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.UAV.pResource = m_cpHistogramBuffer.Get();
    pCmdList->ResourceBarrier(1, &uavBarrier);

    // 2x2ピクセルにつき1点だけ数える
    m_buildShader.Begin();
    m_buildShader.SetConstants(constants);
    m_buildShader.Dispatch(ComputeShader::DivideRoundUp((constants.width + 1) / 2, kThreadGroupSize), ComputeShader::DivideRoundUp((constants.height + 1) / 2, kThreadGroupSize));

    const int slot = static_cast<int>(m_dispatchCount % kReadbackSlotCount);
    pContext->TransitionResource(m_cpHistogramBuffer.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE);
    pContext->FlushResourceBarriers();
    pCmdList->CopyBufferRegion(m_cpReadbackBuffers[slot].Get(), 0, m_cpHistogramBuffer.Get(), 0, kHistogramSize);
    pContext->TransitionResource(m_cpHistogramBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    pContext->FlushResourceBarriers();
    ++m_dispatchCount;
}

bool AutoExposure::ComputeAverageLog2Luminance(const uint32_t* pBins, float lowPercent, float highPercent, float& outAverage) const
{
    double total = 0.0;
    for (UINT bin = 0; bin < kHistogramBinCount; ++bin)
    {
        total += pBins[bin];
    }
    if (total <= 0.0)
    {
        return false;
    }

    // 暗い方から数えてlow%～high%に入る分だけを平均する(真っ暗な影や光源そのものに引っ張られないように)
    const double lowCount = total * std::clamp(lowPercent, 0.0f, 100.0f) / 100.0;
    const double highCount = total * std::clamp((std::max)(highPercent, lowPercent), 0.0f, 100.0f) / 100.0;
    const float binWidth = (kMaxLog2Luminance - kMinLog2Luminance) / static_cast<float>(kHistogramBinCount);

    double accumulated = 0.0;
    double weightedSum = 0.0;
    double weightSum = 0.0;
    for (UINT bin = 0; bin < kHistogramBinCount; ++bin)
    {
        const double count = pBins[bin];
        const double binStart = accumulated;
        const double binEnd = accumulated + count;
        accumulated = binEnd;
        const double inside = (std::min)(binEnd, highCount) - (std::max)(binStart, lowCount);
        if (inside <= 0.0)
        {
            continue;
        }
        const float binCenter = kMinLog2Luminance + (static_cast<float>(bin) + 0.5f) * binWidth;
        weightedSum += inside * binCenter;
        weightSum += inside;
    }
    if (weightSum <= 0.0)
    {
        return false;
    }
    outAverage = static_cast<float>(weightedSum / weightSum);
    return true;
}

void AutoExposure::Update(float deltaTime, const PostProcessSettings& settings, float manualEV100)
{
    if (!m_isInitialized || !settings.EnableAutoExposure)
    {
        // 次にオンにしたとき、今の手動の露出から順応を始める
        m_isActive = false;
        m_dispatchCount = 0;
        m_ev100 = manualEV100;
        m_targetEV100 = manualEV100;
        return;
    }
    m_isActive = true;

    // GPUが書き終わった一番古い読み戻しを使う
    if (m_dispatchCount < kReadbackSlotCount)
    {
        return;
    }
    const int slot = static_cast<int>((m_dispatchCount - kReadbackSlotCount) % kReadbackSlotCount);
    void* pMapped = nullptr;
    const D3D12_RANGE readRange = { 0, static_cast<SIZE_T>(kHistogramSize) };
    if (FAILED(m_cpReadbackBuffers[slot]->Map(0, &readRange, &pMapped)))
    {
        return;
    }
    uint32_t bins[kHistogramBinCount] = {};
    std::memcpy(bins, pMapped, sizeof(bins));
    const D3D12_RANGE writtenRange = { 0, 0 };
    m_cpReadbackBuffers[slot]->Unmap(0, &writtenRange);

    float averageLog2Luminance = 0.0f;
    if (!ComputeAverageLog2Luminance(bins, settings.AutoExposureLowPercent, settings.AutoExposureHighPercent, averageLog2Luminance))
    {
        return;
    }

    // 平均の輝度がちょうど中間の明るさになるEV100: EV100 = log2(L × S / K)、S = 100、K = 12.5 → log2(L) + 3。
    // 補正(段)を足すほど明るくするので、EV100からは引く
    m_targetEV100 = std::clamp(averageLog2Luminance + 3.0f - settings.ExposureCompensation,
        settings.AutoExposureMinEV100, (std::max)(settings.AutoExposureMaxEV100, settings.AutoExposureMinEV100));

    // 明るい場面へ(EV100が上がる)は速く、暗い場面へはゆっくり順応する
    const float difference = m_targetEV100 - m_ev100;
    const float speed = (difference > 0.0f) ? settings.AutoExposureSpeedUp : settings.AutoExposureSpeedDown;
    m_ev100 += difference * (1.0f - std::exp(-(std::max)(deltaTime, 0.0f) * (std::max)(speed, 0.0f)));
}
