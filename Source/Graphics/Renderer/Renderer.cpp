#include "../../Pch.h"
#include "Renderer.h"
#include "../GPUResource/RenderTarget/RenderTarget.h"
#include "../../Graphics/Device/GraphicsDevice.h"

static RenderContext s_renderContext;

RenderContext& Renderer::BeginFrame()
{
	// Initialization/clear operations for the frame can be done here.
	return s_renderContext;
}

void Renderer::EndFrame()
{
	// Frame presentation or ending logic
}

void Renderer::BindViewport(RenderTarget* pRT)
{
	if (!pRT) return;
	auto* cmd = GraphicsDevice::Instance().GetCmdList();
	D3D12_VIEWPORT viewport = {};
	viewport.Width = (float)pRT->GetWidth();
	viewport.Height = (float)pRT->GetHeight();
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	D3D12_RECT rect = {};
	rect.right = pRT->GetWidth();
	rect.bottom = pRT->GetHeight();

	cmd->RSSetViewports(1, &viewport);
	cmd->RSSetScissorRects(1, &rect);
}

RenderContext& Renderer::GetContext()
{
	return s_renderContext;
}
