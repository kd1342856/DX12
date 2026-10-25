#include "../../../Pch.h"
#include "ContextManager.h"

bool ContextManager::Init(ID3D12Device* pDevice)
{
    m_pDevice = pDevice;

    m_upGraphicsContext = std::make_unique<GraphicsCommandContext>();
    if (!m_upGraphicsContext->Init(m_pDevice))
    {
        return false;
    }

    m_uploadPool.Init(m_pDevice);

    return true;
}

void ContextManager::Shutdown()
{
    m_uploadPool.Shutdown();

    if (m_upGraphicsContext)
    {
        m_upGraphicsContext->Shutdown();
        m_upGraphicsContext.reset();
    }
}

ScopedCommandContext<UploadCommandContext> ContextManager::AcquireUploadContext()
{
    UploadCommandContext* pCtx = m_uploadPool.Acquire();
    return ScopedCommandContext<UploadCommandContext>(&m_uploadPool, pCtx);
}
