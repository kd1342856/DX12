#pragma once
#include "../GraphicsShader/GraphicsShader.h"

class RenderTarget;

class BloomShader : public GraphicsShader
{
public:
	virtual void Create(GraphicsDevice* pGraphicsDevice) override;
	
	// Extracts bright pixels from srcRT to destRT
	void DrawExtract(RenderTarget* pSrcRT, RenderTarget* pDestRT, const CBufferData::PostProcess& postProcessData);

	// Applies a Gaussian blur from srcRT to destRT
	void DrawBlur(RenderTarget* pSrcRT, RenderTarget* pDestRT, const CBufferData::PostProcess& postProcessData, float dirX, float dirY);

private:
	ShaderProgram* m_pExtractProgram = nullptr;
	ShaderProgram* m_pBlurProgram = nullptr;

	ID3D12PipelineState* m_pExtractPipelineState = nullptr;
	ID3D12PipelineState* m_pBlurPipelineState = nullptr;
};
