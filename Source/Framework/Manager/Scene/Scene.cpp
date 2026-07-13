#include "../../../Pch.h"
#include "Scene.h"
#include "../Collision/CollisionManager.h"
#include "../Resource/PrefabManager.h"

Scene::Scene() {}
Scene::~Scene() {}

std::shared_ptr<GameObject> Scene::CreateGameObject(const std::string& name) {
    auto obj = std::make_shared<GameObject>();
    obj->SetName(name);
    obj->SetScene(this);
    auto& ecs = GameManager::Instance().GetECS();
    Entity id = ecs.CreateEntity();
    obj->SetEntityID(id);
    ecs.AddComponent(id, TransformData{});
    m_gameObjects.push_back(obj);
    RegisterGameObject(id, obj);
    return obj;
}

std::shared_ptr<GameObject> Scene::Instantiate(const std::string& filepath, const Math::Vector3& position) {
    auto json = PrefabManager::Instance().GetPrefab(filepath);
    if (json.is_null()) return nullptr;

    size_t prevSize = m_gameObjects.size();
    
    // JSON root check
    if (json.is_array()) {
        if (!json.empty()) {
            DeserializeGameObject(json[0], nullptr);
        }
    } else {
        DeserializeGameObject(json, nullptr);
    }

    if (m_gameObjects.size() > prevSize) {
        auto obj = m_gameObjects.back();
        obj->SetDynamic(true);
        auto& ecs = GameManager::Instance().GetECS();
        
        // Transform pos override
        if (auto* ptrans = ecs.TryGetComponent<TransformData>(obj->GetEntityID())) {
        auto& trans = *ptrans;
            trans.m_position = position;
        }

        // Script Init
        if (auto* pscript = ecs.TryGetComponent<NativeScriptData>(obj->GetEntityID())) {
        auto& script = *pscript;
            if (script.Instance) {
                script.Instance->Awake();
                script.Instance->Start();
            }
        }
        
        return obj;
    }
    return nullptr;
}




void Scene::Init() {
    auto& ecs = GameManager::Instance().GetECS();
    
    std::function<void(const std::shared_ptr<GameObject>&)> callAwake = [&](const std::shared_ptr<GameObject>& node) {
        if (!node) return;
        if (auto* pscript = ecs.TryGetComponent<NativeScriptData>(node->GetEntityID())) {
            if (pscript->Instance) {
                pscript->Instance->Awake();
            }
        }
        for (auto& child : node->GetChildren()) callAwake(child);
    };

    std::function<void(const std::shared_ptr<GameObject>&)> callStart = [&](const std::shared_ptr<GameObject>& node) {
        if (!node) return;
        if (auto* pscript = ecs.TryGetComponent<NativeScriptData>(node->GetEntityID())) {
            if (pscript->Instance) {
                pscript->Instance->Start();
            }
        }
        for (auto& child : node->GetChildren()) callStart(child);
    };

    for (auto& obj : m_gameObjects) {
        if (!obj->GetParent()) callAwake(obj);
    }
    for (auto& obj : m_gameObjects) {
        if (!obj->GetParent()) callStart(obj);
    }
}

void Scene::Update(float deltaTime) {
    // System 縺ｮ Update 縺ｯ GameManager::Update() 縺瑚｡後≧縲・
}

void Scene::Draw() {
    // Scene 蝗ｺ譛峨・謠冗判・亥ｿ・ｦ√↑繧画ｴｾ逕溘け繝ｩ繧ｹ縺ｧ繧ｪ繝ｼ繝舌・繝ｩ繧､繝会ｼ・
    // RenderSystem / SpriteRenderSystem 縺ｯ GameManager::Update() 縺梧球蠖・
}

void Scene::ImGuiUpdate() {
}

