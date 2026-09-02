#include "../../../Pch.h"
#include "Profiler.h"
#include "Logger.h"
#include <windows.h>
#include <psapi.h>
#include <d3d12.h>

static void LogGPUProfilerHResultFailure(const char* what, HRESULT hr)
{
    char msg[256] = {};
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
        static_cast<DWORD>(hr), 0, msg, static_cast<DWORD>(sizeof(msg)), nullptr);
    Logger::Instance().AddLog(Logger::LogLevel::Error,
        "[Profiler] %s failed: 0x%08X (%s)", what, static_cast<unsigned>(hr), msg);
}

float Profiler::GetSystemRAMUsageMB() const
{
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
    {
        return static_cast<float>(pmc.WorkingSetSize) / (1024.0f * 1024.0f);
    }
    return 0.0f;
}

// ---------------------------------------------------------------
// GPU section timing
// ---------------------------------------------------------------
void Profiler::InitGPU(ID3D12Device* pDevice, ID3D12CommandQueue* pQueue, uint32_t frameCount)
{
    if (!pDevice || !pQueue || frameCount == 0) return;

    ShutdownGPU();

    m_gpuFrameCount = frameCount;
    m_gpuCurrentSlot = 0;
    m_gpuScopeCounter = 0;

    UINT64 freq = 0;
    HRESULT hr = pQueue->GetTimestampFrequency(&freq);
    if (FAILED(hr))
    {
        LogGPUProfilerHResultFailure("GetTimestampFrequency", hr);
        // Fall through and still create the heap/buffers - freq==0 just means timings
        // stay unreported (ReadGPUResultsForSlot guards on m_gpuFrequency != 0), but at
        // least the rest of GPU profiling can be diagnosed independently of this.
    }
    m_gpuFrequency = freq;
    if (freq == 0)
    {
        Logger::Instance().AddLog(Logger::LogLevel::Warning,
            "[Profiler] GPU timestamp frequency is 0 - GPU pass timings will not be reported.");
    }

    D3D12_QUERY_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    heapDesc.Count = kMaxGPUScopesPerFrame * 2 * frameCount;
    hr = pDevice->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(&m_spQueryHeap));
    if (FAILED(hr))
    {
        LogGPUProfilerHResultFailure("CreateQueryHeap", hr);
        m_spQueryHeap.Reset();
        return;
    }

    m_gpuReadbackBuffers.resize(frameCount);
    m_gpuScopeNamesPerSlot.assign(frameCount, std::vector<std::string>(kMaxGPUScopesPerFrame));
    m_gpuScopeCountPerSlot.assign(frameCount, 0);

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_READBACK;

    D3D12_RESOURCE_DESC bufDesc = {};
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Width = kMaxGPUScopesPerFrame * 2 * sizeof(uint64_t);
    bufDesc.Height = 1;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.MipLevels = 1;
    bufDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufDesc.SampleDesc.Count = 1;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    for (uint32_t i = 0; i < frameCount; ++i)
    {
        hr = pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_gpuReadbackBuffers[i]));
        if (FAILED(hr))
        {
            LogGPUProfilerHResultFailure("CreateCommittedResource (GPU timing readback buffer)", hr);
        }
    }

    Logger::Instance().AddLog(Logger::LogLevel::Info,
        "[Profiler] GPU timing initialized. frequency=%llu Hz, frameCount=%u", m_gpuFrequency, frameCount);
}

void Profiler::ShutdownGPU()
{
    m_spQueryHeap.Reset();
    m_gpuReadbackBuffers.clear();
    m_gpuScopeNamesPerSlot.clear();
    m_gpuScopeCountPerSlot.clear();
    m_gpuTimings.clear();
    m_gpuFrequency = 0;
    m_gpuFrameCount = 0;
    m_gpuCurrentSlot = 0;
    m_gpuScopeCounter = 0;
}

