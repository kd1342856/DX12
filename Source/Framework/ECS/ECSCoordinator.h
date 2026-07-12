#pragma once

// =============================================
// ECSCoordinator
// EntityManager + ComponentManager + SystemManager
// 繧堤ｵｱ蜷医☆繧九ヵ繧｡繧ｵ繝ｼ繝峨け繝ｩ繧ｹ
// ECS蜈ｨ菴薙・遯灘哨
// =============================================
class ECSCoordinator
{
public:
    // 蛻晄悄蛹・
    void Init()
    {
        m_upEntityManager = std::make_unique<EntityManager>();
        m_upComponentManager = std::make_unique<ComponentManager>();
        m_upSystemManager = std::make_unique<SystemManager>();
    }

    // === Entity邂｡逅・===

    Entity CreateEntity()
    {
        return m_upEntityManager->CreateEntity();
    }

    // Entity遐ｴ譽・ｼ育函蟄倥メ繧ｧ繝・け莉倥″繝ｻ莠碁㍾遐ｴ譽・ｒ螳牙・縺ｫ繧ｹ繧ｭ繝・・・・
    void DestroyEntity(Entity entity)
    {
        if (!IsAlive(entity)) return;
        m_upEntityManager->DestroyEntity(entity);
        m_upComponentManager->EntityDestroyed(entity);
        m_upSystemManager->EntityDestroyed(entity);
    }

    // Entity縺檎函蟄倥＠縺ｦ縺・ｋ縺狗｢ｺ隱・
    bool IsAlive(Entity entity) const
    {
        return m_upEntityManager->IsAlive(entity);
    }

    // === Component邂｡逅・===

    template<typename T>
    void RegisterComponent()
    {
        m_upComponentManager->RegisterComponent<T>();
    }

    template<typename T>
    void AddComponent(Entity entity, const T& component)
    {
        m_upComponentManager->AddComponent<T>(entity, component);

        // Signature繧呈峩譁ｰ縺励※蜷Тystem縺ｫ騾夂衍
        auto signature = m_upEntityManager->GetSignature(entity);        
        signature.set(m_upComponentManager->GetComponentType<T>(), true);
        m_upEntityManager->SetSignature(entity, signature);
        m_upSystemManager->EntitySignatureChanged(entity, signature);
    }

    template<typename T, typename... Args>
    T& EmplaceComponent(Entity entity, Args&&... args)
    {
        T& component = m_upComponentManager->EmplaceComponent<T>(entity, std::forward<Args>(args)...);

        // Signature繧呈峩譁ｰ縺励※蜷Тystem縺ｫ騾夂衍
        auto signature = m_upEntityManager->GetSignature(entity);
        signature.set(m_upComponentManager->GetComponentType<T>(), true);
        m_upEntityManager->SetSignature(entity, signature);
        m_upSystemManager->EntitySignatureChanged(entity, signature);

        return component;
    }

    template<typename T>
    void RemoveComponent(Entity entity)
    {
        m_upComponentManager->RemoveComponent<T>(entity);

        // Signature繧呈峩譁ｰ縺励※蜷Тystem縺ｫ騾夂衍
        auto signature = m_upEntityManager->GetSignature(entity);
        signature.set(m_upComponentManager->GetComponentType<T>(), false);
        m_upEntityManager->SetSignature(entity, signature);
        m_upSystemManager->EntitySignatureChanged(entity, signature);
    }

    template<typename T>
    T& GetComponent(Entity entity)
    {
        return m_upComponentManager->GetComponent<T>(entity);
    }

    template<typename T>
    const T& GetComponent(Entity entity) const
    {
        return m_upComponentManager->GetComponent<T>(entity);
    }

    template<typename T>
    T* TryGetComponent(Entity entity)
    {
        return m_upComponentManager->TryGetComponent<T>(entity);
    }

    template<typename T>
    const T* TryGetComponent(Entity entity) const
    {
        return m_upComponentManager->TryGetComponent<T>(entity);
    }

    template<typename T>
    ComponentType GetComponentType()
    {
        return m_upComponentManager->GetComponentType<T>();
    }

    // === System邂｡逅・===

    template<typename T>
    std::shared_ptr<T> RegisterSystem()
    {
        auto system = m_upSystemManager->RegisterSystem<T>();
        system->m_pCoordinator = this;
        return system;
    }

    template<typename T>
    void SetSystemSignature(Signature signature)
    {
        m_upSystemManager->SetSignature<T>(signature);
    }

private:
    std::unique_ptr<EntityManager> m_upEntityManager;
    std::unique_ptr<ComponentManager> m_upComponentManager;
    std::unique_ptr<SystemManager> m_upSystemManager;
};


