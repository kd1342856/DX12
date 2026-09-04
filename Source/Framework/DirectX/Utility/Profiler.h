#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <chrono>
#include <unordered_map>
#include <wrl/client.h>

struct ID3D12Device;
struct ID3D12CommandQueue;
struct ID3D12GraphicsCommandList;
struct ID3D12QueryHeap;
struct ID3D12Resource;

class Profiler
{
public:
    static Profiler& Instance()
    {
        static Profiler instance;
        return instance;
    }

    void ResetPerFrameCounters()
    {
        m_drawCallCount = 0;
        m_drawCallBreakdown.clear();
        m_instanceCount = 0;
        m_instanceBreakdown.clear();
        m_dispatchCount = 0;
        m_cpuTimings.clear();
        m_entitiesVisible = 0;
        m_entitiesCulled = 0;
        m_meshesVisible = 0;
        m_meshesCulled = 0;
    }

    void AddDrawCall(const std::string& name, uint32_t instanceCount = 1)
    {
        m_drawCallCount++;
        m_drawCallBreakdown[name]++;
        m_instanceCount += instanceCount;
        m_instanceBreakdown[name] += instanceCount;
    }

    void AddDispatch()
    {
        m_dispatchCount++;
    }

    uint32_t GetDrawCallCount() const { return m_drawCallCount; }
    uint32_t GetInstanceCount() const { return m_instanceCount; }
    const std::unordered_map<std::string, uint32_t>& GetInstanceBreakdown() const { return m_instanceBreakdown; }
    uint32_t GetDispatchCount() const { return m_dispatchCount; }
    const std::unordered_map<std::string, uint32_t>& GetDrawCallBreakdown() const { return m_drawCallBreakdown; }

    // フラスタムカリング統計(RenderSystem::RenderScene)、毎フレームリセットされる。
    void AddEntityCullResult(bool culled) { if (culled) m_entitiesCulled++; else m_entitiesVisible++; }
    void AddMeshCullResult(bool culled) { if (culled) m_meshesCulled++; else m_meshesVisible++; }
    uint32_t GetEntitiesVisible() const { return m_entitiesVisible; }
    uint32_t GetEntitiesCulled() const { return m_entitiesCulled; }
    uint32_t GetMeshesVisible() const { return m_meshesVisible; }
    uint32_t GetMeshesCulled() const { return m_meshesCulled; }

    // メモリ使用量の問い合わせ
    float GetSystemRAMUsageMB() const;

    // ---------------------------------------------------------------
    // CPU区間計測（ResetPerFrameCountersで毎フレームクリアされる）
    // ---------------------------------------------------------------
    void AddCPUTiming(const std::string& name, float ms)
    {
        // 同じ名前が1フレームで複数回使われた場合(カメラごとに呼ばれる等)は加算していく。
        m_cpuTimings[name] += ms;
    }
    const std::unordered_map<std::string, float>& GetCPUTimings() const { return m_cpuTimings; }

    class ScopedCPUTimer
    {
    public:
        explicit ScopedCPUTimer(std::string name)
            : m_name(std::move(name)), m_start(std::chrono::high_resolution_clock::now()) {}
        ~ScopedCPUTimer()
        {
            auto end = std::chrono::high_resolution_clock::now();
            float ms = std::chrono::duration<float, std::milli>(end - m_start).count();
            Profiler::Instance().AddCPUTiming(m_name, ms);
        }
        ScopedCPUTimer(const ScopedCPUTimer&) = delete;
        ScopedCPUTimer& operator=(const ScopedCPUTimer&) = delete;
    private:
        std::string m_name;
        std::chrono::high_resolution_clock::time_point m_start;
    };

    // ---------------------------------------------------------------
    // GPU区間計測（D3D12タイムスタンプクエリ）。
    // キューは、記録されてからkFrameCount-1フレーム後でもまだそのスコープの
    // コマンドを実行している可能性があるため、フレームインフライトのスロットが
    // 再利用しても安全だと分かった時点で結果を読み戻す
    // (フレームリソース管理の他の部分と同じルール)。
    // ---------------------------------------------------------------
    static constexpr uint32_t kMaxGPUScopesPerFrame = 32;

    void InitGPU(ID3D12Device* pDevice, ID3D12CommandQueue* pQueue, uint32_t frameCount);
    void ShutdownGPU();

