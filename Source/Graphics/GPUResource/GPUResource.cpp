#include "../../Pch.h"
#include "GPUResource.h"
#include "../Device/GraphicsDevice.h"

GPUResource::~GPUResource()
{
    // 終了処理が始まっている場合はm_deviceに一切触らない。
    // (静的破棄順序次第でGraphicsDevice側のオブジェクト自体が既に壊れていることがあり、
    //  m_device->GetQueueManager()等を呼ぶと不正アクセスで落ちるため。
    //  IsShuttingDown()は単純なstatic boolなのでこの状況でも安全に読める)
    if (m_resource && m_device && !GraphicsDevice::IsShuttingDown())
    {
        auto* pQueueMgr = m_device->GetQueueManager();
        if (pQueueMgr && pQueueMgr->GetGraphicsQueue())
        {
            uint64_t fenceVal = pQueueMgr->GetGraphicsQueue()->GetFenceValue();
            m_device->DeferDeleteResource(std::move(m_resource), fenceVal);
        }
        else
        {
            // アプリケーション終了時など、キューが既に破棄されている場合はそのまま解放
            m_resource.Reset();
        }
    }
    else if (m_resource)
    {
        // 終了処理中はGPU側の後始末を待たず、そのまま解放するだけにする
        m_resource.Reset();
    }
}
