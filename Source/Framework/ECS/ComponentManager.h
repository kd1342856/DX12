#pragma once
#include <typeindex>
#include <unordered_map>
#include <vector>
#include <memory>
#include <cassert>

// =============================================
// Component基盤
// ComponentArray: パッキング配列でComponent格納
// ComponentManager: 型登録・追加・削除・取得
// =============================================

// 型消去用インターフェース
class IComponentArray
{
public:
	virtual ~IComponentArray() = default;

	virtual void EntityDestroyed(Entity entity) = 0;
};

// テンプレート型のコンポネント配列
template<typename T>
class ComponentArray : public IComponentArray
{
public:
	ComponentArray() {
		m_componentArray.reserve(MAX_ENTITIES);
	}

	// コンポネントを追加
	void InsertData(Entity entity, const T& component)
	{
		assert(m_entityToIndex.find(entity) == m_entityToIndex.end() && "同じEntityに二重追加はできません");
		size_t newIndex = m_size;
		m_entityToIndex[entity] = newIndex;
		m_indexToEntity[newIndex] = entity;
		m_componentArray.emplace_back(component);
		++m_size;
	}

	template<typename... Args>
	T& EmplaceData(Entity entity, Args&&... args)
	{
		assert(m_entityToIndex.find(entity) == m_entityToIndex.end() && "同じEntityに二重追加はできません");
		size_t newIndex = m_size;
		m_entityToIndex[entity] = newIndex;
		m_indexToEntity[newIndex] = entity;
		m_componentArray.emplace_back(std::forward<Args>(args)...);
		++m_size;
		return m_componentArray.back();
	}

	// コンポネントを削除
	void RemoveData(Entity entity)
	{
		assert(m_entityToIndex.find(entity) != m_entityToIndex.end() && "存在しないEntity");
		size_t removedIndex = m_entityToIndex[entity];
		size_t lastIndex = m_size - 1;

		// 末尾の要素を削除位置に移動
		if (removedIndex != lastIndex)
		{
			m_componentArray[removedIndex] = std::move(m_componentArray[lastIndex]);

			Entity lastEntity = m_indexToEntity[lastIndex];

			m_entityToIndex[lastEntity] = removedIndex;
			m_indexToEntity[removedIndex] = lastEntity;
		}

		m_componentArray.pop_back();

		m_entityToIndex.erase(entity);
		m_indexToEntity.erase(lastIndex);
		--m_size;
	}

	// コンポネントへの参照を取得
	T& GetData(Entity entity)
	{
		assert(m_entityToIndex.find(entity) != m_entityToIndex.end() && "存在しないEntity");
		return m_componentArray[m_entityToIndex[entity]];
	}

	const T& GetData(Entity entity) const
	{
		assert(m_entityToIndex.find(entity) != m_entityToIndex.end() && "存在しないEntity");
		return m_componentArray.at(m_entityToIndex.at(entity));
	}

	T* TryGetData(Entity entity)
	{
		auto it = m_entityToIndex.find(entity);
		if (it != m_entityToIndex.end())
		{
			return &m_componentArray[it->second];
		}
		return nullptr;
	}

	const T* TryGetData(Entity entity) const
	{
		auto it = m_entityToIndex.find(entity);
		if (it != m_entityToIndex.end())
		{
			return &m_componentArray[it->second];
		}
		return nullptr;
	}

	// Entity破壊時の処理
	void EntityDestroyed(Entity entity) override
	{
		if (m_entityToIndex.find(entity) != m_entityToIndex.end())
		{
			RemoveData(entity);
		}
	}

private:
	std::vector<T> m_componentArray;
	std::unordered_map<Entity, size_t> m_entityToIndex;
	std::unordered_map<size_t, Entity> m_indexToEntity;
	size_t m_size = 0;
};

// コンポネント型の管理
class ComponentManager
{
public:
	// コンポネント型を登録
	template<typename T>
	void RegisterComponent()
	{
		std::type_index typeName = std::type_index(typeid(T));
		assert(m_componentTypes.find(typeName) == m_componentTypes.end() && "同じ型の二重登録はできません");
		m_componentTypes.insert({ typeName, m_nextComponentType });
		m_componentArrays.insert({ typeName, std::make_shared<ComponentArray<T>>() });
		++m_nextComponentType;
	}

	// コンポネント型のIDを取得
	template<typename T>
	ComponentType GetComponentType()
	{
		std::type_index typeName = std::type_index(typeid(T));
		assert(m_componentTypes.find(typeName) != m_componentTypes.end() && "未登録のコンポネント型");
		return m_componentTypes[typeName];
	}

	// コンポネントを追加
	template<typename T>
	void AddComponent(Entity entity, const T& component)
	{
		GetComponentArray<T>().InsertData(entity, component);
	}

	template<typename T, typename... Args>
	T& EmplaceComponent(Entity entity, Args&&... args)
	{
		return GetComponentArray<T>().EmplaceData(entity, std::forward<Args>(args)...);
	}

	// コンポネントを削除
	template<typename T>
	void RemoveComponent(Entity entity)
	{
		GetComponentArray<T>().RemoveData(entity);
	}

	// コンポネントへの参照を取得
	template<typename T>
	T& GetComponent(Entity entity)
	{
		return GetComponentArray<T>().GetData(entity);
	}

	template<typename T>
	const T& GetComponent(Entity entity) const
	{
		return GetComponentArray<T>().GetData(entity);
	}

	template<typename T>
	T* TryGetComponent(Entity entity)
	{
		return GetComponentArray<T>().TryGetData(entity);
	}

	template<typename T>
	const T* TryGetComponent(Entity entity) const
	{
		return GetComponentArray<T>().TryGetData(entity);
	}

	// Entity破壊時に全ComponentArrayに通知
	void EntityDestroyed(Entity entity)
	{
		for (auto const& pair : m_componentArrays)
		{
			pair.second->EntityDestroyed(entity);
		}
	}

private:
	// 型名 ⇔ ComponentType（ID）のマッピング
	std::unordered_map<std::type_index, ComponentType> m_componentTypes;
	// 型名 ⇔ ComponentArrayのマッピング
	std::unordered_map<std::type_index, std::shared_ptr<IComponentArray>> m_componentArrays;
	// 次に割り当てるComponentType
	ComponentType m_nextComponentType = 0;

	// 型指定でComponentArray取得
	template<typename T>
	ComponentArray<T>& GetComponentArray()
	{
		std::type_index typeName = std::type_index(typeid(T));
		assert(m_componentTypes.find(typeName) != m_componentTypes.end() && "未登録のコンポネント型");

		return *std::static_pointer_cast<ComponentArray<T>>(m_componentArrays[typeName]);
	}

	template<typename T>
	const ComponentArray<T>& GetComponentArray() const
	{
		std::type_index typeName = std::type_index(typeid(T));
		assert(m_componentTypes.find(typeName) != m_componentTypes.end() && "未登録のコンポネント型");

		return *std::static_pointer_cast<ComponentArray<T>>(m_componentArrays.at(typeName));
	}
};
