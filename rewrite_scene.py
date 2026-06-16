import re

with open('C:/GitHub/DX12/Source/Framework/Manager/Scene.cpp', 'r', encoding='shift_jis') as f:
    content = f.read()

# Fix 1: ecs.RegisterComponent<AnimationData>(); -> ecs.RegisterComponent<AnimationDataComponent>();
content = content.replace('ecs.RegisterComponent<AnimationData>();', 'ecs.RegisterComponent<AnimationDataComponent>();')

# Fix 2: animSig.set(ecs.GetComponentType<AnimationData>()); -> animSig.set(ecs.GetComponentType<AnimationDataComponent>());
content = content.replace('animSig.set(ecs.GetComponentType<AnimationData>());', 'animSig.set(ecs.GetComponentType<AnimationDataComponent>());')

# Fix 3: Scene::Serialize
serialize_code = '''void Scene::Serialize(nlohmann::json& out) const {
    auto& ecs = GameManager::Instance().GetECS();
    nlohmann::json objs = nlohmann::json::array();

    std::function<nlohmann::json(std::shared_ptr<GameObject>)> serializeObj = [&](std::shared_ptr<GameObject> obj) {
        nlohmann::json oj;
        oj["Name"] = obj->GetName();
        oj["IsActive"] = obj->IsActive();
        
        Entity e = obj->GetEntityID();
        nlohmann::json comps = nlohmann::json::array();
        
        if (ecs.HasComponent<TransformData>(e)) {
            auto& d = ecs.GetComponent<TransformData>(e);
            nlohmann::json cj;
            cj["Type"] = "TransformData";
            cj["Position"] = {d.m_position.x, d.m_position.y, d.m_position.z};
            cj["Rotation"] = {d.m_rotation.x, d.m_rotation.y, d.m_rotation.z};
            cj["Scale"] = {d.m_scale.x, d.m_scale.y, d.m_scale.z};
            comps.push_back(cj);
        }
        
        if (ecs.HasComponent<CameraData>(e)) {
            auto& d = ecs.GetComponent<CameraData>(e);
            nlohmann::json cj;
            cj["Type"] = "CameraData";
            cj["Fov"] = d.m_fov;
            cj["NearZ"] = d.m_nearZ;
            cj["FarZ"] = d.m_farZ;
            cj["MoveSpeed"] = d.m_moveSpeed;
            cj["CameraMode"] = static_cast<int>(d.m_cameraMode);
            comps.push_back(cj);
        }

        if (ecs.HasComponent<ModelRenderData>(e)) {
            auto& d = ecs.GetComponent<ModelRenderData>(e);
            nlohmann::json cj;
            cj["Type"] = "ModelRenderData";
            cj["FilePath"] = d.m_filePath;
            cj["ModelType"] = static_cast<int>(d.m_modelType);
            comps.push_back(cj);
        }

        if (ecs.HasComponent<ColliderData>(e)) {
            auto& d = ecs.GetComponent<ColliderData>(e);
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

        if (ecs.HasComponent<AnimationDataComponent>(e)) {
            auto& d = ecs.GetComponent<AnimationDataComponent>(e);
            nlohmann::json cj;
            cj["Type"] = "AnimationDataComponent";
            cj["AnimationIndex"] = d.currentAnim.AnimationIndex;
            cj["ProgressTime"] = d.currentAnim.ProgressTime;
            cj["Speed"] = d.currentAnim.Speed;
            cj["IsPlaying"] = d.currentAnim.IsPlaying;
            cj["IsLoop"] = d.currentAnim.IsLoop;
            comps.push_back(cj);
        }

        if (ecs.HasComponent<NativeScriptData>(e)) {
            auto& d = ecs.GetComponent<NativeScriptData>(e);
            nlohmann::json cj;
            cj["Type"] = "NativeScriptData";
            cj["ScriptName"] = d.ScriptName;
            if (d.Instance) d.Instance->Serialize(cj);
            comps.push_back(cj);
        }

        oj["Components"] = comps;

        nlohmann::json children = nlohmann::json::array();
        for (auto& child : obj->GetChildren()) {
            children.push_back(serializeObj(child));
        }
        oj["Children"] = children;
        return oj;
    };

    for (auto& obj : m_gameObjects) {
        if (!obj->GetParent()) {
            objs.push_back(serializeObj(obj));
        }
    }
    out["GameObjects"] = objs;
}'''

content = re.sub(r'void Scene::Serialize\(nlohmann::json& out\) const \{.*?\n\}\n', serialize_code + '\n\n', content, flags=re.DOTALL)

# Fix 4: Scene::DeserializeGameObject
deserialize_code = '''void Scene::DeserializeGameObject(const nlohmann::json& oj, std::shared_ptr<GameObject> parent) {
    auto& ecs = GameManager::Instance().GetECS();
    auto obj = std::make_shared<GameObject>();
    obj->SetScene(this);
    if (parent) {
        obj->SetParent(parent);
    }
    Entity id = ecs.CreateEntity();
    obj->SetEntityID(id);
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
            else if (type == "ColliderData" || type == "ColliderComponent") {
                ColliderData d;
                if (cj.contains("IsStatic")) d.m_isStatic = cj["IsStatic"];
                if (cj.contains("Shapes")) {
                    for (const auto& sj : cj["Shapes"]) {
                        if (sj.contains("ShapeId")) {
                            int shapeId = sj["ShapeId"];
                            std::shared_ptr<CollisionShape> shape;
                            if (shapeId == CollisionShape::Box) shape = std::make_shared<CollisionShapeBox>();
                            else if (shapeId == CollisionShape::Sphere) shape = std::make_shared<CollisionShapeSphere>();
                            else if (shapeId == CollisionShape::Capsule) shape = std::make_shared<CollisionShapeCapsule>();
                            else if (shapeId == CollisionShape::Mesh) shape = std::make_shared<CollisionShapeMesh>();
                            if (shape) {
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

    // 親なし（ルートオブジェクト）だけm_gameObjectsに追加する
    if (!parent)
    {
        m_gameObjects.push_back(obj);
    }
    RegisterGameObject(id, obj);
}'''

content = re.sub(r'void Scene::DeserializeGameObject\(const nlohmann::json& oj, std::shared_ptr<GameObject> parent\) \{.*?\n\}\n', deserialize_code + '\n', content, flags=re.DOTALL)

with open('C:/GitHub/DX12/Source/Framework/Manager/Scene.cpp', 'w', encoding='shift_jis') as f:
    f.write(content)
