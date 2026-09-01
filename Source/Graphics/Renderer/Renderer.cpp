#include "../../Pch.h"
#include "Renderer.h"
#include "../GPUResource/RenderTarget/RenderTarget.h"
#include "../../Graphics/Device/GraphicsDevice.h"
#include "../GDF/GDF.h"
#include "../Shader/ShaderManager/ShaderManager.h"

static RenderContext s_renderContext;

RenderContext& Renderer::BeginFrame()
{
	ShaderManager::Instance().UpdateConstantBuffers();
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

void Renderer::BindDefaultViewport()
{
	auto* cmd = GraphicsDevice::Instance().GetCmdList();
	D3D12_VIEWPORT viewport = {};
	viewport.Width = 1280.0f;
	viewport.Height = 720.0f;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	D3D12_RECT rect = {};
	rect.right = 1280;
	rect.bottom = 720;

	cmd->RSSetViewports(1, &viewport);
	cmd->RSSetScissorRects(1, &rect);
}

RenderContext& Renderer::GetContext()
{
	return s_renderContext;
}



static std::unique_ptr<RenderTarget> s_sceneHDR;
static std::unique_ptr<RenderTarget> s_sceneOpaqueCopy;
static std::unique_ptr<RenderTarget> s_planarReflection;
static std::unique_ptr<RenderTarget> s_bloomExtract;
static std::unique_ptr<RenderTarget> s_bloomBlur[2];

void Renderer::InitializeRenderTargets(int width, int height)
{
	// HDR (16-bit Float for bright values > 1.0)
	s_sceneHDR = std::make_unique<RenderTarget>();
	s_sceneHDR->Create(width, height, DXGI_FORMAT_R16G16B16A16_FLOAT);

	// Opaque Copy for Refraction
	s_sceneOpaqueCopy = std::make_unique<RenderTarget>();
	s_sceneOpaqueCopy->Create(width, height, DXGI_FORMAT_R16G16B16A16_FLOAT);

	// Planar Reflection (1024x1024 fixed or scaled, we use screen width/height for now or fixed 1024)
	s_planarReflection = std::make_unique<RenderTarget>();
	s_planarReflection->Create(1024, 1024, DXGI_FORMAT_R16G16B16A16_FLOAT);

	// Bloom Extract (Half resolution, also HDR)
	s_bloomExtract = std::make_unique<RenderTarget>();
	s_bloomExtract->Create(width / 2, height / 2, DXGI_FORMAT_R16G16B16A16_FLOAT);

	// Bloom Blur Ping-Pong (Half resolution)
	s_bloomBlur[0] = std::make_unique<RenderTarget>();
	s_bloomBlur[0]->Create(width / 2, height / 2, DXGI_FORMAT_R16G16B16A16_FLOAT);
	s_bloomBlur[1] = std::make_unique<RenderTarget>();
	s_bloomBlur[1]->Create(width / 2, height / 2, DXGI_FORMAT_R16G16B16A16_FLOAT);
}

RenderTarget* Renderer::GetSceneHDRRenderTarget() { return s_sceneHDR.get(); }
RenderTarget* Renderer::GetSceneOpaqueCopyRenderTarget() { return s_sceneOpaqueCopy.get(); }
RenderTarget* Renderer::GetPlanarReflectionRenderTarget() { return s_planarReflection.get(); }
RenderTarget* Renderer::GetBloomExtractRenderTarget() { return s_bloomExtract.get(); }
RenderTarget* Renderer::GetBloomBlurRenderTarget(int index) { return s_bloomBlur[index % 2].get(); }
