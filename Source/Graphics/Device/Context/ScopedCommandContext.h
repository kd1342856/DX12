#pragma once

#include "ContextPool.h"

template<class T>
class ScopedCommandContext
{
public:
    ScopedCommandContext(ContextPool<T>* pool, T* ctx)
        : m_pPool(pool)
        , m_pContext(ctx)
    {
    }

    ~ScopedCommandContext()
    {
        if (m_pPool && m_pContext)
        {
            m_pPool->Release(m_pContext);
        }
    }

    // コピー禁止、ムーブ許可
    ScopedCommandContext(const ScopedCommandContext&) = delete;
    ScopedCommandContext& operator=(const ScopedCommandContext&) = delete;

    ScopedCommandContext(ScopedCommandContext&& other) noexcept
        : m_pPool(other.m_pPool)
        , m_pContext(other.m_pContext)
    {
        other.m_pPool = nullptr;
        other.m_pContext = nullptr;
    }

    ScopedCommandContext& operator=(ScopedCommandContext&& other) noexcept
    {
        if (this != &other)
        {
            if (m_pPool && m_pContext)
            {
                m_pPool->Release(m_pContext);
            }
            m_pPool = other.m_pPool;
            m_pContext = other.m_pContext;
            other.m_pPool = nullptr;
            other.m_pContext = nullptr;
        }
        return *this;
    }

    T* operator->() const { return m_pContext; }
    T& operator*() const { return *m_pContext; }
    T* Get() const { return m_pContext; }

private:
    ContextPool<T>* m_pPool = nullptr;
    T* m_pContext = nullptr;
};
