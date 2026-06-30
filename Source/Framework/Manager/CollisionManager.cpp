#include "CollisionManager.h"
#include <DirectXCollision.h>
#include "Scene.h"
#include "GameManager.h"
#include "../DirectX/Utility/Logger.h"



#include "../../Graphics/Geometry/Model/Model.h"
#include "../../Graphics/Geometry/Mesh/Mesh.h"
#include "../Manager/CollisionSolver.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

static CollisionManager* s_collisionSystem = nullptr;

void CollisionManager::Init() {
    s_collisionSystem = this;
}

void CollisionManager::Shutdown() {
    s_collisionSystem = nullptr;
}

void CollisionManager::Solve(Scene* scene) {
    if (!s_collisionSystem || !scene) return;
    auto& ecs = GameManager::Instance().GetECS();

    auto getRootEntity = [&](Entity e) -> Entity {
        auto obj = scene->GetGameObject(e);
        if (!obj) return e;
        GameObject* current = obj.get();
        while (current->GetParent()) {
            current = current->GetParent();
        }
        return current->GetEntityID();
    };

    std::vector<Entity> entitiesWithCollider;
    for (Entity entity = 0; entity < MAX_ENTITIES; ++entity) {
        if (!ecs.IsAlive(entity)) continue;
        if (ecs.HasComponent<TransformData>(entity) && ecs.HasComponent<ColliderData>(entity)) {
            entitiesWithCollider.push_back(entity);
        }
    }

    std::set<CollisionPair> currentCollisionPairs;

    for (size_t i = 0; i < entitiesWithCollider.size(); ++i) {
        Entity entityA = entitiesWithCollider[i];
        auto& colDataA = ecs.GetComponent<ColliderData>(entityA);
        auto& transDataA = ecs.GetComponent<TransformData>(entityA);

        for (auto& shapeA : colDataA.m_shapes) {
            shapeA->UpdateWorldAABB(transDataA.m_worldMatrix);
        }

        for (size_t j = i + 1; j < entitiesWithCollider.size(); ++j) {
            Entity entityB = entitiesWithCollider[j];
            auto& colDataB = ecs.GetComponent<ColliderData>(entityB);
            auto& transDataB = ecs.GetComponent<TransformData>(entityB);

            // 霑壹・蟀ｿ邵ｺ遒∵瀦騾ｧ繝ｻ繝ｻ陜｣・ｴ陷ｷ蛹ｻ繝ｻ邵ｲ竏晁劒騾ｧ繝ｻ竊題ｭ・ｽｹ邵ｺ・ｽ邵ｺ莉｣・定恪霈板ｰ邵ｺ繝ｻ
            if (colDataA.m_isStatic && colDataB.m_isStatic) continue;

            for (auto& shapeB : colDataB.m_shapes) {
                shapeB->UpdateWorldAABB(transDataB.m_worldMatrix);
            }

            for (auto& shapeA : colDataA.m_shapes) {
                for (auto& shapeB : colDataB.m_shapes) {
                    if (!shapeA->m_worldAABB.Intersects(shapeB->m_worldAABB)) continue;

                    CollisionResult result;
                    bool isHit = false;
                    bool swapped = false;

                    if (shapeA->GetShapeId() == CollisionShape::Mesh && shapeB->GetShapeId() != CollisionShape::Mesh) {
                        isHit = CollisionSolver::CheckCollisionShape(result, shapeB.get(), transDataB.m_worldMatrix, shapeA.get(), transDataA.m_worldMatrix);
                        swapped = true;
                    } else if (shapeA->GetShapeId() != CollisionShape::Mesh && shapeB->GetShapeId() == CollisionShape::Mesh) {
                        isHit = CollisionSolver::CheckCollisionShape(result, shapeA.get(), transDataA.m_worldMatrix, shapeB.get(), transDataB.m_worldMatrix);
                        swapped = false;
                    } else {
                        isHit = CollisionSolver::CheckCollisionShape(result, shapeA.get(), transDataA.m_worldMatrix, shapeB.get(), transDataB.m_worldMatrix);
                        swapped = false;
                    }

                    if (isHit) {
                        if (swapped) {
                            result.pushVector = -result.pushVector;
                        }

                        CollisionPair pair;
                        pair.a = entityA; pair.b = entityB;
                        currentCollisionPairs.insert(pair);

                        if (!shapeA->m_isTrigger && !shapeB->m_isTrigger) {
                            Entity rootA = getRootEntity(entityA);
                            Entity rootB = getRootEntity(entityB);
                            
                            if (ecs.HasComponent<TransformData>(rootA) && ecs.HasComponent<TransformData>(rootB)) {
                                auto& rootTransA = ecs.GetComponent<TransformData>(rootA);
                                auto& rootTransB = ecs.GetComponent<TransformData>(rootB);
                                
                                if (!colDataA.m_isStatic && !colDataB.m_isStatic) {
                                    rootTransA.m_position += result.pushVector * 0.5f;
                                    rootTransB.m_position -= result.pushVector * 0.5f;
                                } else if (!colDataA.m_isStatic) {
                                    rootTransA.m_position += result.pushVector;
                                } else if (!colDataB.m_isStatic) {
                                    rootTransB.m_position -= result.pushVector;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    m_prevCollisionPairs = currentCollisionPairs;
}

RaycastHit CollisionManager::Raycast(const Math::Vector3& origin, const Math::Vector3& direction, float maxDistance, uint32_t collisionMask) {
    RaycastHit hitResult;
    hitResult.distance = maxDistance;

    if (!s_collisionSystem) return hitResult;
    auto& ecs = GameManager::Instance().GetECS();

    RayInfo rayInfo;
    rayInfo.startPos = origin;
    rayInfo.rayDir = direction;
    rayInfo.collisionMask = collisionMask;
    rayInfo.range = maxDistance;
    

    for (Entity entity = 0; entity < MAX_ENTITIES; ++entity) {
        if (!ecs.IsAlive(entity)) continue;
        if (!ecs.HasComponent<ColliderData>(entity) || !ecs.HasComponent<TransformData>(entity)) continue;

        auto& colData = ecs.GetComponent<ColliderData>(entity);
        auto& transData = ecs.GetComponent<TransformData>(entity);

        for (const auto& shape : colData.m_shapes) {
            if ((shape->m_tags & rayInfo.collisionMask) == 0) continue;
            RayResult out;
            if (shape->RayCast(rayInfo, transData.m_worldMatrix, out)) {
                if (out.distance < hitResult.distance) {
                    hitResult.hit = true;
                    hitResult.distance = out.distance;
                    hitResult.point = out.hitPos;
                    hitResult.normal = out.hitNormal;
                    hitResult.hitEntity = entity;
                    hitResult.hitShape = shape;
                }
            }
        }
    }
    return hitResult;
}

RaycastHit CollisionManager::RaycastAgainstMesh(const Math::Vector3& origin, const Math::Vector3& direction, float maxDistance, const std::string& targetName) {
    RaycastHit hitResult;
    hitResult.distance = maxDistance;

    if (!m_pCurrentScene) return hitResult;

    std::function<void(const std::shared_ptr<GameObject>&)> checkObj = [&](const std::shared_ptr<GameObject>& obj) {
        if (!obj || !obj->IsActive()) return;

        auto& ecs = GameManager::Instance().GetECS();
        if (ecs.HasComponent<ModelRenderData>(obj->GetEntityID()) && ecs.HasComponent<TransformData>(obj->GetEntityID())) {
            auto& modelData = ecs.GetComponent<ModelRenderData>(obj->GetEntityID());
            auto& transData = ecs.GetComponent<TransformData>(obj->GetEntityID());
            
            if (modelData.m_spModelData) {
                Math::Matrix worldMat = transData.m_worldMatrix;
                Math::Matrix invWorld = worldMat.Invert();

                Math::Vector3 localOrigin = Math::Vector3::Transform(origin, invWorld);
                Math::Vector3 localDir = Math::Vector3::TransformNormal(direction, invWorld);
                localDir.Normalize();

                for (const auto& node : modelData.m_spModelData->GetNodes()) {
                    for (const auto& mesh : node.meshes) {
                        auto& vertices = mesh->GetVertices();
                        auto& faces = mesh->GetFaces();

                        for (const auto& face : faces) {
                            Math::Vector3 v0 = vertices[face.Idx[0]].Position;
                            Math::Vector3 v1 = vertices[face.Idx[1]].Position;
                            Math::Vector3 v2 = vertices[face.Idx[2]].Position;

                            float dist;
                            if (DirectX::TriangleTests::Intersects(localOrigin, localDir, v0, v1, v2, dist)) {
                                if (dist >= 0 && dist < hitResult.distance) {
                                    hitResult.hit = true;
                                    hitResult.distance = dist;
                                    hitResult.point = origin + direction * dist;

                                    Math::Vector3 edge1 = v1 - v0;
                                    Math::Vector3 edge2 = v2 - v0;
                                    Math::Vector3 normal;
                                    edge1.Cross(edge2, normal);
                                    normal = Math::Vector3::TransformNormal(normal, worldMat);
                                    normal.Normalize();
                                    hitResult.normal = normal;

                                    hitResult.hitEntity = obj->GetEntityID();
                                }
                            }
                        }
                    }
                }
            }
        }

        for (const auto& child : obj->GetChildren()) {
            checkObj(child);
        }
    };

    // 郢ｧ・ｷ郢晢ｽｼ郢晢ｽｳ陷繝ｻ繝ｻ郢ｧ・ｪ郢晄じ縺夂ｹｧ・ｧ郢ｧ・ｯ郢晏現・定ｬ暦ｽ｢驍擾ｽ｢
    auto& gameObjects = m_pCurrentScene->GetGameObjects();
    for (auto& obj : gameObjects) {
        if (!targetName.empty()) {
            // 隰悶・・ｮ螢ｼ骭千ｸｺ・ｮ郢ｧ・ｪ郢晄じ縺夂ｹｧ・ｧ郢ｧ・ｯ郢晁肩・ｼ蛹ｻ竊堤ｸｺ譏ｴ繝ｻ陝・腸・ｼ蟲ｨ繝ｻ邵ｺ・ｿ郢昶・縺臥ｹ昴・縺・
            if (obj->GetName() == targetName) {
                checkObj(obj);
            }
        } else {
            checkObj(obj);
        }
    }
    return hitResult;
}
void CollisionManager::ClearDebugLines() {
    m_debugLines.clear();
}

void CollisionManager::AddDebugLine(const Math::Vector3& start, const Math::Vector3& end, ImU32 color) {
    if (!m_debugWireEnabled) return;
    m_debugLines.push_back({ start, end, color });
}

void CollisionManager::DrawDebugWires(float screenX, float screenY, float screenW, float screenH, Entity cameraEntity) {
    if (!m_debugWireEnabled) return;
    if (cameraEntity == INVALID_ENTITY) return;

    auto& ecs = GameManager::Instance().GetECS();
    if (!ecs.HasComponent<CameraData>(cameraEntity) || !ecs.HasComponent<TransformData>(cameraEntity)) return;

    auto& cameraData = ecs.GetComponent<CameraData>(cameraEntity);
    auto& cameraTrans = ecs.GetComponent<TransformData>(cameraEntity);

    Math::Matrix viewMatrix = cameraTrans.m_worldMatrix.Invert();
    Math::Matrix projMatrix = cameraData.m_projMatrix;

    ImDrawList* pDrawList = ImGui::GetBackgroundDrawList();

        auto WorldToScreen = [&](const Math::Vector3& worldPos, ImVec2& outScreenPos) -> bool {
        Math::Vector4 clipPos;
        Math::Vector3::Transform(worldPos, viewMatrix * projMatrix, clipPos);
        if (clipPos.w < 0.1f) return false;

        float ndcX = clipPos.x / clipPos.w;
        float ndcY = -clipPos.y / clipPos.w; 

        outScreenPos.x = screenX + (ndcX + 1.0f) * 0.5f * screenW;
        outScreenPos.y = screenY + (ndcY + 1.0f) * 0.5f * screenH;
        return true;
    };

    for (const auto& line : m_debugLines) {
        ImVec2 p1, p2;
        if (WorldToScreen(line.start, p1) && WorldToScreen(line.end, p2)) {
            pDrawList->AddLine(p1, p2, line.color);
        }
    }

    if (!s_collisionSystem) return;

    for (Entity entity = 0; entity < MAX_ENTITIES; ++entity) {
        if (!ecs.IsAlive(entity)) continue;
        if (!ecs.HasComponent<ColliderData>(entity) || !ecs.HasComponent<TransformData>(entity)) continue;
        auto& colData = ecs.GetComponent<ColliderData>(entity);
        auto& transData = ecs.GetComponent<TransformData>(entity);

        for (const auto& shape : colData.m_shapes) {
                       ImU32 color = IM_COL32(0, 255, 0, 255); 
            if (shape->m_isTrigger) color = IM_COL32(255, 255, 0, 255);

            // Draw AABB for Mesh shapes (since they don't have a specific narrow-phase debug draw)
            if (shape->GetShapeId() == CollisionShape::Mesh) {
                Math::Vector3 cornersAABB[8];
                shape->m_worldAABB.GetCorners(cornersAABB);
                int edgesAABB[12][2] = {
                    {0,1}, {1,2}, {2,3}, {3,0},
                    {4,5}, {5,6}, {6,7}, {7,4},
                    {0,4}, {1,5}, {2,6}, {3,7}
                };
                ImU32 colorAABB = IM_COL32(0, 255, 255, 128); // Cyan for AABB
                for (int i = 0; i < 12; ++i) {
                    ImVec2 p1, p2;
                    if (WorldToScreen(cornersAABB[edgesAABB[i][0]], p1) && WorldToScreen(cornersAABB[edgesAABB[i][1]], p2)) {
                        pDrawList->AddLine(p1, p2, colorAABB);
                    }
                }
            }

            if (shape->GetShapeId() == CollisionShape::Box) {
                auto boxShape = std::static_pointer_cast<CollisionShapeBox>(shape);
                Math::Vector3 center = Math::Vector3::Transform(boxShape->m_offset, transData.m_worldMatrix);
                Math::Vector3 extents = { boxShape->m_width * 0.5f, boxShape->m_height * 0.5f, boxShape->m_depth * 0.5f };
                BoundingOrientedBox bob(center, extents, Quaternion::CreateFromRotationMatrix(transData.m_worldMatrix));

                Math::Vector3 corners[8];
                bob.GetCorners(corners);

                int edges[12][2] = {
                    {0,1}, {1,2}, {2,3}, {3,0},
                    {4,5}, {5,6}, {6,7}, {7,4},
                    {0,4}, {1,5}, {2,6}, {3,7}
                };

                for (int i = 0; i < 12; ++i) {
                    ImVec2 p1, p2;
                    if (WorldToScreen(corners[edges[i][0]], p1) && WorldToScreen(corners[edges[i][1]], p2)) {
                        pDrawList->AddLine(p1, p2, color);
                    }
                }
            } else if (shape->GetShapeId() == CollisionShape::Sphere) {
                auto sphereShape = std::static_pointer_cast<CollisionShapeSphere>(shape);
                Math::Vector3 center = Math::Vector3::Transform(sphereShape->m_offset, transData.m_worldMatrix);
                ImVec2 p;
                if (WorldToScreen(center, p)) {
                    pDrawList->AddCircle(p, sphereShape->radius * screenW / 10.0f, color);
                }
            }
        }
    }
}

CollisionManager& CollisionManager::Instance()
{
    static CollisionManager instance;
    return instance;
}
