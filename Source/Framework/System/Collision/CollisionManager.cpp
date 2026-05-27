#include "CollisionManager.h"
#include "../../../Graphics/Device/GraphicsDevice.h"
#include "../../../Application/Application.h"
#include "../../Manager/Scene.h"
#include "../../../Application/Scene/GameScene/GameScene.h"
#include "../../Manager/GameManager.h"
#include "../../Object/GameObject.h"
#include "../../ECS/Components/TransformComponent.h"
#include "../../ECS/Components/ColliderComponent.h"
#include "../../ECS/CompSystem/System.h"
#include <PrimitiveBatch.h>
#include <VertexTypes.h>
#include <Effects.h>
#include <CommonStates.h>
#include <DirectXColors.h>

using namespace DirectX;
using namespace DirectX::SimpleMath;

class CollisionSystem : public SystemBase { public: void Update() override {} };

class CollisionManager::DebugRendererImpl {
public:
    std::unique_ptr<PrimitiveBatch<VertexPositionColor>> m_batch;
    std::unique_ptr<BasicEffect> m_effect;

    void Init() {
        auto device = GraphicsDevice::Instance().GetDevice();
        
        m_batch = std::make_unique<PrimitiveBatch<VertexPositionColor>>(device);
        
        RenderTargetState rtState(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D32_FLOAT);
        EffectPipelineStateDescription pd(
            &VertexPositionColor::InputLayout,
            CommonStates::Opaque,
            CommonStates::DepthDefault,
            CommonStates::CullNone,
            rtState,
            D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE
        );
        
        m_effect = std::make_unique<BasicEffect>(device, EffectFlags::VertexColor, pd);
    }

    void DrawShape(const ColliderShape& shape, const Matrix& worldMat, const XMVECTORF32& color) {
        if (!m_batch) return;

        if (shape.type == ColliderType::AABB) {
            BoundingBox box(shape.offset, shape.extents);
            box.Transform(box, worldMat);
            Vector3 corners[8];
            box.GetCorners(corners);
            DrawCube(corners, color);
        } else if (shape.type == ColliderType::OBB) {
            BoundingOrientedBox box(shape.offset, shape.extents, Quaternion::Identity);
            box.Transform(box, worldMat);
            Vector3 corners[8];
            box.GetCorners(corners);
            DrawCube(corners, color);
        } else if (shape.type == ColliderType::Sphere) {
            BoundingSphere sphere(shape.offset, shape.radius);
            sphere.Transform(sphere, worldMat);
            DrawSphere(sphere, color);
        }
    }

private:
    void DrawCube(const Vector3* corners, const XMVECTORF32& color) {
        VertexPositionColor v[8];
        for(int i=0; i<8; ++i) v[i] = VertexPositionColor(corners[i], color);

        m_batch->DrawLine(v[0], v[1]); m_batch->DrawLine(v[1], v[2]);
        m_batch->DrawLine(v[2], v[3]); m_batch->DrawLine(v[3], v[0]);
        m_batch->DrawLine(v[4], v[5]); m_batch->DrawLine(v[5], v[6]);
        m_batch->DrawLine(v[6], v[7]); m_batch->DrawLine(v[7], v[4]);
        m_batch->DrawLine(v[0], v[4]); m_batch->DrawLine(v[1], v[5]);
        m_batch->DrawLine(v[2], v[6]); m_batch->DrawLine(v[3], v[7]);
    }

    void DrawSphere(const BoundingSphere& sphere, const XMVECTORF32& color) {
        const int segments = 16;
        for (int i=0; i<segments; ++i) {
            float angle1 = XM_2PI * i / segments;
            float angle2 = XM_2PI * (i+1) / segments;
            
            m_batch->DrawLine(
                VertexPositionColor(sphere.Center + Vector3(cos(angle1)*sphere.Radius, sin(angle1)*sphere.Radius, 0), color),
                VertexPositionColor(sphere.Center + Vector3(cos(angle2)*sphere.Radius, sin(angle2)*sphere.Radius, 0), color)
            );
            m_batch->DrawLine(
                VertexPositionColor(sphere.Center + Vector3(cos(angle1)*sphere.Radius, 0, sin(angle1)*sphere.Radius), color),
                VertexPositionColor(sphere.Center + Vector3(cos(angle2)*sphere.Radius, 0, sin(angle2)*sphere.Radius), color)
            );
            m_batch->DrawLine(
                VertexPositionColor(sphere.Center + Vector3(0, cos(angle1)*sphere.Radius, sin(angle1)*sphere.Radius), color),
                VertexPositionColor(sphere.Center + Vector3(0, cos(angle2)*sphere.Radius, sin(angle2)*sphere.Radius), color)
            );
        }
    }
};

