#pragma once

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>

class RoomArea;
class Mesh;
using Entity = uint32_t;

// ポータル/ルーム単位のオクルージョンカリング。フラスタムカリングだけだと、視錐台には
// 入っているが壁に隠れて見えないはずの部屋まで描いてしまう（例: 家の外から見た時、
// 反対側の部屋も視錐台上は「見えている」扱いになる）。これはその上にもう1段フィルタを
// 重ねるもので、カメラが今いる部屋のメッシュ、または開いているドア経由で辿れる部屋の
// メッシュだけを可視とする。
//
// NavMesh用に既に配置されているRoomArea（GameSequence::GetRooms()）をそのまま流用する
// ので、レベル側の追加作業は不要。部屋同士の隣接関係は、AABBが壁を共有できるくらい
// 近いかどうかで一度だけ推定する。各隣接エッジは、位置ベースで"Door..."という名前の
// アニメーションを持つドア（Playerの TryInteractDoor が使っているのと同じ検出方法）と
// 突き合わせる。該当するドアが見つかればその開閉状態（RuntimeAnimationData::ProgressTime）
// で可視性を制御し、ドアが見つからないエッジ（開いた通路など）は常に通行可能扱いにする。
class RoomVisibilityManager
{
public:
    static RoomVisibilityManager& Instance();

    // RenderSceneごとに1回（メッシュごとではなく）カメラのワールド座標を渡して呼ぶ。
    // カメラが今どの部屋にいるかを判定し、可視部屋の集合を更新する。
    void UpdateVisibleRooms(const Math::Vector3& viewerPos);

    // pMesh（ノード/エンティティはmeshWorldTransformの位置）が、現在可視な部屋に属しているか。
    // メッシュは、そのワールド空間AABBが重なっている部屋「すべて」に割り当てる（中心点を
    // 含む部屋1つだけではない）。そうしないと、窓や壁のパネルのように2部屋にまたがる境界
    // ジオメトリが、たまたま勝った方の部屋に固定されてしまい、すぐ隣に立っているのに
    // 消えてしまうことがある。この判定は一度だけ行いキャッシュするので、静的なジオメトリ
    // にのみ呼び出すこと（動くものについては ModelData::IsNodeAnimated を参照）。
    // どの部屋にも重ならないメッシュ（部屋データが全く無い場合や、視点がどの部屋にも
    // 入っていない場合も含む）はここではカリングされず、呼び出し側のフラスタム判定にのみ従う。
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
    std::vector<DirectX::BoundingBox> m_paddedRoomBoxes; // 外側にパディングした部屋のAABB - 隣接判定/ドア突き合わせ/メッシュ割り当てで共有
    std::vector<std::vector<int>> m_adjacency;         // 部屋インデックス -> 隣接する部屋インデックスのリスト
    std::unordered_map<uint64_t, DoorLink> m_edgeDoors; // EdgeKey(i,j) -> ドア（該当するドアが見つかったエッジのみ）
    std::unordered_map<Mesh*, std::vector<int>> m_meshRoomIndices; // 空 = どの部屋にも割り当てられなかった
    std::unordered_set<int> m_visibleRoomIndices;
    bool m_graphBuilt = false;
    bool m_allVisible = true; // UpdateVisibleRoomsがカメラを既知の部屋の中に見つけるまではtrue
};
