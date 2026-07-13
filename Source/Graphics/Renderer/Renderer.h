#pragma once
#include "RenderContext.h"

class RenderTarget;

class Renderer {
public:
	static RenderContext& BeginFrame();
	static void EndFrame();
	
	static void BindViewport(RenderTarget* pRT);

	static RenderContext& GetContext();
};