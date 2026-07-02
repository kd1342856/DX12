#include "../../../Pch.h"
#include "LitShader.h"

void LitShader::Create(GraphicsDevice* pGraphicsDevice)
{
	m_pDevice = pGraphicsDevice;
	LoadShaderFile(L"LitShader");

	// Registers for LitShader:
	// CBV0: cbPerCamera (b0)
	// CBV1: cbPerDraw (b1)
	// CBV2: cbPerMaterial (b2)
	// CBV3: cbLight (b3)
	// CBV4: cbSystem (b4)
	// SRV0~SRV3: Material Textures (t0~t3)
	// SRV4~SRV8: Common Textures (t4~t8)

	std::vector<RangeType> rangeTypes = {
		RangeType::CBV, RangeType::CBV, RangeType::CBV, RangeType::CBV, RangeType::CBV, // 5 CBVs
		RangeType::SRV, RangeType::SRV, RangeType::SRV, RangeType::SRV,                 // t0-t3
		RangeType::SRV, RangeType::SRV, RangeType::SRV, RangeType::SRV, RangeType::SRV,  // t4-t8
		RangeType::SRV																	// t9
	};

	m_upRootSignature = std::make_unique<RootSignature>();
	m_upRootSignature->Create(pGraphicsDevice, rangeTypes, m_cbvCount);

	RenderingSetting setting = {};
	setting.InputLayouts = {
		InputLayout::POSITION, InputLayout::TEXCOORD, InputLayout::NORMAL, InputLayout::COLOR, InputLayout::TANGENT
	};
	setting.Formats = { DXGI_FORMAT_R8G8B8A8_UNORM };
	setting.BlendMode = BlendMode::None; // Opaque default

	m_upPipeline = std::make_unique<Pipeline>();
	m_upPipeline->SetRenderSettings(pGraphicsDevice, m_upRootSignature.get(), setting.InputLayouts,
		setting.CullMode, setting.BlendMode, setting.PrimitiveTopologyType);
	m_upPipeline->Create({ m_pVSBlob, m_pHSBlob, m_pDSBlob, m_pGSBlob, m_pPSBlob }, setting.Formats,
		setting.IsDepth, setting.IsDepthMask, setting.RTVCount, setting.IsWireFrame);
}

void LitShader::Begin()
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
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	rect.right = 1280;
	rect.bottom = 720;

	GraphicsDevice::Instance().GetCmdList()->RSSetViewports(1, &viewport);
	GraphicsDevice::Instance().GetCmdList()->RSSetScissorRects(1, &rect);

	// シャドウマップのバインド (t7 = m_cbvCount + 7)
	auto* pShadowMap = GraphicsDevice::Instance().GetShadowMap();
	if (pShadowMap && pShadowMap->GetSRVNumber() != -1)
	{
		auto handle = GraphicsDevice::Instance().GetCBVSRVUAVHeap()->GetGPUHandle(pShadowMap->GetSRVNumber());
		GraphicsDevice::Instance().GetCmdList()->SetGraphicsRootDescriptorTable(m_cbvCount + 7, handle);
	}
	//	スポットライトシャドウマップのバインド(t9 = m_cbvCount + 9)
	auto* pSpotShadowMap = GraphicsDevice::Instance().GetSpotShadowMap();
	if (pSpotShadowMap && pSpotShadowMap->GetSRVNumber() != -1)
	{
		auto handle = GraphicsDevice::Instance().GetCBVSRVUAVHeap()->GetGPUHandle(pSpotShadowMap->GetSRVNumber());
		GraphicsDevice::Instance().GetCmdList()->SetGraphicsRootDescriptorTable(m_cbvCount + 9, handle);
	}
	ShaderManager::Instance().BindCameraMatrix(0);
	ShaderManager::Instance().BindLightData(3);
}

void LitShader::DrawModel(const ModelData& modelData, const Math::Matrix& mWorld)
{
	Begin();

	CBufferData::PerDraw cbDraw;
	cbDraw.mWorld = mWorld;
	GDF::Instance().BindCBuffer(1, cbDraw);

	for (auto& node : modelData.GetNodes())
	{
		for (const auto& spMesh : node.meshes)
		{
			DrawMesh(*spMesh);
		}
	}
}

void LitShader::DrawMesh(const Mesh& mesh)
{
	SetMaterial(mesh.GetMaterial());
	mesh.DrawInstanced(mesh.GetInstanceCount());
}

void LitShader::SetMaterial(const Material& material)
{
	CBufferData::Material cbMat = {};
	cbMat.BaseColor = material.BaseColor;
	cbMat.EmissiveColor = Math::Vector4(material.Emissive.x, material.Emissive.y, material.Emissive.z, 1.0f);
	cbMat.Metallic = material.Metallic;
	cbMat.Smoothness = material.Roughness; // Mapping roughness to smoothness to match hlsl name if needed
	
	GDF::Instance().BindCBuffer(2, cbMat);

	if (material.spBaseColorTex) material.spBaseColorTex->Set(m_cbvCount);
	else GraphicsDevice::Instance().GetWhiteTex()->Set(m_cbvCount);

	if (material.spNormalTex) material.spNormalTex->Set(m_cbvCount + 1);
	else GraphicsDevice::Instance().GetNormalTex()->Set(m_cbvCount + 1);

	if (material.spMetallicRoughnessTex) material.spMetallicRoughnessTex->Set(m_cbvCount + 2);
	else GraphicsDevice::Instance().GetWhiteTex()->Set(m_cbvCount + 2);

	if (material.spEmissiveTex) material.spEmissiveTex->Set(m_cbvCount + 3);
	else GraphicsDevice::Instance().GetBlackTex()->Set(m_cbvCount + 3);
}

void LitShader::LoadShaderFile(const std::wstring& filePath)
{
	ID3DInclude* include = D3D_COMPILE_STANDARD_FILE_INCLUDE;
	UINT flag = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
	ID3DBlob* pErrorBlob = nullptr;

	std::wstring format = L".hlsl";
	std::wstring currentPath = L"Asset/Shader/LitShader/";

	// VS
	{
		std::wstring fullPath = currentPath + filePath + L"_VS" + format;
		auto hResult = D3DCompileFromFile(fullPath.c_str(), nullptr, include, "VS",
			"vs_5_0", flag, 0, &m_pVSBlob, &pErrorBlob);
		if (FAILED(hResult))
		{
			if (pErrorBlob)
			{
				OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
			}
			assert(0 && "頂点シェーダーのコンパイルに失敗しました");
			return;
		}
	}
	// PS
	{
		std::wstring fullPath = currentPath + filePath + L"_PS" + format;
		auto hResult = D3DCompileFromFile(fullPath.c_str(), nullptr, include, "PS",
			"ps_5_0", flag, 0, &m_pPSBlob, &pErrorBlob);
		if (FAILED(hResult))
		{
			if (pErrorBlob)
			{
				OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
			}
			assert(0 && "ピクセルシェーダーのコンパイルに失敗しました");
			return;
		}
	}
}