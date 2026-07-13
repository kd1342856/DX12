#pragma once
#include <vector>
#include <cstddef>
#include <cstring>
#include "ECSCoordinator.h"

class ECSCoordinator;

enum class ECSCommandType : uint8_t
{
    InitializeEntity,
    DestroyEntity,
    AddComponent,
    RemoveComponent
};

using ECSExecuteFn = void(*)(ECSCoordinator*, Entity, const void*);

struct ECSCommandHeader
{
    ECSCommandType type;
    Entity entity;
    ECSExecuteFn executeFn;
    uint32_t dataSize;
};

class ECSCommandBuffer
{
public:
    ECSCommandBuffer()
    {
        m_buffer.reserve(1024 * 64);
    }

    void InitializeEntity(Entity entity)
    {
        WriteCommand(ECSCommandType::InitializeEntity, entity, nullptr, 0, nullptr);
    }

    void DestroyEntity(Entity entity)
    {
        WriteCommand(ECSCommandType::DestroyEntity, entity, nullptr, 0, nullptr);
    }

    template<typename T>
    void AddComponent(Entity entity, const T& component);

    template<typename T>
    void RemoveComponent(Entity entity);

    void Playback(ECSCoordinator* coordinator);

    void Clear()
    {
        m_buffer.clear();
    }

private:
    std::vector<std::byte> m_buffer;

    void WriteCommand(ECSCommandType type, Entity entity, ECSExecuteFn fn, uint32_t dataSize, const void* data)
    {
        ECSCommandHeader header;
        header.type = type;
        header.entity = entity;
        header.executeFn = fn;
        header.dataSize = dataSize;

        size_t currentSize = m_buffer.size();
        m_buffer.resize(currentSize + sizeof(ECSCommandHeader) + dataSize);

        std::memcpy(&m_buffer[currentSize], &header, sizeof(ECSCommandHeader));
        if (dataSize > 0 && data != nullptr)
        {
            std::memcpy(&m_buffer[currentSize + sizeof(ECSCommandHeader)], data, dataSize);
        }
    }
};