    // GPUスコープを記録し始める前、フレームの一番最初に1回だけ呼ぶ。
    void BeginGPUFrame(ID3D12GraphicsCommandList* pCmdList);
    // フレームの一番最後に1回だけ呼ぶ(上と同じコマンドリストの中で)。
    void EndGPUFrame(ID3D12GraphicsCommandList* pCmdList);

    // EndGPUScopeに渡すハンドルを返す。GPU計測が未初期化、またはフレームあたりの
    // スコープ数上限(kMaxGPUScopesPerFrame)に達している場合はUINT32_MAXを返す。
    uint32_t BeginGPUScope(ID3D12GraphicsCommandList* pCmdList, const std::string& name);
    void EndGPUScope(ID3D12GraphicsCommandList* pCmdList, uint32_t scopeHandle);

    const std::unordered_map<std::string, float>& GetGPUTimings() const { return m_gpuTimings; }

    class ScopedGPUTimer
    {
    public:
        ScopedGPUTimer(ID3D12GraphicsCommandList* pCmdList, std::string name)
            : m_pCmdList(pCmdList)
            , m_handle(Profiler::Instance().BeginGPUScope(pCmdList, name)) {}
        ~ScopedGPUTimer()
        {
            Profiler::Instance().EndGPUScope(m_pCmdList, m_handle);
        }
        ScopedGPUTimer(const ScopedGPUTimer&) = delete;
        ScopedGPUTimer& operator=(const ScopedGPUTimer&) = delete;
    private:
        ID3D12GraphicsCommandList* m_pCmdList;
        uint32_t m_handle;
    };

    // ---------------------------------------------------------------
    // フレーム時間の履歴(ms)、グラフ表示用のリングバッファ。
    // ---------------------------------------------------------------
    static constexpr int kFrameHistorySize = 120;

    void AddFrameTime(float ms);
    const std::array<float, kFrameHistorySize>& GetFrameTimeHistory() const { return m_frameTimeHistory; }
    int GetFrameTimeHistoryIndex() const { return m_frameTimeHistoryIdx; }

private:
    Profiler() = default;
    ~Profiler() = default;

    void ReadGPUResultsForSlot(uint32_t slot);

    uint32_t m_drawCallCount = 0;
    std::unordered_map<std::string, uint32_t> m_drawCallBreakdown;
    uint32_t m_instanceCount = 0;
    std::unordered_map<std::string, uint32_t> m_instanceBreakdown;
    uint32_t m_dispatchCount = 0;

    // フラスタムカリング統計
    uint32_t m_entitiesVisible = 0;
    uint32_t m_entitiesCulled = 0;
    uint32_t m_meshesVisible = 0;
    uint32_t m_meshesCulled = 0;

    // CPU計測
    std::unordered_map<std::string, float> m_cpuTimings;

    // GPU計測
    uint64_t m_gpuFrequency = 0;
    uint32_t m_gpuFrameCount = 0;
    uint32_t m_gpuCurrentSlot = 0;
    uint32_t m_gpuScopeCounter = 0;
    Microsoft::WRL::ComPtr<ID3D12QueryHeap> m_spQueryHeap;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_gpuReadbackBuffers; // フレームインフライトのスロットごとに1つ
    std::vector<std::vector<std::string>> m_gpuScopeNamesPerSlot;            // 各スロットに記録されたスコープ名
    std::vector<uint32_t> m_gpuScopeCountPerSlot;                            // 各スロットに記録されたスコープ数
    std::unordered_map<std::string, float> m_gpuTimings;

    // フレーム時間の履歴
    std::array<float, kFrameHistorySize> m_frameTimeHistory = {};
    int m_frameTimeHistoryIdx = 0;
};

// 使い方: 計測したいブロックの先頭で PROFILE_CPU_SCOPE("Shadow"); のように呼ぶ。
#define PROFILE_CPU_SCOPE_CONCAT_INNER(a, b) a##b
#define PROFILE_CPU_SCOPE_CONCAT(a, b) PROFILE_CPU_SCOPE_CONCAT_INNER(a, b)
#define PROFILE_CPU_SCOPE(name) Profiler::ScopedCPUTimer PROFILE_CPU_SCOPE_CONCAT(cpuScope_, __LINE__)(name)

// 使い方: 計測したいブロックの先頭で PROFILE_GPU_SCOPE(cmdList, "Shadow"); のように呼ぶ。
#define PROFILE_GPU_SCOPE(cmdList, name) Profiler::ScopedGPUTimer PROFILE_CPU_SCOPE_CONCAT(gpuScope_, __LINE__)(cmdList, name)
