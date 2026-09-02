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

    // Frustum culling stats (RenderSystem::RenderScene), reset every frame.
    void AddEntityCullResult(bool culled) { if (culled) m_entitiesCulled++; else m_entitiesVisible++; }
    void AddMeshCullResult(bool culled) { if (culled) m_meshesCulled++; else m_meshesVisible++; }
    uint32_t GetEntitiesVisible() const { return m_entitiesVisible; }
    uint32_t GetEntitiesCulled() const { return m_entitiesCulled; }
    uint32_t GetMeshesVisible() const { return m_meshesVisible; }
    uint32_t GetMeshesCulled() const { return m_meshesCulled; }

    // Memory query
    float GetSystemRAMUsageMB() const;

    // ---------------------------------------------------------------
    // CPU section timing (ResetPerFrameCounters clears this every frame)
    // ---------------------------------------------------------------
    void AddCPUTiming(const std::string& name, float ms)
    {
        // Same name used more than once in a frame (e.g. called per-camera) accumulates.
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
    // GPU section timing (D3D12 timestamp queries).
    // The queue may still be executing a scope's commands kFrameCount-1
    // frames after it was recorded, so results are read back once the
    // frame-in-flight slot is known to be safe to reuse (same rule the
    // rest of the frame-resource system already follows).
    // ---------------------------------------------------------------
    static constexpr uint32_t kMaxGPUScopesPerFrame = 32;

    void InitGPU(ID3D12Device* pDevice, ID3D12CommandQueue* pQueue, uint32_t frameCount);
    void ShutdownGPU();

    // Call once at the very start of the frame, before recording any GPU scopes.
    void BeginGPUFrame(ID3D12GraphicsCommandList* pCmdList);
    // Call once at the very end of the frame (still inside the same command list used above).
    void EndGPUFrame(ID3D12GraphicsCommandList* pCmdList);

    // Returns a handle to pass to EndGPUScope. Returns UINT32_MAX if GPU timing isn't
    // initialized or the per-frame scope budget (kMaxGPUScopesPerFrame) is exhausted.
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
    // Frame time history (ms), ring buffer for graphing.
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

    // Frustum culling stats
    uint32_t m_entitiesVisible = 0;
    uint32_t m_entitiesCulled = 0;
    uint32_t m_meshesVisible = 0;
    uint32_t m_meshesCulled = 0;

    // CPU timings
    std::unordered_map<std::string, float> m_cpuTimings;

    // GPU timings
    uint64_t m_gpuFrequency = 0;
    uint32_t m_gpuFrameCount = 0;
    uint32_t m_gpuCurrentSlot = 0;
    uint32_t m_gpuScopeCounter = 0;
    Microsoft::WRL::ComPtr<ID3D12QueryHeap> m_spQueryHeap;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_gpuReadbackBuffers; // one per frame-in-flight slot
    std::vector<std::vector<std::string>> m_gpuScopeNamesPerSlot;            // scope names recorded into each slot
    std::vector<uint32_t> m_gpuScopeCountPerSlot;                            // how many scopes were recorded into each slot
    std::unordered_map<std::string, float> m_gpuTimings;

    // Frame time history
    std::array<float, kFrameHistorySize> m_frameTimeHistory = {};
    int m_frameTimeHistoryIdx = 0;
};

// Usage: PROFILE_CPU_SCOPE("Shadow"); at the top of the block being timed.
#define PROFILE_CPU_SCOPE_CONCAT_INNER(a, b) a##b
#define PROFILE_CPU_SCOPE_CONCAT(a, b) PROFILE_CPU_SCOPE_CONCAT_INNER(a, b)
#define PROFILE_CPU_SCOPE(name) Profiler::ScopedCPUTimer PROFILE_CPU_SCOPE_CONCAT(cpuScope_, __LINE__)(name)

// Usage: PROFILE_GPU_SCOPE(cmdList, "Shadow"); at the top of the block being timed.
#define PROFILE_GPU_SCOPE(cmdList, name) Profiler::ScopedGPUTimer PROFILE_CPU_SCOPE_CONCAT(gpuScope_, __LINE__)(cmdList, name)
