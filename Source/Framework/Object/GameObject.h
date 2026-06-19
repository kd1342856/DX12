#pragma once
#include "../../../Library/nlohmann/json.hpp"
#include "Object.h"
#include "../ECS/Entity/Entity.h"

class Scene;

// =============================================
// GameObject
// ECS EntitybvQ[IuWFNg?[gNX
// [U[ECS EntityID????
// =============================================
class GameObject : public Object, public std::enable_shared_from_this<GameObject> {
public:
    GameObject() {}
    ~GameObject();

    void SetScene(Scene* scene) { m_scene = scene; }
    Scene* GetScene() const { return m_scene; }

    void SetEntityID(Entity id) { m_entityId = id; }
    Entity GetEntityID() const { return m_entityId; }

    void SetDynamic(bool isDynamic) { m_isDynamic = isDynamic; }
    bool IsDynamic() const { return m_isDynamic; }

    void SetParent(std::shared_ptr<GameObject> parent);
    void Destroy();
    
    GameObject* GetParent() const { return m_pParent; }
    const std::vector<std::shared_ptr<GameObject>>& GetChildren() const { return m_children; }

    // =============================================
    // RWR[obNdwp[
    // =============================================
    void NotifyCollisionEnter(GameObject* other);
    void NotifyCollisionStay(GameObject* other);
    void NotifyCollisionExit(GameObject* other);
    void NotifyTriggerEnter(GameObject* other);
    void NotifyTriggerStay(GameObject* other);
    void NotifyTriggerExit(GameObject* other);

    template<class T>
    void AddComponent(T data = T{}) {
        GameManager::Instance().GetECS().AddComponent(m_entityId, data);
    }

    template<class T>
    T& GetComponent() {
        return GameManager::Instance().GetECS().GetComponent<T>(m_entityId);
    }

    template<class T>
    bool HasComponent() const {
        return GameManager::Instance().GetECS().HasComponent<T>(m_entityId);
    }

protected:
    bool m_isStarted = false;

private:
    std::vector<std::shared_ptr<GameObject>> m_children;
    GameObject* m_pParent = nullptr;
    Scene* m_scene = nullptr;
    Entity m_entityId = INVALID_ENTITY;
    bool m_isDynamic = false;
};