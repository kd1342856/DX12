#include "../../../Pch.h"
#include "RoomVisibilityManager.h"
#include "../../../Application/Object/Script/System/GameSequence.h"
#include "../../../Application/Object/Script/System/RoomArea.h"
#include "../../../Graphics/Geometry/Mesh/Mesh.h"
#include "../../../Graphics/Geometry/Model/Model.h"
#include "../../ECS/Entity/Entity.h"
#include "../../ECS/Components/Data/AnimationData.h"
#include "../../ECS/Components/Data/ModelRenderData.h"
#include "../../ECS/Components/Data/TransformData.h"
#include "../GameManager.h"

RoomVisibilityManager& RoomVisibilityManager::Instance()
{
    static RoomVisibilityManager instance;
    return instance;
}

uint64_t RoomVisibilityManager::EdgeKey(int a, int b)
{
    if (a > b) std::swap(a, b);
    return (static_cast<uint64_t>(static_cast<uint32_t>(a)) << 32) | static_cast<uint32_t>(b);
}

void RoomVisibilityManager::RebuildRoomGraphIfNeeded()
{
    auto* gs = GameSequence::GetInstance();
    if (!gs)
    {
        m_rooms.clear();
        m_paddedRoomBoxes.clear();
        m_adjacency.clear();
        m_edgeDoors.clear();
        m_meshRoomIndices.clear();
        m_graphBuilt = false;
        return;
    }

    const auto& liveRooms = gs->GetRooms();
    // 軽い変化チェック - 部屋数が変わる（レベルの(再)ロード、エディタでの追加/削除）
    // 時だけ実際に再構築が必要になる。
    if (m_graphBuilt && liveRooms.size() == m_rooms.size()) return;

    m_rooms = liveRooms;
    m_adjacency.assign(m_rooms.size(), {});
    m_edgeDoors.clear();
    m_meshRoomIndices.clear(); // 部屋インデックスの意味が変わるので、キャッシュは古くなる

    // 以下の内包/重なり判定は全て、この余白の分だけ部屋を外側にパディングして行う。
    // 隣接判定では「共有する壁に開口部がある」ことの近似になり、メッシュ割り当てでは
    // 壁の厚みに埋まったドアや窓を拾える。単に近いだけの部屋同士を繋げてしまわない
    // 程度に小さい値にしている。
    constexpr float kTouchEpsilon = 0.5f;

    m_paddedRoomBoxes.assign(m_rooms.size(), DirectX::BoundingBox());
    for (size_t i = 0; i < m_rooms.size(); ++i)
    {
        if (!m_rooms[i]) continue;
        Math::Vector3 mn = m_rooms[i]->m_min - Math::Vector3(kTouchEpsilon, kTouchEpsilon, kTouchEpsilon);
        Math::Vector3 mx = m_rooms[i]->m_max + Math::Vector3(kTouchEpsilon, kTouchEpsilon, kTouchEpsilon);
        DirectX::BoundingBox::CreateFromPoints(m_paddedRoomBoxes[i], DirectX::XMLoadFloat3(&mn), DirectX::XMLoadFloat3(&mx));
    }

    for (size_t i = 0; i < m_rooms.size(); ++i)
    {
        if (!m_rooms[i]) continue;
        for (size_t j = i + 1; j < m_rooms.size(); ++j)
        {
            if (!m_rooms[j]) continue;
            if (m_paddedRoomBoxes[i].Intersects(m_paddedRoomBoxes[j]))
            {
                m_adjacency[i].push_back(static_cast<int>(j));
                m_adjacency[j].push_back(static_cast<int>(i));
            }
        }
    }

    DiscoverDoorLinks();

    m_graphBuilt = true;
}

