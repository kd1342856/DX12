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
	// width/heightは描画解像度(ウィンドウ解像度×レンダースケール)
	static void InitializeRenderTargets(int width, int height);
	// 描画解像度が変わったときに、画面の大きさに比例するRTを作り直す。GPUの処理が終わってから呼ぶこと
	static void ResizeRenderTargets(int width, int height);
	// 描画解像度(シーンを描くRTの大きさ)。UIとバックバッファはウィンドウ解像度(GraphicsDevice::GetBackBufferWidth)
	static int GetRenderWidth();
	static int GetRenderHeight();

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