nlohmann::json Scene::SerializeGameObject(std::shared_ptr<GameObject> obj) const {
    auto& ecs = GameManager::Instance().GetECS();
    nlohmann::json oj;
    oj["UUID"] = obj->GetUUID();
    oj["Name"] = obj->GetName();
    oj["IsActive"] = obj->IsActive();
    
    Entity e = obj->GetEntityID();
    nlohmann::json comps = nlohmann::json::array();
    
    if (auto* p_d = ecs.TryGetComponent<TransformData>(e)) {
        auto& d = *p_d;
        nlohmann::json cj;
        cj["Type"] = "TransformData";
        cj["Position"] = {d.m_position.x, d.m_position.y, d.m_position.z};
        cj["Rotation"] = {d.m_rotation.x, d.m_rotation.y, d.m_rotation.z};
        cj["Scale"] = {d.m_scale.x, d.m_scale.y, d.m_scale.z};
        comps.push_back(cj);
    }
    
    if (auto* p_d = ecs.TryGetComponent<CameraData>(e)) {
        auto& d = *p_d;
        nlohmann::json cj;
        cj["Type"] = "CameraData";
        cj["Fov"] = d.m_fov;
        cj["NearZ"] = d.m_nearZ;
        cj["FarZ"] = d.m_farZ;
        cj["MoveSpeed"] = d.m_moveSpeed;
        cj["CameraMode"] = static_cast<int>(d.m_cameraMode);
        cj["TargetOffset"] = {d.m_targetOffset.x, d.m_targetOffset.y, d.m_targetOffset.z};
        cj["FpsOffset"] = {d.m_fpsOffset.x, d.m_fpsOffset.y, d.m_fpsOffset.z};
        comps.push_back(cj);
    }

    if (auto* p_d = ecs.TryGetComponent<ModelRenderData>(e)) {
        auto& d = *p_d;
        nlohmann::json cj;
        cj["Type"] = "ModelRenderData";
        cj["FilePath"] = d.m_filePath;
        cj["ModelType"] = static_cast<int>(d.m_modelType);
        comps.push_back(cj);
    }

    if (auto* p_d = ecs.TryGetComponent<SpriteData>(e)) {
        auto& d = *p_d;
        nlohmann::json cj;
        cj["Type"] = "SpriteData";
        cj["FilePath"] = d.m_filePath;
        cj["Size"] = {d.m_size.x, d.m_size.y};
        cj["Pivot"] = {d.m_pivot.x, d.m_pivot.y};
        cj["Color"] = {d.m_color.x, d.m_color.y, d.m_color.z, d.m_color.w};
        cj["OrderInLayer"] = d.m_orderInLayer;
        comps.push_back(cj);
    }

    if (auto* p_d = ecs.TryGetComponent<ColliderData>(e)) {
        auto& d = *p_d;
        nlohmann::json cj;
        cj["Type"] = "ColliderData";
        cj["IsStatic"] = d.m_isStatic;
        nlohmann::json shapes = nlohmann::json::array();
        for (auto& shape : d.m_shapes) {
            nlohmann::json sj;
            sj["ShapeId"] = static_cast<int>(shape->GetShapeId());
            shape->Serialize(sj);
            shapes.push_back(sj);
        }
        cj["Shapes"] = shapes;
        comps.push_back(cj);
    }

    if (auto* p_d = ecs.TryGetComponent<AnimationDataComponent>(e)) {
        auto& d = *p_d;
        nlohmann::json cj;
        cj["Type"] = "AnimationDataComponent";
        cj["AnimationIndex"] = d.currentAnim.AnimationIndex;
        cj["ProgressTime"] = d.currentAnim.ProgressTime;
        cj["Speed"] = d.currentAnim.Speed;
        cj["IsPlaying"] = d.currentAnim.IsPlaying;
        cj["IsLoop"] = d.currentAnim.IsLoop;
        comps.push_back(cj);
    }

    if (auto* p_d = ecs.TryGetComponent<NativeScriptData>(e)) {
        auto& d = *p_d;
        nlohmann::json cj;
        cj["Type"] = "NativeScriptData";
        cj["ScriptName"] = d.ScriptName;
        if (d.Instance) d.Instance->Serialize(cj);
        comps.push_back(cj);
    }

    oj["Components"] = comps;

    nlohmann::json children = nlohmann::json::array();
    for (auto& child : obj->GetChildren()) {
        if (child && !child->IsDynamic()) {
            children.push_back(SerializeGameObject(child));
        }
    }
    oj["Children"] = children;
    
    return oj;
}

void Scene::Serialize(nlohmann::json& out) const {
    nlohmann::json objs = nlohmann::json::array();
    for (auto& obj : m_gameObjects) {
        if (obj && !obj->GetParent() && !obj->IsDynamic()) {
            objs.push_back(SerializeGameObject(obj));
        }
    }
    out["GameObjects"] = objs;
}

