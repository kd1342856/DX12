#pragma once
#include "ECSCommandBuffer.h"
#include <vector>

// =============================================
// ECSCoordinator
// EntityManager + ComponentManager + SystemManager
// を統合するファサードクラス
// ECS全体の窓口
// =============================================
class ECSCoordinator
{
public:
    // 初期化
    void Init()
    {
        m_upEntityManager = std::make_unique<EntityManager>();
        m_upComponentManager = std::make_unique<ComponentManager>();
        m_upSystemManager = std::make_unique<SystemManager>();
    }

    // 終了処理
    void Shutdown()
    {
        m_commandBuffers.clear();
        m_upSystemManager.reset();
        m_upComponentManager.reset();
        m_upEntityManager.reset();
    }

    // === Entity管理 ===

    Entity CreateEntity()
    {
        return m_upEntityManager->CreateEntity();
    }

    Entity AllocateEntity()
    {
        return m_upEntityManager->AllocateEntity();
    }

    void InitializeEntity(Entity entity)
    {
        m_upEntityManager->InitializeEntity(entity);
    }

    // Entity破棄(生存チェック付き・二重破棄を安全にスキップ)
    void DestroyEntity(Entity entity)
    {
        if (!IsAlive(entity)) return;
        m_upEntityManager->DestroyEntity(entity);
        m_upComponentManager->EntityDestroyed(entity);
        m_upSystemManager->EntityDestroyed(entity);
    }

    // Entityが生存しているか確認
    bool IsAlive(Entity entity) const
    {
        return m_upEntityManager->IsAlive(entity);
    }

    // === Component管理 ===

    template<typename T>
    void RegisterComponent()
    {
        m_upComponentManager->RegisterComponent<T>();
    }

    template<typename T>
    void AddComponent(Entity entity, const T& component)
    {
        m_upComponentManager->AddComponent<T>(entity, component);

        // Signatureを更新して各Systemに通知
        auto signature = m_upEntityManager->GetSignature(entity);        
        signature.set(m_upComponentManager->GetComponentType<T>(), true);
        m_upEntityManager->SetSignature(entity, signature);
        m_upSystemManager->EntitySignatureChanged(entity, signature);
    }

    template<typename T, typename... Args>
    T& EmplaceComponent(Entity entity, Args&&... args)
    {
        T& component = m_upComponentManager->EmplaceComponent<T>(entity, std::forward<Args>(args)...);

        // Signatureを更新して各Systemに通知
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

        // Signatureを更新して各Systemに通知
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
    ComponentArray<T>& GetComponentArray()
    {
        return m_upComponentManager->GetComponentArray<T>();
    }

    template<typename T>
    const ComponentArray<T>& GetComponentArray() const
    {
        return m_upComponentManager->GetComponentArray<T>();
    }

    template<typename T>
    ComponentType GetComponentType()
    {
        return m_upComponentManager->GetComponentType<T>();
    }

    // === System管理 ===

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

public:
    ECSCommandBuffer& GetCommandBuffer(int threadIndex = 0) {
        if (threadIndex >= m_commandBuffers.size()) { m_commandBuffers.resize(threadIndex + 1); }
        return m_commandBuffers[threadIndex];
    }
    void FlushCommands() {
        for (auto& cb : m_commandBuffers) { cb.Playback(this); }
    }
private:
    std::vector<ECSCommandBuffer> m_commandBuffers;
    std::unique_ptr<EntityManager> m_upEntityManager;
    std::unique_ptr<ComponentManager> m_upComponentManager;
    std::unique_ptr<SystemManager> m_upSystemManager;
};





template<typename T>
inline void ECSCommandBuffer::AddComponent(Entity entity, const T& component)
{
    ECSExecuteFn fn = [](ECSCoordinator* coord, Entity e, const void* data) {
        coord->AddComponent<T>(e, *static_cast<const T*>(data));
    };
    WriteCommand(ECSCommandType::AddComponent, entity, fn, sizeof(T), &component);
}

template<typename T>
inline void ECSCommandBuffer::RemoveComponent(Entity entity)
{
    ECSExecuteFn fn = [](ECSCoordinator* coord, Entity e, const void*) {
        coord->RemoveComponent<T>(e);
    };
    WriteCommand(ECSCommandType::RemoveComponent, entity, fn, 0, nullptr);
}


