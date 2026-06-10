#include "GameObject.h"
#include "../Manager/Scene.h"
#include "../Manager/GameManager.h"
#include "../ECS/Components/ScriptComponent.h"

GameObject::~GameObject() {
    if (m_entityId != INVALID_ENTITY && GameManager::IsInstanceAlive()) {
        GameManager::Instance().GetECS().DestroyEntity(m_entityId);
        m_entityId = INVALID_ENTITY;
    }
}

void GameObject::Serialize(nlohmann::json& out) const {
    out["Name"] = m_name;
    out["IsActive"] = m_isActive;
    nlohmann::json comps = nlohmann::json::array();
    for (auto& c : m_components) {
        nlohmann::json cj;
        cj["Type"] = c->GetComponentName();
        c->Serialize(cj);
        comps.push_back(cj);
    }
    out["Components"] = comps;

    nlohmann::json children = nlohmann::json::array();
    for (auto& child : m_children) {
        nlohmann::json cj;
        child->Serialize(cj);
        children.push_back(cj);
    }
    out["Children"] = children;
}

void GameObject::Deserialize(const nlohmann::json& in) {
    if (in.contains("Name")) m_name = in["Name"];
    if (in.contains("IsActive")) m_isActive = in["IsActive"];
    if (in.contains("Components")) {
        for (const auto& cj : in["Components"]) {
            std::string type = cj["Type"];
            auto comp = GameManager::Instance().GetClassAssembly().Create(type);
            if (comp) {
                comp->SetGameObject(this);
                m_components.push_back(comp);
                comp->RegisterECSData();
                comp->Awake();
                comp->Deserialize(cj);
            }
        }
    }
    if (in.contains("Children")) {
        for (const auto& cj : in["Children"]) {
            auto child = std::make_shared<GameObject>();
            child->SetScene(m_scene);
            Entity id = GameManager::Instance().GetECS().CreateEntity();
            child->SetEntityID(id);
            child->m_pParent = this;
            m_children.push_back(child);
            if (m_scene) m_scene->RegisterGameObject(id, child);
            child->Deserialize(cj);
        }
    }
}

void GameObject::Destroy() {
    if (m_pParent) {
        auto it = std::find(m_pParent->m_children.begin(), m_pParent->m_children.end(), shared_from_this());
        if (it != m_pParent->m_children.end()) m_pParent->m_children.erase(it);
    } else if (m_scene) {
        m_scene->RemoveGameObject(shared_from_this());
    }
    m_pParent = nullptr;
    m_scene = nullptr;
}

void GameObject::SetParent(std::shared_ptr<GameObject> parent) {
    if (m_pParent) {
        auto it = std::find(m_pParent->m_children.begin(), m_pParent->m_children.end(), shared_from_this());
        if (it != m_pParent->m_children.end()) m_pParent->m_children.erase(it);
    } else if (m_scene) {
        m_scene->RemoveGameObject(shared_from_this());
    }

    m_pParent = parent ? parent.get() : nullptr;

    if (parent) {
        parent->m_children.push_back(shared_from_this());
    } else if (m_scene) {
        m_scene->AddGameObject(shared_from_this());
    }
}

void GameObject::Start() {
    if (!m_isActive || m_isStarted) return;
    for (auto& comp : m_components) {
        if (comp->IsActive()) comp->Start();
    }
    for (auto& child : m_children) {
        child->Start();
    }
    m_isStarted = true;
}

void GameObject::Update() {
    if (!m_isActive) return;
    for (auto& comp : m_components) {
        if (comp->IsActive()) {
            comp->Update();
        }
    }
    for (auto& child : m_children) {
        child->Update();
    }
}

void GameObject::PostUpdate() {
    if (!m_isActive) return;
    for (auto& comp : m_components) {
        if (comp->IsActive()) comp->PostUpdate();
    }
    for (auto& child : m_children) {
        child->PostUpdate();
    }
}

void GameObject::PreDraw() {
    if (!m_isActive) return;
    for (auto& comp : m_components) {
        if (comp->IsActive()) comp->PreDraw();
    }
    for (auto& child : m_children) {
        child->PreDraw();
    }
}

void GameObject::Draw() {
    if (!m_isActive) return;
    for (auto& comp : m_components) {
        if (comp->IsActive()) comp->Draw();
    }
    for (auto& child : m_children) {
        child->Draw();
    }
}

void GameObject::ImGuiUpdate() {
    if (!m_isActive) return;
    for (auto& comp : m_components) {
        if (comp->IsActive()) {
            comp->ImGuiUpdate();
        }
    }
    for (auto& child : m_children) {
        child->ImGuiUpdate();
    }
}

// =============================================
// コリジョンコールバック伝播実装
// このGameObjectが持つ全ScriptComponentへ通知する
// =============================================
void GameObject::NotifyCollisionEnter(GameObject* other) {
    for (auto& comp : m_components) {
        if (auto script = std::dynamic_pointer_cast<ScriptComponent>(comp)) {
            if (script->IsActive()) script->OnCollisionEnter(other);
        }
    }
}

void GameObject::NotifyCollisionStay(GameObject* other) {
    for (auto& comp : m_components) {
        if (auto script = std::dynamic_pointer_cast<ScriptComponent>(comp)) {
            if (script->IsActive()) script->OnCollisionStay(other);
        }
    }
}

void GameObject::NotifyCollisionExit(GameObject* other) {
    for (auto& comp : m_components) {
        if (auto script = std::dynamic_pointer_cast<ScriptComponent>(comp)) {
            if (script->IsActive()) script->OnCollisionExit(other);
        }
    }
}

void GameObject::NotifyTriggerEnter(GameObject* other) {
    for (auto& comp : m_components) {
        if (auto script = std::dynamic_pointer_cast<ScriptComponent>(comp)) {
            if (script->IsActive()) script->OnTriggerEnter(other);
        }
    }
}

void GameObject::NotifyTriggerStay(GameObject* other) {
    for (auto& comp : m_components) {
        if (auto script = std::dynamic_pointer_cast<ScriptComponent>(comp)) {
            if (script->IsActive()) script->OnTriggerStay(other);
        }
    }
}

void GameObject::NotifyTriggerExit(GameObject* other) {
    for (auto& comp : m_components) {
        if (auto script = std::dynamic_pointer_cast<ScriptComponent>(comp)) {
            if (script->IsActive()) script->OnTriggerExit(other);
        }
    }
}


