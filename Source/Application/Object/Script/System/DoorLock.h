#pragma once
#include <set>
#include <utility>

// 施錠中のドア(家モデル内のドアアニメーション = エンティティ + アニメーション番号)の一覧。
// ハント中に玄関ドアを閉めて開けられなくするのに使う(GhostAIが施錠・解錠し、
// Player::TryInteractDoor / GhostAI::TryOpenNearbyDoor は施錠中のドアを開けない)。
namespace DoorLock
{
    inline std::set<std::pair<Entity, int>>& LockedSet()
    {
        static std::set<std::pair<Entity, int>> s_locked;
        return s_locked;
    }

    inline void Lock(Entity entity, int animIndex) { LockedSet().insert({ entity, animIndex }); }
    inline void UnlockAll() { LockedSet().clear(); }
    inline bool IsLocked(Entity entity, int animIndex) { return LockedSet().count({ entity, animIndex }) > 0; }
}
