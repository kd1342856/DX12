#pragma once

#include <vector>
#include <queue>
#include <memory>
#include <mutex>

template<class T>
class ContextPool
{
public:
    ContextPool() = default;
    ~ContextPool() { Shutdown(); }

    void Init(ID3D12Device* pDevice)
    {
        m_pDevice = pDevice;
    }

    T* Acquire()
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!m_freeContexts.empty())
        {
            T* ctx = m_freeContexts.front();
            m_freeContexts.pop();
            ctx->Begin();
            return ctx;
        }

        // êVãKçÏê¨
        auto newCtx = std::make_unique<T>();
        if (!newCtx->Init(m_pDevice))
        {
            return nullptr;
        }
        
        T* pCtx = newCtx.get();
        m_contexts.push_back(std::move(newCtx));
        
        pCtx->Begin();
        return pCtx;
    }

    void Release(T* ctx)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_freeContexts.push(ctx);
    }

    void Shutdown()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        for (auto& ctx : m_contexts)
        {
            ctx->Shutdown();
        }
        m_contexts.clear();

        // std::queueÇ…ÇÕclearÇ™Ç»Ç¢ÇÃÇ≈swapÇ≈ãÛÇ…Ç∑ÇÈ
        std::queue<T*> emptyQueue;
        std::swap(m_freeContexts, emptyQueue);
    }

private:
    ID3D12Device* m_pDevice = nullptr;
    std::vector<std::unique_ptr<T>> m_contexts;
    std::queue<T*> m_freeContexts;
    std::mutex m_mutex;
};
