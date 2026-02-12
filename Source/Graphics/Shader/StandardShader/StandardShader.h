#pragma once
#include "../Pipeline/Pipeline.h"
#include "../RootSignature/RootSignature.h"

// =============================================
// StandardShader
// 3Dモデル描画用シェーダー
// ShaderManagerから呼び出して使う
// =============================================
class StandardShader
{
public:
	// 初期化
	void Create(GraphicsDevice* pGraphicsDevice);

	// モデル描画(ワールド行列込み)
	void DrawModel(const ModelData& modelData, const Math::Matrix& mWorld);

private:
	void Begin();
	void DrawMesh(const Mesh& mesh);
	void SetMaterial(const Material& material);
	void LoadShaderFile(const std::wstring& filePath);

	GraphicsDevice* m_pDevice = nullptr;
	std::unique_ptr<Pipeline>		m_upPipeline = nullptr;
	std::unique_ptr<RootSignature>	m_upRootSignature = nullptr;

	ID3DBlob* m_pVSBlob = nullptr;
	ID3DBlob* m_pHSBlob = nullptr;
	ID3DBlob* m_pDSBlob = nullptr;
	ID3DBlob* m_pGSBlob = nullptr;
	ID3DBlob* m_pPSBlob = nullptr;

	UINT m_cbvCount = 0;
};