static std::shared_ptr<CollisionSystem> s_collisionSystem;

CollisionManager::CollisionManager() {}
CollisionManager::~CollisionManager() {}

void CollisionManager::Init() {
    m_pDebugRenderer = std::make_unique<DebugRendererImpl>();
    m_pDebugRenderer->Init();

    // Register System
    auto& ecs = GameManager::Instance().GetECS();
    s_collisionSystem = ecs.RegisterSystem<CollisionSystem>();
    Signature sig;
    sig.set(ecs.GetComponentType<ColliderData>());
    sig.set(ecs.GetComponentType<TransformData>());
    ecs.SetSystemSignature<CollisionSystem>(sig);
}

void CollisionManager::Shutdown() {
    m_pDebugRenderer.reset();
    s_collisionSystem.reset();
}

RaycastHit CollisionManager::Raycast(const Vector3& origin, const Vector3& direction, float maxDistance) {
    RaycastHit hitResult;
    hitResult.distance = maxDistance;

    if (!s_collisionSystem) return hitResult;

    auto& ecs = GameManager::Instance().GetECS();
    for (auto entity : s_collisionSystem->m_entities) {
        auto& colData = ecs.GetComponent<ColliderData>(entity);
        auto& transData = ecs.GetComponent<TransformData>(entity);

        for (const auto& shape : colData.m_shapes) {
            float dist = -1.0f;
            if (shape.type == ColliderType::AABB) {
                BoundingBox box(shape.offset, shape.extents);
                box.Transform(box, transData.m_worldMatrix);
                if (box.Intersects(origin, direction, dist) && dist < hitResult.distance) {
                    hitResult.hit = true;
                    hitResult.distance = dist;
                    hitResult.point = origin + direction * dist;
                    hitResult.normal = Vector3::Up; 
                }
            } else if (shape.type == ColliderType::OBB) {
                BoundingOrientedBox box(shape.offset, shape.extents, Quaternion::Identity);
                box.Transform(box, transData.m_worldMatrix);
                if (box.Intersects(origin, direction, dist) && dist < hitResult.distance) {
                    hitResult.hit = true;
                    hitResult.distance = dist;
                    hitResult.point = origin + direction * dist;
                    hitResult.normal = Vector3::Up;
                }
            } else if (shape.type == ColliderType::Sphere) {
                BoundingSphere sphere(shape.offset, shape.radius);
                sphere.Transform(sphere, transData.m_worldMatrix);
                if (sphere.Intersects(origin, direction, dist) && dist < hitResult.distance) {
                    hitResult.hit = true;
                    hitResult.distance = dist;
                    hitResult.point = origin + direction * dist;
                    hitResult.normal = hitResult.point - sphere.Center;
                    hitResult.normal.Normalize();
                }
            }
        }
    }
    return hitResult;
}

void CollisionManager::DrawDebugWires(RenderTarget* pRenderTarget) {
    if (!m_debugWireEnabled || !m_pDebugRenderer->m_batch || !s_collisionSystem) return;

    auto cmdList = GraphicsDevice::Instance().GetCmdList();
    auto& ecs = GameManager::Instance().GetECS();

    // Get Active Camera View/Proj
    auto spScene = Application::Instance().GetScene();
    if (!spScene) return;

    auto pScene = std::dynamic_pointer_cast<Scene>(spScene);
    if (!pScene) return;
    Entity cameraEntity = pScene->GetRenderSystem()->GetCameraEntity();
    if (cameraEntity == INVALID_ENTITY) return;

    auto& camData = ecs.GetComponent<CameraData>(cameraEntity);
    m_pDebugRenderer->m_effect->SetView(camData.m_viewMatrix);
    m_pDebugRenderer->m_effect->SetProjection(camData.m_projMatrix);
    m_pDebugRenderer->m_effect->Apply(cmdList);

    m_pDebugRenderer->m_batch->Begin(cmdList);

    for (auto entity : s_collisionSystem->m_entities) {
        auto& colData = ecs.GetComponent<ColliderData>(entity);
        auto& transData = ecs.GetComponent<TransformData>(entity);

        for (const auto& shape : colData.m_shapes) {
            m_pDebugRenderer->DrawShape(shape, transData.m_worldMatrix, Colors::Red);
        }
    }

    m_pDebugRenderer->m_batch->End();
}