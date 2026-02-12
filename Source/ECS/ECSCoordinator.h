#pragma once

// =============================================
// ECSCoordinator
// EntityManager + ComponentManager + SystemManager
// を統合するファサードクラス
// 全ECS操作の窓口
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

	// === Entity操作 ===

	Entity CreateEntity()
	{
		return m_upEntityManager->CreateEntity();
	}

	void DestroyEntity(Entity entity)
	{
		m_upEntityManager->DestroyEntity(entity);
		m_upComponentManager->EntityDestroyed(entity);
		m_upSystemManager->EntityDestroyed(entity);
	}

	// === Component操作 ===

	template<typename T>
	void RegisterComponent()
	{
		m_upComponentManager->RegisterComponent<T>();
	}

	template<typename T>
	void AddComponent(Entity entity, T component)
	{
		m_upComponentManager->AddComponent<T>(entity, component);

		// Signature更新
		auto signature = m_upEntityManager->GetSignature(entity);
		signature.set(m_upComponentManager->GetComponentType<T>(), true);
		m_upEntityManager->SetSignature(entity, signature);

		// 全Systemに通知
		m_upSystemManager->EntitySignatureChanged(entity, signature);
	}

	template<typename T>
	void RemoveComponent(Entity entity)
	{
		m_upComponentManager->RemoveComponent<T>(entity);

		// Signature更新
		auto signature = m_upEntityManager->GetSignature(entity);
		signature.set(m_upComponentManager->GetComponentType<T>(), false);
		m_upEntityManager->SetSignature(entity, signature);

		// 全Systemに通知
		m_upSystemManager->EntitySignatureChanged(entity, signature);
	}

	template<typename T>
	T& GetComponent(Entity entity)
	{
		return m_upComponentManager->GetComponent<T>(entity);
	}

	template<typename T>
	ComponentType GetComponentType()
	{
		return m_upComponentManager->GetComponentType<T>();
	}

	// === System操作 ===

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
