#include <fstream>
#include "CollisionManager.h"
#include "../../../Graphics/Device/GraphicsDevice.h"
#include "../../../Application/Application.h"
#include "../../Manager/Scene.h"
#include "../../../Application/Scene/GameScene/GameScene.h"
#include "../../Manager/GameManager.h"
#include "../../Object/GameObject.h"
#include "../../ECS/Components/TransformComponent.h"
#include "../../ECS/Components/ColliderComponent.h"
#include "../../ECS/Components/ModelRendererComponent.h"
#include "../../ECS/Components/ScriptComponent.h"
#include "../../ECS/CompSystem/System.h"
#include <DirectXColors.h>
#include <DirectXCollision.h>

using namespace DirectX;
using namespace DirectX::SimpleMath;

class CollisionSystem : public SystemBase { public: void Update() override {} };

static std::shared_ptr<CollisionSystem> s_collisionSystem;

CollisionManager::CollisionManager() {}
CollisionManager::~CollisionManager() {}

void CollisionManager::Init() {
    // ECSシステム登録
    auto& ecs = GameManager::Instance().GetECS();
    s_collisionSystem = ecs.RegisterSystem<CollisionSystem>();
    Signature sig;
    sig.set(ecs.GetComponentType<ColliderData>());
    sig.set(ecs.GetComponentType<TransformData>());
    ecs.SetSystemSignature<CollisionSystem>(sig);
}

void CollisionManager::Shutdown() {
    s_collisionSystem.reset();
}

// ============================================
// レイキャスト（変更なし）
// ============================================
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

// ============================================
// ImGui描画ヘルパー（ファイルスコープstatic関数）
// ============================================

// ワールド座標→スクリーン座標変換
// ViewProjection行列で3D座標をNDCに変換し、スクリーンピクセル座標にする
// カメラ背後の頂点はfalseを返してクリッピング
static bool WorldToScreen(const Vector3& worldPos,
                           const Matrix& mViewProj,
                           float screenX, float screenY, float screenW, float screenH,
                           ImVec2& outScreen) {
    // ワールド座標をクリップ空間に変換
    Vector4 clipPos = Vector4::Transform(
        Vector4(worldPos.x, worldPos.y, worldPos.z, 1.0f), mViewProj);

    // ニアプレーン手前（カメラ背後）ならクリッピング
    if (clipPos.w <= 0.001f) return false;

    // 透視除算でNDC(-1?+1)に変換
    float invW = 1.0f / clipPos.w;
    float ndcX = clipPos.x * invW;
    float ndcY = clipPos.y * invW;

    // NDC → スクリーンピクセル座標
    // LH座標系: NDC.x=-1が左端, +1が右端 / NDC.y=+1が上端, -1が下端
    outScreen.x = screenX + (ndcX * 0.5f + 0.5f) * screenW;
    outScreen.y = screenY + (1.0f - (ndcY * 0.5f + 0.5f)) * screenH;

    return true;
}

// ワイヤーキューブ描画（8頂点→12辺）
// BoundingBoxのGetCornersで取得した8頂点をスクリーン投影して12本のエッジを描画
static void DrawWireCube(ImDrawList* pDrawList,
                          const Vector3* corners,
                          ImU32 color,
                          const Matrix& mViewProj,
                          float screenX, float screenY, float screenW, float screenH) {
    // 8頂点をスクリーン座標に変換
    ImVec2 screenPts[8];
    bool visible[8];
    for (int idx = 0; idx < 8; ++idx) {
        visible[idx] = WorldToScreen(corners[idx], mViewProj, screenX, screenY, screenW, screenH, screenPts[idx]);
    }

    // キューブの12辺の接続テーブル
    // DirectXのGetCornersの頂点順序に基づく
    static const int edges[12][2] = {
        // 上面の4辺
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        // 下面の4辺
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        // 上面と下面を繋ぐ4辺
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };

    float lineThickness = 2.0f;
    for (int edgeIdx = 0; edgeIdx < 12; ++edgeIdx) {
        int vertA = edges[edgeIdx][0];
        int vertB = edges[edgeIdx][1];
        // 両端の頂点がカメラ前方にある場合のみ描画
        if (visible[vertA] && visible[vertB]) {
            pDrawList->AddLine(screenPts[vertA], screenPts[vertB], color, lineThickness);
        }
    }
}

