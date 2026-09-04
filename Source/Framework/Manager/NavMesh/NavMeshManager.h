#pragma once

struct rcHeightfield;
struct rcCompactHeightfield;
struct rcContourSet;
struct rcPolyMesh;
struct rcPolyMeshDetail;
class dtNavMesh;
class dtNavMeshQuery;
class dtQueryFilter;

class ModelData;
class RoomArea;

struct NavMeshBakeSettings
{
    float cellSize       = 0.3f;
    float cellHeight     = 0.2f;

    float agentRadius    = 0.5f;
    float agentHeight    = 2.0f;

    float agentMaxClimb  = 0.4f;
    float agentMaxSlope  = 45.0f;

    bool useSeedPoint    = false;
    Math::Vector3 seedPoint = { 0.0f, 0.0f, 0.0f };

    bool isDirty         = false;
};

class NavMeshManager
{
public:
    static NavMeshManager& Instance();

    void Init();
    void Release();

    bool BuildNavMesh(
        std::shared_ptr<ModelData> stageModel,
        const Math::Matrix& worldTransform);

    bool BuildManualNavMesh(const std::vector<RoomArea*>& rooms);

    bool IsBuilt() const { return m_navMesh != nullptr; }

    NavMeshBakeSettings GetBakeSettings() const { return m_bakeSettings; }
    void SetBakeSettings(const NavMeshBakeSettings& settings) { m_bakeSettings = settings; }

    void SetDebugDrawEnabled(bool enabled) { m_debugDrawEnabled = enabled; }
    bool IsDebugDrawEnabled() const { return m_debugDrawEnabled; }
    void DrawDebugMesh();

    Math::Vector3 MoveToward(
        int entityID,
        const Math::Vector3& current,
        const Math::Vector3& target,
        float speed,
        float deltaTime,
        float pathUpdateInterval = 0.5f,
        float nodeReachThreshold = 0.5f);

    bool IsInRange(
        const Math::Vector3& from,
        const Math::Vector3& to,
        float radius) const;

    float GetPathLength(
        const Math::Vector3& start,
        const Math::Vector3& end);

    bool IsReachable(
        const Math::Vector3& start,
        const Math::Vector3& end);

    Math::Vector3 GetMoveDirection(
        int entityID,
        const Math::Vector3& current,
        const Math::Vector3& target,
        float pathUpdateInterval = 0.5f,
        float nodeReachThreshold = 0.5f);

    void ClearPath(int entityID);

    void ClearAllPaths();

    bool FindPath(
        const Math::Vector3& start,
        const Math::Vector3& end,
        std::vector<Math::Vector3>& outPath);

    // Peeks at the currently cached waypoint list for an entity (as last computed by MoveToward /
    // GetMoveDirection), for debug visualization. Returns nullptr if there's no cache entry yet.
    const std::vector<Math::Vector3>* GetCachedPath(int entityID) const;

    // MoveTowardがバックグラウンドでの経路再計算(JobSystem)を何回開始したか、プロセス
    // 生存期間中の合計。このNavMeshは小さいのでfindPathは1フレームより十分速く終わって
    // しまい、Statisticsウィンドウの「Active Jobs」が1になる瞬間を目で追うのは難しい -
    // このカウンタは非同期パスが実際に動いているかを確実に確認する手段。
    int GetAsyncRecomputeCount() const { return m_asyncRecomputeCount.load(); }

private:
    NavMeshManager();
    ~NavMeshManager();

    NavMeshBakeSettings m_bakeSettings;

    bool m_debugDrawEnabled = false;
    std::vector<std::vector<Math::Vector3>> m_manualPolygons;

    // あるエンティティのバックグラウンド経路再計算(MoveToward参照)が実行中の間だけ使う
    // 共有ステージング領域。PathCache/m_pathCache自体の寿命とは独立して、shared_ptrで
    // 生かし続ける。そのため、ジョブがまだ実行中の間にそのエンティティのキャッシュ
    // エントリが消えても(ClearPath、エンティティ破棄等)、JobSystemのワーカーは常に安全に
    // 書き込める - 結果が誰にも回収されず捨てられるだけで、解放済みメモリに触れることはない。
    struct AsyncPathResult
    {
        std::mutex mutex;
        bool ready = false;
        std::vector<Math::Vector3> waypoints;
    };

    struct PathCache
    {
        std::vector<Math::Vector3> waypoints;
        float timer = 0.0f;
        Math::Vector3 lastTarget = { 0, 0, 0 };
        bool hasTarget = false;

        // 非同期再計算の状態 - MoveToward参照。初回使用時に遅延生成される。
        std::shared_ptr<std::atomic<bool>> computing;
        std::shared_ptr<AsyncPathResult> pending;
    };
    std::unordered_map<int, PathCache> m_pathCache;

    // 全てのdtNavMeshQuery呼び出しを保護する(findNearestPoly/findPath/findStraightPathは
    // どれもクエリオブジェクトの内部作業バッファを書き換えるため、同じdtNavMeshQueryに
    // 複数スレッドから同時に呼び込むのは安全ではない)。FindPath()は今やJobSystemの
    // ワーカースレッドで実行される(MoveToward参照)一方、IsReachable()等は引き続き
    // メインスレッドから同時に呼ばれ得る - これによって、2つ目のクエリオブジェクトを
    // 持たなくても安全にしている。
    std::mutex m_navQueryMutex;

    std::atomic<int> m_asyncRecomputeCount{0};

    rcHeightfield*        m_solid   = nullptr;
    rcCompactHeightfield* m_chf     = nullptr;
    rcContourSet*         m_cset    = nullptr;
    rcPolyMesh*           m_pmesh   = nullptr;
    rcPolyMeshDetail*     m_dmesh   = nullptr;

    dtNavMesh*      m_navMesh  = nullptr;
    dtNavMeshQuery* m_navQuery = nullptr;
    dtQueryFilter*  m_filter   = nullptr;

    void CleanupRecast();

    Math::Vector3 AdvanceAlongPath(
        PathCache& cache,
        const Math::Vector3& current,
        float speed,
        float deltaTime,
        float nodeReachThreshold) const;
};