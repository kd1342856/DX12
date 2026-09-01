#pragma once
#include <cstdint>
#include <cstdlib>

class LinearAllocator {
public:
    LinearAllocator(size_t size) : m_totalSize(size), m_offset(0) {
        m_startPtr = malloc(size);
    }
    ~LinearAllocator() {
        free(m_startPtr);
    }

    void* Allocate(size_t size, size_t alignment = sizeof(void*)) {
        size_t currentAddr = reinterpret_cast<size_t>(m_startPtr) + m_offset;
        size_t padding = (alignment - (currentAddr % alignment)) % alignment;
        
        if (m_offset + padding + size > m_totalSize) {
            return nullptr; // Out of memory
        }
        
        m_offset += padding;
        size_t nextAddr = currentAddr + padding;
        m_offset += size;
        
        return reinterpret_cast<void*>(nextAddr);
    }

    void Reset() {
        m_offset = 0;
    }

    size_t GetTotalSize() const { return m_totalSize; }
    size_t GetAllocatedSize() const { return m_offset; }

private:
    void* m_startPtr;
    size_t m_totalSize;
    size_t m_offset;
};