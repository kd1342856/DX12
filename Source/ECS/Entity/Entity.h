#pragma once

// =============================================
// Entity基盤
// Entity = ただのID（uint32_t）
// SignatureでどのComponentを持つか管理
// =============================================

using Entity = uint32_t;
constexpr Entity MAX_ENTITIES = 4096;
constexpr uint8_t MAX_COMPONENTS = 32;

using ComponentType = uint8_t;
using Signature = std::bitset<MAX_COMPONENTS>;

// =============================================
// EntityManager
// Entityの生成・破棄・Signature管理
// =============================================
class EntityManager
{
public:
	EntityManager()
	{
		// 使用可能なIDを全部キューに入れとく
		for (Entity entity = 0; entity < MAX_ENTITIES; ++entity)
		{
			m_availableEntities.push(entity);
		}
	}

	// 新しいEntityを生成
	Entity CreateEntity()
	{
		assert(m_livingEntityCount < MAX_ENTITIES && "Entity数が上限超えてるで");
		Entity entityId = m_availableEntities.front();
		m_availableEntities.pop();
		++m_livingEntityCount;
		return entityId;
	}

	// Entityを破棄
	void DestroyEntity(Entity entity)
	{
		assert(entity < MAX_ENTITIES && "範囲外のEntityや");
		m_signatures[entity].reset();
		m_availableEntities.push(entity);
		--m_livingEntityCount;
	}

	// Signatureをセット
	void SetSignature(Entity entity, Signature signature)
	{
		assert(entity < MAX_ENTITIES && "範囲外のEntityや");
		m_signatures[entity] = signature;
	}

	// Signatureを取得
	Signature GetSignature(Entity entity) const
	{
		assert(entity < MAX_ENTITIES && "範囲外のEntityや");
		return m_signatures[entity];
	}

private:
	std::queue<Entity> m_availableEntities;
	std::array<Signature, MAX_ENTITIES> m_signatures{};
	uint32_t m_livingEntityCount = 0;
};
