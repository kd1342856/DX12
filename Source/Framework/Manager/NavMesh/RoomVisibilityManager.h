#pragma once

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>

class RoomArea;
class Mesh;
using Entity = uint32_t;

// Portal/room-based occlusion culling. Frustum culling alone still draws a room that's
// technically inside the view frustum but hidden behind walls (e.g. looking at the house
// from outside - every room on the far side is still "in view" as far as the frustum is
// concerned). This adds a second filter on top: only meshes belonging to the room the
// camera is currently in, or a room reachable from it through an open door, are visible.
//
// Reuses the RoomArea components already placed for NavMesh (GameSequence::GetRooms()) -
// no additional level authoring needed. Room adjacency is inferred once from which room
// AABBs are close enough to share a wall. Each adjacency edge is then matched, by
// position, against a door's "Door..."-named animation (the same detection Player's
// TryInteractDoor already uses) - if a door is found for that edge its open/closed state
// (RuntimeAnimationData::ProgressTime) gates visibility; edges with no matching door
// (open archways, etc.) stay always-passable.
class RoomVisibilityManager
{
public:
    static RoomVisibilityManager& Instance();

    // Call once per RenderScene (not per mesh) with the camera's world position. Figures
    // out which room the camera is in and refreshes the visible-room set from it.
    void UpdateVisibleRooms(const Math::Vector3& viewerPos);

    // Whether pMesh (whose node/entity is at meshWorldTransform) belongs to a room that's
    // currently visible. A mesh is assigned to every room its world-space AABB overlaps
    // (not just the one containing its centroid) - boundary geometry like a window or wall
    // panel straddling two rooms would otherwise get pinned to whichever room happened to
    // win an arbitrary tie-break, and could then vanish while standing right next to it.
    // This is resolved once and cached, so it should only be called for static geometry -
    // see ModelData::IsNodeAnimated for anything that moves. Meshes that don't overlap any
    // known room (or when there's no room data at all, or the viewer isn't inside any room)
    // are never culled by this, only by whatever frustum test the caller also applies.
    bool IsMeshInVisibleRoom(Mesh* pMesh, const Math::Matrix& meshWorldTransform);

private:
    struct DoorLink
    {
        Entity entity = 0xFFFFFFFFu; // INVALID_ENTITY
        int animIndex = -1;
    };

    RoomVisibilityManager() = default;

    void RebuildRoomGraphIfNeeded();
    void DiscoverDoorLinks();
    bool IsDoorOpen(const DoorLink& link) const;
    static uint64_t EdgeKey(int a, int b);

    std::vector<RoomArea*> m_rooms;
    std::vector<DirectX::BoundingBox> m_paddedRoomBoxes; // room AABBs padded outward - shared by adjacency, door matching, and mesh assignment
    std::vector<std::vector<int>> m_adjacency;         // room index -> adjacent room indices
    std::unordered_map<uint64_t, DoorLink> m_edgeDoors; // EdgeKey(i,j) -> door, only for edges with a matched door
    std::unordered_map<Mesh*, std::vector<int>> m_meshRoomIndices; // empty = couldn't place it in a room
    std::unordered_set<int> m_visibleRoomIndices;
    bool m_graphBuilt = false;
    bool m_allVisible = true; // true until UpdateVisibleRooms finds the camera inside a known room
};
