#pragma once
#include "../FrameResource.h"
#include <vector>

class FrameManager
{
public:
    static constexpr int kFrameCount = 2;

    FrameManager() = default;
    ~FrameManager() = default;

    bool Init(GraphicsDevice* pDevice);
    void Shutdown();

    FrameResource& AcquireFrame();

    // 現在のフレームインチE??クスを進める
    void MoveNextFrame();

    FrameResource& GetCurrentFrameResource() { return m_frames[m_frameIndex % kFrameCount]; }
    UINT GetFrameIndex() const { return m_frameIndex; }

private:
    std::vector<FrameResource> m_frames;
    UINT m_frameIndex = 0;
};


