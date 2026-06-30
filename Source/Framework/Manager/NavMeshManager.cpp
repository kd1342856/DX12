#include "NavMeshManager.h"
#include "Recast/Recast.h"
#include "Detour/DetourNavMesh.h"
#include "Detour/DetourNavMeshBuilder.h"
#include "Detour/DetourNavMeshQuery.h"

NavMeshManager& NavMeshManager::Instance()
{
    static NavMeshManager instance;
    return instance;
}

NavMeshManager::NavMeshManager()
{
    m_filter = new dtQueryFilter();
    m_filter->setIncludeFlags(0xFFFF);
    m_filter->setExcludeFlags(0);
}

NavMeshManager::~NavMeshManager()
{
    Release();
    delete m_filter;
    m_filter = nullptr;
}

void NavMeshManager::Init()
{
}

void NavMeshManager::Release()
{
    CleanupRecast();
    m_pathCache.clear();
}

void NavMeshManager::CleanupRecast()
{
    if (m_solid)    { rcFreeHeightField(m_solid);            m_solid    = nullptr; }
    if (m_chf)      { rcFreeCompactHeightfield(m_chf);       m_chf      = nullptr; }
    if (m_cset)     { rcFreeContourSet(m_cset);              m_cset     = nullptr; }
    if (m_pmesh)    { rcFreePolyMesh(m_pmesh);               m_pmesh    = nullptr; }
    if (m_dmesh)    { rcFreePolyMeshDetail(m_dmesh);         m_dmesh    = nullptr; }
    if (m_navMesh)  { dtFreeNavMesh(m_navMesh);              m_navMesh  = nullptr; }
    if (m_navQuery) { dtFreeNavMeshQuery(m_navQuery);        m_navQuery = nullptr; }
}

