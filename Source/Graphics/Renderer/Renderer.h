#pragma once
#include "RenderContext.h"
#include "../../Framework/ImGuiEditor/EditorContext.h"

class RenderTarget;

class Renderer {
public:
	static RenderContext& BeginFrame();
	static void EndFrame();
	
	static void BindViewport(RenderTarget* pRT);
	static void BindDefaultViewport();

	static RenderContext& GetContext();

	// Post Process Render Targets
	static void InitializeRenderTargets(int width, int height);

	// Reflection
	static class RenderTarget* GetPlanarReflectionRenderTarget();

	static RenderTarget* GetSceneHDRRenderTarget();
	static RenderTarget* GetSceneOpaqueCopyRenderTarget();
	static RenderTarget* GetBloomExtractRenderTarget();
	static RenderTarget* GetBloomBlurRenderTarget(int index);
};