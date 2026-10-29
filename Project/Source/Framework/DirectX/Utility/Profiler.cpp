#include "../../../Pch.h"
#include "Profiler.h"
#include "Logger.h"
#include "Thread.h"
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
// 区間名の登録
// ---------------------------------------------------------------
Profiler::ScopeName& Profiler::RegisterLiteralName(ScopeSite& site, const char* pName)
{
    ScopeName& scopeName = FindOrAddName(site, pName, false);
    std::lock_guard<std::mutex> lock(m_scopeNameMutex);
    // 2つのスレッドが同時に初めて通った場合も、同じ結果を書くだけなので問題ない
    if (!site.pLiteralName.load(std::memory_order_relaxed))
    {
#ifdef TRACY_ENABLE
        site.srcloc = { pName, site.pFunction, site.pFile, site.line, 0 };
#endif
        // srclocを書き終えてから公開する
        site.pLiteralName.store(&scopeName, std::memory_order_release);
    }
    return scopeName;
}

Profiler::ScopeName& Profiler::FindOrAddName(const ScopeSite& site, std::string_view name, bool copyName)
{
    std::lock_guard<std::mutex> lock(m_scopeNameMutex);
    auto it = m_scopeNameLookup.find(name);
    if (it != m_scopeNameLookup.end())
    {
        return *it->second;
    }

    // 新しい名前のときだけここに来る(名前の種類ぶんしか確保しない)
    ScopeName& scopeName = m_scopeNames.emplace_back();
    if (copyName)
    {
        scopeName.ownedName.assign(name.data(), name.size());
        scopeName.pName = scopeName.ownedName.c_str();
    }
    else
    {
        scopeName.pName = name.data();
    }
#ifdef TRACY_ENABLE
    scopeName.srcloc = { scopeName.pName, site.pFunction, site.pFile, site.line, 0 };
#endif
    m_scopeNameLookup.emplace(std::string_view(scopeName.pName, name.size()), &scopeName);
    return scopeName;
}

// ---------------------------------------------------------------
// CPU区間計測
// ---------------------------------------------------------------
void Profiler::AddCPUTiming(ScopeName& name, float ms)
{
    // 集計用の配列はメインスレッドでしか触らない。ジョブの中などの区間はTracyでだけ見る
    if (!Thread::IsMainThread())
    {
        return;
    }

    // 同じ名前が1フレームで複数回使われた場合(カメラごとに呼ばれる等)は加算していく。
    if (name.cpuFrameStamp != m_cpuFrameStamp)
    {
        name.cpuFrameStamp = m_cpuFrameStamp;
        name.cpuTimingIndex = static_cast<uint32_t>(m_cpuTimings.size());
        m_cpuTimings.push_back({ name.pName, 0.0f });
    }
    m_cpuTimings[name.cpuTimingIndex].ms += ms;
}

// ---------------------------------------------------------------
// GPU区間計測
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
        // 失敗してもそのままヒープ/バッファは作る - freq==0の場合は計測結果が
        // 報告されないだけ(ReadGPUResultsForSlotがm_gpuFrequency != 0でガードしている)
        // だが、少なくともGPUプロファイリングの他の部分はこれとは独立して診断できる。
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
    m_gpuScopeNamesPerSlot.assign(frameCount, std::vector<ScopeName*>(kMaxGPUScopesPerFrame, nullptr));
    m_gpuTimings.reserve(kMaxGPUScopesPerFrame);
    m_cpuTimings.reserve(256);
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

#ifdef TRACY_ENABLE
    m_pTracyGPUContext = tracy::CreateD3D12Context(pDevice, pQueue);
    const char kContextName[] = "Graphics";
    m_pTracyGPUContext->Name(kContextName, static_cast<uint16_t>(sizeof(kContextName) - 1));
#endif
}