bool NavMeshManager::BuildNavMesh(std::shared_ptr<ModelData> stageModel, const Math::Matrix& worldTransform)
{
    if (!stageModel) return false;

    CleanupRecast();
    m_pathCache.clear();

    std::vector<float> verts;
    std::vector<int>   tris;

    for (const auto& node : stageModel->GetNodes())
    {
        for (const auto& mesh : node.meshes)
        {
            const auto& meshVerts = mesh->GetVertices();
            const auto& meshFaces = mesh->GetFaces();

            int baseVertex = (int)(verts.size() / 3);

            for (const auto& v : meshVerts)
            {
                Math::Vector3 pos = Math::Vector3::Transform(v.Position, worldTransform);
                verts.push_back(pos.x);
                verts.push_back(pos.y);
                verts.push_back(pos.z);
            }

            for (const auto& f : meshFaces)
            {
                tris.push_back(baseVertex + f.Idx[0]);
                tris.push_back(baseVertex + f.Idx[1]);
                tris.push_back(baseVertex + f.Idx[2]);
            }
        }
    }

    if (verts.empty() || tris.empty())
    {
        Logger::Instance().AddLog(Logger::LogLevel::Error, "NavMesh build failed: no geometry data.");
        return false;
    }

    int nverts = (int)(verts.size() / 3);
    int ntris  = (int)(tris.size()  / 3);

    rcConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.cs                   = 0.3f;
    cfg.ch                   = 0.2f;
    cfg.walkableSlopeAngle   = 45.0f;
    cfg.walkableHeight       = (int)ceilf (2.0f  / cfg.ch);
    cfg.walkableClimb        = (int)floorf(0.9f  / cfg.ch);
    cfg.walkableRadius       = (int)ceilf (0.6f  / cfg.cs);
    cfg.maxEdgeLen           = (int)(12.0f / cfg.cs);
    cfg.maxSimplificationError = 1.3f;
    cfg.minRegionArea        = (int)rcSqr(8.0f);
    cfg.mergeRegionArea      = (int)rcSqr(20.0f);
    cfg.maxVertsPerPoly      = 6;
    cfg.detailSampleDist     = 6.0f;
    cfg.detailSampleMaxError = 1.0f;

    rcCalcBounds  (verts.data(), nverts, cfg.bmin, cfg.bmax);
    rcCalcGridSize(cfg.bmin, cfg.bmax, cfg.cs, &cfg.width, &cfg.height);

    rcContext ctx;

    m_solid = rcAllocHeightfield();
    if (!rcCreateHeightfield(&ctx, *m_solid, cfg.width, cfg.height, cfg.bmin, cfg.bmax, cfg.cs, cfg.ch))
    {
        Logger::Instance().AddLog(Logger::LogLevel::Error, "NavMesh: rcCreateHeightfield failed.");
        return false;
    }

    std::vector<unsigned char> triareas(ntris, 0);
    rcMarkWalkableTriangles(&ctx, cfg.walkableSlopeAngle, verts.data(), nverts, tris.data(), ntris, triareas.data());
    if (!rcRasterizeTriangles(&ctx, verts.data(), nverts, tris.data(), triareas.data(), ntris, *m_solid, cfg.walkableClimb))
    {
        Logger::Instance().AddLog(Logger::LogLevel::Error, "NavMesh: rcRasterizeTriangles failed.");
        return false;
    }

    rcFilterLowHangingWalkableObstacles(&ctx, cfg.walkableClimb, *m_solid);
    rcFilterLedgeSpans                 (&ctx, cfg.walkableHeight, cfg.walkableClimb, *m_solid);
    rcFilterWalkableLowHeightSpans     (&ctx, cfg.walkableHeight, *m_solid);

    m_chf = rcAllocCompactHeightfield();
    if (!rcBuildCompactHeightfield(&ctx, cfg.walkableHeight, cfg.walkableClimb, *m_solid, *m_chf)) return false;
    if (!rcErodeWalkableArea      (&ctx, cfg.walkableRadius, *m_chf))                              return false;
    if (!rcBuildDistanceField     (&ctx, *m_chf))                                                  return false;
    if (!rcBuildRegions           (&ctx, *m_chf, cfg.borderSize, cfg.minRegionArea, cfg.mergeRegionArea)) return false;

    m_cset = rcAllocContourSet();
    if (!rcBuildContours    (&ctx, *m_chf, cfg.maxSimplificationError, cfg.maxEdgeLen, *m_cset)) return false;

    m_pmesh = rcAllocPolyMesh();
    if (!rcBuildPolyMesh    (&ctx, *m_cset, cfg.maxVertsPerPoly, *m_pmesh)) return false;

    m_dmesh = rcAllocPolyMeshDetail();
    if (!rcBuildPolyMeshDetail(&ctx, *m_pmesh, *m_chf, cfg.detailSampleDist, cfg.detailSampleMaxError, *m_dmesh)) return false;

    for (int i = 0; i < m_pmesh->npolys; ++i)
    {
        if (m_pmesh->areas[i] == RC_WALKABLE_AREA)
        {
            m_pmesh->areas[i] = 0;
            m_pmesh->flags[i] = 1;
        }
    }

    dtNavMeshCreateParams params;
    memset(&params, 0, sizeof(params));
    params.verts           = m_pmesh->verts;
    params.vertCount       = m_pmesh->nverts;
    params.polys           = m_pmesh->polys;
    params.polyAreas       = m_pmesh->areas;
    params.polyFlags       = m_pmesh->flags;
    params.polyCount       = m_pmesh->npolys;
    params.nvp             = m_pmesh->nvp;
    params.detailMeshes    = m_dmesh->meshes;
    params.detailVerts     = m_dmesh->verts;
    params.detailVertsCount= m_dmesh->nverts;
    params.detailTris      = m_dmesh->tris;
    params.detailTriCount  = m_dmesh->ntris;
    params.walkableHeight  = 2.0f;
    params.walkableRadius  = 0.6f;
    params.walkableClimb   = 0.9f;
    params.bmin[0] = m_pmesh->bmin[0]; params.bmin[1] = m_pmesh->bmin[1]; params.bmin[2] = m_pmesh->bmin[2];
    params.bmax[0] = m_pmesh->bmax[0]; params.bmax[1] = m_pmesh->bmax[1]; params.bmax[2] = m_pmesh->bmax[2];
    params.cs = cfg.cs;
    params.ch = cfg.ch;

    unsigned char* navData     = 0;
    int            navDataSize = 0;
    if (!dtCreateNavMeshData(&params, &navData, &navDataSize))
    {
        Logger::Instance().AddLog(Logger::LogLevel::Error, "NavMesh: dtCreateNavMeshData failed.");
        return false;
    }

    m_navMesh = dtAllocNavMesh();
    if (dtStatusFailed(m_navMesh->init(navData, navDataSize, DT_TILE_FREE_DATA)))
    {
        dtFree(navData);
        Logger::Instance().AddLog(Logger::LogLevel::Error, "NavMesh: dtNavMesh::init failed.");
        return false;
    }

    m_navQuery = dtAllocNavMeshQuery();
    m_navQuery->init(m_navMesh, 2048);

    Logger::Instance().AddLog(Logger::LogLevel::Info, "NavMesh built successfully! Polys: %d", m_pmesh->npolys);
    return true;
}

