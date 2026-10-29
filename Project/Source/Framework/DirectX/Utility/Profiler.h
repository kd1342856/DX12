#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <deque>
#include <array>
#include <atomic>
#include <mutex>
#include <chrono>
#include <unordered_map>
#include <wrl/client.h>

// TRACY_ENABLE をプロジェクトの定義から外すと、Tracyの呼び出しはすべて消える
#ifdef TRACY_ENABLE
#include "Tracy/tracy/Tracy.hpp"
#include "Tracy/tracy/TracyD3D12.hpp"
#endif

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
        // clearは確保済みの領域を残すので、毎フレームのメモリ確保は起きない
        m_cpuTimings.clear();
        m_cpuFrameStamp++;
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
    // 区間名の登録
    // 区間名は初めて出てきたときに1回だけ登録し、以後は登録した const char* で扱う
    // (毎フレームの std::string の作成やmapへの挿入をしない)。同じ文字列の名前は
    // 1つにまとまるので、CPUとGPUの計測結果はポインタの比較で対応が取れる。
    // ---------------------------------------------------------------
    struct ScopeName
    {
        const char* pName = nullptr;  // 登録した名前。終了まで有効
        std::string ownedName;        // 実行時に決まる名前のときだけ、ここに写して持つ
        uint32_t cpuFrameStamp = 0;   // 最後にCPU計測を足したフレーム(m_cpuFrameStamp)
        uint32_t cpuTimingIndex = 0;  // そのフレームの m_cpuTimings の中での位置
        uint32_t gpuFrameStamp = 0;   // 最後にGPU計測を足したフレーム(m_gpuFrameStamp)
        uint32_t gpuTimingIndex = 0;  // そのフレームの m_gpuTimings の中での位置
#ifdef TRACY_ENABLE
        tracy::SourceLocationData srcloc = {};  // 実行時に決まる名前のとき、Tracyに渡す場所の情報
#endif
    };

    // PROFILE_*_SCOPE を書いた場所1つぶん。マクロの中で static に作る。
    struct ScopeSite
    {
        const char* pFile = nullptr;
        const char* pFunction = nullptr;
        uint32_t line = 0;
        std::atomic<ScopeName*> pLiteralName = nullptr;  // 文字列リテラルの名前を最初に登録した結果
#ifdef TRACY_ENABLE
        tracy::SourceLocationData srcloc = {};  // 文字列リテラルの名前のとき、Tracyに渡す場所の情報
#endif
    };

    // 計測を始めるときに渡す、登録済みの名前とTracyの場所の情報
    struct ScopeRef
    {
        ScopeName* pName = nullptr;
#ifdef TRACY_ENABLE
        const tracy::SourceLocationData* pSrcloc = nullptr;
#endif
    };

    // 文字列リテラルの名前。最初の1回だけ登録し、以後はその場所に覚えた結果を使う。
    // 書き換わる char 配列を渡すと最初の中身のまま残るので、その場合は std::string にすること。
    template <size_t N>
    static ScopeRef ResolveScope(ScopeSite& site, const char (&name)[N])
    {
        ScopeName* pName = site.pLiteralName.load(std::memory_order_acquire);
        if (!pName)
        {
            pName = &Instance().RegisterLiteralName(site, name);
        }
        ScopeRef ref;
        ref.pName = pName;
#ifdef TRACY_ENABLE
        ref.pSrcloc = &site.srcloc;
#endif
        return ref;
    }

    // 実行時に決まる名前(RenderGraphのパス名、スクリプトの型名など)。呼ぶたびに名前で引く。
    // 新しい名前のときだけ写しを作る。
    static ScopeRef ResolveScope(ScopeSite& site, std::string_view name)
    {
        ScopeName& scopeName = Instance().FindOrAddName(site, name, true);
        ScopeRef ref;
        ref.pName = &scopeName;
#ifdef TRACY_ENABLE
        ref.pSrcloc = &scopeName.srcloc;
#endif
        return ref;
    }

    // 1フレームぶんの区間の計測結果(同じ名前は合計)。並びは、そのフレームで最初に出てきた順。
    struct ScopeTiming
    {
        const char* pName;
        float ms;
    };

    // ---------------------------------------------------------------
    // CPU区間計測（ResetPerFrameCountersで毎フレームクリアされる）
    // メインスレッドの区間だけを集計する。ほかのスレッドの区間はTracyにだけ送る。
    // ---------------------------------------------------------------
    void AddCPUTiming(ScopeName& name, float ms);
    const std::vector<ScopeTiming>& GetCPUTimings() const { return m_cpuTimings; }

    class ScopedCPUTimer
    {
    public:
        explicit ScopedCPUTimer(const ScopeRef& scope)
            : m_pName(scope.pName)
#ifdef TRACY_ENABLE
            , m_tracyZone(scope.pSrcloc)
#endif
            , m_start(std::chrono::high_resolution_clock::now())
        {
        }
        ~ScopedCPUTimer()
        {
            auto end = std::chrono::high_resolution_clock::now();
            float ms = std::chrono::duration<float, std::milli>(end - m_start).count();
            Profiler::Instance().AddCPUTiming(*m_pName, ms);
        }
        ScopedCPUTimer(const ScopedCPUTimer&) = delete;
        ScopedCPUTimer& operator=(const ScopedCPUTimer&) = delete;
    private:
        ScopeName* m_pName;
#ifdef TRACY_ENABLE
        tracy::ScopedZone m_tracyZone;
#endif
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
    uint32_t BeginGPUScope(ID3D12GraphicsCommandList* pCmdList, ScopeName& name);
    void EndGPUScope(ID3D12GraphicsCommandList* pCmdList, uint32_t scopeHandle);

    // 最後に読み戻せたフレームの結果(同じ名前は合計)
    const std::vector<ScopeTiming>& GetGPUTimings() const { return m_gpuTimings; }

    class ScopedGPUTimer
    {
    public:
        ScopedGPUTimer(ID3D12GraphicsCommandList* pCmdList, const ScopeRef& scope)
            : m_pCmdList(pCmdList)
#ifdef TRACY_ENABLE
            , m_tracyZone(Profiler::Instance().m_pTracyGPUContext, pCmdList, scope.pSrcloc,
                  Profiler::Instance().m_pTracyGPUContext != nullptr && pCmdList != nullptr)
#endif
            , m_handle(Profiler::Instance().BeginGPUScope(pCmdList, *scope.pName))
        {
        }
        ~ScopedGPUTimer()
        {
            Profiler::Instance().EndGPUScope(m_pCmdList, m_handle);
        }
        ScopedGPUTimer(const ScopedGPUTimer&) = delete;
        ScopedGPUTimer& operator=(const ScopedGPUTimer&) = delete;
    private:
        ID3D12GraphicsCommandList* m_pCmdList;
#ifdef TRACY_ENABLE
        tracy::D3D12ZoneScope m_tracyZone;
#endif
        uint32_t m_handle;
    };

    // ---------------------------------------------------------------
    // フレーム時間の履歴(ms)、グラフ表示用のリングバッファ。
    // ---------------------------------------------------------------
    static constexpr int kFrameHistorySize = 120;

    void AddFrameTime(float ms);
    const std::array<float, kFrameHistorySize>& GetFrameTimeHistory() const { return m_frameTimeHistory; }
    int GetFrameTimeHistoryIndex() const { return m_frameTimeHistoryIdx; }

    // フレームの先頭で1回呼ぶ。前フレームの所要時間を記録し、平均より極端に遅かったら
    // そのフレームのCPU区間計測をログに出してから、毎フレームのカウンタをリセットする。
    void BeginCPUFrame();

private:
    void ReportFrameSpike(float frameMs) const;

    Profiler() = default;
    ~Profiler() = default;

    void ReadGPUResultsForSlot(uint32_t slot);

    ScopeName& RegisterLiteralName(ScopeSite& site, const char* pName);
    // copyName が false のときは pName をそのまま持つ(文字列リテラル用)
    ScopeName& FindOrAddName(const ScopeSite& site, std::string_view name, bool copyName);

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

    // 区間名の登録先。dequeは追加しても既存の要素が動かないので、要素へのポインタを持ち続けてよい
    std::mutex m_scopeNameMutex;
    std::deque<ScopeName> m_scopeNames;
    std::unordered_map<std::string_view, ScopeName*> m_scopeNameLookup;  // キーは各要素の pName を指す

    // CPU計測
    std::vector<ScopeTiming> m_cpuTimings;
    uint32_t m_cpuFrameStamp = 1;

    // GPU計測
    uint64_t m_gpuFrequency = 0;
    uint32_t m_gpuFrameCount = 0;
    uint32_t m_gpuCurrentSlot = 0;
    uint32_t m_gpuScopeCounter = 0;
    Microsoft::WRL::ComPtr<ID3D12QueryHeap> m_spQueryHeap;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_gpuReadbackBuffers; // フレームインフライトのスロットごとに1つ
    std::vector<std::vector<ScopeName*>> m_gpuScopeNamesPerSlot;            // 各スロットに記録されたスコープ名
    std::vector<uint32_t> m_gpuScopeCountPerSlot;                            // 各スロットに記録されたスコープ数
    std::vector<ScopeTiming> m_gpuTimings;
    uint32_t m_gpuFrameStamp = 1;

#ifdef TRACY_ENABLE
    // TracyのGPU計測(グラフィックスキュー)。自前のタイムスタンプとは別のクエリヒープを使う
    tracy::D3D12QueueCtx* m_pTracyGPUContext = nullptr;
#endif

    // フレーム時間の履歴
    std::array<float, kFrameHistorySize> m_frameTimeHistory = {};
    int m_frameTimeHistoryIdx = 0;

    // スパイク検出用の平均フレーム時間(ms)と、平均が安定するまでの待ちフレーム数
    float m_avgFrameMs = 0.0f;
    int m_spikeWarmupFrames = 120;
};