void RoomVisibilityManager::DiscoverDoorLinks()
{
    // Player::TryInteractDoor が使っているのと同じ検出方法: Modelを持ち、名前に
    // "Door"/"door" を含むアニメーションを持つエンティティを探す。ドアがどの2部屋の
    // 間にあるかを判定するにはワールド座標（最初のチャンネルのノードから取得）だけ
    // あればよい - 実際の開閉判定は後で毎フレーム IsDoorOpen() 側で行う。
    auto& ecs = GameManager::Instance().GetECS();

    for (Entity entity = 0; entity < MAX_ENTITIES; ++entity)
    {
        auto* pModel = ecs.TryGetComponent<ModelRenderData>(entity);
        auto* pTransform = ecs.TryGetComponent<TransformData>(entity);
        if (!pModel || !pTransform) continue;
        if (!pModel->m_spModelData || !pModel->m_spModelData->IsLoaded()) continue;

        const auto& anims = pModel->m_spModelData->GetAnimations();
        if (anims.empty()) continue;

        for (int animIdx = 0; animIdx < static_cast<int>(anims.size()); ++animIdx)
        {
            const std::string& animName = anims[animIdx].name;
            if (animName.find("Door") == std::string::npos && animName.find("door") == std::string::npos)
                continue;
            if (anims[animIdx].channels.empty()) continue;

            const std::string& doorNodeName = anims[animIdx].channels[0].nodeName;
            Math::Vector3 doorPos = pTransform->m_position;
            bool foundNode = false;
            for (const auto& node : pModel->m_spModelData->GetNodes())
            {
                if (node.name == doorNodeName)
                {
                    Math::Vector3 scale, trans;
                    Math::Quaternion rot;
                    (node.globalTransform * pTransform->m_worldMatrix).Decompose(scale, rot, trans);
                    doorPos = trans;
                    foundNode = true;
                    break;
                }
            }
            if (!foundNode) continue;

            // ドアの位置を含む（パディング済み）部屋のAABBはどれか？ 共有する壁にある
            // ドアなら、両隣のパディング済みボックスの両方に入っているはず。
            int roomsFound[2] = { -1, -1 };
            int roomCount = 0;
            for (size_t r = 0; r < m_paddedRoomBoxes.size() && roomCount < 2; ++r)
            {
                if (m_paddedRoomBoxes[r].Contains(doorPos) != DirectX::DISJOINT)
                {
                    roomsFound[roomCount++] = static_cast<int>(r);
                }
            }

            if (roomCount == 2)
            {
                DoorLink link;
                link.entity = entity;
                link.animIndex = animIdx;
                m_edgeDoors[EdgeKey(roomsFound[0], roomsFound[1])] = link;
            }
        }
    }
}

bool RoomVisibilityManager::IsDoorOpen(const DoorLink& link) const
{
    auto& ecs = GameManager::Instance().GetECS();

    auto* pAnimComp = ecs.TryGetComponent<AnimationDataComponent>(link.entity);
    if (!pAnimComp) return true; // まだ実行時アニメーション状態が無い - 通行可能とみなす

    auto it = pAnimComp->multiAnims.find(link.animIndex);
    if (it == pAnimComp->multiAnims.end()) return true; // 一度も操作されていない - 部屋を永久に隠すよりは開いている扱いにする

    auto* pModel = ecs.TryGetComponent<ModelRenderData>(link.entity);
    if (!pModel || !pModel->m_spModelData) return true;
    const auto& anims = pModel->m_spModelData->GetAnimations();
    if (link.animIndex < 0 || link.animIndex >= static_cast<int>(anims.size())) return true;

    // AnimationSystem自身の「全開」の定義（duration * 0.5、AnimationSystem.h参照）に
    // 合わせている - 半分以上開いていればドアは通行可能とみなす。
    float targetTime = anims[link.animIndex].duration * 0.5f;
    if (targetTime <= 0.0f) return true;

    constexpr float kOpenFraction = 0.5f;
    return it->second.ProgressTime >= targetTime * kOpenFraction;
}

void RoomVisibilityManager::UpdateVisibleRooms(const Math::Vector3& viewerPos)
{
    RebuildRoomGraphIfNeeded();
    m_visibleRoomIndices.clear();

    int currentRoom = -1;
    for (size_t i = 0; i < m_rooms.size(); ++i)
    {
        if (m_rooms[i] && m_rooms[i]->IsInside(viewerPos))
        {
            currentRoom = static_cast<int>(i);
            break;
        }
    }

    if (currentRoom < 0)
    {
        // カメラがどの既知の部屋にも入っていない（FreeCamで家の外に出た、部屋データが
        // まだ無い等）- 安全側に倒して、レベル全体を誤って隠すよりはカリングしない。
        m_allVisible = true;
        return;
    }

    m_allVisible = false;
    m_visibleRoomIndices.insert(currentRoom);
    for (int adj : m_adjacency[currentRoom])
    {
        auto it = m_edgeDoors.find(EdgeKey(currentRoom, adj));
        bool passable = (it == m_edgeDoors.end()) || IsDoorOpen(it->second);
        if (passable) m_visibleRoomIndices.insert(adj);
    }
}

bool RoomVisibilityManager::IsMeshInVisibleRoom(Mesh* pMesh, const Math::Matrix& meshWorldTransform)
{
    if (m_allVisible || m_rooms.empty()) return true;

    auto it = m_meshRoomIndices.find(pMesh);
    if (it == m_meshRoomIndices.end())
    {
        DirectX::BoundingBox worldBounds;
        pMesh->GetLocalAABB().Transform(worldBounds, meshWorldTransform);

        std::vector<int> rooms;
        for (size_t i = 0; i < m_paddedRoomBoxes.size(); ++i)
        {
            if (m_rooms[i] && worldBounds.Intersects(m_paddedRoomBoxes[i]))
            {
                rooms.push_back(static_cast<int>(i));
            }
        }
        it = m_meshRoomIndices.emplace(pMesh, std::move(rooms)).first;
    }

    if (it->second.empty()) return true; // どの部屋にも属さなかった - 推測せず、カリングもしない

    for (int roomIdx : it->second)
    {
        if (m_visibleRoomIndices.count(roomIdx) != 0) return true;
    }
    return false;
}
