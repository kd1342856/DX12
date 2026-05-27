#pragma once
#include "../../DirectX/Utility/ClassAssembly.h"
#include "../ECS/Components/Data/ColliderData.h"
#include <memory>
#include <vector>

class GameObject;
class RenderTarget;

struct RaycastHit {
    bool hit = false;
    float distance = 0.0f;
    DirectX::SimpleMath::Vector3 point;
    DirectX::SimpleMath::Vector3 normal;
    std::shared_ptr<GameObject> hitObject = nullptr;
};

class CollisionManager {
public:
    static CollisionManager& Instance() {
        static CollisionManager instance;
        return instance;
    }

    void Init();
    void Shutdown();

    // Perform a raycast against all ColliderComponents in the scene
    RaycastHit Raycast(const DirectX::SimpleMath::Vector3& origin, const DirectX::SimpleMath::Vector3& direction, float maxDistance = 1000.0f);

    // Debug Drawing
    void SetDebugWireEnabled(bool enabled) { m_debugWireEnabled = enabled; }
    bool IsDebugWireEnabled() const { return m_debugWireEnabled; }
    
    // Call this in the GameScene to draw the wireframes
    void DrawDebugWires(RenderTarget* pRenderTarget = nullptr);

private:
    CollisionManager();
    ~CollisionManager();

    bool m_debugWireEnabled = true;

    class DebugRendererImpl;
    std::unique_ptr<DebugRendererImpl> m_pDebugRenderer;
};