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
#include "../../ECS/CompSystem/System.h"
#include <DirectXColors.h>

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