void Profiler::ShutdownGPU()
{
#ifdef TRACY_ENABLE
    // 記録済みのクエリの結果を回収してから解放する
    if (m_pTracyGPUContext)
    {
        tracy::DestroyD3D12Context(m_pTracyGPUContext);
        m_pTracyGPUContext = nullptr;
    }
#endif
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
        // このスロットのフレームの結果で置き換える(そのフレームに無かったパスは表示から消える)
        m_gpuTimings.clear();
        m_gpuFrameStamp++;
        for (uint32_t scopeIndex = 0; scopeIndex < count; ++scopeIndex)
        {
            uint64_t begin = pData[scopeIndex * 2 + 0];
            uint64_t end = pData[scopeIndex * 2 + 1];
            ScopeName* pName = m_gpuScopeNamesPerSlot[slot][scopeIndex];
            if (pName && end >= begin && m_gpuFrequency != 0)
            {
                float ms = static_cast<float>(end - begin) / static_cast<float>(m_gpuFrequency) * 1000.0f;
                // 同じ名前が1フレームで複数回使われた場合はCPU計測と同じく加算する
                if (pName->gpuFrameStamp != m_gpuFrameStamp)
                {
                    pName->gpuFrameStamp = m_gpuFrameStamp;
                    pName->gpuTimingIndex = static_cast<uint32_t>(m_gpuTimings.size());
                    m_gpuTimings.push_back({ pName->pName, 0.0f });
                }
                m_gpuTimings[pName->gpuTimingIndex].ms += ms;
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

    // これから(再)使おうとしているスロットは、kFrameCountフレーム前に最後に書き込まれた
    // ものであり、フレームリソースのフェンス管理のルールにより、GPUは既にそれを使い終えて
    // いることが保証されている。なので、このフレームのクエリで上書きする前に結果を
    // 読んでも安全。
    ReadGPUResultsForSlot(m_gpuCurrentSlot);

    m_gpuScopeCounter = 0;

#ifdef TRACY_ENABLE
    // 終わったクエリの結果をTracyに送る(待たない。接続していないときは何もしない)
    if (m_pTracyGPUContext)
    {
        m_pTracyGPUContext->Collect();
    }
#endif
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

uint32_t Profiler::BeginGPUScope(ID3D12GraphicsCommandList* pCmdList, ScopeName& name)
{
    if (!m_spQueryHeap || !pCmdList || m_gpuScopeCounter >= kMaxGPUScopesPerFrame) return UINT32_MAX;

    uint32_t index = m_gpuScopeCounter++;
    uint32_t heapIndex = m_gpuCurrentSlot * kMaxGPUScopesPerFrame * 2 + index * 2;
    pCmdList->EndQuery(m_spQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, heapIndex);
    m_gpuScopeNamesPerSlot[m_gpuCurrentSlot][index] = &name;
    return index;
}

void Profiler::EndGPUScope(ID3D12GraphicsCommandList* pCmdList, uint32_t scopeHandle)
{
    if (!m_spQueryHeap || !pCmdList || scopeHandle == UINT32_MAX) return;

    uint32_t heapIndex = m_gpuCurrentSlot * kMaxGPUScopesPerFrame * 2 + scopeHandle * 2 + 1;
    pCmdList->EndQuery(m_spQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, heapIndex);
}

// ---------------------------------------------------------------
// フレーム時間の履歴
// ---------------------------------------------------------------
void Profiler::AddFrameTime(float ms)
{
    m_frameTimeHistory[m_frameTimeHistoryIdx] = ms;
    m_frameTimeHistoryIdx = (m_frameTimeHistoryIdx + 1) % kFrameHistorySize;
}

// ---------------------------------------------------------------
// フレーム境界 + スパイク検出
// ---------------------------------------------------------------
void Profiler::BeginCPUFrame()
{
    // 前回のBeginCPUFrame～今回までの実時間 = 直前の1フレーム全体(アップロード処理・GPU待ち・Present込み)。
    // m_cpuTimingsにはまだ同じフレームの区間計測が残っているので、リセット前に判定する。
    static auto s_last = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::high_resolution_clock::now();
    float frameMs = std::chrono::duration<float, std::milli>(now - s_last).count();
    s_last = now;

#ifdef TRACY_ENABLE
    // Tracyのフレームの区切り。自前の計測と同じ位置で区切る
    FrameMark;
#endif

    AddFrameTime(frameMs);

    if (m_spikeWarmupFrames > 0)
    {
        // 起動直後はロード等で荒れるので、平均を作るだけで判定はしない
        --m_spikeWarmupFrames;
        m_avgFrameMs = (m_avgFrameMs == 0.0f) ? frameMs : m_avgFrameMs * 0.9f + frameMs * 0.1f;
    }
    else if (frameMs > m_avgFrameMs * 3.0f && frameMs > m_avgFrameMs + 10.0f)
    {
        // スパイクは平均に混ぜない(混ぜると直後の判定が鈍る)
        ReportFrameSpike(frameMs);
    }
    else
    {
        m_avgFrameMs = m_avgFrameMs * 0.95f + frameMs * 0.05f;
    }

    ResetPerFrameCounters();
}

void Profiler::ReportFrameSpike(float frameMs) const
{
    // スパイクのときだけ通るので、ここでの確保は気にしない
    std::vector<ScopeTiming> sorted(m_cpuTimings.begin(), m_cpuTimings.end());
    std::sort(sorted.begin(), sorted.end(), [](const ScopeTiming& lhs, const ScopeTiming& rhs)
    {
        return lhs.ms > rhs.ms;
    });

    // 区間は入れ子になっている(SceneManager::Updateの中にShadow等がある)ので合計は一致しない
    std::string detail;
    const size_t count = (std::min)(sorted.size(), static_cast<size_t>(10));
    for (size_t i = 0; i < count; ++i)
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "%s%s=%.2fms", i ? ", " : "", sorted[i].pName, sorted[i].ms);
        detail += buf;
    }

    Logger::Instance().AddLog(Logger::LogLevel::Warning,
        "[Spike] frame %.1fms (avg %.1fms) : %s", frameMs, m_avgFrameMs, detail.c_str());
}
