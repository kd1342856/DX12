#include "../../../Pch.h"
#include "Scene.h"
#include "../Collision/CollisionManager.h"
#include "../Resource/PrefabManager.h"
#include "../../ECS/ComponentSerializerRegistry.h"
#include "../../../Graphics/Shader/ShaderManager/ShaderManager.h"

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
    // System �� Update �� GameManager::Update() ���s���AE
}

void Scene::Draw() {
    // Scene �ŗL?E�`��?E??E??�Ȃ�h���N���X�ŃI�[�o?E���C�h�IE
    // RenderSystem / SpriteRenderSystem �� GameManager::Update() ���S�ZE
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
    
    ComponentSerializerRegistry::Instance().SerializeAllComponents(ecs, e, comps, obj.get());

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

    // Fog / hunt-visual settings (ShaderEditor's Fog panel). These aren't tied to any
    // GameObject, so they're saved as their own section at the scene root.
    const auto& fog = ShaderManager::Instance().GetFogSettings();
    nlohmann::json fj;
    fj["EnableFog"] = fog.EnableFog;
    fj["FogColor"] = { fog.FogColor.x, fog.FogColor.y, fog.FogColor.z };
    fj["FogDensity"] = fog.FogDensity;
    fj["HuntFogDensity"] = fog.HuntFogDensity;
    fj["EnableHuntDarken"] = fog.EnableHuntDarken;
    fj["HuntDarkenAmount"] = fog.HuntDarkenAmount;
    fj["HuntTransitionSpeed"] = fog.HuntTransitionSpeed;
    out["Fog"] = fj;
}

void Scene::DeserializeGameObject(const nlohmann::json& oj, std::shared_ptr<GameObject> parent) {
    auto obj = std::make_shared<GameObject>();
    obj->SetScene(this);
    if (parent) {
        obj->SetParent(parent);
    }

    auto& ecs = GameManager::Instance().GetECS();
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
            }
            ComponentSerializerRegistry::Instance().DeserializeComponent(ecs, id, type, cj, obj.get());
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
    // GameObject���N���A��???�f�X�g���N�^��ECS��Entity���j?E??���
    m_gameObjects.clear();

    if (in.contains("GameObjects")) {
        for (const auto& oj : in["GameObjects"]) {
            DeserializeGameObject(oj, nullptr);
        }
    }

    if (in.contains("Fog")) {
        auto& fog = ShaderManager::Instance().GetFogSettings();
        const auto& fj = in["Fog"];
        if (fj.contains("EnableFog")) fog.EnableFog = fj["EnableFog"];
        if (fj.contains("FogColor")) {
            auto c = fj["FogColor"];
            fog.FogColor = { c[0].get<float>(), c[1].get<float>(), c[2].get<float>() };
        }
        if (fj.contains("FogDensity")) fog.FogDensity = fj["FogDensity"];
        if (fj.contains("HuntFogDensity")) fog.HuntFogDensity = fj["HuntFogDensity"];
        if (fj.contains("EnableHuntDarken")) fog.EnableHuntDarken = fj["EnableHuntDarken"];
        if (fj.contains("HuntDarkenAmount")) fog.HuntDarkenAmount = fj["HuntDarkenAmount"];
        if (fj.contains("HuntTransitionSpeed")) fog.HuntTransitionSpeed = fj["HuntTransitionSpeed"];
        fog.IsDirty = true;
    }
}









