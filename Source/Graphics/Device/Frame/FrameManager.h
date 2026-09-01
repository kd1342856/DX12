#pragma once
#include "../FrameResource.h"
#include <vector>

class FrameManager
{
public:
    // 3 frames in flight so the CPU does not stall on the previous frame.
    // NOTE: each FrameResource allocates 10000 CBV descriptors, so raising
    // this also requires a bigger CBV/SRV/UAV heap (see GraphicsDevice::Init).
    static constexpr int kFrameCount = 3;

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


