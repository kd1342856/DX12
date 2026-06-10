#pragma once
#include "../../DirectX/Utility/ClassAssembly.h"
#include "../ECS/Components/Data/ColliderData.h"
#include <memory>
#include <vector>

class GameObject;
class Scene;
class RenderTarget;

struct RaycastHit {
    bool hit = false;
    float distance = 0.0f;
    DirectX::SimpleMath::Vector3 point;
    DirectX::SimpleMath::Vector3 normal;
    std::shared_ptr<GameObject> hitObject = nullptr;
    Entity hitEntity = INVALID_ENTITY;
};

class CollisionManager {
public:
    static CollisionManager& Instance() {
        static CollisionManager instance;
        return instance;
    }

    void Init();
    void Shutdown();

    void Solve(Scene* scene);

    // シーン内の全ColliderComponentに対してレイキャストを行う
    RaycastHit Raycast(const DirectX::SimpleMath::Vector3& origin, const DirectX::SimpleMath::Vector3& direction, float maxDistance = 1000.0f);

    // デバッグ描画の有効/無効
    void SetDebugWireEnabled(bool enabled) { m_debugWireEnabled = enabled; }
    bool IsDebugWireEnabled() const { return m_debugWireEnabled; }
    
    // ImGui ImDrawListを使ったワイヤーフレーム描画
    void DrawDebugWires(float screenX = 0.0f, float screenY = 0.0f, float screenW = 0.0f, float screenH = 0.0f, Entity cameraEntity = INVALID_ENTITY);

private:
    CollisionManager();
    ~CollisionManager();

    bool m_debugWireEnabled = true;

    struct CollisionPair {
        Entity a, b;
        bool operator<(const CollisionPair& o) const { if (a != o.a) return a < o.a; return b < o.b; }
    };
    std::set<CollisionPair> m_prevCollisionPairs;
};
