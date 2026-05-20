#pragma once

// =============================================
// Entity
// Entity = エンティティID (uint32_t)
// Signature でどのComponentを持つか管理する
// =============================================

using Entity = uint32_t;
constexpr Entity MAX_ENTITIES = 4096;
constexpr uint8_t MAX_COMPONENTS = 32;
// 無効なEntityIDの定数（0は有効なIDとして使うため UINT32_MAX を使用）
constexpr Entity INVALID_ENTITY = UINT32_MAX;

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
        // 使用可能なIDを全てキューに積む（0 から MAX_ENTITIES-1 まで全て有効）
        for (Entity entity = 0; entity < MAX_ENTITIES; ++entity)
        {
            m_availableEntities.push(entity);
        }
    }

    // 新規Entityを生成
    Entity CreateEntity()
    {
        assert(m_livingEntityCount < MAX_ENTITIES && "Entityの上限に達しました");
        Entity entityId = m_availableEntities.front();
        m_availableEntities.pop();
        ++m_livingEntityCount;
        m_alive[entityId] = true;
        return entityId;
    }

    // Entityを破棄（二重破棄もガード）
    void DestroyEntity(Entity entity)
    {
        assert(entity < MAX_ENTITIES && "範囲外のEntityIDです");
        assert(m_alive[entity] && "すでに破棄済みのEntityを破棄しようとしています");
        m_signatures[entity].reset();
        m_availableEntities.push(entity);
        m_alive[entity] = false;
        --m_livingEntityCount;
    }

    // Signatureセット
    void SetSignature(Entity entity, Signature signature)
    {
        assert(entity < MAX_ENTITIES && "範囲外のEntityIDです");
        m_signatures[entity] = signature;
    }

    // Signature取得
    Signature GetSignature(Entity entity) const
    {
        assert(entity < MAX_ENTITIES && "範囲外のEntityIDです");
        return m_signatures[entity];
    }

    // 生存確認
    bool IsAlive(Entity entity) const
    {
        if (entity >= MAX_ENTITIES) return false;
        return m_alive[entity];
    }

private:
    std::queue<Entity> m_availableEntities;
    std::array<Signature, MAX_ENTITIES> m_signatures{};
    std::array<bool, MAX_ENTITIES> m_alive{};
    uint32_t m_livingEntityCount = 0;
};