#include "../../../Pch.h"
#include "Scene.h"
#include "../Collision/CollisionManager.h"
#include "../Resource/PrefabManager.h"
#include "../../ECS/ComponentSerializerRegistry.h"
#include "../../../Graphics/Shader/ShaderManager/ShaderManager.h"

Scene::Scene()
{
}
Scene::~Scene()
{
}

std::shared_ptr<GameObject> Scene::CreateGameObject(const std::string& name)
{
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

std::shared_ptr<GameObject> Scene::Instantiate(const std::string& filepath, const Math::Vector3& position)
{
    auto json = PrefabManager::Instance().GetPrefab(filepath);
    if (json.is_null())
    {
        return nullptr;
    }

    size_t prevSize = m_gameObjects.size();

    // JSON root check
    if (json.is_array())
    {
        if (!json.empty())
        {
            DeserializeGameObject(json[0], nullptr);
        }
    }
    else
    {
        DeserializeGameObject(json, nullptr);
    }

    if (m_gameObjects.size() > prevSize)
    {
        auto obj = m_gameObjects.back();
        obj->SetDynamic(true);
        auto& ecs = GameManager::Instance().GetECS();

        // Transform pos override
        if (auto* ptrans = ecs.TryGetComponent<TransformData>(obj->GetEntityID()))
        {
            auto& trans = *ptrans;
            trans.m_position = position;
        }

        // Script Init
        if (auto* pscript = ecs.TryGetComponent<NativeScriptData>(obj->GetEntityID()))
        {
            auto& script = *pscript;
            if (script.Instance)
            {
                script.Instance->Awake();
                script.Instance->Start();
            }
        }

        return obj;
    }
    return nullptr;
}


void Scene::Init()
{
    auto& ecs = GameManager::Instance().GetECS();

    std::function<void(const std::shared_ptr<GameObject>&)> callAwake = [&](const std::shared_ptr<GameObject>& node)
    {
        if (!node)
        {
            return;
        }
        if (auto* pscript = ecs.TryGetComponent<NativeScriptData>(node->GetEntityID()))
        {
            if (pscript->Instance)
            {
                pscript->Instance->Awake();
            }
        }
        for (auto& child : node->GetChildren())
        {
            callAwake(child);
        }
    };

    std::function<void(const std::shared_ptr<GameObject>&)> callStart = [&](const std::shared_ptr<GameObject>& node)
    {
        if (!node)
        {
            return;
        }
        if (auto* pscript = ecs.TryGetComponent<NativeScriptData>(node->GetEntityID()))
        {
            if (pscript->Instance)
            {
                pscript->Instance->Start();
            }
        }
        for (auto& child : node->GetChildren())
        {
            callStart(child);
        }
    };

    // Awake/Start内でCreateGameObjectされるとm_gameObjectsが再確保され、直接回していると
    // イテレータが無効になって落ちるので、呼び出し前の一覧をコピーしてから回す
    const std::vector<std::shared_ptr<GameObject>> awakeTargets = m_gameObjects;
    for (auto& obj : awakeTargets)
    {
        if (!obj->GetParent())
        {
            callAwake(obj);
        }
    }
    const std::vector<std::shared_ptr<GameObject>> startTargets = m_gameObjects;
    for (auto& obj : startTargets)
    {
        if (!obj->GetParent())
        {
            callStart(obj);
        }
    }
}

void Scene::Update(float deltaTime)
{
    // System の Update は GameManager::Update() が行う
}

void Scene::Draw()
{
    // Scene 固有の描画処理があれば派生クラスでオーバーライドする
    // RenderSystem / SpriteRenderSystem は GameManager::Update() が担当
}

void Scene::ImGuiUpdate()
{
}

nlohmann::json Scene::SerializeGameObject(std::shared_ptr<GameObject> obj) const
{
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
    for (auto& child : obj->GetChildren())
    {
        if (child && !child->IsDynamic())
        {
            children.push_back(SerializeGameObject(child));
        }
    }
    oj["Children"] = children;

    return oj;
}

void Scene::Serialize(nlohmann::json& out) const
{
    nlohmann::json objs = nlohmann::json::array();
    for (auto& obj : m_gameObjects)
    {
        if (obj && !obj->GetParent() && !obj->IsDynamic())
        {
            objs.push_back(SerializeGameObject(obj));
        }
    }
    out["GameObjects"] = objs;

    // Fog / hunt-visual settings (ShaderEditor's Fog panel). These aren't tied to any
    // GameObject, so they're saved as their own section at the scene root.
    const auto& fog = ShaderManager::Instance().GetFogSettings();
    nlohmann::json fj;
    fj["EnableFog"] = fog.EnableFog;
    fj["FogColor"] = {fog.FogColor.x, fog.FogColor.y, fog.FogColor.z};
    fj["FogDensity"] = fog.FogDensity;
    fj["HuntFogDensity"] = fog.HuntFogDensity;
    fj["EnableHuntDarken"] = fog.EnableHuntDarken;
    fj["HuntDarkenAmount"] = fog.HuntDarkenAmount;
    fj["HuntTransitionSpeed"] = fog.HuntTransitionSpeed;
    out["Fog"] = fj;

    // Lighting (directional/ambient) and Shadow settings from the ShaderEditor panels - saved
    // per scene the same way as Fog, so tuning them doesn't reset on the next launch.
    const auto& lit = ShaderManager::Instance().GetLightingSettings();
    nlohmann::json lj;
    lj["DirectionalLightDir"] = {lit.DirectionalLightDir.x, lit.DirectionalLightDir.y, lit.DirectionalLightDir.z};
    lj["DirectionalLightColor"] = {lit.DirectionalLightColor.x, lit.DirectionalLightColor.y, lit.DirectionalLightColor.z};
    lj["DirectionalLightIntensity"] = lit.DirectionalLightIntensity;
    lj["AmbientLight"] = {lit.AmbientLight.x, lit.AmbientLight.y, lit.AmbientLight.z};
    lj["IndirectShadowFloor"] = lit.IndirectShadowFloor;
    lj["EnableIndoorMask"] = lit.EnableIndoorMask;
    lj["IndoorMargin"] = lit.IndoorMargin;
    out["Lighting"] = lj;

    const auto& sh = ShaderManager::Instance().GetShadowSettings();
    nlohmann::json sj;
    sj["EnableShadows"] = sh.EnableShadows;
    sj["ShadowPower"] = sh.ShadowPower;
    sj["ShadowBias"] = sh.ShadowBias;
    sj["CascadeSplits"] = {sh.CascadeSplits.x, sh.CascadeSplits.y, sh.CascadeSplits.z, sh.CascadeSplits.w};
    out["Shadow"] = sj;
}

void Scene::DeserializeGameObject(const nlohmann::json& oj, std::shared_ptr<GameObject> parent)
{
    auto obj = std::make_shared<GameObject>();
    obj->SetScene(this);
    if (parent)
    {
        obj->SetParent(parent);
    }

    auto& ecs = GameManager::Instance().GetECS();
    Entity id = ecs.CreateEntity();
    obj->SetEntityID(id);

    if (oj.contains("UUID"))
    {
        obj->SetUUID(oj["UUID"]);
    }
    if (oj.contains("Name"))
    {
        obj->SetName(oj["Name"]);
    }
    if (oj.contains("IsActive"))
    {
        obj->SetActive(oj["IsActive"]);
    }

    bool hasTransform = false;
    if (oj.contains("Components"))
    {
        for (const auto& cj : oj["Components"])
        {
            std::string type = cj["Type"];
            if (type == "TransformData" || type == "TransformComponent")
            {
                hasTransform = true;
            }
            ComponentSerializerRegistry::Instance().DeserializeComponent(ecs, id, type, cj, obj.get());
        }
    }

    if (!hasTransform)
    {
        ecs.AddComponent(id, TransformData{});
    }

    if (oj.contains("Children"))
    {
        for (const auto& childJ : oj["Children"])
        {
            DeserializeGameObject(childJ, obj);
        }
    }

    if (!parent)
    {
        m_gameObjects.push_back(obj);
    }
    RegisterGameObject(id, obj);
}

void Scene::Deserialize(const nlohmann::json& in)
{
    // GameObjectをクリアすると、デストラクタでECSのEntityも破棄される
    m_gameObjects.clear();

    if (in.contains("GameObjects"))
    {
        for (const auto& oj : in["GameObjects"])
        {
            DeserializeGameObject(oj, nullptr);
        }
    }

    if (in.contains("Fog"))
    {
        auto& fog = ShaderManager::Instance().GetFogSettings();
        const auto& fj = in["Fog"];
        if (fj.contains("EnableFog"))
        {
            fog.EnableFog = fj["EnableFog"];
        }
        if (fj.contains("FogColor"))
        {
            auto c = fj["FogColor"];
            fog.FogColor = {c[0].get<float>(), c[1].get<float>(), c[2].get<float>()};
        }
        if (fj.contains("FogDensity"))
        {
            fog.FogDensity = fj["FogDensity"];
        }
        if (fj.contains("HuntFogDensity"))
        {
            fog.HuntFogDensity = fj["HuntFogDensity"];
        }
        if (fj.contains("EnableHuntDarken"))
        {
            fog.EnableHuntDarken = fj["EnableHuntDarken"];
        }
        if (fj.contains("HuntDarkenAmount"))
        {
            fog.HuntDarkenAmount = fj["HuntDarkenAmount"];
        }
        if (fj.contains("HuntTransitionSpeed"))
        {
            fog.HuntTransitionSpeed = fj["HuntTransitionSpeed"];
        }
        fog.IsDirty = true;
    }

    // Scenes saved before these sections existed simply keep whatever values are currently set.
    auto readFloat3 = [](const nlohmann::json& a, DirectX::XMFLOAT3& out)
    {
        if (a.is_array() && a.size() >= 3)
        {
            out = {a[0].get<float>(), a[1].get<float>(), a[2].get<float>()};
        }
    };

    if (in.contains("Lighting"))
    {
        auto& lit = ShaderManager::Instance().GetLightingSettings();
        const auto& lj = in["Lighting"];
        if (lj.contains("DirectionalLightDir"))
        {
            readFloat3(lj["DirectionalLightDir"], lit.DirectionalLightDir);
        }
        if (lj.contains("DirectionalLightColor"))
        {
            readFloat3(lj["DirectionalLightColor"], lit.DirectionalLightColor);
        }
        if (lj.contains("DirectionalLightIntensity"))
        {
            lit.DirectionalLightIntensity = lj["DirectionalLightIntensity"];
        }
        if (lj.contains("AmbientLight"))
        {
            readFloat3(lj["AmbientLight"], lit.AmbientLight);
        }
        if (lj.contains("IndirectShadowFloor"))
        {
            lit.IndirectShadowFloor = lj["IndirectShadowFloor"];
        }
        if (lj.contains("EnableIndoorMask"))
        {
            lit.EnableIndoorMask = lj["EnableIndoorMask"];
        }
        if (lj.contains("IndoorMargin"))
        {
            lit.IndoorMargin = lj["IndoorMargin"];
        }
        lit.IsDirty = true;
    }

    if (in.contains("Shadow"))
    {
        auto& sh = ShaderManager::Instance().GetShadowSettings();
        const auto& sj = in["Shadow"];
        if (sj.contains("EnableShadows"))
        {
            sh.EnableShadows = sj["EnableShadows"];
        }
        if (sj.contains("ShadowPower"))
        {
            sh.ShadowPower = sj["ShadowPower"];
        }
        if (sj.contains("ShadowBias"))
        {
            sh.ShadowBias = sj["ShadowBias"];
        }
        if (sj.contains("CascadeSplits"))
        {
            const auto& c = sj["CascadeSplits"];
            if (c.is_array() && c.size() >= 4)
            {
                sh.CascadeSplits = {c[0].get<float>(), c[1].get<float>(), c[2].get<float>(), c[3].get<float>()};
            }
        }
        sh.IsDirty = true;
    }
}
