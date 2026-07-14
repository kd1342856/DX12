#include "../../Pch.h"
#include "ComponentSerializerRegistration.h"
#include "ComponentSerializerRegistry.h"
#include "ECS.h"
#include "../Manager/GameManager.h"
#include "../Manager/Scene/SceneManager.h"
#include "../Manager/Scene/Scene.h"
#include "../Manager/Collision/CollisionShape.h"
#include "../Object/GameObject.h"
#include "../Manager/Resource/ResourceManager.h"

void RegisterComponentSerializers() {
    auto& reg = ComponentSerializerRegistry::Instance();

    reg.Register("TransformData",
        [](ECSCoordinator& ecs, Entity e, const nlohmann::json& cj, GameObject* obj) {
            TransformData d;
            if (cj.contains("Position")) { d.m_position.x = cj["Position"][0]; d.m_position.y = cj["Position"][1]; d.m_position.z = cj["Position"][2]; }
            else {
                if (cj.contains("PosX")) d.m_position.x = cj["PosX"];
                if (cj.contains("PosY")) d.m_position.y = cj["PosY"];
                if (cj.contains("PosZ")) d.m_position.z = cj["PosZ"];
            }
            if (cj.contains("Rotation")) { d.m_rotation.x = cj["Rotation"][0]; d.m_rotation.y = cj["Rotation"][1]; d.m_rotation.z = cj["Rotation"][2]; }
            else {
                if (cj.contains("RotX")) d.m_rotation.x = cj["RotX"];
                if (cj.contains("RotY")) d.m_rotation.y = cj["RotY"];
                if (cj.contains("RotZ")) d.m_rotation.z = cj["RotZ"];
            }
            if (cj.contains("Scale")) { d.m_scale.x = cj["Scale"][0]; d.m_scale.y = cj["Scale"][1]; d.m_scale.z = cj["Scale"][2]; }
            else {
                if (cj.contains("ScaX")) d.m_scale.x = cj["ScaX"];
                if (cj.contains("ScaY")) d.m_scale.y = cj["ScaY"];
                if (cj.contains("ScaZ")) d.m_scale.z = cj["ScaZ"];
            }
            ecs.AddComponent(e, d);
        },
        [](ECSCoordinator& ecs, Entity e, nlohmann::json& cj, GameObject* obj) -> bool {
            if (auto* p_d = ecs.TryGetComponent<TransformData>(e)) {
                cj["Position"] = {p_d->m_position.x, p_d->m_position.y, p_d->m_position.z};
                cj["Rotation"] = {p_d->m_rotation.x, p_d->m_rotation.y, p_d->m_rotation.z};
                cj["Scale"] = {p_d->m_scale.x, p_d->m_scale.y, p_d->m_scale.z};
                return true;
            }
            return false;
        });
    reg.RegisterAlias("TransformComponent", "TransformData");

    reg.Register("CameraData",
        [](ECSCoordinator& ecs, Entity e, const nlohmann::json& cj, GameObject* obj) {
            CameraData d;
            if (cj.contains("Fov")) d.m_fov = cj["Fov"];
            if (cj.contains("NearZ")) d.m_nearZ = cj["NearZ"];
            if (cj.contains("FarZ")) d.m_farZ = cj["FarZ"];
            if (cj.contains("MoveSpeed")) d.m_moveSpeed = cj["MoveSpeed"];
            if (cj.contains("CameraMode")) d.m_cameraMode = static_cast<CameraMode>(cj["CameraMode"]);
            if (cj.contains("TargetOffset")) { d.m_targetOffset = {cj["TargetOffset"][0], cj["TargetOffset"][1], cj["TargetOffset"][2]}; }
            if (cj.contains("FpsOffset")) { d.m_fpsOffset = {cj["FpsOffset"][0], cj["FpsOffset"][1], cj["FpsOffset"][2]}; }
            ecs.AddComponent(e, d);
        },
        [](ECSCoordinator& ecs, Entity e, nlohmann::json& cj, GameObject* obj) -> bool {
            if (auto* p_d = ecs.TryGetComponent<CameraData>(e)) {
                cj["Fov"] = p_d->m_fov;
                cj["NearZ"] = p_d->m_nearZ;
                cj["FarZ"] = p_d->m_farZ;
                cj["MoveSpeed"] = p_d->m_moveSpeed;
                cj["CameraMode"] = static_cast<int>(p_d->m_cameraMode);
                cj["TargetOffset"] = {p_d->m_targetOffset.x, p_d->m_targetOffset.y, p_d->m_targetOffset.z};
                cj["FpsOffset"] = {p_d->m_fpsOffset.x, p_d->m_fpsOffset.y, p_d->m_fpsOffset.z};
                return true;
            }
            return false;
        });
    reg.RegisterAlias("CameraComponent", "CameraData");

    reg.Register("ModelRenderData",
        [](ECSCoordinator& ecs, Entity e, const nlohmann::json& cj, GameObject* obj) {
            ModelRenderData d;
            if (cj.contains("ModelType")) d.m_modelType = static_cast<ModelType>(cj["ModelType"]);
            if (cj.contains("FilePath")) {
                d.m_filePath = cj["FilePath"];
                if (!d.m_filePath.empty()) { d.m_spModelData = ResourceManager::Instance().LoadModelAsync(d.m_filePath); }
            }
            ecs.AddComponent(e, d);
        },
        [](ECSCoordinator& ecs, Entity e, nlohmann::json& cj, GameObject* obj) -> bool {
            if (auto* p_d = ecs.TryGetComponent<ModelRenderData>(e)) {
                cj["FilePath"] = p_d->m_filePath;
                cj["ModelType"] = static_cast<int>(p_d->m_modelType);
                return true;
            }
            return false;
        });
    reg.RegisterAlias("ModelRendererComponent", "ModelRenderData");

    reg.Register("AnimationDataComponent",
        [](ECSCoordinator& ecs, Entity e, const nlohmann::json& cj, GameObject* obj) {
            AnimationDataComponent d;
            if (cj.contains("AnimationIndex")) d.currentAnim.AnimationIndex = cj["AnimationIndex"];
            if (cj.contains("ProgressTime")) d.currentAnim.ProgressTime = cj["ProgressTime"];
            if (cj.contains("Speed")) d.currentAnim.Speed = cj["Speed"];
            if (cj.contains("IsPlaying")) d.currentAnim.IsPlaying = cj["IsPlaying"];
            if (cj.contains("IsLoop")) d.currentAnim.IsLoop = cj["IsLoop"];
            ecs.AddComponent(e, d);
        },
        [](ECSCoordinator& ecs, Entity e, nlohmann::json& cj, GameObject* obj) -> bool {
            if (auto* p_d = ecs.TryGetComponent<AnimationDataComponent>(e)) {
                cj["AnimationIndex"] = p_d->currentAnim.AnimationIndex;
                cj["ProgressTime"] = p_d->currentAnim.ProgressTime;
                cj["Speed"] = p_d->currentAnim.Speed;
                cj["IsPlaying"] = p_d->currentAnim.IsPlaying;
                cj["IsLoop"] = p_d->currentAnim.IsLoop;
                return true;
            }
            return false;
        });
    reg.RegisterAlias("AnimationComponent", "AnimationDataComponent");

    reg.Register("ColliderData",
        [](ECSCoordinator& ecs, Entity e, const nlohmann::json& cj, GameObject* obj) {
            ColliderData d;
            if (cj.contains("IsStatic")) d.m_isStatic = cj["IsStatic"];
            if (cj.contains("Shapes")) {
                for (const auto& sj : cj["Shapes"]) {
                    int shapeId = -1;
                    if (sj.contains("ShapeId")) shapeId = sj["ShapeId"];
                    else if (sj.contains("Type")) {
                        std::string t = sj["Type"];
                        if (t == "Box") shapeId = CollisionShape::Box;
                        else if (t == "Sphere") shapeId = CollisionShape::Sphere;
                        else if (t == "Capsule") shapeId = CollisionShape::Capsule;
                        else if (t == "Mesh") shapeId = CollisionShape::Mesh;
                    }
                    if (shapeId != -1) {
                        std::shared_ptr<CollisionShape> shape;
                        if (shapeId == CollisionShape::Box) shape = std::make_shared<CollisionShapeBox>();
                        else if (shapeId == CollisionShape::Sphere) shape = std::make_shared<CollisionShapeSphere>();
                        else if (shapeId == CollisionShape::Capsule) shape = std::make_shared<CollisionShapeCapsule>();
                        else if (shapeId == CollisionShape::Mesh) shape = std::make_shared<CollisionShapeMesh>();
                        if (shape) {
                            shape->m_entity = e;
                            shape->Deserialize(sj);
                            d.m_shapes.push_back(shape);
                        }
                    }
                }
            }
            ecs.AddComponent(e, d);
        },
        [](ECSCoordinator& ecs, Entity e, nlohmann::json& cj, GameObject* obj) -> bool {
            if (auto* p_d = ecs.TryGetComponent<ColliderData>(e)) {
                cj["IsStatic"] = p_d->m_isStatic;
                nlohmann::json shapes = nlohmann::json::array();
                for (auto& shape : p_d->m_shapes) {
                    nlohmann::json sj;
                    sj["ShapeId"] = static_cast<int>(shape->GetShapeId());
                    shape->Serialize(sj);
                    shapes.push_back(sj);
                }
                cj["Shapes"] = shapes;
                return true;
            }
            return false;
        });
    reg.RegisterAlias("ColliderComponent", "ColliderData");

    reg.Register("NativeScriptData",
        [](ECSCoordinator& ecs, Entity e, const nlohmann::json& cj, GameObject* obj) {
            NativeScriptData d;
            if (cj.contains("ScriptName")) {
                d.ScriptName = cj["ScriptName"];
                auto scriptObj = GameManager::Instance().GetClassAssembly().Create(d.ScriptName);
                if (scriptObj) {
                    d.Instance = std::dynamic_pointer_cast<NativeScript>(scriptObj);
                    if (d.Instance) {
                        d.Instance->SetGameObject(obj);
                        d.Instance->Deserialize(cj);
                    }
                }
            }
            ecs.AddComponent(e, d);
        },
        [](ECSCoordinator& ecs, Entity e, nlohmann::json& cj, GameObject* obj) -> bool {
            if (auto* p_d = ecs.TryGetComponent<NativeScriptData>(e)) {
                cj["ScriptName"] = p_d->ScriptName;
                if (p_d->Instance) p_d->Instance->Serialize(cj);
                return true;
            }
            return false;
        });
    reg.RegisterAlias("ScriptComponent", "NativeScriptData");

    reg.Register("SpriteData",
        [](ECSCoordinator& ecs, Entity e, const nlohmann::json& cj, GameObject* obj) {
            SpriteData d;
            if (cj.contains("FilePath")) {
                d.m_filePath = cj["FilePath"];
                if (!d.m_filePath.empty()) { d.m_spTexture = ResourceManager::Instance().LoadTextureAsync(d.m_filePath); }
            }
            if (cj.contains("Size")) { d.m_size.x = cj["Size"][0]; d.m_size.y = cj["Size"][1]; }
            if (cj.contains("Pivot")) { d.m_pivot.x = cj["Pivot"][0]; d.m_pivot.y = cj["Pivot"][1]; }
            if (cj.contains("Color")) { d.m_color.x = cj["Color"][0]; d.m_color.y = cj["Color"][1]; d.m_color.z = cj["Color"][2]; d.m_color.w = cj["Color"][3]; }
            if (cj.contains("OrderInLayer")) d.m_orderInLayer = cj["OrderInLayer"];
            ecs.AddComponent(e, d);
        },
        [](ECSCoordinator& ecs, Entity e, nlohmann::json& cj, GameObject* obj) -> bool {
            if (auto* p_d = ecs.TryGetComponent<SpriteData>(e)) {
                cj["FilePath"] = p_d->m_filePath;
                cj["Size"] = {p_d->m_size.x, p_d->m_size.y};
                cj["Pivot"] = {p_d->m_pivot.x, p_d->m_pivot.y};
                cj["Color"] = {p_d->m_color.x, p_d->m_color.y, p_d->m_color.z, p_d->m_color.w};
                cj["OrderInLayer"] = p_d->m_orderInLayer;
                return true;
            }
            return false;
        });
}
