#include "../../../Pch.h"
#include "NavMeshManager.h"
#include "../Asset/MeshManager.h"
#include "../../../Application/Object/Script/System/RoomArea.h"
#include "../../Object/GameObject.h"
#include "../../System/JobSystem/JobSystem.h"
#include "../Collision/CollisionManager.h"
#include "../Scene/Scene.h"
#include "../../ImGuiEditor/Editor/Editor.h"
#include <functional>
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
    m_manualPolygons.clear();
}

bool NavMeshManager::BuildNavMesh(std::shared_ptr<ModelData> stageModel, const Math::Matrix& worldTransform)
{
    if (!stageModel) return false;

    CleanupRecast();
    m_pathCache.clear();

    // Door leaves are baked into the source mesh in their closed pose at build time
    // (door open/close is driven by an animation, not yet reflected here). Excluding
    // them keeps every doorway walkable in the NavMesh regardless of the door's visual
    // state, so ghosts aren't blocked from leaving a room just because a door is shut.
    // Matches how Player::TryInteractDoor() identifies door nodes (by animation name).
    std::set<std::string> doorNodeNames;
    for (const auto& anim : stageModel->GetAnimations())
    {
        if (anim.name.find("Door") == std::string::npos && anim.name.find("door") == std::string::npos)
            continue;
        for (const auto& channel : anim.channels)
        {
            doorNodeNames.insert(channel.nodeName);
        }
    }

    // The house model also carries a single giant flat "Ground" plane (a -30..30m quad sitting
    // right around the same height as the room floors) and a "Roof_Main" mesh. Neither is meant
    // to be walkable interior floor - including a room-scale flat surface stacked directly under
    // every room's floor confuses Recast's region/contour building right where it matters most
    // (room-to-room doorways), so keep them out of the source geometry entirely.
    auto isEnvironmentNode = [](const std::string& name) {
        return name == "Ground" || name.find("Roof") != std::string::npos;
    };

    // The house also has several "Ground_Band_*" objects - thin foundation/skirting strips that
    // trace along every wall (both exterior AND interior partition walls). Their flat top sits
    // ~0.3m above the floor, well within walkableClimb, so Recast happily lets the pathfinder
    // climb onto them - but the strip is only ~0.4m wide and doesn't itself connect anywhere
    // useful, so it becomes a dead-end shelf sitting right across every doorway. All static
    // geometry loses its individual node name during import (merged into "$MergedNode_N" blobs
    // for rendering), so by-name exclusion (like the door leaves above) isn't possible here.
    // Instead, drop any triangle that lies entirely within the known height band of that shelf
    // top; real floors sit at/near height 0 and are unaffected.
    // Widened + loosened from an earlier "all 3 vertices in [0.2,0.4]" version, which only
    // matched 26 faces and had zero effect on the specific doorway that was tested - the shelf
    // triangles right at a doorway are welded to neighboring wall/floor geometry, so many of them
    // have one vertex down near floor height (0) instead of all 3 sitting neatly on the shelf top.
    // This version only requires the triangle's lowest vertex to already be above ankle height
    // (still comfortably below the shelf top) and its highest vertex to stay under typical
    // furniture height, plus a near-horizontal face normal so real (steep) wall faces are left
    // alone regardless of height.
    constexpr float kShelfMinFloorClearance = 0.12f;
    constexpr float kShelfMaxHeight         = 0.5f;
    constexpr float kShelfMinUpDot          = 0.5f; // ~60 degrees from vertical, matches walkableSlopeAngle
    auto isShelfLikeFace = [&](const Math::Vector3& a, const Math::Vector3& b, const Math::Vector3& c) {
        float minY = std::min({ a.y, b.y, c.y });
        float maxY = std::max({ a.y, b.y, c.y });
        if (minY < kShelfMinFloorClearance || maxY > kShelfMaxHeight) return false;

        Math::Vector3 edge1 = b - a;
        Math::Vector3 edge2 = c - a;
        Math::Vector3 normal;
        edge1.Cross(edge2, normal);
        if (normal.LengthSquared() < 0.0000001f) return false;
        normal.Normalize();
        return fabsf(normal.y) >= kShelfMinUpDot;
    };

    std::vector<float> verts;
    std::vector<int>   tris;
    int skippedDoorNodes = 0;
    int skippedEnvNodes = 0;
    int skippedShelfFaces = 0;

    // one-time dump of every node name actually present in the loaded model, to compare
    // against the source .blend object names when the by-name exclusion filters don't match
    {
        std::string allNames;
        for (const auto& node : stageModel->GetNodes()) {
            if (!allNames.empty()) allNames += " | ";
            allNames += node.name;
        }
        Logger::Instance().AddLog(Logger::LogLevel::Info, "NavMesh: model node names = [%s]", allNames.c_str());
    }

    for (const auto& node : stageModel->GetNodes())
    {
        if (doorNodeNames.count(node.name))
        {
            ++skippedDoorNodes;
            continue;
        }
        if (isEnvironmentNode(node.name))
        {
            ++skippedEnvNodes;
            continue;
        }

        for (const auto& meshHandle : node.meshes)
        {
            Mesh* mesh = MeshManager::Instance().Get(meshHandle);
            if (!mesh) continue;

            const auto& meshVerts = mesh->GetVertices();
            const auto& meshFaces = mesh->GetFaces();

            int baseVertex = (int)(verts.size() / 3);

            std::vector<Math::Vector3> worldPositions;
            worldPositions.reserve(meshVerts.size());
            for (const auto& v : meshVerts)
            {
                Math::Vector3 pos = Math::Vector3::Transform(v.Position, worldTransform);
                worldPositions.push_back(pos);
                verts.push_back(pos.x);
                verts.push_back(pos.y);
                verts.push_back(pos.z);
            }

            for (const auto& f : meshFaces)
            {
                const Math::Vector3& p0 = worldPositions[f.Idx[0]];
                const Math::Vector3& p1 = worldPositions[f.Idx[1]];
                const Math::Vector3& p2 = worldPositions[f.Idx[2]];
                if (isShelfLikeFace(p0, p1, p2))
                {
                    ++skippedShelfFaces;
                    continue;
                }

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

    cfg.cs                   = m_bakeSettings.cellSize;
    cfg.ch                   = m_bakeSettings.cellHeight;
    cfg.walkableSlopeAngle   = m_bakeSettings.agentMaxSlope;
    cfg.walkableHeight       = (int)ceilf (m_bakeSettings.agentHeight / cfg.ch);
    cfg.walkableClimb        = (int)floorf(m_bakeSettings.agentMaxClimb / cfg.ch);
    cfg.walkableRadius       = (int)ceilf (m_bakeSettings.agentRadius / cfg.cs);
    cfg.maxEdgeLen           = (int)(12.0f / cfg.cs);
    cfg.maxSimplificationError = 0.2f;
    cfg.minRegionArea        = (int)rcSqr(2.0f);
    cfg.mergeRegionArea      = (int)rcSqr(6.0f);
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

    if (m_bakeSettings.useSeedPoint)
    {
        int bestPoly = -1;
        float bestDistSq = FLT_MAX;
        
        for (int i = 0; i < m_pmesh->npolys; ++i)
        {
            const unsigned short* p = &m_pmesh->polys[i * m_pmesh->nvp * 2];
            Math::Vector3 centroid = {0,0,0};
            int vCount = 0;
            for (int j = 0; j < m_pmesh->nvp; ++j)
            {
                if (p[j] == RC_MESH_NULL_IDX) break;
                const unsigned short* v = &m_pmesh->verts[p[j] * 3];
                centroid += Math::Vector3(
                    m_pmesh->bmin[0] + v[0] * m_pmesh->cs,
                    m_pmesh->bmin[1] + v[1] * m_pmesh->ch,
                    m_pmesh->bmin[2] + v[2] * m_pmesh->cs);
                vCount++;
            }
            if (vCount > 0)
            {
                centroid /= (float)vCount;
                float distSq = (centroid - m_bakeSettings.seedPoint).LengthSquared();
                if (distSq < bestDistSq)
                {
                    bestDistSq = distSq;
                    bestPoly = i;
                }
            }
        }
        
        if (bestPoly != -1)
        {
            std::vector<bool> visited(m_pmesh->npolys, false);
            std::vector<int> queue;
            queue.push_back(bestPoly);
            visited[bestPoly] = true;
            
            while (!queue.empty())
            {
                int curr = queue.back();
                queue.pop_back();
                
                const unsigned short* p = &m_pmesh->polys[curr * m_pmesh->nvp * 2];
                for (int j = 0; j < m_pmesh->nvp; ++j)
                {
                    if (p[j] == RC_MESH_NULL_IDX) break;
                    
                    int neighbor = p[m_pmesh->nvp + j];
                    if (neighbor != RC_MESH_NULL_IDX && neighbor < m_pmesh->npolys)
                    {
                        if (!visited[neighbor])
                        {
                            visited[neighbor] = true;
                            queue.push_back(neighbor);
                        }
                    }
                }
            }
            
            int disabledCount = 0;
            for (int i = 0; i < m_pmesh->npolys; ++i)
            {
                if (!visited[i])
                {
                    m_pmesh->flags[i] = 0; // Filter out this polygon
                    disabledCount++;
                }
            }
            Logger::Instance().AddLog(Logger::LogLevel::Info, "NavMesh Seed Filtering: disabled %d unreachable polys out of %d", disabledCount, m_pmesh->npolys);
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
    params.walkableHeight  = m_bakeSettings.agentHeight;
    params.walkableRadius  = m_bakeSettings.agentRadius;
    params.walkableClimb   = m_bakeSettings.agentMaxClimb;
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

    Logger::Instance().AddLog(Logger::LogLevel::Info,
        "NavMesh built successfully! Polys: %d (skipped %d door node(s), %d env node(s), %d shelf-band face(s)) verts=%d tris=%d bounds=(%.1f,%.1f,%.1f)-(%.1f,%.1f,%.1f)",
        m_pmesh->npolys, skippedDoorNodes, skippedEnvNodes, skippedShelfFaces, nverts, ntris,
        cfg.bmin[0], cfg.bmin[1], cfg.bmin[2], cfg.bmax[0], cfg.bmax[1], cfg.bmax[2]);
    return true;
}

bool NavMeshManager::IsStageCollisionReady()
{
    auto scene = Editor::GetScene();
    if (!scene) return false;
    auto& ecs = GameManager::Instance().GetECS();
    bool foundAny = false;
    bool allReady = true;
    std::function<void(const std::shared_ptr<GameObject>&)> check = [&](const std::shared_ptr<GameObject>& obj) {
        if (auto* pModel = ecs.TryGetComponent<ModelRenderData>(obj->GetEntityID())) {
            if (!pModel->m_filePath.empty()) {
                foundAny = true;
                DirectX::BoundingBox bounds;
                // TryGetLocalBoundsは全メッシュのGPU/CPUデータが揃っていない間はfalseを返す
                if (!pModel->m_spModelData || !pModel->m_spModelData->IsLoaded() ||
                    !pModel->m_spModelData->TryGetLocalBounds(bounds)) {
                    allReady = false;
                }
            }
        }
        for (auto& child : obj->GetChildren()) check(child);
    };
    for (auto& obj : scene->GetGameObjects()) {
        if (obj && obj->GetName() == "Stage") check(obj);
    }
    return foundAny && allReady;
}

// 家のモデル内のドア(名前に"Door"を含むアニメーション)が動かすノード名を集める。
// 開口部を探すレイキャストではこれらのノード(ドアの板)を無視して、閉まっているドアでも
// ドア枠の内側(実際の出入口)を通れる場所として拾えるようにする。
static std::unordered_set<std::string> CollectDoorNodeNames()
{
    std::unordered_set<std::string> names;
    auto& ecs = GameManager::Instance().GetECS();
    for (Entity entity = 0; entity < MAX_ENTITIES; ++entity)
    {
        auto* pModel = ecs.TryGetComponent<ModelRenderData>(entity);
        if (!pModel || !pModel->m_spModelData || !pModel->m_spModelData->IsLoaded()) continue;
        for (const auto& anim : pModel->m_spModelData->GetAnimations())
        {
            if (anim.name.find("Door") == std::string::npos && anim.name.find("door") == std::string::npos) continue;
            for (const auto& channel : anim.channels) names.insert(channel.nodeName);
        }
    }
    return names;
}

bool NavMeshManager::BuildManualNavMesh(const std::vector<RoomArea*>& rooms)
{
    CleanupRecast();
    m_pathCache.clear();
    m_manualPolygons.clear();

    // Inset from each room's own bounds so the polygon stays clear of its walls.
    const float kMargin = 0.5f;
    // How close two rooms' *raw* (pre-margin) bounds need to be (gap, accounting for wall
    // thickness / doorway) to be considered adjacent, and how much they need to overlap on the
    // shared side to bother connecting them (guards against rooms that only touch at a corner).
    // This must be checked against the raw bounds, not the margin-inset rect: insetting each side
    // by kMargin adds up to 2*kMargin to the apparent gap between two rooms, which previously made
    // real doorway-width gaps (~0.3m) look like ~1.3m gaps and got silently rejected.
    const float kAdjacencyGapTolerance = 1.0f;
    const float kMinOverlap = 0.5f;
    // How far apart two rooms' floor heights can be and still be treated as the same floor.
    const float kSameFloorTolerance = 1.5f;

    struct RoomRect {
        float minX, maxX, minZ, maxZ, floorY;
        float rawMinX, rawMaxX, rawMinZ, rawMaxZ;
        int stairPairWith = -1; // index of this rect's other end, if this rect came from a stairs RoomArea
        bool isStair = false;   // true for both ends of a stairs RoomArea
        std::string name;       // for diagnostic logging only
    };
    std::vector<RoomRect> rects;
    for (RoomArea* room : rooms)
    {
        if (!room) continue;
        float minX = room->m_min.x + kMargin;
        float maxX = room->m_max.x - kMargin;
        float minZ = room->m_min.z + kMargin;
        float maxZ = room->m_max.z - kMargin;
        if (minX >= maxX || minZ >= maxZ) continue; // room too small for this margin, skip it

        auto pObj = room->GetGameObject();
        std::string baseName = pObj ? pObj->GetName() : "?";

        if (room->m_isStairs)
        {
            // A staircase spans two floor levels rather than sitting flat on one, so it can't be
            // represented as a single rect the way normal rooms are - the usual same-floor
            // adjacency check below would just reject it (its own m_min.y/m_max.y are typically
            // ~4m apart, far past kSameFloorTolerance). Instead, emit two rects sharing this same
            // XZ footprint: one at the bottom's floor level, one at the top's. Each end then finds
            // its own normal room neighbor through the usual adjacency check further down, and the
            // two ends are bridged directly to each other afterward (search stairPairWith below).
            int bottomIdx = (int)rects.size();
            rects.push_back({ minX, maxX, minZ, maxZ, room->m_min.y,
                room->m_min.x, room->m_max.x, room->m_min.z, room->m_max.z, -1, true, baseName + "(bottom)" });
            int topIdx = (int)rects.size();
            rects.push_back({ minX, maxX, minZ, maxZ, room->m_max.y,
                room->m_min.x, room->m_max.x, room->m_min.z, room->m_max.z, -1, true, baseName + "(top)" });
            rects[bottomIdx].stairPairWith = topIdx; // only the bottom end carries the link, so the
            // bridge-building loop below (which iterates on stairPairWith) processes each stair
            // exactly once instead of building the same bridge polygon twice (forward + a
            // redundant mirror-image copy, which would fight over the same neighbor slots).
            continue;
        }

        rects.push_back({ minX, maxX, minZ, maxZ, room->m_min.y,
            room->m_min.x, room->m_max.x, room->m_min.z, room->m_max.z, -1, false, baseName });
    }
    if (rects.empty())
    {
        Logger::Instance().AddLog(Logger::LogLevel::Error, "NavMesh (manual): no usable RoomArea volumes.");
        return false;
    }

    // Vertex pool with position-based dedup: as long as adjoining polygons compute their shared
    // corner using the *same* source values (they do, below), those corners land on the exact
    // same float bits and get welded to a single index - which is how dtCreateNavMeshData finds
    // polygon adjacency (it links polys that share an edge's two vertex indices).
    std::vector<Math::Vector3> vertPool;
    auto getOrAdd = [&](const Math::Vector3& p) -> int {
        for (size_t i = 0; i < vertPool.size(); ++i) {
            if ((vertPool[i] - p).LengthSquared() < 0.0004f) return (int)i;
        }
        vertPool.push_back(p);
        return (int)vertPool.size() - 1;
    };

    std::vector<std::array<int, 4>> polyIndices;

    // Each X/Z-adjacency connector attaches to one physical side of the two rooms it bridges
    // (0=south at minZ, 1=east at maxX, 2=north at maxZ, 3=west at minX - matching the c0..c3
    // room-quad winding built below). A room bordering two different neighbors along the very same
    // wall (LivingRoom's west wall touches both Room1 and Room2 at different Z ranges; JPRoomF1's
    // south wall touches both Room2 and LivingRoom at different X ranges) can't be represented by a
    // single quad, which has only one edge per side - forcing the second link onto some unrelated
    // edge, which made findStraightPath insert a waypoint on the wrong side of the room entirely
    // (confirmed via a "[DIAG] room edges" dump: JPRoomF1's LivingRoom link landed on its east edge
    // even though the connector itself sits on its south wall, sending the ghost the wrong way
    // right after crossing the stairs whenever the live Hunt target sat near that side of the
    // room). Connector geometry doesn't depend on any of this - only room-quad generation does -
    // so gather every connector's (side, room, along-the-wall range) here first, then decide per
    // room below whether it needs splitting before its quad(s) get built.
    struct ConnSide { int side; int connIdx; float lo, hi; }; // lo/hi along the wall (X for south/north sides, Z for east/west sides)
    std::vector<std::vector<ConnSide>> roomConnSides(rects.size());

    // 部屋同士のつながりを、実際に通れる開口部だけに絞る。以前は隣り合う部屋の範囲が重なる区間
    // (ほぼ壁の全長)をそのままつないでいたため、経路が壁の真ん中を突っ切り、壁にぶつかって詰まった
    // ゴーストが「詰まった時の一時的な壁抜け」で壁を通り抜けていた。
    // 共有する壁に沿って腰の高さでStageへレイを飛ばし(ドアの板は無視する)、抜ける区間を開口部とみなす。
    // その中で一番広い区間を、ゴーストの体の半径分だけ両端から内側へ寄せてつなぐ(経路の曲がり角が
    // ドア枠や壁の角ぎりぎりに来ると、体がぶつかってその地点に近づけず、壁の方を向いたまま進み続けるため)。
    const bool detectOpenings = IsStageCollisionReady();
    std::unordered_set<std::string> doorNodeNames;
    if (detectOpenings) {
        // RaycastAgainstMeshはCollisionManagerの対象シーンが未設定だと何にも当たらない(=全部開いている扱い)
        CollisionManager::Instance().SetScene(Editor::GetScene().get());
        doorNodeNames = CollectDoorNodeNames();
        Logger::Instance().AddLog(Logger::LogLevel::Info,
            "NavMesh (manual): detecting openings from Stage collision (%d door nodes ignored).", (int)doorNodeNames.size());
    } else {
        Logger::Instance().AddLog(Logger::LogLevel::Warning,
            "NavMesh (manual): Stage collision not ready - connecting whole shared walls (ghosts may cut through walls).");
    }
    // rayAlongX: 壁がZ方向に伸びている(X-connector)ならtrue。acrossFrom/To: レイを飛ばす区間(壁をまたぐ)。
    // [lo, hi]: 壁に沿った候補区間。開口部が見つかればoutLo/outHiに入れてtrue。
    auto findOpening = [&](bool rayAlongX, float acrossFrom, float acrossTo, float floorY,
                           float lo, float hi, float& outLo, float& outHi) -> bool {
        const float kStep = 0.1f;
        const float kRayHeight = 1.0f;   // 腰の高さ(テーブル等の家具より上、ドア枠の上端より下)
        const float kMinWidth = 0.5f;    // これより狭い隙間は通れない
        const float kAgentRadius = 0.45f; // ゴーストの当たり判定の半径(0.44m)。両端からこの分だけ内側へ寄せる
        float bestLo = 0.0f, bestHi = -1.0f, runLo = 0.0f, lastOpen = 0.0f;
        bool inRun = false;
        int count = (int)ceilf((hi - lo) / kStep);
        for (int k = 0; k <= count; ++k)
        {
            float s = (std::min)(lo + k * kStep, hi);
            Math::Vector3 origin = rayAlongX ? Math::Vector3(acrossFrom, floorY + kRayHeight, s)
                                             : Math::Vector3(s, floorY + kRayHeight, acrossFrom);
            Math::Vector3 dir = rayAlongX ? Math::Vector3(1, 0, 0) : Math::Vector3(0, 0, 1);
            RaycastHit hit = CollisionManager::Instance().RaycastAgainstMesh(origin, dir, acrossTo - acrossFrom, "Stage", &doorNodeNames);
            bool open = !hit.hit;
            if (open) {
                if (!inRun) { inRun = true; runLo = s; }
                lastOpen = s;
            }
            if (inRun && (!open || k == count)) {
                if (lastOpen - runLo > bestHi - bestLo) { bestLo = runLo; bestHi = lastOpen; }
                inRun = false;
            }
        }
        if (bestHi - bestLo < kMinWidth) return false;
        // 体の半径分だけ内側へ。狭い出入口で幅が残らない時は中央の一点を通らせる
        float insetLo = bestLo + kAgentRadius;
        float insetHi = bestHi - kAgentRadius;
        if (insetHi - insetLo < 0.1f) {
            float mid = (bestLo + bestHi) * 0.5f;
            insetLo = mid - 0.05f;
            insetHi = mid + 0.05f;
        }
        outLo = insetLo;
        outHi = insetHi;
        return true;
    };
    const float kOpeningRayMargin = 0.6f; // 壁をまたぐレイを、それぞれの部屋の内側この距離から飛ばす

    for (size_t i = 0; i < rects.size(); ++i)
    {
        for (size_t j = i + 1; j < rects.size(); ++j)
        {
            const RoomRect& A = rects[i];
            const RoomRect& B = rects[j];
            if (fabsf(A.floorY - B.floorY) > kSameFloorTolerance) continue;

            // X-adjacency: whichever room is to the west ("left") connects to the one to the east.
            // Gap test uses raw (pre-margin) bounds - see kAdjacencyGapTolerance comment above.
            const RoomRect* left = nullptr;
            const RoomRect* right = nullptr;
            if (fabsf(A.rawMaxX - B.rawMinX) <= kAdjacencyGapTolerance) { left = &A; right = &B; }
            else if (fabsf(B.rawMaxX - A.rawMinX) <= kAdjacencyGapTolerance) { left = &B; right = &A; }
            if (left && right)
            {
                float overlapMin = (left->minZ > right->minZ) ? left->minZ : right->minZ;
                float overlapMax = (left->maxZ < right->maxZ) ? left->maxZ : right->maxZ;
                if (overlapMax - overlapMin >= kMinOverlap)
                {
                    // Clipped to the actual overlap range (the real doorway-ish gap) instead of
                    // each room's full edge width - a full-width connector previously marked wall
                    // area as walkable navmesh, letting the pathfinder route waypoints straight
                    // into a wall corner outside the real opening. Inset slightly so this clipped
                    // edge can never exactly equal a room's own full edge (which happens whenever
                    // one room's Z-range sits entirely inside the other's) - an exact match would
                    // let the automatic adjacency pass below *also* auto-link this connector.
                    constexpr float kConnEdgeEpsilon = 0.05f;
                    float clipMin = overlapMin + kConnEdgeEpsilon;
                    float clipMax = overlapMax - kConnEdgeEpsilon;
                    if (clipMin >= clipMax) { clipMin = overlapMin; clipMax = overlapMax; }
                    bool connect = true;
                    if (detectOpenings) {
                        float acrossFrom = (std::min)(left->rawMaxX, right->rawMinX) - kOpeningRayMargin;
                        float acrossTo   = (std::max)(left->rawMaxX, right->rawMinX) + kOpeningRayMargin;
                        float oLo, oHi;
                        if (findOpening(true, acrossFrom, acrossTo, (std::min)(left->floorY, right->floorY), clipMin, clipMax, oLo, oHi)) {
                            clipMin = oLo;
                            clipMax = oHi;
                        } else {
                            connect = false;
                            Logger::Instance().AddLog(Logger::LogLevel::Info,
                                "NavMesh (manual): no opening between %s and %s - not connected", left->name.c_str(), right->name.c_str());
                        }
                    }
                    if (connect) {
                        Math::Vector3 c0(left->maxX,  left->floorY,  clipMax);
                        Math::Vector3 c1(left->maxX,  left->floorY,  clipMin);
                        Math::Vector3 c2(right->minX, right->floorY, clipMin);
                        Math::Vector3 c3(right->minX, right->floorY, clipMax);
                        int connIdx = (int)polyIndices.size();
                        polyIndices.push_back({ getOrAdd(c0), getOrAdd(c1), getOrAdd(c2), getOrAdd(c3) });
                        int leftIdx  = (left  == &A) ? (int)i : (int)j;
                        int rightIdx = (right == &A) ? (int)i : (int)j;
                        roomConnSides[leftIdx].push_back({ 1, connIdx, clipMin, clipMax });  // left room's east side
                        roomConnSides[rightIdx].push_back({ 3, connIdx, clipMin, clipMax }); // right room's west side
                        Logger::Instance().AddLog(Logger::LogLevel::Info,
                            "NavMesh (manual): X-connector %s <-> %s (z %.2f..%.2f)", left->name.c_str(), right->name.c_str(), clipMin, clipMax);
                    }
                }
            }

            // Z-adjacency: whichever room is to the south connects to the one to the north.
            // Gap test uses raw (pre-margin) bounds - see kAdjacencyGapTolerance comment above.
            const RoomRect* south = nullptr;
            const RoomRect* north = nullptr;
            if (fabsf(A.rawMaxZ - B.rawMinZ) <= kAdjacencyGapTolerance) { south = &A; north = &B; }
            else if (fabsf(B.rawMaxZ - A.rawMinZ) <= kAdjacencyGapTolerance) { south = &B; north = &A; }
            if (south && north)
            {
                float overlapMin = (south->minX > north->minX) ? south->minX : north->minX;
                float overlapMax = (south->maxX < north->maxX) ? south->maxX : north->maxX;
                if (overlapMax - overlapMin >= kMinOverlap)
                {
                    // Clipped to the overlap range, inset by an epsilon - see the X-adjacency
                    // comment above for why.
                    constexpr float kConnEdgeEpsilon = 0.05f;
                    float clipMin = overlapMin + kConnEdgeEpsilon;
                    float clipMax = overlapMax - kConnEdgeEpsilon;
                    if (clipMin >= clipMax) { clipMin = overlapMin; clipMax = overlapMax; }
                    bool connect = true;
                    if (detectOpenings) {
                        float acrossFrom = (std::min)(south->rawMaxZ, north->rawMinZ) - kOpeningRayMargin;
                        float acrossTo   = (std::max)(south->rawMaxZ, north->rawMinZ) + kOpeningRayMargin;
                        float oLo, oHi;
                        if (findOpening(false, acrossFrom, acrossTo, (std::min)(south->floorY, north->floorY), clipMin, clipMax, oLo, oHi)) {
                            clipMin = oLo;
                            clipMax = oHi;
                        } else {
                            connect = false;
                            Logger::Instance().AddLog(Logger::LogLevel::Info,
                                "NavMesh (manual): no opening between %s and %s - not connected", south->name.c_str(), north->name.c_str());
                        }
                    }
                    if (connect) {
                        Math::Vector3 c0(clipMin, south->floorY, south->maxZ);
                        Math::Vector3 c1(clipMax, south->floorY, south->maxZ);
                        Math::Vector3 c2(clipMax, north->floorY, north->minZ);
                        Math::Vector3 c3(clipMin, north->floorY, north->minZ);
                        int connIdx = (int)polyIndices.size();
                        polyIndices.push_back({ getOrAdd(c0), getOrAdd(c1), getOrAdd(c2), getOrAdd(c3) });
                        int southIdx = (south == &A) ? (int)i : (int)j;
                        int northIdx = (north == &A) ? (int)i : (int)j;
                        roomConnSides[southIdx].push_back({ 2, connIdx, clipMin, clipMax }); // south room's north side
                        roomConnSides[northIdx].push_back({ 0, connIdx, clipMin, clipMax }); // north room's south side
                        Logger::Instance().AddLog(Logger::LogLevel::Info,
                            "NavMesh (manual): Z-connector %s <-> %s (x %.2f..%.2f)", south->name.c_str(), north->name.c_str(), clipMin, clipMax);
                    }
                }
            }
        }
    }

    // Now build each room's own quad(s). A room whose single physical side carries 2+ connectors
    // (per roomConnSides above) gets that side's quad split into one sub-quad per connector,
    // partitioned along the wall at the midpoints between neighboring connectors' ranges. Split
    // sub-quads share an exact edge with each other, which the automatic edge-adjacency pass below
    // welds back into one walkable room with no extra bookkeeping for that internal seam.
    struct SubQuad { int polyIdx; float lo, hi; }; // lo/hi along the split axis; {-inf,+inf} if this room wasn't split
    std::vector<std::vector<SubQuad>> rectRoomPolys(rects.size());
    std::vector<int> rectSplitSide(rects.size(), -1); // which side (0-3) was split, -1 if none

    for (size_t i = 0; i < rects.size(); ++i)
    {
        const RoomRect& rc = rects[i];
        std::array<std::vector<ConnSide*>, 4> bySide;
        for (auto& cs : roomConnSides[i]) bySide[cs.side].push_back(&cs);

        int splitSide = -1;
        for (int s = 0; s < 4; ++s) {
            if (bySide[s].size() >= 2) { splitSide = s; break; }
        }
        rectSplitSide[i] = splitSide;

        if (splitSide < 0)
        {
            Math::Vector3 c0(rc.minX, rc.floorY, rc.minZ);
            Math::Vector3 c1(rc.maxX, rc.floorY, rc.minZ);
            Math::Vector3 c2(rc.maxX, rc.floorY, rc.maxZ);
            Math::Vector3 c3(rc.minX, rc.floorY, rc.maxZ);
            int polyIdx = (int)polyIndices.size();
            polyIndices.push_back({ getOrAdd(c0), getOrAdd(c1), getOrAdd(c2), getOrAdd(c3) });
            rectRoomPolys[i].push_back({ polyIdx, -FLT_MAX, FLT_MAX });
            continue;
        }

        bool splitAlongX = (splitSide == 0 || splitSide == 2); // south/north sides vary along X
        auto& claims = bySide[splitSide];
        std::sort(claims.begin(), claims.end(), [](ConnSide* a, ConnSide* b) { return a->lo < b->lo; });

        std::vector<float> bounds;
        bounds.push_back(splitAlongX ? rc.minX : rc.minZ);
        for (size_t k = 0; k + 1 < claims.size(); ++k)
            bounds.push_back((claims[k]->hi + claims[k + 1]->lo) * 0.5f);
        bounds.push_back(splitAlongX ? rc.maxX : rc.maxZ);

        for (size_t k = 0; k < claims.size(); ++k)
        {
            float lo = bounds[k], hi = bounds[k + 1];
            Math::Vector3 c0, c1, c2, c3;
            if (splitAlongX) {
                c0 = Math::Vector3(lo, rc.floorY, rc.minZ);
                c1 = Math::Vector3(hi, rc.floorY, rc.minZ);
                c2 = Math::Vector3(hi, rc.floorY, rc.maxZ);
                c3 = Math::Vector3(lo, rc.floorY, rc.maxZ);
            } else {
                c0 = Math::Vector3(rc.minX, rc.floorY, lo);
                c1 = Math::Vector3(rc.maxX, rc.floorY, lo);
                c2 = Math::Vector3(rc.maxX, rc.floorY, hi);
                c3 = Math::Vector3(rc.minX, rc.floorY, hi);
            }
            int polyIdx = (int)polyIndices.size();
            polyIndices.push_back({ getOrAdd(c0), getOrAdd(c1), getOrAdd(c2), getOrAdd(c3) });
            rectRoomPolys[i].push_back({ polyIdx, lo, hi });
        }
        Logger::Instance().AddLog(Logger::LogLevel::Info,
            "NavMesh (manual): split %s into %d sub-quads along its %s side (multiple neighbors share that wall)",
            rc.name.c_str(), (int)claims.size(),
            splitSide == 0 ? "south" : splitSide == 1 ? "east" : splitSide == 2 ? "north" : "west");
    }

    // Bridge each staircase's bottom and top rects directly - see the stairPairWith comment above.
    // This deliberately bypasses kAdjacencyGapTolerance/kMinOverlap/kSameFloorTolerance entirely:
    // we already know these two rects came from the same physical RoomArea, sharing the exact same
    // XZ footprint, so there's nothing to test. Reuses the same south/north winding convention as
    // the Z-adjacency connector above so the shared edges weld and link correctly.
    // These bridge polys are geometrically nonsense (a long diagonal quad from floor to floor, not
    // an actual physical surface), so they're excluded from the debug-draw list below - drawing
    // them looked like a stray diagonal ray/beam cutting through the middle of the house. They stay
    // in polyIndices/the navmesh data itself since pathfinding connectivity still needs them.
    size_t stairBridgeRangeStart = polyIndices.size();
    for (size_t i = 0; i < rects.size(); ++i)
    {
        int j = rects[i].stairPairWith;
        if (j < 0) continue;
        const RoomRect& south = rects[i];           // bottom end of the stairs
        const RoomRect& north = rects[(size_t)j];   // top end of the stairs
        Math::Vector3 c0(south.minX, south.floorY, south.maxZ);
        Math::Vector3 c1(south.maxX, south.floorY, south.maxZ);
        Math::Vector3 c2(north.maxX, north.floorY, north.minZ);
        Math::Vector3 c3(north.minX, north.floorY, north.minZ);
        polyIndices.push_back({ getOrAdd(c0), getOrAdd(c1), getOrAdd(c2), getOrAdd(c3) });
        Logger::Instance().AddLog(Logger::LogLevel::Info,
            "NavMesh (manual): stair-bridge %s <-> %s", south.name.c_str(), north.name.c_str());
    }
    size_t stairBridgeRangeEnd = polyIndices.size();

    // Quantize into the unsigned-short grid format dtNavMeshCreateParams expects (same convention
    // rcPolyMesh uses: worldPos = bmin + gridCoord * cellSize).
    Math::Vector3 worldMin(FLT_MAX, FLT_MAX, FLT_MAX);
    Math::Vector3 worldMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    for (const auto& v : vertPool)
    {
        worldMin = Math::Vector3::Min(worldMin, v);
        worldMax = Math::Vector3::Max(worldMax, v);
    }
    worldMin -= Math::Vector3(1.0f, 1.0f, 1.0f);
    worldMax += Math::Vector3(1.0f, 1.0f, 1.0f);

    constexpr float kCellSize = 0.02f; // 2cm grid resolution, comfortably precise for room-scale geometry
    float bmin[3] = { worldMin.x, worldMin.y, worldMin.z };
    float bmax[3] = { worldMax.x, worldMax.y, worldMax.z };

    std::vector<unsigned short> quantVerts(vertPool.size() * 3);
    for (size_t i = 0; i < vertPool.size(); ++i)
    {
        quantVerts[i * 3 + 0] = (unsigned short)((vertPool[i].x - bmin[0]) / kCellSize + 0.5f);
        quantVerts[i * 3 + 1] = (unsigned short)((vertPool[i].y - bmin[1]) / kCellSize + 0.5f);
        quantVerts[i * 3 + 2] = (unsigned short)((vertPool[i].z - bmin[2]) / kCellSize + 0.5f);
    }

    constexpr int kNvp = 4;
    std::vector<unsigned short> polys(polyIndices.size() * kNvp * 2, RC_MESH_NULL_IDX);
    std::vector<unsigned char>  areas(polyIndices.size(), 0);
    std::vector<unsigned short> flags(polyIndices.size(), 1);
    for (size_t i = 0; i < polyIndices.size(); ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            polys[i * kNvp * 2 + j] = (unsigned short)polyIndices[i][j];
        }
    }

    // dtCreateNavMeshData does NOT compute polygon-to-polygon adjacency itself - it expects the
    // second half of `polys` (the neighbor slots) to already be filled in, which is normally done
    // by rcBuildPolyMesh's region-based adjacency pass. Since we skip that pass entirely, we have
    // to compute it by hand here: for every edge of every polygon, find the other polygon that has
    // the same edge with its two vertex indices reversed (adjacent polys share an edge but wind
    // opposite directions along it), and record that neighbor's poly index. Leaving this all
    // RC_MESH_NULL_IDX (as it starts out) makes every polygon topologically isolated, which is
    // silently accepted by dtCreateNavMeshData/dtNavMesh::init but makes findPath() unable to ever
    // step past the starting polygon.
    for (size_t i = 0; i < polyIndices.size(); ++i)
    {
        for (int e = 0; e < kNvp; ++e)
        {
            int a = polyIndices[i][e];
            int b = polyIndices[i][(e + 1) % kNvp];
            for (size_t j = 0; j < polyIndices.size() && polys[i * kNvp * 2 + kNvp + e] == RC_MESH_NULL_IDX; ++j)
            {
                if (j == i) continue;
                for (int e2 = 0; e2 < kNvp; ++e2)
                {
                    int c = polyIndices[j][e2];
                    int d = polyIndices[j][(e2 + 1) % kNvp];
                    if (c == b && d == a)
                    {
                        polys[i * kNvp * 2 + kNvp + e] = (unsigned short)j;
                        break;
                    }
                }
            }
        }
    }

    // Containment fallback for stairs: a staircase is usually carved into the floor of whichever
    // room it starts/ends in (all 4 sides open into that same room) rather than sitting beside it,
    // so its footprint overlaps or sits entirely inside that room's footprint instead of touching
    // it along one edge. The edge-based adjacency above can't catch that (there's no shared
    // boundary edge to build a welded connector from), so for stairs rects specifically, fall back
    // to a plain XZ-overlap test against ordinary (non-stair) rects and link them directly by
    // writing into a free neighbor slot on each side - skipping the "must share an actual edge"
    // requirement entirely.
    //
    // Which of the 4 free slots gets picked matters more than the comment above used to assume:
    // dtNavMeshQuery's straight-path portal for a given link is that edge's own two vertices, so a
    // room with several neighbors (e.g. LivingRoom bordering Room1, Room2, JPRoomF1 and Stairs all
    // at once) can't just take "whichever slot is still empty" in call order - a room offering the
    // same physical wall to two different neighbors (Room1 and Room2 are both to LivingRoom's west,
    // just at different Z ranges) would otherwise hand the second one an unrelated edge (its south
    // or east wall), and the pathfinder would then insert a waypoint on the wrong side of the room
    // entirely before doubling back, which is what produced the "walks into an unrelated room after
    // crossing the stairs" behaviour this fallback was implicated in. Instead, pick whichever
    // still-free edge's midpoint sits closest (in XZ) to the other polygon's centroid, so a forced
    // link always lands on the most plausible side of the room even when the geometrically exact
    // edge was already claimed by an earlier link.
    auto polyCentroidXZ = [&](int p) -> Math::Vector3 {
        Math::Vector3 c(0, 0, 0);
        for (int k = 0; k < kNvp; ++k) c += vertPool[(size_t)polyIndices[(size_t)p][k]];
        return c / (float)kNvp;
    };
    auto bestFreeEdge = [&](int p, const Math::Vector3& towardXZ) -> int {
        int best = -1;
        float bestDistSq = FLT_MAX;
        for (int e = 0; e < kNvp; ++e) {
            if (polys[(size_t)p * kNvp * 2 + kNvp + e] != RC_MESH_NULL_IDX) continue;
            const Math::Vector3& v0 = vertPool[(size_t)polyIndices[(size_t)p][e]];
            const Math::Vector3& v1 = vertPool[(size_t)polyIndices[(size_t)p][(e + 1) % kNvp]];
            float mx = (v0.x + v1.x) * 0.5f;
            float mz = (v0.z + v1.z) * 0.5f;
            float dx = mx - towardXZ.x;
            float dz = mz - towardXZ.z;
            float distSq = dx * dx + dz * dz;
            if (distSq < bestDistSq) { bestDistSq = distSq; best = e; }
        }
        return best;
    };
    auto forceLink = [&](int a, int b) -> bool {
        int ea = bestFreeEdge(a, polyCentroidXZ(b));
        int eb = bestFreeEdge(b, polyCentroidXZ(a));
        if (ea < 0 || eb < 0) return false;
        polys[(size_t)a * kNvp * 2 + kNvp + ea] = (unsigned short)b;
        polys[(size_t)b * kNvp * 2 + kNvp + eb] = (unsigned short)a;
        return true;
    };

    // Resolve each connector's (side, range) claim to the specific room sub-quad it actually
    // touches, then link them. For an uncontested side this is just "the room's only quad"; for a
    // split side it's the sub-quad whose range this exact claim helped define; for a side
    // perpendicular to the split axis (e.g. LivingRoom's JPRoomF1 link, when LivingRoom's west side
    // was the one split into two Z-bands) it's whichever sub-quad's range reaches that side's fixed
    // coordinate on the room's true outer boundary.
    auto pickSubQuad = [&](size_t rectIdx, const ConnSide& cs) -> int {
        auto& subs = rectRoomPolys[rectIdx];
        if (subs.size() == 1) return subs[0].polyIdx;
        const RoomRect& rc = rects[rectIdx];
        bool splitAlongX = (rectSplitSide[rectIdx] == 0 || rectSplitSide[rectIdx] == 2);
        bool sideSpansX = (cs.side == 0 || cs.side == 2);
        float coord;
        if (splitAlongX == sideSpansX) {
            // This claim's own range is measured along the split axis - use its midpoint.
            coord = (cs.lo + cs.hi) * 0.5f;
        } else {
            // This side's position along the split axis is fixed at the room's outer boundary.
            coord = splitAlongX ? ((cs.side == 3) ? rc.minX : rc.maxX)
                                 : ((cs.side == 0) ? rc.minZ : rc.maxZ);
        }
        for (auto& sq : subs) {
            if (coord >= sq.lo - 0.01f && coord <= sq.hi + 0.01f) return sq.polyIdx;
        }
        return subs.front().polyIdx; // shouldn't happen; fall back rather than link nothing
    };

    for (size_t i = 0; i < rects.size(); ++i)
    {
        for (const auto& cs : roomConnSides[i])
        {
            int roomPolyIdx = pickSubQuad(i, cs);
            bool ok = forceLink(cs.connIdx, roomPolyIdx);
            if (!ok)
            {
                Logger::Instance().AddLog(Logger::LogLevel::Warning,
                    "NavMesh (manual): connector link failed (poly %d <-> %s) - no free neighbor slot",
                    cs.connIdx, rects[i].name.c_str());
            }
        }
    }

    // Containment fallback for stairs: a staircase is usually carved into the floor of whichever
    // room it starts/ends in (all 4 sides open into that same room) rather than sitting beside it,
    // so its footprint overlaps or sits entirely inside that room's footprint instead of touching
    // it along one edge. The edge-based adjacency above can't catch that (there's no shared
    // boundary edge to build a welded connector from), so for stairs rects specifically, fall back
    // to a plain XZ-overlap test against ordinary (non-stair) rects and link them directly, picking
    // whichever of the target room's sub-quads (see the splitting above) sits closest to the stairs
    // rect, then the nearest still-free edge on that specific sub-quad.
    for (size_t i = 0; i < rects.size(); ++i)
    {
        if (!rects[i].isStair) continue;
        int stairsPolyIdx = rectRoomPolys[i][0].polyIdx;
        for (size_t j = 0; j < rects.size(); ++j)
        {
            if (j == i || rects[j].isStair) continue;
            if (fabsf(rects[i].floorY - rects[j].floorY) > kSameFloorTolerance) continue;

            // 重なりは壁から内側へ寄せる前の実際の範囲(raw)で判定する。寄せた後の範囲だと、幅1.5mしかない
            // 階段は0.5mしか残らず、壁際に接している部屋(1階のLivingRoom)との重なりが0.4mになって
            // kMinOverlapに届かず、1階からは階段へ経路がつながっていなかった。
            float ox0 = (rects[i].rawMinX > rects[j].rawMinX) ? rects[i].rawMinX : rects[j].rawMinX;
            float ox1 = (rects[i].rawMaxX < rects[j].rawMaxX) ? rects[i].rawMaxX : rects[j].rawMaxX;
            float oz0 = (rects[i].rawMinZ > rects[j].rawMinZ) ? rects[i].rawMinZ : rects[j].rawMinZ;
            float oz1 = (rects[i].rawMaxZ < rects[j].rawMaxZ) ? rects[i].rawMaxZ : rects[j].rawMaxZ;
            if (ox1 - ox0 >= kMinOverlap && oz1 - oz0 >= kMinOverlap)
            {
                auto& subs = rectRoomPolys[j];
                int targetPolyIdx = subs[0].polyIdx;
                if (subs.size() > 1)
                {
                    Math::Vector3 stairsCentroid = polyCentroidXZ(stairsPolyIdx);
                    float bestDistSq = FLT_MAX;
                    for (auto& sq : subs) {
                        Math::Vector3 c = polyCentroidXZ(sq.polyIdx);
                        float dx = c.x - stairsCentroid.x, dz = c.z - stairsCentroid.z;
                        float d = dx * dx + dz * dz;
                        if (d < bestDistSq) { bestDistSq = d; targetPolyIdx = sq.polyIdx; }
                    }
                }
                bool ok = forceLink(stairsPolyIdx, targetPolyIdx);
                Logger::Instance().AddLog(Logger::LogLevel::Info,
                    "NavMesh (manual): overlap-link %s <-> %s (%s)",
                    rects[i].name.c_str(), rects[j].name.c_str(), ok ? "ok" : "FAILED - no free neighbor slot");
            }
        }
    }

    dtNavMeshCreateParams params;
    memset(&params, 0, sizeof(params));
    params.verts     = quantVerts.data();
    params.vertCount = (int)vertPool.size();
    params.polys     = polys.data();
    params.polyAreas = areas.data();
    params.polyFlags = flags.data();
    params.polyCount = (int)polyIndices.size();
    params.nvp       = kNvp;
    params.walkableHeight = 2.0f;
    params.walkableRadius = 0.25f;
    params.walkableClimb  = 0.9f;
    params.bmin[0] = bmin[0]; params.bmin[1] = bmin[1]; params.bmin[2] = bmin[2];
    params.bmax[0] = bmax[0]; params.bmax[1] = bmax[1]; params.bmax[2] = bmax[2];
    params.cs = kCellSize;
    params.ch = kCellSize;

    unsigned char* navData     = 0;
    int            navDataSize = 0;
    if (!dtCreateNavMeshData(&params, &navData, &navDataSize))
    {
        Logger::Instance().AddLog(Logger::LogLevel::Error, "NavMesh (manual): dtCreateNavMeshData failed.");
        return false;
    }

    m_navMesh = dtAllocNavMesh();
    if (dtStatusFailed(m_navMesh->init(navData, navDataSize, DT_TILE_FREE_DATA)))
    {
        dtFree(navData);
        Logger::Instance().AddLog(Logger::LogLevel::Error, "NavMesh (manual): dtNavMesh::init failed.");
        return false;
    }

    m_navQuery = dtAllocNavMeshQuery();
    m_navQuery->init(m_navMesh, 2048);

    // Keep world-space copies for debug drawing (there's no rcPolyMesh to read back from here).
    // Skips the stair-bridge polys (see comment above where stairBridgeRangeStart is set) since
    // drawing those looks like a stray diagonal ray through the middle of the house.
    for (size_t i = 0; i < polyIndices.size(); ++i)
    {
        if (i >= stairBridgeRangeStart && i < stairBridgeRangeEnd) continue;
        std::vector<Math::Vector3> poly;
        poly.reserve(4);
        for (int k : polyIndices[i]) poly.push_back(vertPool[k]);
        m_manualPolygons.push_back(std::move(poly));
    }

    int totalRoomPolys = 0;
    for (const auto& subs : rectRoomPolys) totalRoomPolys += (int)subs.size();
    Logger::Instance().AddLog(Logger::LogLevel::Info,
        "NavMesh (manual) built successfully! Rooms: %d Polys: %d (rooms=%d connectors=%d)",
        (int)rects.size(), (int)polyIndices.size(), totalRoomPolys, (int)(polyIndices.size() - totalRoomPolys));
    return true;
}

void NavMeshManager::DrawDebugMesh()
{
    if (!m_debugDrawEnabled) return;

    if (!m_manualPolygons.empty())
    {
        const ImU32 fillColorManual = IM_COL32(0, 200, 120, 110);
        const ImU32 edgeColorManual = IM_COL32(60, 255, 170, 230);
        for (const auto& poly : m_manualPolygons)
        {
            std::vector<Math::Vector3> lifted;
            lifted.reserve(poly.size());
            for (const auto& v : poly) lifted.push_back(v + Math::Vector3(0, 0.05f, 0));
            CollisionManager::Instance().AddDebugPolyAlways(lifted, fillColorManual);
            for (size_t j = 0; j < lifted.size(); ++j) {
                CollisionManager::Instance().AddDebugLineAlways(lifted[j], lifted[(j + 1) % lifted.size()], edgeColorManual);
            }
        }
        return;
    }

    if (!m_pmesh) return;

    const rcPolyMesh& mesh = *m_pmesh;
    const float cs = mesh.cs;
    const float ch = mesh.ch;
    const float* bmin = mesh.bmin;

    auto toWorld = [&](int vertIdx) -> Math::Vector3 {
        const unsigned short* v = &mesh.verts[vertIdx * 3];
        return Math::Vector3(
            bmin[0] + v[0] * cs,
            bmin[1] + v[1] * ch,
            bmin[2] + v[2] * cs);
    };

    // filled so it stays legible next to other debug wireframes; brighter edge on top of a translucent fill
    const ImU32 fillColor = IM_COL32(0, 200, 120, 110);
    const ImU32 edgeColor = IM_COL32(60, 255, 170, 230);

    for (int i = 0; i < mesh.npolys; ++i)
    {
        if (mesh.flags[i] == 0) continue; // Skip disabled polys (e.g. from seed filtering)

        const unsigned short* p = &mesh.polys[i * mesh.nvp * 2];

        std::vector<Math::Vector3> verts;
        verts.reserve(mesh.nvp);
        for (int j = 0; j < mesh.nvp; ++j)
        {
            if (p[j] == RC_MESH_NULL_IDX) break;
            Math::Vector3 v = toWorld(p[j]);
            // slightly lift off the floor to avoid z-fighting with the stage mesh
            v.y += 0.05f;
            verts.push_back(v);
        }
        if (verts.size() < 3) continue;

        CollisionManager::Instance().AddDebugPolyAlways(verts, fillColor);
        for (size_t j = 0; j < verts.size(); ++j) {
            CollisionManager::Instance().AddDebugLineAlways(verts[j], verts[(j + 1) % verts.size()], edgeColor);
        }
    }
}

bool NavMeshManager::FindPath(
    const Math::Vector3& start,
    const Math::Vector3& end,
    std::vector<Math::Vector3>& outPath)
{
    outPath.clear();
    if (!m_navQuery || !m_navMesh) return false;

    // FindPathは今やJobSystemのワーカースレッドで実行され得る(MoveToward参照)一方、
    // メインスレッドは引き続きIsReachable()等を自由に呼べる - どちらも同じdtNavMeshQueryを
    // 触るが、複数スレッドから同時に使うのは安全ではないため、こことIsReachable()で
    // ロックしている。関数全体で保持する; findPath自体はこの規模のNavMeshなら十分速いので、
    // 直列化しても実質的なコストにはならない。
    std::lock_guard<std::mutex> lock(m_navQueryMutex);

    // Vertical extent must stay well under the ~4.3m floor-to-floor gap in this house - too large
    // (this used to be 4.0f) and a query point near one floor can snap to the nearest poly on the
    // *other* floor instead (e.g. Hunt targeting the player's position while both ghost and player
    // are vertically stacked one floor apart), producing a path that never actually uses the
    // stairs and instead just walks back and forth on the wrong floor above/below the real target.
    float extents[3] = { 2.0f, 3.0f, 2.0f };

    dtPolyRef startRef = 0, endRef = 0;
    float     startPt[3], endPt[3];

    m_navQuery->findNearestPoly(&start.x, extents, m_filter, &startRef, startPt);
    m_navQuery->findNearestPoly(&end.x,   extents, m_filter, &endRef,   endPt);

    if (!startRef || !endRef)
    {
        static int s_logCounter = 0;
        if ((s_logCounter++ % 120) == 0) // throttle: only log occasionally, this can be called every frame
        {
            Logger::Instance().AddLog(Logger::LogLevel::Warning,
                "NavMesh FindPath: no nearby polygon for %s point. start=(%.2f,%.2f,%.2f) end=(%.2f,%.2f,%.2f)",
                (!startRef && !endRef) ? "start&end" : (!startRef ? "start" : "end"),
                start.x, start.y, start.z, end.x, end.y, end.z);
        }
        return false;
    }

    static const int MAX_POLYS = 256;
    dtPolyRef path[MAX_POLYS];
    int       pathCount = 0;
    m_navQuery->findPath(startRef, endRef, startPt, endPt, m_filter, path, &pathCount, MAX_POLYS);

    if (pathCount <= 0)
    {
        Logger::Instance().AddLog(Logger::LogLevel::Warning,
            "NavMesh FindPath: findPath returned 0 polys even though start/end refs were valid. start=(%.2f,%.2f,%.2f) end=(%.2f,%.2f,%.2f)",
            start.x, start.y, start.z, end.x, end.y, end.z);
        return false;
    }

    // If the last polygon in the path isn't the requested end polygon, start and end are not
    // connected in the NavMesh graph - Detour has instead handed back the partial path toward
    // whichever reachable polygon is closest to the goal (it does not fail outright). This is
    // exactly the "walks up to some point and then just stops" symptom.
    if (path[pathCount - 1] != endRef)
    {
        static int s_partialLogCounter = 0;
        if ((s_partialLogCounter++ % 60) == 0)
        {
            Logger::Instance().AddLog(Logger::LogLevel::Warning,
                "NavMesh FindPath: start and end are NOT connected (partial path only). start=(%.2f,%.2f,%.2f) end=(%.2f,%.2f,%.2f)",
                start.x, start.y, start.z, end.x, end.y, end.z);
        }
    }

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
    if (!cache.computing) cache.computing = std::make_shared<std::atomic<bool>>(false);
    if (!cache.pending) cache.pending = std::make_shared<AsyncPathResult>();

    // 新しく再計算を始めるか決める前に、完了しているバックグラウンド再計算があれば
    // 取り込む。再計算が進行中の間も、移動主体は*それまでの*cache.waypointsをそのまま
    // 辿り続ける - 停止もスナップもしない。まさに「既に分かっているウェイポイントに
    // 向かって歩きつつ、裏で次のウェイポイントを計算する」という動き。
    {
        std::lock_guard<std::mutex> lock(cache.pending->mutex);
        if (cache.pending->ready)
        {
            cache.waypoints = std::move(cache.pending->waypoints);
            cache.pending->waypoints.clear();
            cache.pending->ready = false;
            cache.computing->store(false, std::memory_order_release);
            DropPassedWaypoints(cache.waypoints, current);
        }
    }

    // 実際に理由がある時だけ再計算する: まだ経路が無い、ターゲットが本当に動いた
    // (Huntのライブターゲット等)、または何か他のことが裏で変わっている可能性に備えた
    // 長めの安全網インターバルが経過した、のいずれか。*何も変わっていなくても*短い
    // 固定タイマーで再計算していた頃は、位置のわずかなノイズだけで、完璧にまだ進行中の
    // 経路をわずかに違う経路に置き換えてしまっていた(狭く区切られたコネクタで特に
    // 顕著で、少し間を置いた2回の再計算がそれぞれ微妙に違うルートに解決することがある)
    // - 移動主体側から見ると、ウェイポイントに向かい始めた直後にすぐ反転する、を
    // 延々と繰り返しているように見える。始まったばかりの経路が完了する機会を
    // 一度も得られないため。
    const float kTargetMovedThreshold = 1.0f;
    bool targetMoved = cache.hasTarget && (target - cache.lastTarget).LengthSquared() > (kTargetMovedThreshold * kTargetMovedThreshold);
    cache.timer -= deltaTime;
    // "waypoints.empty()" が即座の再計算を強制するのは、このターゲットについて
    // 一度も問い合わせていない時(hasTarget が false)だけにしている - 既に計算済みの
    // 場合、リストが空なのは単に到着したことを意味するだけで、「今すぐもう一度試す」
    // ではない。!hasTarget のガードが無いと、目的地に座って次の指示を待っている間
    // 毎フレーム発火し続けてしまう(Async Path Recomputesがアイドル中に急増して見える -
    // ジョブが即座に開始・完了し、ターゲットが変わるまで延々と次のフレームで
    // 再トリガーされる)。
    bool needsRecompute = (cache.waypoints.empty() && !cache.hasTarget) || targetMoved || cache.timer <= 0.0f;

    if (needsRecompute && !cache.computing->load(std::memory_order_acquire))
    {
        cache.computing->store(true, std::memory_order_release);
        cache.timer = pathUpdateInterval;
        cache.lastTarget = target;
        cache.hasTarget = true;

        // pending/computingはshared_ptrで、このラムダによってPathCache/m_pathCache自体の
        // 寿命とは独立して生かされる - ジョブがまだ実行中の間にClearPath()がこの
        // エンティティのキャッシュエントリを消しても安全(AsyncPathResultのコメント参照)。
        auto pending = cache.pending;
        auto computing = cache.computing;
        Math::Vector3 startCopy = current;
        Math::Vector3 targetCopy = target;

        JobSystem::Instance().Execute([this, startCopy, targetCopy, pending, computing]() {
            std::vector<Math::Vector3> result;
            FindPath(startCopy, targetCopy, result); // 内部でm_navQueryMutexをロックする
            m_asyncRecomputeCount.fetch_add(1, std::memory_order_relaxed);

            std::lock_guard<std::mutex> lock(pending->mutex);
            pending->waypoints = std::move(result);
            pending->ready = true;
            // 'computing' はここでは意図的にtrueのままにしている - 上のMoveTowardの
            // 取り込みステップが実際にこの結果を受け取った時点でクリアされるので、
            // それより前に2つ目のジョブが始まって('pending'を読み取り中に上書きする)
            // しまうことはない。
        });
    }

    Math::Vector3 nextPos = AdvanceAlongPath(cache, current, speed, deltaTime, nodeReachThreshold);
    return nextPos;
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
    // Note: this deliberately does NOT just call FindPath() and check its bool result.
    // Detour's findPath() does not fail when start and end are in disconnected parts of the
    // NavMesh - it instead returns a *partial* path toward whichever reachable polygon is
    // closest to the goal, and FindPath()/MoveToward() happily follow that partial path (useful
    // for "walk toward but stop at the wall" behavior). For a yes/no reachability check we need
    // to explicitly verify the path actually terminates at the destination polygon.
    if (!m_navQuery || !m_navMesh) return false;

    // FindPath()のロックを参照 - 同じdtNavMeshQueryに、バックグラウンドの経路再計算
    // (MoveToward)とこの呼び出し(今も引き続きメインスレッド)が同時に触り得るようになった。
    std::lock_guard<std::mutex> lock(m_navQueryMutex);

    // Kept in sync with FindPath()'s extents - see its comment for why the vertical component must
    // stay small relative to the floor-to-floor gap.
    float extents[3] = { 2.0f, 3.0f, 2.0f };
    dtPolyRef startRef = 0, endRef = 0;
    float     startPt[3], endPt[3];

    m_navQuery->findNearestPoly(&start.x, extents, m_filter, &startRef, startPt);
    m_navQuery->findNearestPoly(&end.x,   extents, m_filter, &endRef,   endPt);
    if (!startRef || !endRef) {
        Logger::Instance().AddLog(Logger::LogLevel::Info,
            "IsReachable: no nearby poly. start=(%.2f,%.2f,%.2f) startRef=%llu snappedStart=(%.2f,%.2f,%.2f) | end=(%.2f,%.2f,%.2f) endRef=%llu snappedEnd=(%.2f,%.2f,%.2f)",
            start.x, start.y, start.z, (unsigned long long)startRef, startPt[0], startPt[1], startPt[2],
            end.x, end.y, end.z, (unsigned long long)endRef, endPt[0], endPt[1], endPt[2]);
        return false;
    }

    static const int MAX_POLYS = 256;
    dtPolyRef path[MAX_POLYS];
    int       pathCount = 0;
    m_navQuery->findPath(startRef, endRef, startPt, endPt, m_filter, path, &pathCount, MAX_POLYS);
    if (pathCount <= 0) {
        Logger::Instance().AddLog(Logger::LogLevel::Info,
            "IsReachable: refs valid but findPath returned 0. snappedStart=(%.2f,%.2f,%.2f) snappedEnd=(%.2f,%.2f,%.2f)",
            startPt[0], startPt[1], startPt[2], endPt[0], endPt[1], endPt[2]);
        return false;
    }

    // 注: ここは以前、呼ばれるたびに無条件でログを出していた。IsReachable()は
    // 頻繁に呼ばれる(例: GhostAIが新しい徘徊先を選ぶたびに候補の部屋をフィルタする)
    // ため、Logger::AddLog()の呼び出し1回1回が同期のディスク書き込みであり、
    // 呼び出し側が戻り値として既に受け取っている結果のために実質的な毎フレームの
    // コストを追加していた。この呼び出しは(想定内でよくある)成功/失敗の経路では
    // 静かにしておく; 上の2つの分岐は、本当に想定外のケース(近くにポリゴンが無い、
    // findPathが何も返さない)は引き続きログに残す。
    return path[pathCount - 1] == endRef;
}

const std::vector<Math::Vector3>* NavMeshManager::GetCachedPath(int entityID) const
{
    auto it = m_pathCache.find(entityID);
    if (it == m_pathCache.end()) return nullptr;
    return &it->second.waypoints;
}

void NavMeshManager::ClearPath(int entityID)
{
    m_pathCache.erase(entityID);
}

void NavMeshManager::ClearAllPaths()
{
    m_pathCache.clear();
}

// 非同期で計算した経路を取り込む時に、既に通り過ぎた先頭のウェイポイントを捨てる。
// findStraightPathの先頭は「計算を頼んだ時点の位置(をNavMesh上に寄せた点)」なので、結果が届く
// までに進んだ分や、当たり判定でNavMeshの外へ少し押し出されていた分だけ、その点が後ろや横に
// ずれていることがある。そのまま辿ると一度そこへ引き返そうとして、追跡中に急に向きを変えて
// 見える(ハント中はターゲットが動くたびに経路を引き直すので頻繁に起きる)。
// 対象は先頭の1点(=計算開始時の位置)だけ。近く(kNearDist以内)にあり、wp0->wp1の線分上で現在位置が
// 既にwp0より先なら捨てる。2点目以降(階段の入口やドアなど、これから通る曲がり角)は絶対に捨てない:
// 以前は近い点を続けて何個でも捨てていたため、階段の入口まで捨てて上の階の地点の真下へ向かい、
// そこで「到着」扱いになって経路が空になり、階段前で止まってしまっていた。
void NavMeshManager::DropPassedWaypoints(std::vector<Math::Vector3>& waypoints, const Math::Vector3& current)
{
    const float kNearDist = 1.5f;
    if (waypoints.size() < 2) return;

    Math::Vector3 seg = waypoints[1] - waypoints[0];
    Math::Vector3 toCur = current - waypoints[0];
    seg.y = 0.0f;
    toCur.y = 0.0f;
    if (toCur.LengthSquared() > kNearDist * kNearDist) return;
    if (seg.LengthSquared() < 1e-6f || toCur.Dot(seg) >= 0.0f)
    {
        waypoints.erase(waypoints.begin());
    }
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
        // 曲がり角の地点に(壁に押されるなどして)ぴったり触れなくても、近く(kPassNear以内)で既に
        // 次の地点へ向かう側へ回り込んでいれば通過したとみなす。これが無いと、体が角に当たって
        // nodeReachThreshold以内に入れない時に、その角(=壁の方)へ向かって進み続けてしまう。
        const float kPassNear = 1.0f;
        if (cache.waypoints.size() >= 2)
        {
            Math::Vector3 seg = cache.waypoints[1] - cache.waypoints[0];
            Math::Vector3 fromNode = pos - cache.waypoints[0];
            seg.y = 0;
            fromNode.y = 0;
            if (seg.LengthSquared() > 1e-6f && fromNode.LengthSquared() <= kPassNear * kPassNear && fromNode.Dot(seg) > 0.0f)
            {
                cache.waypoints.erase(cache.waypoints.begin());
                continue;
            }
        }

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