void Profiler::ReadGPUResultsForSlot(uint32_t slot)
{
    if (slot >= m_gpuReadbackBuffers.size() || !m_gpuReadbackBuffers[slot]) return;

    uint32_t count = m_gpuScopeCountPerSlot[slot];
    if (count == 0) return;

    D3D12_RANGE readRange = { 0, count * 2 * sizeof(uint64_t) };
    uint64_t* pData = nullptr;
    HRESULT hr = m_gpuReadbackBuffers[slot]->Map(0, &readRange, reinterpret_cast<void**>(&pData));
    if (SUCCEEDED(hr))
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            uint64_t begin = pData[i * 2 + 0];
            uint64_t end = pData[i * 2 + 1];
            const std::string& name = m_gpuScopeNamesPerSlot[slot][i];
            if (!name.empty() && end >= begin && m_gpuFrequency != 0)
            {
                float ms = static_cast<float>(end - begin) / static_cast<float>(m_gpuFrequency) * 1000.0f;
                m_gpuTimings[name] = ms;
            }
        }
        D3D12_RANGE writeRange = { 0, 0 };
        m_gpuReadbackBuffers[slot]->Unmap(0, &writeRange);
    }
    else
    {
        LogGPUProfilerHResultFailure("Readback buffer Map", hr);
    }
}

void Profiler::BeginGPUFrame(ID3D12GraphicsCommandList* pCmdList)
{
    if (!m_spQueryHeap) return;

    // The slot we're about to (re)use was last written kFrameCount frames ago; by the
    // frame-resource fencing rules the GPU is guaranteed to be done with it by now, so
    // it's safe to read its results before overwriting it with this frame's queries.
    ReadGPUResultsForSlot(m_gpuCurrentSlot);

    m_gpuScopeCounter = 0;
}

void Profiler::EndGPUFrame(ID3D12GraphicsCommandList* pCmdList)
{
    if (!m_spQueryHeap || !pCmdList) return;

    uint32_t heapOffset = m_gpuCurrentSlot * kMaxGPUScopesPerFrame * 2;
    uint32_t queryCount = m_gpuScopeCounter * 2;
    if (queryCount > 0)
    {
        pCmdList->ResolveQueryData(m_spQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP,
            heapOffset, queryCount, m_gpuReadbackBuffers[m_gpuCurrentSlot].Get(), 0);
    }
    m_gpuScopeCountPerSlot[m_gpuCurrentSlot] = m_gpuScopeCounter;

    m_gpuCurrentSlot = (m_gpuCurrentSlot + 1) % m_gpuFrameCount;
}

uint32_t Profiler::BeginGPUScope(ID3D12GraphicsCommandList* pCmdList, const std::string& name)
{
    if (!m_spQueryHeap || !pCmdList || m_gpuScopeCounter >= kMaxGPUScopesPerFrame) return UINT32_MAX;

    uint32_t index = m_gpuScopeCounter++;
    uint32_t heapIndex = m_gpuCurrentSlot * kMaxGPUScopesPerFrame * 2 + index * 2;
    pCmdList->EndQuery(m_spQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, heapIndex);
    m_gpuScopeNamesPerSlot[m_gpuCurrentSlot][index] = name;
    return index;
}

void Profiler::EndGPUScope(ID3D12GraphicsCommandList* pCmdList, uint32_t scopeHandle)
{
    if (!m_spQueryHeap || !pCmdList || scopeHandle == UINT32_MAX) return;

    uint32_t heapIndex = m_gpuCurrentSlot * kMaxGPUScopesPerFrame * 2 + scopeHandle * 2 + 1;
    pCmdList->EndQuery(m_spQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, heapIndex);
}

// ---------------------------------------------------------------
// Frame time history
// ---------------------------------------------------------------
void Profiler::AddFrameTime(float ms)
{
    m_frameTimeHistory[m_frameTimeHistoryIdx] = ms;
    m_frameTimeHistoryIdx = (m_frameTimeHistoryIdx + 1) % kFrameHistorySize;
}
