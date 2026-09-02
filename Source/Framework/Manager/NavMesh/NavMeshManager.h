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

    // How many times MoveToward has kicked off a background path recompute (JobSystem), total
    // for the process's lifetime. On a navmesh this small, findPath finishes in well under a
    // frame, so "Active Jobs" in the Statistics window flickering to 1 is easy to miss just by
    // eye - this counter is a way to actually confirm the async path is being exercised at all.
    int GetAsyncRecomputeCount() const { return m_asyncRecomputeCount.load(); }

private:
    NavMeshManager();
    ~NavMeshManager();

    NavMeshBakeSettings m_bakeSettings;

    bool m_debugDrawEnabled = false;
    std::vector<std::vector<Math::Vector3>> m_manualPolygons;

    // Shared staging area for one entity's in-flight background path recompute
    // (see MoveToward). Kept alive by shared_ptr independent of PathCache/m_pathCache's own
    // lifetime, so it's always safe for the JobSystem worker to write into even if the
    // entity's cache entry gets erased (ClearPath, entity destroyed) while the job is still
    // running - the result just ends up uncollected instead of touching freed memory.
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

        // Async recompute state - see MoveToward. Lazily created on first use.
        std::shared_ptr<std::atomic<bool>> computing;
        std::shared_ptr<AsyncPathResult> pending;
    };
    std::unordered_map<int, PathCache> m_pathCache;

    // Guards every dtNavMeshQuery call (findNearestPoly/findPath/findStraightPath all mutate
    // the query object's internal working buffers, so it's not safe to call into the same
    // dtNavMeshQuery from more than one thread at once). FindPath() now runs on a JobSystem
    // worker (see MoveToward), while IsReachable() etc. can still be called from the main
    // thread at the same time - this makes that safe without needing a second query object.
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