// ワイヤースフィア描画（3つの円リング: XY/XZ/YZ平面）
// 各平面に円を描画して球のワイヤーフレームを表現
static void DrawWireSphere(ImDrawList* pDrawList,
                            const BoundingSphere& sphere,
                            ImU32 color,
                            const Matrix& mViewProj,
                            float screenX, float screenY, float screenW, float screenH) {
    const int segments = 24;
    float lineThickness = 2.0f;
    Vector3 center(sphere.Center.x, sphere.Center.y, sphere.Center.z);
    float rad = sphere.Radius;

    // 3つの平面リングを描画
    for (int ring = 0; ring < 3; ++ring) {
        ImVec2 prevScreen;
        bool prevVisible = false;

        for (int seg = 0; seg <= segments; ++seg) {
            float angle = XM_2PI * seg / segments;
            float cosA = cosf(angle);
            float sinA = sinf(angle);

            Vector3 point;
            if (ring == 0) {
                // XY平面リング
                point = center + Vector3(cosA * rad, sinA * rad, 0.0f);
            } else if (ring == 1) {
                // XZ平面リング
                point = center + Vector3(cosA * rad, 0.0f, sinA * rad);
            } else {
                // YZ平面リング
                point = center + Vector3(0.0f, cosA * rad, sinA * rad);
            }

            ImVec2 curScreen;
            bool curVisible = WorldToScreen(point, mViewProj, screenX, screenY, screenW, screenH, curScreen);

            // 前の頂点と現在の頂点が両方カメラ前方なら線を引く
            if (seg > 0 && prevVisible && curVisible) {
                pDrawList->AddLine(prevScreen, curScreen, color, lineThickness);
            }

            prevScreen = curScreen;
            prevVisible = curVisible;
        }
    }
}

// ============================================
// メイン描画関数: ImGui BackgroundDrawListでオーバーレイ描画
// ============================================
void CollisionManager::DrawDebugWires(float screenX, float screenY, float screenW, float screenH, Entity cameraEntity) {
    if (!m_debugWireEnabled || !s_collisionSystem) return;

    auto& ecs = GameManager::Instance().GetECS();

    // アクティブなView/Projectionを取得
    if (cameraEntity == INVALID_ENTITY) {
        auto spScene = Application::Instance().GetScene();
        if (spScene) {
            auto pGameScene = std::dynamic_pointer_cast<GameScene>(spScene);
            if (pGameScene) {
                auto pInnerScene = pGameScene->GetScene();
                if (pInnerScene) {
                    cameraEntity = pInnerScene->GetRenderSystem()->GetCameraEntity();
                }
            }
        }
    }

    if (cameraEntity == INVALID_ENTITY) return;

    auto& camData = ecs.GetComponent<CameraData>(cameraEntity);
    Matrix mViewProj = camData.m_viewMatrix * camData.m_projMatrix;

    // スクリーンサイズ取得（ImGuiのメインビューポートから）
    ImGuiViewport* pViewport = ImGui::GetMainViewport();
    if (screenW <= 0.0f) screenW = pViewport->Size.x;
    if (screenH <= 0.0f) screenH = pViewport->Size.y;

    // BackgroundDrawListでImGuiウィンドウに依存しないオーバーレイ描画
    ImDrawList* pDrawList = ImGui::GetForegroundDrawList();
    if (!pDrawList) return;

    // コライダーの色設定（半透明の緑色で視認性向上）
    ImU32 wireColor = IM_COL32(0, 255, 100, 200);

    // 全コライダーEntityを走査して描画
    for (auto entity : s_collisionSystem->m_entities) {
        auto& colData = ecs.GetComponent<ColliderData>(entity);
        auto& transData = ecs.GetComponent<TransformData>(entity);

        for (const auto& shape : colData.m_shapes) {
            if (shape.type == ColliderType::AABB) {
                // AABB: バウンディングボックスをワールド変換して描画
                BoundingBox box(shape.offset, shape.extents);
                box.Transform(box, transData.m_worldMatrix);
                Vector3 corners[8];
                box.GetCorners(corners);
                DrawWireCube(pDrawList, corners, wireColor, mViewProj, screenX, screenY, screenW, screenH);

            } else if (shape.type == ColliderType::OBB) {
                // OBB: 向き付きバウンディングボックスをワールド変換して描画
                BoundingOrientedBox box(shape.offset, shape.extents, Quaternion::Identity);
                box.Transform(box, transData.m_worldMatrix);
                Vector3 corners[8];
                box.GetCorners(corners);
                DrawWireCube(pDrawList, corners, wireColor, mViewProj, screenX, screenY, screenW, screenH);

            } else if (shape.type == ColliderType::Sphere) {
                // Sphere: バウンディングスフィアをワールド変換して描画
                BoundingSphere bSphere(shape.offset, shape.radius);
                bSphere.Transform(bSphere, transData.m_worldMatrix);
                DrawWireSphere(pDrawList, bSphere, wireColor, mViewProj, screenX, screenY, screenW, screenH);
            }
        }
    }
}

