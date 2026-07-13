#pragma once
#include "../GraphicsShader/GraphicsShader.h"

class RenderTarget;

class PostProcessShader : public GraphicsShader
{
public:
	virtual void Create(GraphicsDevice* pGraphicsDevice) override;
	void Draw(RenderTarget* pRenderTarget, float exposure);
};