void Scene::DeserializeGameObject(const nlohmann::json& oj, std::shared_ptr<GameObject> parent) {
    auto& ecs = GameManager::Instance().GetECS();
    auto obj = std::make_shared<GameObject>();
    obj->SetScene(this);
    if (parent) {
        obj->SetParent(parent);
    }
    Entity id = ecs.CreateEntity();
    obj->SetEntityID(id);
    if (oj.contains("UUID")) obj->SetUUID(oj["UUID"]);
    if (oj.contains("Name")) obj->SetName(oj["Name"]);
    if (oj.contains("IsActive")) obj->SetActive(oj["IsActive"]);

    bool hasTransform = false;
    if (oj.contains("Components")) {
        for (const auto& cj : oj["Components"]) {
            std::string type = cj["Type"];
            if (type == "TransformData" || type == "TransformComponent") {
                hasTransform = true;
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
                ecs.AddComponent(id, d);
            }
            else if (type == "CameraData" || type == "CameraComponent") {
                CameraData d;
                if (cj.contains("Fov")) d.m_fov = cj["Fov"];
                if (cj.contains("NearZ")) d.m_nearZ = cj["NearZ"];
                if (cj.contains("FarZ")) d.m_farZ = cj["FarZ"];
                if (cj.contains("MoveSpeed")) d.m_moveSpeed = cj["MoveSpeed"];
                if (cj.contains("CameraMode")) d.m_cameraMode = static_cast<CameraMode>(cj["CameraMode"]);
                if (cj.contains("TargetOffset")) {
                    d.m_targetOffset = {cj["TargetOffset"][0], cj["TargetOffset"][1], cj["TargetOffset"][2]};
                }
                if (cj.contains("FpsOffset")) {
                    d.m_fpsOffset = {cj["FpsOffset"][0], cj["FpsOffset"][1], cj["FpsOffset"][2]};
                }
                ecs.AddComponent(id, d);
            }
            else if (type == "ModelRenderData" || type == "ModelRendererComponent") {
                ModelRenderData d;
                if (cj.contains("ModelType")) d.m_modelType = static_cast<ModelType>(cj["ModelType"]);
                if (cj.contains("FilePath")) {
                    d.m_filePath = cj["FilePath"];
                    if (!d.m_filePath.empty()) {
                        d.m_spModelData = ResourceManager::Instance().LoadModelAsync(d.m_filePath);
                    }
                }
                ecs.AddComponent(id, d);
            }
            else if (type == "SpriteData") {
                SpriteData d;
                if (cj.contains("FilePath")) {
                    d.m_filePath = cj["FilePath"];
                    if (!d.m_filePath.empty()) {
                        d.m_spTexture = ResourceManager::Instance().LoadTextureAsync(d.m_filePath);
                    }
                }
                if (cj.contains("Size")) { d.m_size.x = cj["Size"][0]; d.m_size.y = cj["Size"][1]; }
                if (cj.contains("Pivot")) { d.m_pivot.x = cj["Pivot"][0]; d.m_pivot.y = cj["Pivot"][1]; }
                if (cj.contains("Color")) { d.m_color.x = cj["Color"][0]; d.m_color.y = cj["Color"][1]; d.m_color.z = cj["Color"][2]; d.m_color.w = cj["Color"][3]; }
                if (cj.contains("OrderInLayer")) d.m_orderInLayer = cj["OrderInLayer"];
                ecs.AddComponent(id, d);
            }
            else if (type == "ColliderData" || type == "ColliderComponent") {
                ColliderData d;
                if (cj.contains("IsStatic")) d.m_isStatic = cj["IsStatic"];
                if (cj.contains("Shapes")) {
                    for (const auto& sj : cj["Shapes"]) {
                        int shapeId = -1;
                        if (sj.contains("ShapeId")) {
                            shapeId = sj["ShapeId"];
                        } else if (sj.contains("Type")) {
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
                                shape->m_entity = id;
                                shape->Deserialize(sj);
                                d.m_shapes.push_back(shape);
                            }
                        }
                    }
                }
                ecs.AddComponent(id, d);
            }
            else if (type == "AnimationDataComponent" || type == "AnimationComponent") {
                AnimationDataComponent d;
                if (cj.contains("AnimationIndex")) d.currentAnim.AnimationIndex = cj["AnimationIndex"];
                if (cj.contains("ProgressTime")) d.currentAnim.ProgressTime = cj["ProgressTime"];
                if (cj.contains("Speed")) d.currentAnim.Speed = cj["Speed"];
                if (cj.contains("IsPlaying")) d.currentAnim.IsPlaying = cj["IsPlaying"];
                if (cj.contains("IsLoop")) d.currentAnim.IsLoop = cj["IsLoop"];
                ecs.AddComponent(id, d);
            }
            else if (type == "NativeScriptData" || type == "ScriptComponent") {
                NativeScriptData d;
                if (cj.contains("ScriptName")) {
                    d.ScriptName = cj["ScriptName"];
                    auto scriptObj = GameManager::Instance().GetClassAssembly().Create(d.ScriptName);
                    if (scriptObj) {
                        d.Instance = std::dynamic_pointer_cast<NativeScript>(scriptObj);
                        if (d.Instance) {
                            d.Instance->SetGameObject(obj.get());
                            d.Instance->Deserialize(cj);
                        }
                    }
                }
                ecs.AddComponent(id, d);
            }
        }
    }

    if (!hasTransform) {
        ecs.AddComponent(id, TransformData{});
    }

    if (oj.contains("Children")) {
        for (const auto& childJ : oj["Children"]) {
            DeserializeGameObject(childJ, obj);
        }
    }

    if (!parent)
    {
        m_gameObjects.push_back(obj);
    }
    RegisterGameObject(id, obj);
}

void Scene::Deserialize(const nlohmann::json& in) {
    // GameObject繧偵け繝ｪ繧｢縺励√ョ繧ｹ繝医Λ繧ｯ繧ｿ縺ｧECS縺ｮEntity繧らｴ譽・＆繧後ｋ
    m_gameObjects.clear();

    if (in.contains("GameObjects")) {
        for (const auto& oj : in["GameObjects"]) {
            DeserializeGameObject(oj, nullptr);
        }
    }
}









