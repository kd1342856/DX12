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
static std::unique_ptr<RenderTarget> s_dofBlur[2];
static std::unique_ptr<RenderTarget> s_godRays;
static std::unique_ptr<RenderTarget> s_normalPrepass;
static std::unique_ptr<RenderTarget> s_ssao;
static std::unique_ptr<RenderTarget> s_ssaoBlur;

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

	// Bloom Extract (1/4解像度, HDR)。以前は1/2解像度+固定ぼかし半径5texelだったため、
	// Intensityをいじってもほぼ変化が無かった。1/4解像度化＋可変半径＋複数回ブラーの
	// 組み合わせで、ちゃんと画面全体に広がる"ふわっと漏れる光"にする。
	s_bloomExtract = std::make_unique<RenderTarget>();
	s_bloomExtract->Create(width / 4, height / 4, DXGI_FORMAT_R16G16B16A16_FLOAT);

	// Bloom Blur Ping-Pong (1/4解像度)
	s_bloomBlur[0] = std::make_unique<RenderTarget>();
	s_bloomBlur[0]->Create(width / 4, height / 4, DXGI_FORMAT_R16G16B16A16_FLOAT);
	s_bloomBlur[1] = std::make_unique<RenderTarget>();
	s_bloomBlur[1]->Create(width / 4, height / 4, DXGI_FORMAT_R16G16B16A16_FLOAT);

	// Depth of Field用: シーン全体(閾値なし)をコピーしてぼかしたバッファ。
	// BloomExtract/Blurのパスをそのまま(閾値0で)使い回すので専用シェーダーは不要。
	s_dofBlur[0] = std::make_unique<RenderTarget>();
	s_dofBlur[0]->Create(width / 4, height / 4, DXGI_FORMAT_R16G16B16A16_FLOAT);
	s_dofBlur[1] = std::make_unique<RenderTarget>();
	s_dofBlur[1]->Create(width / 4, height / 4, DXGI_FORMAT_R16G16B16A16_FLOAT);

	// God Rays(光条)結果 (1/4解像度。ラジアルブラーなので低解像度でも十分)
	s_godRays = std::make_unique<RenderTarget>();
	s_godRays->Create(width / 4, height / 4, DXGI_FORMAT_R16G16B16A16_FLOAT);

	// SSAO/SSR用のビュー空間法線プリパス(等倍解像度。深度はこのRT自身の専用DepthBufferを使う)
	s_normalPrepass = std::make_unique<RenderTarget>();
	s_normalPrepass->Create(width, height, DXGI_FORMAT_R8G8B8A8_UNORM);

	// SSAO結果 + ぼかし用ピンポン (等倍だとノイズが目立つので半解像度)。
	// フォーマットはBloomのブラーパイプライン(DrawBlur)をそのまま流用するため、
	// BloomShaderのPSOと同じR16G16B16A16_FLOATに合わせてある。
	s_ssao = std::make_unique<RenderTarget>();
	s_ssao->Create(width / 2, height / 2, DXGI_FORMAT_R16G16B16A16_FLOAT);
	s_ssaoBlur = std::make_unique<RenderTarget>();
	s_ssaoBlur->Create(width / 2, height / 2, DXGI_FORMAT_R16G16B16A16_FLOAT);
}

RenderTarget* Renderer::GetSceneHDRRenderTarget() { return s_sceneHDR.get(); }
RenderTarget* Renderer::GetSceneOpaqueCopyRenderTarget() { return s_sceneOpaqueCopy.get(); }
RenderTarget* Renderer::GetPlanarReflectionRenderTarget() { return s_planarReflection.get(); }
RenderTarget* Renderer::GetBloomExtractRenderTarget() { return s_bloomExtract.get(); }
RenderTarget* Renderer::GetBloomBlurRenderTarget(int index) { return s_bloomBlur[index % 2].get(); }
RenderTarget* Renderer::GetDOFBlurRenderTarget(int index) { return s_dofBlur[index % 2].get(); }
RenderTarget* Renderer::GetGodRaysRenderTarget() { return s_godRays.get(); }
RenderTarget* Renderer::GetNormalPrepassRenderTarget() { return s_normalPrepass.get(); }
RenderTarget* Renderer::GetSSAORenderTarget(int index) { return index == 0 ? s_ssao.get() : s_ssaoBlur.get(); }
