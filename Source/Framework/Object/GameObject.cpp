#include "../../Pch.h"
#include "GameObject.h"
#include "../Manager/Scene/Scene.h"
#include "../Manager/GameManager.h"
#include "../ECS/Components/Data/NativeScriptData.h"

GameObject::~GameObject() {
    if (m_entityId != INVALID_ENTITY && GameManager::IsInstanceAlive()) {
        GameManager::Instance().GetECS().DestroyEntity(m_entityId);
        m_entityId = INVALID_ENTITY;
    }
}

void GameObject::Destroy() {
    // Make a copy of children to safely destroy them
    auto childrenCopy = m_children;
    for (auto& child : childrenCopy) {
        child->Destroy();
    }
    m_children.clear();

    if (m_pParent) {
        auto it = std::find(m_pParent->m_children.begin(), m_pParent->m_children.end(), shared_from_this());
        if (it != m_pParent->m_children.end()) m_pParent->m_children.erase(it);
    } else if (m_scene) {
        m_scene->RemoveGameObject(shared_from_this());
    }
    m_pParent = nullptr;
    m_scene = nullptr;

    if (m_entityId != INVALID_ENTITY && GameManager::IsInstanceAlive()) {
        GameManager::Instance().GetECS().DestroyEntity(m_entityId);
        m_entityId = INVALID_ENTITY;
    }
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
