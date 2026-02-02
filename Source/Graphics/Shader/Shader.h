#pragma once
#include "Pipeline/Pipeline.h"
#include "RootSignature/RootSignature.h"

struct RenderingSetting
{
	std::vector<InputLayout> InputLayouts;
	std::vector<DXGI_FORMAT> Formats;
	CullMode CullMode = CullMode::Back;
	BlendMode BlendMode = BlendMode::Alpha;
	PrimitiveTopologyType PrimitiveTopologyType = PrimitiveTopologyType::Triangle;
	bool IsDepth = true;
	bool IsDepthMask = true;
	int RTVCount = 1;
	bool IsWireFrame = false;
};

class Shader
{
public:
	//	作成
	void Create(GraphicsDevice* pGraphicsDevice, const std::wstring& filePath,
		const RenderingSetting& renderingSetting, const std::vector<RangeType>& rangeTypes);

	//	描画開始
	void Begin(int w, int h);

	//	メッシュの描画
	void DrawMesh(const Mesh& mesh);
private:
	void LoadShaderFile(const std::wstring& filePath);

	GraphicsDevice* m_pDevice = nullptr;


	std::unique_ptr<Pipeline>		m_upPipeline = nullptr;
	std::unique_ptr<RootSignature>	m_upRootSignature = nullptr;

	ID3DBlob* m_pVSBlob = nullptr;		//	頂点シェーダー
	ID3DBlob* m_pHSBlob = nullptr;		//	ハルシェーダー
	ID3DBlob* m_pDSBlob = nullptr;		//	ドメインシェーダー
	ID3DBlob* m_pGSBlob = nullptr;		//	ジオメトリシェーダー
	ID3DBlob* m_pPSBlob = nullptr;		//	ピクセルシェーダー
};
