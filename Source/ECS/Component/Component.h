#pragma once

// =============================================
// Component基盤
// ComponentArray: 密パッキング配列でComponent格納
// ComponentManager: 型登録・追加・削除・取得
// =============================================

// 型消去用インターフェース
class IComponentArray
{
public:
	virtual ~IComponentArray() = default;

	virtual void EntityDestroyed(Entity entity) = 0;
};

// テンプレート型のコンポーネント配列
template<typename T>
class ComponentArray : public IComponentArray
{
public:
	// コンポーネントを追加
	void InsertData(Entity entity, T component)
	{
		assert(m_entityToIndex.find(entity) == m_entityToIndex.end() && "同じEntityに二重追加はできません");
		size_t newIndex = m_size;
		m_entityToIndex[entity] = newIndex;
		m_indexToEntity[newIndex] = entity;
		m_componentArray[newIndex] = component;
		++m_size;
	}

	// コンポーネントを削除
	void RemoveData(Entity entity)
	{
		assert(m_entityToIndex.find(entity) != m_entityToIndex.end() && "存在しないEntity");
		size_t removedIndex = m_entityToIndex[entity];
		size_t lastIndex = m_size - 1;

		// 末尾のデータを削除位置に移動
		m_componentArray[removedIndex] = m_componentArray[lastIndex];

		// マッピング更新
		Entity lastEntity = m_indexToEntity[lastIndex];
		m_entityToIndex[lastEntity] = removedIndex;
		m_indexToEntity[removedIndex] = lastEntity;

		m_entityToIndex.erase(entity);
		m_indexToEntity.erase(lastIndex);
		--m_size;
	}

	// コンポーネントへの参照を取得
	T& GetData(Entity entity)
	{
		assert(m_entityToIndex.find(entity) != m_entityToIndex.end() && "存在しないEntity");
		return m_componentArray[m_entityToIndex[entity]];
	}

	// Entity破棄時の処理
	void EntityDestroyed(Entity entity) override
	{
		if (m_entityToIndex.find(entity) != m_entityToIndex.end())
		{
			RemoveData(entity);
		}
	}

private:
	std::array<T, MAX_ENTITIES> m_componentArray{};
	std::unordered_map<Entity, size_t> m_entityToIndex;
	std::unordered_map<size_t, Entity> m_indexToEntity;
	size_t m_size = 0;
};

// コンポーネント型の管理
class ComponentManager
{
public:
	// コンポーネント型を登録
	template<typename T>
	void RegisterComponent()
	{
		const char* typeName = typeid(T).name();
		assert(m_componentTypes.find(typeName) == m_componentTypes.end() && "同じ型は二重登録できません");
		m_componentTypes.insert({ typeName, m_nextComponentType });
		m_componentArrays.insert({ typeName, std::make_shared<ComponentArray<T>>() });
		++m_nextComponentType;
	}

	// コンポーネント型のIDを取得
	template<typename T>
	ComponentType GetComponentType()
	{
		const char* typeName = typeid(T).name();
		assert(m_componentTypes.find(typeName) != m_componentTypes.end() && "未登録のコンポーネント型");
		return m_componentTypes[typeName];
	}

	// コンポーネントを追加
	template<typename T>
	void AddComponent(Entity entity, T component)
	{
		GetComponentArray<T>()->InsertData(entity, component);
	}

	// コンポーネントを削除
	template<typename T>
	void RemoveComponent(Entity entity)
	{
		GetComponentArray<T>()->RemoveData(entity);
	}

	// コンポーネントへの参照を取得
	template<typename T>
	T& GetComponent(Entity entity)
	{
		return GetComponentArray<T>()->GetData(entity);
	}

	// Entity破棄時に全ComponentArrayに通知
	void EntityDestroyed(Entity entity)
	{
		for (auto const& pair : m_componentArrays)
		{
			pair.second->EntityDestroyed(entity);
		}
	}

private:
	// 型名 → ComponentType（ID）のマッピング
	std::unordered_map<const char*, ComponentType> m_componentTypes;
	// 型名 → ComponentArrayのマッピング
	std::unordered_map<const char*, std::shared_ptr<IComponentArray>> m_componentArrays;
	// 次に割り当てるComponentType
	ComponentType m_nextComponentType = 0;

	// 型指定でComponentArrayを取得
	template<typename T>
	std::shared_ptr<ComponentArray<T>> GetComponentArray()
	{
		const char* typeName = typeid(T).name();
		assert(m_componentTypes.find(typeName) != m_componentTypes.end() && "未登録のコンポーネント型");
		return std::static_pointer_cast<ComponentArray<T>>(m_componentArrays[typeName]);
	}
};
