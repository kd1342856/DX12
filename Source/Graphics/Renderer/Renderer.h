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

	// Reflection - 最大3枚(3窓)まで同時にアクティブな平面反射をサポートする。
	static class RenderTarget* GetPlanarReflectionRenderTarget(int slot);

	static RenderTarget* GetSceneHDRRenderTarget();
	static RenderTarget* GetSceneOpaqueCopyRenderTarget();
	static RenderTarget* GetBloomExtractRenderTarget();
	static RenderTarget* GetBloomBlurRenderTarget(int index);
	static RenderTarget* GetDOFBlurRenderTarget(int index);
	static RenderTarget* GetGodRaysRenderTarget();
	static RenderTarget* GetNormalPrepassRenderTarget();
	static RenderTarget* GetSSAORenderTarget(int index);

	// Debug Preview Camera - CameraData.m_isDebugPreviewを立てたカメラのTransformから見た絵。
	static RenderTarget* GetDebugPreviewRenderTarget();
};