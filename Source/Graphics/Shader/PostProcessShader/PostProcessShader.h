#pragma once
#include "../Pipeline/Pipeline.h"
#include "../RootSignature/RootSignature.h"
#include "Buffer/RenderTarget/RenderTarget.h"

class PostProcessShader
{
public:
	void Create(GraphicsDevice* pGraphicsDevice);
	void Draw(RenderTarget* pRenderTarget);

private:
	void LoadShaderFile(const std::wstring& filePath);

	GraphicsDevice* m_pDevice = nullptr;
	std::unique_ptr<Pipeline>		m_upPipeline = nullptr;
	std::unique_ptr<RootSignature>	m_upRootSignature = nullptr;

	ID3DBlob* m_pVSBlob = nullptr;
	ID3DBlob* m_pPSBlob = nullptr;
	UINT m_cbvCount = 0;
};