// 使い方: 計測したいブロックの先頭で PROFILE_CPU_SCOPE("Shadow"); のように呼ぶ。
// 名前は文字列リテラルのほか、std::string や const char* (実行時に決まる名前)も渡せる。
// 書いた場所ごとに static な ScopeSite を作るので、if の後ろなどに括弧なしで書かないこと。
#define PROFILE_CPU_SCOPE_CONCAT_INNER(a, b) a##b
#define PROFILE_CPU_SCOPE_CONCAT(a, b) PROFILE_CPU_SCOPE_CONCAT_INNER(a, b)
#define PROFILE_SCOPE_SITE(var) \
    static Profiler::ScopeSite var { __FILE__, __FUNCTION__, static_cast<uint32_t>(__LINE__) }
#define PROFILE_CPU_SCOPE(name) \
    PROFILE_SCOPE_SITE(PROFILE_CPU_SCOPE_CONCAT(cpuScopeSite_, __LINE__)); \
    Profiler::ScopedCPUTimer PROFILE_CPU_SCOPE_CONCAT(cpuScope_, __LINE__)( \
        Profiler::ResolveScope(PROFILE_CPU_SCOPE_CONCAT(cpuScopeSite_, __LINE__), name))

// 使い方: 計測したいブロックの先頭で PROFILE_GPU_SCOPE(cmdList, "Shadow"); のように呼ぶ。
#define PROFILE_GPU_SCOPE(cmdList, name) \
    PROFILE_SCOPE_SITE(PROFILE_CPU_SCOPE_CONCAT(gpuScopeSite_, __LINE__)); \
    Profiler::ScopedGPUTimer PROFILE_CPU_SCOPE_CONCAT(gpuScope_, __LINE__)( \
        cmdList, Profiler::ResolveScope(PROFILE_CPU_SCOPE_CONCAT(gpuScopeSite_, __LINE__), name))
