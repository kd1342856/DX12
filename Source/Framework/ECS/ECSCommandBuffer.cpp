#include "ECSCommandBuffer.h"
#include "ECSCoordinator.h"

void ECSCommandBuffer::Playback(ECSCoordinator* coordinator)
{
    size_t offset = 0;
    while (offset < m_buffer.size())
    {
        ECSCommandHeader* header = reinterpret_cast<ECSCommandHeader*>(&m_buffer[offset]);
        offset += sizeof(ECSCommandHeader);

        const void* data = nullptr;
        if (header->dataSize > 0)
        {
            data = &m_buffer[offset];
            offset += header->dataSize;
        }

        switch (header->type)
        {
        case ECSCommandType::InitializeEntity:
            coordinator->InitializeEntity(header->entity);
            break;
        case ECSCommandType::DestroyEntity:
            coordinator->DestroyEntity(header->entity);
            break;
        case ECSCommandType::AddComponent:
        case ECSCommandType::RemoveComponent:
            if (header->executeFn)
            {
                header->executeFn(coordinator, header->entity, data);
            }
            break;
        }
    }
    m_buffer.clear();
}