static std::shared_ptr<ModelData> FindModelData(GameObject* pObj) {
    if (!pObj) return nullptr;
    if (auto pModelComp = pObj->GetComponent<ModelRendererComponent>()) {
        if (pModelComp->GetData().m_spModelData) return pModelComp->GetData().m_spModelData;
    }
    if (auto pParent = pObj->GetParent()) {
        if (auto pModelComp = pParent->GetComponent<ModelRendererComponent>()) {
            if (pModelComp->GetData().m_spModelData) return pModelComp->GetData().m_spModelData;
        }
        for (auto& child : pParent->GetChildren()) {
            if (auto pModelComp = child->GetComponent<ModelRendererComponent>()) {
                if (pModelComp->GetData().m_spModelData) return pModelComp->GetData().m_spModelData;
            }
        }
    }
    for (auto& child : pObj->GetChildren()) {
        if (auto pModelComp = child->GetComponent<ModelRendererComponent>()) {
            if (pModelComp->GetData().m_spModelData) return pModelComp->GetData().m_spModelData;
        }
    }
    return nullptr;
}

void CollisionManager::Solve(Scene* scene) {
    if (!s_collisionSystem) return;

    auto& ecs = GameManager::Instance().GetECS();
    auto& entities = s_collisionSystem->m_entities;

    for (auto itA = entities.begin(); itA != entities.end(); ++itA) {
        auto itB = itA;
        ++itB;
        for (; itB != entities.end(); ++itB) {
            Entity a = *itA;
            Entity b = *itB;

            auto& colA = ecs.GetComponent<ColliderData>(a);
            auto& colB = ecs.GetComponent<ColliderData>(b);
            auto& transA = ecs.GetComponent<TransformData>(a);
            auto& transB = ecs.GetComponent<TransformData>(b);

            bool isHit = false;

            if (colA.m_useModelBounds || colB.m_useModelBounds) {
                bool aIsMesh = colA.m_useModelBounds;
                auto& meshCol = aIsMesh ? colA : colB;
                auto& aabbCol = aIsMesh ? colB : colA;
                auto& meshTrans = aIsMesh ? transA : transB;
                auto& aabbTrans = aIsMesh ? transB : transA;
                Entity meshEnt = aIsMesh ? a : b;
                Entity aabbEnt = aIsMesh ? b : a;

                GameObject* meshObj = scene->GetGameObject(meshEnt).get();
                GameObject* aabbObj = scene->GetGameObject(aabbEnt).get();

                if (meshObj && aabbObj && !aabbCol.m_shapes.empty() && aabbCol.m_shapes[0].type == ColliderType::AABB) {
                    auto spModelData = FindModelData(meshObj);
                    if (spModelData) {
                        DirectX::BoundingBox boxB(aabbCol.m_shapes[0].offset, aabbCol.m_shapes[0].extents);
                        boxB.Transform(boxB, aabbTrans.m_worldMatrix);

                        Math::Vector3 totalPush(0, 0, 0);
                        bool hitMesh = false;

                        for (auto& node : spModelData->GetNodes()) {
                            for (auto& spMesh : node.meshes) {
                                auto& vertices = spMesh->GetVertices();
                                auto& faces = spMesh->GetFaces();

                                for (auto& face : faces) {
                                    Math::Vector3 v0 = Math::Vector3::Transform(vertices[face.Idx[0]].Position, meshTrans.m_worldMatrix);
                                    Math::Vector3 v1 = Math::Vector3::Transform(vertices[face.Idx[1]].Position, meshTrans.m_worldMatrix);
                                    Math::Vector3 v2 = Math::Vector3::Transform(vertices[face.Idx[2]].Position, meshTrans.m_worldMatrix);

                                    DirectX::XMVECTOR vec0 = DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(&v0));
                                    DirectX::XMVECTOR vec1 = DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(&v1));
                                    DirectX::XMVECTOR vec2 = DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(&v2));

                                    if (boxB.Intersects(vec0, vec1, vec2)) {
                                        Math::Vector3 N = (v1 - v0).Cross(v2 - v0);
                                        N.Normalize();
                                        if (N.Dot(boxB.Center - v0) < 0) N = -N;

                                        float r = boxB.Extents.x * abs(N.x) + boxB.Extents.y * abs(N.y) + boxB.Extents.z * abs(N.z);
                                        float dist = N.Dot(boxB.Center - v0);
                                        float pen = r - dist;

                                        if (pen > 0) {
                                            hitMesh = true;
                                            isHit = true;
                                            totalPush += N * pen;
                                            boxB.Center.x += N.x * pen;
                                            boxB.Center.y += N.y * pen;
                                            boxB.Center.z += N.z * pen;
                                        }
                                    }
                                }
                            }
                        }

                        if (hitMesh && !meshCol.m_isTrigger && !aabbCol.m_isTrigger) {
                            GameObject* root = aabbObj;
                            while (root && root->GetParent()) root = root->GetParent();
                            if (root) {
                                auto& rootTrans = GameManager::Instance().GetECS().GetComponent<TransformData>(root->GetEntityID());
                                rootTrans.m_position += totalPush;
                                rootTrans.m_worldMatrix.Translation(rootTrans.m_position);
                            }
                        }
                    }
                }
            } else {
                for (const auto& shapeA : colA.m_shapes) {
                    for (const auto& shapeB : colB.m_shapes) {
                        if (shapeA.type == ColliderType::AABB && shapeB.type == ColliderType::AABB) {
                            DirectX::BoundingBox boxA(shapeA.offset, shapeA.extents);
                            DirectX::BoundingBox boxB(shapeB.offset, shapeB.extents);
                            boxA.Transform(boxA, transA.m_worldMatrix);
                            boxB.Transform(boxB, transB.m_worldMatrix);

                            if (boxA.Intersects(boxB)) {
                                isHit = true;
                                if (!colA.m_isTrigger && !colB.m_isTrigger) {
                                    float overlapX = (boxA.Extents.x + boxB.Extents.x) - abs(boxA.Center.x - boxB.Center.x);
                                    float overlapY = (boxA.Extents.y + boxB.Extents.y) - abs(boxA.Center.y - boxB.Center.y);
                                    float overlapZ = (boxA.Extents.z + boxB.Extents.z) - abs(boxA.Center.z - boxB.Center.z);

                                    if (overlapX > 0 && overlapY > 0 && overlapZ > 0) {
                                        Math::Vector3 push(0,0,0);
                                        if (overlapX < overlapY && overlapX < overlapZ) push.x = (boxA.Center.x > boxB.Center.x) ? overlapX : -overlapX;
                                        else if (overlapY < overlapX && overlapY < overlapZ) push.y = (boxA.Center.y > boxB.Center.y) ? overlapY : -overlapY;
                                        else push.z = (boxA.Center.z > boxB.Center.z) ? overlapZ : -overlapZ;

                                        float pushA = colA.m_isStatic ? 0.0f : (colB.m_isStatic ? 1.0f : 0.5f);
                                        float pushB = colB.m_isStatic ? 0.0f : (colA.m_isStatic ? 1.0f : 0.5f);

                                        GameObject* rootA = scene->GetGameObject(a).get();
                                        while (rootA && rootA->GetParent()) rootA = rootA->GetParent();
                                        GameObject* rootB = scene->GetGameObject(b).get();
                                        while (rootB && rootB->GetParent()) rootB = rootB->GetParent();

                                        if (!rootA || !rootB) continue;

                                        auto& rootTransA = GameManager::Instance().GetECS().GetComponent<TransformData>(rootA->GetEntityID());
                                        auto& rootTransB = GameManager::Instance().GetECS().GetComponent<TransformData>(rootB->GetEntityID());

                                        rootTransA.m_position += push * pushA;
                                        rootTransB.m_position -= push * pushB;

                                        rootTransA.m_worldMatrix.Translation(rootTransA.m_position);
                                        rootTransB.m_worldMatrix.Translation(rootTransB.m_position);
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if (isHit && scene) {
                auto objA = scene->GetGameObject(a);
                auto objB = scene->GetGameObject(b);
                if (objA && objB) {
                    GameObject* rootA = objA.get();
                    while (rootA && rootA->GetParent()) rootA = rootA->GetParent();
                    GameObject* rootB = objB.get();
                    while (rootB && rootB->GetParent()) rootB = rootB->GetParent();

                    if (auto scriptA = rootA->GetComponent<ScriptComponent>()) {
                        if (colA.m_isTrigger || colB.m_isTrigger) scriptA->OnTriggerEnter(rootB);
                        else scriptA->OnCollisionEnter(rootB);
                    }
                    if (auto scriptB = rootB->GetComponent<ScriptComponent>()) {
                        if (colA.m_isTrigger || colB.m_isTrigger) scriptB->OnTriggerEnter(rootA);
                        else scriptB->OnCollisionEnter(rootA);
                    }
                }
            }
        }
    }
}
