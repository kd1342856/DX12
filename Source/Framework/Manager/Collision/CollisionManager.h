#pragma once

class GameObject;
class Scene;
class RenderTarget;

#include "CollisionShape.h"

struct RaycastHit {
    bool hit = false;
    float distance = 0.0f;
    Math::Vector3 point;
    Math::Vector3 normal;
    Entity hitEntity = INVALID_ENTITY;
    std::weak_ptr<CollisionShape> hitShape;
};

class CollisionManager {
public:

    static CollisionManager& Instance();

    void Init();
    void Shutdown();

    void NotifyDestroy(Entity entity);

    void Solve(Scene* scene);

    // ColliderComponentに対してレイキャスト
    RaycastHit Raycast(const Math::Vector3& origin, const Math::Vector3& direction, float maxDistance = 1000.0f, uint32_t collisionMask = 0xFFFFFFFF);

    // メッシュポリゴンに対して直接レイキャスト（ColliderComponent不要）
    RaycastHit RaycastAgainstMesh(const Math::Vector3& origin, const Math::Vector3& direction, float maxDistance, const std::string& targetName = "");

    // 現在のシーンを設定
    void SetScene(Scene* scene) { m_pCurrentScene = scene; }
    Scene* GetCurrentScene() const { return m_pCurrentScene; }

    // デバッグ描画の有効/無効
    void SetDebugWireEnabled(bool enabled) { m_debugWireEnabled = enabled; }
    bool IsDebugWireEnabled() const { return m_debugWireEnabled; }

    // デバッグラインの追加
    void AddDebugLine(const Math::Vector3& start, const Math::Vector3& end, ImU32 color);
    void ClearDebugLines();

    // ImGui ImDrawListを使ったワイヤーフレーム描画
    void DrawDebugWires(float screenX = 0.0f, float screenY = 0.0f, float screenW = 0.0f, float screenH = 0.0f, Entity cameraEntity = INVALID_ENTITY, struct ImDrawList* pDrawList = nullptr);

private:
    struct DebugLine {
        Math::Vector3 start;
        Math::Vector3 end;
        ImU32 color;
    };
    std::vector<DebugLine> m_debugLines;

    CollisionManager() {}
    ~CollisionManager() {}

    bool m_debugWireEnabled = true;
    Scene* m_pCurrentScene = nullptr;

    struct CollisionPair {
        Entity a, b;
        bool operator<(const CollisionPair& o) const { if (a != o.a) return a < o.a; return b < o.b; }
    };
    std::set<CollisionPair> m_prevCollisionPairs;
};

