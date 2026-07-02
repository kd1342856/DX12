#pragma once

#include "../Pipeline/Pipeline.h"
#include "../RootSignature/RootSignature.h"
#include "../../Device/GraphicsDevice.h"
#include "../../../Framework/DirectX/GDF/GDF.h"

class ShadowShader
{
public:
	void Create(GraphicsDevice* pGraphicsDevice);
	void Begin();
	void End();
	void DrawModel(const ModelData& modelData, const Math::Matrix& mWorld);
	bool IsCreated() const { return m_upPipeline != nullptr; }

private:
	void DrawMesh(const Mesh& mesh);
	void LoadShaderFile(const std::wstring& filePath);

	GraphicsDevice* m_pDevice = nullptr;
	std::unique_ptr<Pipeline>		m_upPipeline = nullptr;
	std::unique_ptr<RootSignature>	m_upRootSignature = nullptr;

	Microsoft::WRL::ComPtr<ID3DBlob> m_pVSBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> m_pPSBlob = nullptr;

	UINT m_cbvCount = 0;
};
