#pragma once
#include <cstdint>
#include <cstdlib>
#include <new>
#include <utility>
#include <vector>
template <typename T, size_t ChunkSize = 1024>
class PoolAllocator {
    struct Node {
        Node* next;
    };

public:
    PoolAllocator() : m_head(nullptr) {}
    ~PoolAllocator() {
        for (void* chunk : m_chunks) {
            free(chunk);
        }
    }

    template<typename... Args>
    T* Allocate(Args&&... args) {
        if (!m_head) {
            AllocateChunk();
        }
        Node* node = m_head;
        m_head = m_head->next;
        T* ptr = reinterpret_cast<T*>(node);
        new (ptr) T(std::forward<Args>(args)...);
        return ptr;
    }

    void Free(T* ptr) {
        if (!ptr) return;
        ptr->~T();
        Node* node = reinterpret_cast<Node*>(ptr);
        node->next = m_head;
        m_head = node;
    }

private:
    void AllocateChunk() {
        size_t objectSize = sizeof(T) > sizeof(Node) ? sizeof(T) : sizeof(Node);
        void* chunk = malloc(objectSize * ChunkSize);
        m_chunks.push_back(chunk);
        
        char* p = static_cast<char*>(chunk);
        for (size_t i = 0; i < ChunkSize; ++i) {
            Node* node = reinterpret_cast<Node*>(p + i * objectSize);
            node->next = m_head;
            m_head = node;
        }
    }

    Node* m_head;
    std::vector<void*> m_chunks;
};