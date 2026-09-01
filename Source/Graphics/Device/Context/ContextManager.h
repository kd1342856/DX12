#pragma once

#include "ContextPool.h"
#include "ScopedCommandContext.h"
#include "GraphicsCommandContext.h"
#include "UploadCommandContext.h"
#include <memory>

class ContextManager
{
public:
    ContextManager() = default;
    ~ContextManager() { Shutdown(); }

    bool Init(ID3D12Device* pDevice);
    void Shutdown();

    // GraphicsContextは単一インスタンスとして保持する
    GraphicsCommandContext* GetGraphicsContext() const { return m_upGraphicsContext.get(); }

    // UploadContextはPoolから取得する
    ScopedCommandContext<UploadCommandContext> AcquireUploadContext();

private:
    ID3D12Device* m_pDevice = nullptr;

    std::unique_ptr<GraphicsCommandContext> m_upGraphicsContext;
    ContextPool<UploadCommandContext> m_uploadPool;
};