bool NavMeshManager::FindPath(
    const Math::Vector3& start,
    const Math::Vector3& end,
    std::vector<Math::Vector3>& outPath)
{
    outPath.clear();
    if (!m_navQuery || !m_navMesh) return false;

    float extents[3] = { 2.0f, 4.0f, 2.0f };

    dtPolyRef startRef = 0, endRef = 0;
    float     startPt[3], endPt[3];

    m_navQuery->findNearestPoly(&start.x, extents, m_filter, &startRef, startPt);
    m_navQuery->findNearestPoly(&end.x,   extents, m_filter, &endRef,   endPt);

    if (!startRef || !endRef) return false;

    static const int MAX_POLYS = 256;
    dtPolyRef path[MAX_POLYS];
    int       pathCount = 0;
    m_navQuery->findPath(startRef, endRef, startPt, endPt, m_filter, path, &pathCount, MAX_POLYS);

    if (pathCount <= 0) return false;

    float         straightPath[MAX_POLYS * 3];
    unsigned char straightPathFlags[MAX_POLYS];
    dtPolyRef     straightPathPolys[MAX_POLYS];
    int           straightPathCount = 0;

    m_navQuery->findStraightPath(
        startPt, endPt,
        path, pathCount,
        straightPath, straightPathFlags, straightPathPolys,
        &straightPathCount, MAX_POLYS);

    for (int i = 0; i < straightPathCount; ++i)
    {
        outPath.emplace_back(
            straightPath[i * 3],
            straightPath[i * 3 + 1],
            straightPath[i * 3 + 2]);
    }
    return !outPath.empty();
}

Math::Vector3 NavMeshManager::MoveToward(
    int entityID,
    const Math::Vector3& current,
    const Math::Vector3& target,
    float speed,
    float deltaTime,
    float pathUpdateInterval,
    float nodeReachThreshold)
{
    PathCache& cache = m_pathCache[entityID];

    cache.timer -= deltaTime;
    if (cache.timer <= 0.0f || cache.waypoints.empty())
    {
        FindPath(current, target, cache.waypoints);
        cache.timer = pathUpdateInterval;
    }

    return AdvanceAlongPath(cache, current, speed, deltaTime, nodeReachThreshold);
}

Math::Vector3 NavMeshManager::GetMoveDirection(
    int entityID,
    const Math::Vector3& current,
    const Math::Vector3& target,
    float pathUpdateInterval,
    float nodeReachThreshold)
{
    PathCache& cache = m_pathCache[entityID];

    if (cache.waypoints.empty())
    {
        FindPath(current, target, cache.waypoints);
        cache.timer = pathUpdateInterval;
    }

    while (!cache.waypoints.empty())
    {
        Math::Vector3 toNode = cache.waypoints[0] - current;
        toNode.y = 0;
        if (toNode.Length() < nodeReachThreshold)
            cache.waypoints.erase(cache.waypoints.begin());
        else
            break;
    }

    if (!cache.waypoints.empty())
    {
        Math::Vector3 dir = cache.waypoints[0] - current;
        dir.y = 0;
        if (dir.LengthSquared() > 0.0f)
        {
            dir.Normalize();
            return dir;
        }
    }

    Math::Vector3 dir = target - current;
    dir.y = 0;
    if (dir.LengthSquared() > 0.0f) dir.Normalize();
    return dir;
}

bool NavMeshManager::IsInRange(const Math::Vector3& from, const Math::Vector3& to, float radius) const
{
    float dx = from.x - to.x;
    float dz = from.z - to.z;
    return (dx * dx + dz * dz) <= (radius * radius);
}

float NavMeshManager::GetPathLength(const Math::Vector3& start, const Math::Vector3& end)
{
    std::vector<Math::Vector3> path;
    if (!FindPath(start, end, path)) return -1.0f;

    float length = 0.0f;
    for (int i = 1; i < (int)path.size(); ++i)
        length += Math::Vector3::Distance(path[i - 1], path[i]);

    return length;
}

bool NavMeshManager::IsReachable(const Math::Vector3& start, const Math::Vector3& end)
{
    std::vector<Math::Vector3> path;
    return FindPath(start, end, path);
}

void NavMeshManager::ClearPath(int entityID)
{
    m_pathCache.erase(entityID);
}

void NavMeshManager::ClearAllPaths()
{
    m_pathCache.clear();
}

Math::Vector3 NavMeshManager::AdvanceAlongPath(
    PathCache& cache,
    const Math::Vector3& current,
    float speed,
    float deltaTime,
    float nodeReachThreshold) const
{
    if (cache.waypoints.empty()) return current;

    Math::Vector3 pos = current;
    float distLeft = speed * deltaTime;

    while (distLeft > 0.0f && !cache.waypoints.empty())
    {
        Math::Vector3 toNode = cache.waypoints[0] - pos;
        toNode.y = 0;

        float dist = toNode.Length();

        if (dist <= nodeReachThreshold)
        {
            cache.waypoints.erase(cache.waypoints.begin());
            continue;
        }

        if (distLeft >= dist)
        {
            pos += toNode * (1.0f / dist) * dist;
            distLeft -= dist;
            cache.waypoints.erase(cache.waypoints.begin());
        }
        else
        {
            toNode.Normalize();
            pos += toNode * distLeft;
            distLeft = 0.0f;
        }
    }

    return pos;
}
