#include "../../Pch.h"
#include "GameObject.h"
#include "../Manager/Scene/Scene.h"

GameObject::~GameObject() {
    if (m_entityId != INVALID_ENTITY && GameManager::IsInstanceAlive()) {
        GameManager::Instance().GetECS().GetCommandBuffer().DestroyEntity(m_entityId);
        m_entityId = INVALID_ENTITY;
    }
}

void GameObject::Destroy() {
    ExecuteDestroy();
}

void GameObject::ExecuteDestroy() {
    auto& ecs = GameManager::Instance().GetECS();
    if (m_entityId != INVALID_ENTITY && GameManager::IsInstanceAlive()) {
        CollisionManager::Instance().NotifyDestroy(m_entityId);

        if (auto* pScript = ecs.TryGetComponent<NativeScriptData>(m_entityId)) {
            if (pScript->Instance) {
                pScript->Instance->OnDestroy();
            }
        }
    }

    m_children.clear();

    if (auto p = m_wpParent.lock()) {
        auto it = std::find(p->m_children.begin(), p->m_children.end(), shared_from_this());
        if (it != p->m_children.end()) p->m_children.erase(it);
    } else if (m_scene) {
        m_scene->RemoveGameObject(shared_from_this());
    }
    m_wpParent.reset();
    m_scene = nullptr;

    if (m_entityId != INVALID_ENTITY && GameManager::IsInstanceAlive()) {
        GameManager::Instance().GetECS().GetCommandBuffer().DestroyEntity(m_entityId);
        m_entityId = INVALID_ENTITY;
    }
}

void GameObject::SetParent(std::shared_ptr<GameObject> parent) {
    if (auto p = m_wpParent.lock()) {
        auto it = std::find(p->m_children.begin(), p->m_children.end(), shared_from_this());
        if (it != p->m_children.end()) p->m_children.erase(it);
    } else if (m_scene) {
        m_scene->RemoveGameObject(shared_from_this());
    }

    m_wpParent = parent;

    if (parent) {
        parent->m_children.push_back(shared_from_this());
    } else if (m_scene) {
        m_scene->AddGameObject(shared_from_this());
    }
}

void GameObject::NotifyCollisionEnter(GameObject* other) {
    if (m_entityId == INVALID_ENTITY || !GameManager::IsInstanceAlive()) return;
    if (auto* pScript = GameManager::Instance().GetECS().TryGetComponent<NativeScriptData>(m_entityId)) {
        if (pScript->Instance) pScript->Instance->OnCollisionEnter(other);
    }
}

void GameObject::NotifyCollisionStay(GameObject* other) {
    if (m_entityId == INVALID_ENTITY || !GameManager::IsInstanceAlive()) return;
    if (auto* pScript = GameManager::Instance().GetECS().TryGetComponent<NativeScriptData>(m_entityId)) {
        if (pScript->Instance) pScript->Instance->OnCollisionStay(other);
    }
}

void GameObject::NotifyCollisionExit(GameObject* other) {
    if (m_entityId == INVALID_ENTITY || !GameManager::IsInstanceAlive()) return;
    if (auto* pScript = GameManager::Instance().GetECS().TryGetComponent<NativeScriptData>(m_entityId)) {
        if (pScript->Instance) pScript->Instance->OnCollisionExit(other);
    }
}

void GameObject::NotifyTriggerEnter(GameObject* other) {
    if (m_entityId == INVALID_ENTITY || !GameManager::IsInstanceAlive()) return;
    if (auto* pScript = GameManager::Instance().GetECS().TryGetComponent<NativeScriptData>(m_entityId)) {
        if (pScript->Instance) pScript->Instance->OnTriggerEnter(other);
    }
}

void GameObject::NotifyTriggerStay(GameObject* other) {
    if (m_entityId == INVALID_ENTITY || !GameManager::IsInstanceAlive()) return;
    if (auto* pScript = GameManager::Instance().GetECS().TryGetComponent<NativeScriptData>(m_entityId)) {
        if (pScript->Instance) pScript->Instance->OnTriggerStay(other);
    }
}

void GameObject::NotifyTriggerExit(GameObject* other) {
    if (m_entityId == INVALID_ENTITY || !GameManager::IsInstanceAlive()) return;
    if (auto* pScript = GameManager::Instance().GetECS().TryGetComponent<NativeScriptData>(m_entityId)) {
        if (pScript->Instance) pScript->Instance->OnTriggerExit(other);
    }
}


