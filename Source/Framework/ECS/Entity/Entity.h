#include <mutex>
#pragma once
#include <cstdint>

// =============================================
// Entity
// Entity = EntityID (uint32_t)
// =============================================

using Entity = uint32_t;
constexpr Entity MAX_ENTITIES = 4096;
constexpr uint8_t MAX_COMPONENTS = 64;
constexpr Entity INVALID_ENTITY = UINT32_MAX;

using ComponentType = uint8_t;
using Signature = std::bitset<MAX_COMPONENTS>;

// =============================================
// EntityManager
// =============================================
class EntityManager
{
    std::mutex m_mutex;
public:
    EntityManager()
    {
        for (Entity entity = 0; entity < MAX_ENTITIES; ++entity)
        {
            m_availableEntities.push(entity);
        }
    }

        Entity AllocateEntity()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        assert(m_livingEntityCount < MAX_ENTITIES);
        Entity entityId = m_availableEntities.front();
        m_availableEntities.pop();
        ++m_livingEntityCount;
        m_alive[entityId] = false; // まだアクティブではない
        return entityId;
    }

    void InitializeEntity(Entity entity)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_alive[entity] = true;
    }

    Entity CreateEntity()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        assert(m_livingEntityCount < MAX_ENTITIES);
        Entity entityId = m_availableEntities.front();
        m_availableEntities.pop();
        ++m_livingEntityCount;
        m_alive[entityId] = true;
        return entityId;
    }

    void DestroyEntity(Entity entity)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        assert(entity < MAX_ENTITIES);
        assert(m_alive[entity]);
        m_signatures[entity].reset();
        m_availableEntities.push(entity);
        m_alive[entity] = false;
        --m_livingEntityCount;
    }

    void SetSignature(Entity entity, Signature signature)
    {
        assert(entity < MAX_ENTITIES);
        m_signatures[entity] = signature;
    }

    const Signature& GetSignature(Entity entity) const
    {
        assert(entity < MAX_ENTITIES);
        return m_signatures[entity];
    }

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
