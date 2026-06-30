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

class NavMeshManager
{
public:
    static NavMeshManager& Instance();

    void Init();
    void Release();

    bool BuildNavMesh(
        std::shared_ptr<ModelData> stageModel,
        const Math::Matrix& worldTransform);

    bool IsBuilt() const { return m_navMesh != nullptr; }

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

private:
    NavMeshManager();
    ~NavMeshManager();

    struct PathCache
    {
        std::vector<Math::Vector3> waypoints;
        float timer = 0.0f;
    };
    std::unordered_map<int, PathCache> m_pathCache;

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