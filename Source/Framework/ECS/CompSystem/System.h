#pragma once
#include <typeindex>
#include <vector>
#include <unordered_map>
#include <set>
#include <memory>
#include <cassert>

// =============================================
// System基盤
// SystemBase: 全Systemの基底クラス
// SystemManager: System登録・Entity Signature変更通知
// =============================================

class ECSCoordinator;

// System基底クラス
class SystemBase
{
public:
	virtual ~SystemBase() = default;

	// 毎フレーム呼ばれる更新処理（描画系システムはUpdateではなくRenderを使うため空実装を許容）
	virtual void Update(float deltaTime) {}

	// エンティティ追加・削除のコールバック（派生クラスでオーバーライド可能）
	virtual void OnEntityAdded(Entity entity) {}
	virtual void OnEntityRemoved(Entity entity) {}

	// このSystemが対象とするEntity一覧
	std::vector<Entity> m_entities;
	std::unordered_map<Entity, size_t> m_entityToIndex;

	bool Contains(Entity entity) const
	{
		return m_entityToIndex.find(entity) != m_entityToIndex.end();
	}

	void AddEntity(Entity entity)
	{
		if (!Contains(entity))
		{
			size_t newIndex = m_entities.size();
			m_entityToIndex[entity] = newIndex;
			m_entities.push_back(entity);
		}
	}

	void RemoveEntity(Entity entity)
	{
		if (Contains(entity))
		{
			size_t removedIndex = m_entityToIndex[entity];
			Entity lastEntity = m_entities.back();
			
			m_entities[removedIndex] = lastEntity;
			m_entityToIndex[lastEntity] = removedIndex;
			
			m_entityToIndex.erase(entity);
			m_entities.pop_back();
		}
	}

	// コーディネーターへの参照（Component取得用）
	ECSCoordinator* m_pCoordinator = nullptr;
};

// System管理クラス
class SystemManager
{
public:
	// Systemを登録
	template<typename T>
	std::shared_ptr<T> RegisterSystem()
	{
		std::type_index typeName = std::type_index(typeid(T));
		assert(m_systems.find(typeName) == m_systems.end() && "同じSystemは二重登録できません");
		auto system = std::make_shared<T>();
		m_systems.insert({ typeName, system });
		return system;
	}

	// SystemのSignatureをセット（どのComponentを要求するか）
	template<typename T>
	void SetSignature(Signature signature)
	{
		std::type_index typeName = std::type_index(typeid(T));
		assert(m_systems.find(typeName) != m_systems.end() && "未登録のSystem");
		m_signatures[typeName] = signature;
	}

	// Entity破棄時に全Systemから除去
	void EntityDestroyed(Entity entity)
	{
		for (auto const& pair : m_systems)
		{
			if (pair.second->Contains(entity))
			{
				pair.second->OnEntityRemoved(entity);
				pair.second->RemoveEntity(entity);
			}
		}
	}

	// EntityのSignature変更時に各Systemのリストを更新
	void EntitySignatureChanged(Entity entity, Signature entitySignature)
	{
		for (auto const& pair : m_systems)
		{
			auto const& systemSignature = m_signatures[pair.first];
			
			bool wasInSystem = pair.second->Contains(entity);
			bool shouldBeInSystem = ((entitySignature & systemSignature) == systemSignature);

			if (shouldBeInSystem && !wasInSystem)
			{
				pair.second->AddEntity(entity);
				pair.second->OnEntityAdded(entity);
			}
			else if (!shouldBeInSystem && wasInSystem)
			{
				pair.second->OnEntityRemoved(entity);
				pair.second->RemoveEntity(entity);
			}
		}
	}

private:
	//	Signatureのマッピング
	std::unordered_map<std::type_index, Signature> m_signatures;
	//	Systemインスタンスのマッピング
	std::unordered_map<std::type_index, std::shared_ptr<SystemBase>> m_systems;
};


