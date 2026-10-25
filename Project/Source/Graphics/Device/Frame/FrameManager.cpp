#include "../../../Pch.h"
#include "FrameManager.h"
#include "../GraphicsDevice.h"

GraphicsDevice* m_pDevice = nullptr;

bool FrameManager::Init(GraphicsDevice* pDevice)
{
    m_frames.resize(kFrameCount);
    for (int i = 0; i < kFrameCount; ++i)
    {
        if (!m_frames[i].Init(pDevice))
        {
            return false;
        }
    }
    m_frameIndex = 0;
	m_pDevice = pDevice;
	return true;
}

void FrameManager::Shutdown()
{
    m_frames.clear();
}

FrameResource& FrameManager::AcquireFrame()
{
	const int frameIdx = m_frameIndex % kFrameCount;
	FrameResource& fr = m_frames[frameIdx];
	m_pDevice->GetQueueManager()->GetGraphicsQueue()->WaitForFence(fr.GetFenceValue());
	return fr;
}

void FrameManager::MoveNextFrame()
{
    m_frameIndex++;
}



