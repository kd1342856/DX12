#include "../../Pch.h"
#include "Renderer.h"
#include "../../Graphics/Device/GraphicsDevice.h"

void Renderer::BeginFrame() {
    D3D12_VIEWPORT viewport = {};
    viewport.Width = 1280.0f;
    viewport.Height = 720.0f;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    D3D12_RECT scissorRect = { 0, 0, 1280, 720 };
    
    GraphicsDevice::Instance().GetCmdList()->RSSetViewports(1, &viewport);
    GraphicsDevice::Instance().GetCmdList()->RSSetScissorRects(1, &scissorRect);
}

void Renderer::EndFrame() {
    // Currently no-op. Extension point for future post-processing or submission logic.
}
