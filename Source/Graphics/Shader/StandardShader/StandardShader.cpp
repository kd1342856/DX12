#include "StandardShader.h"

void StandardShader::Create(GraphicsDevice* pGraphicsDevice)
{
	m_pDevice = pGraphicsDevice;
	LoadShaderFile(L"Study");

	std::vector<RangeType> rangeTypes = {
		RangeType::CBV, RangeType::CBV,
		RangeType::SRV, RangeType::SRV, RangeType::SRV, RangeType::SRV
	};

	m_upRootSignature = std::make_unique<RootSignature>();
	m_upRootSignature->Create(pGraphicsDevice, rangeTypes, m_cbvCount);

	RenderingSetting setting = {};
	setting.InputLayouts = {
		InputLayout::POSITION, InputLayout::TEXCOORD, InputLayout::COLOR,
		InputLayout::NORMAL, InputLayout::TANGENT
	};
	setting.Formats = { DXGI_FORMAT_R8G8B8A8_UNORM };

	m_upPipeline = std::make_unique<Pipeline>();
	m_upPipeline->SetRenderSettings(pGraphicsDevice, m_upRootSignature.get(), setting.InputLayouts,
		setting.CullMode, setting.BlendMode, setting.PrimitiveTopologyType);
	m_upPipeline->Create({ m_pVSBlob, m_pHSBlob, m_pDSBlob, m_pGSBlob, m_pPSBlob }, setting.Formats,
		setting.IsDepth, setting.IsDepthMask, setting.RTVCount, setting.IsWireFrame);
}

void StandardShader::Begin()
{
	m_pDevice->GetCmdList()->SetPipelineState(m_upPipeline->GetPipeline());
	m_pDevice->GetCmdList()->SetGraphicsRootSignature(m_upRootSignature->GetRootSignature());

	D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType =
		static_cast<D3D12_PRIMITIVE_TOPOLOGY_TYPE>(m_upPipeline->GetTopologyType());

	switch (topologyType)
	{
	case D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT:
		m_pDevice->GetCmdList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
		break;
	case D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE:
		m_pDevice->GetCmdList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
		break;
	case D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE:
		m_pDevice->GetCmdList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		break;
	case D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH:
		m_pDevice->GetCmdList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);
		break;
	}

	D3D12_VIEWPORT viewport = {};
	D3D12_RECT rect = {};
	viewport.Width = 1280.0f;
	viewport.Height = 720.0f;
	rect.right = 1280;
	rect.bottom = 720;

	GraphicsDevice::Instance().GetCmdList()->RSSetViewports(1, &viewport);
	GraphicsDevice::Instance().GetCmdList()->RSSetScissorRects(1, &rect);
}

void StandardShader::DrawModel(const ModelData& modelData, const Math::Matrix& mWorld)
{
	Begin();

	// ワールド行列バインド
	GDF::Instance().BindCBuffer(1, mWorld);

	for (auto& node : modelData.GetNodes())
	{
		DrawMesh(*node.spMesh);
	}
}

void StandardShader::DrawMesh(const Mesh& mesh)
{
	SetMaterial(mesh.GetMaterial());
	mesh.DrawInstanced(mesh.GetInstanceCount());
}

void StandardShader::SetMaterial(const Material& material)
{
	material.spBaseColorTex->Set(m_cbvCount);
	material.spNormalTex->Set(m_cbvCount + 1);
	material.spMetallicRoughnessTex->Set(m_cbvCount + 2);
	material.spEmissiveTex->Set(m_cbvCount + 3);
}

void StandardShader::LoadShaderFile(const std::wstring& filePath)
{
	ID3DInclude* include = D3D_COMPILE_STANDARD_FILE_INCLUDE;
	UINT flag = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
	ID3DBlob* pErrorBlob = nullptr;

	std::wstring format = L".hlsl";
	std::wstring currentPath = L"Asset/Shader/";

	// 頂点シェーダー
	{
		std::wstring fullPath = currentPath + filePath + L"_VS" + format;
		auto hr = D3DCompileFromFile(fullPath.c_str(), nullptr, include, "main",
			"vs_5_0", flag, 0, &m_pVSBlob, &pErrorBlob);
		if (FAILED(hr))
		{
			assert(0 && "VS compile failed");
			return;
		}
	}
	// ハルシェーダー
	{
		std::wstring fullPath = currentPath + filePath + L"_HS" + format;
		D3DCompileFromFile(fullPath.c_str(), nullptr, include, "main",
			"hs_5_0", flag, 0, &m_pHSBlob, &pErrorBlob);
	}
	// ドメインシェーダー
	{
		std::wstring fullPath = currentPath + filePath + L"_DS" + format;
		D3DCompileFromFile(fullPath.c_str(), nullptr, include, "main",
			"ds_5_0", flag, 0, &m_pDSBlob, &pErrorBlob);
	}
	// ジオメトリシェーダー
	{
		std::wstring fullPath = currentPath + filePath + L"_GS" + format;
		D3DCompileFromFile(fullPath.c_str(), nullptr, include, "main",
			"gs_5_0", flag, 0, &m_pGSBlob, &pErrorBlob);
	}
	// ピクセルシェーダー
	{
		std::wstring fullPath = currentPath + filePath + L"_PS" + format;
		auto hr = D3DCompileFromFile(fullPath.c_str(), nullptr, include, "main",
			"ps_5_0", flag, 0, &m_pPSBlob, &pErrorBlob);
		if (FAILED(hr))
		{
			assert(0 && "PS compile failed");
			return;
		}
	}
}
