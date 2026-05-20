#include "SkinningShader.h"
#include "../../Buffer/CBufferAllocator/CBufferData/CBufferData.h"

void SkinningShader::Create(GraphicsDevice* pGraphicsDevice)
{
	m_pDevice = pGraphicsDevice;
	LoadShaderFile(L"SkinningShader");

	std::vector<RangeType> rangeTypes = {
		RangeType::CBV, RangeType::CBV, RangeType::CBV, // cbCamera, cbWorld, cbBones
		RangeType::SRV, RangeType::SRV, RangeType::SRV, RangeType::SRV
	};
	m_cbvCount = 0;

	m_upRootSignature = std::make_unique<RootSignature>();
	m_upRootSignature->Create(pGraphicsDevice, rangeTypes, m_cbvCount);

	RenderingSetting setting = {};
	setting.InputLayouts = {
		InputLayout::POSITION, InputLayout::TEXCOORD, InputLayout::COLOR,
		InputLayout::NORMAL, InputLayout::TANGENT,
		InputLayout::SKININDEX, InputLayout::SKINWEIGHT
	};
	setting.Formats = { DXGI_FORMAT_R8G8B8A8_UNORM };

	m_upPipeline = std::make_unique<Pipeline>();
	m_upPipeline->SetRenderSettings(pGraphicsDevice, m_upRootSignature.get(), setting.InputLayouts,
		setting.CullMode, setting.BlendMode, setting.PrimitiveTopologyType);
	m_upPipeline->Create({ m_pVSBlob, m_pHSBlob, m_pDSBlob, m_pGSBlob, m_pPSBlob }, setting.Formats,
		setting.IsDepth, setting.IsDepthMask, setting.RTVCount, setting.IsWireFrame);
}

void SkinningShader::Begin()
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

	m_pDevice->GetCmdList()->RSSetViewports(1, &viewport);
	m_pDevice->GetCmdList()->RSSetScissorRects(1, &rect);
}

void SkinningShader::DrawModel(const ModelData& modelData, const Math::Matrix& mWorld, const std::vector<Math::Matrix>& boneMatrices)
{
	Begin();

	// ボーン行列を定数バッファにセット
	CBufferData::Bones cbBones;
	for (size_t i = 0; i < boneMatrices.size() && i < 256; ++i)
	{
		cbBones.mBones[i] = boneMatrices[i];
	}
	GDF::Instance().BindCBuffer(2, cbBones);

	const auto& nodes = modelData.GetNodes();
	std::vector<Math::Matrix> nodeWorldMatrices(nodes.size());

	for (int i = 0; i < (int)nodes.size(); ++i)
	{
		const auto& node = nodes[i];
		aiMatrix4x4 aiMat = node.localTransform;
		Math::Matrix matLocal = *(Math::Matrix*)&aiMat;

		if (node.parentIndex == -1)
		{
			nodeWorldMatrices[i] = matLocal * mWorld;
		}
		else
		{
			nodeWorldMatrices[i] = matLocal * nodeWorldMatrices[node.parentIndex];
		}

		GDF::Instance().BindCBuffer(1, mWorld);

		for (const auto& spMesh : node.meshes)
		{
			if (spMesh)
			{
				DrawMesh(*spMesh);
			}
		}
	}
}

void SkinningShader::DrawMesh(const Mesh& mesh)
{
	SetMaterial(mesh.GetMaterial());
	mesh.DrawInstanced(mesh.GetInstanceCount());
}

void SkinningShader::SetMaterial(const Material& material)
{
	if (material.spBaseColorTex) material.spBaseColorTex->Set(m_cbvCount);
	else GraphicsDevice::Instance().GetWhiteTex()->Set(m_cbvCount);

	if (material.spNormalTex) material.spNormalTex->Set(m_cbvCount + 1);
	else GraphicsDevice::Instance().GetWhiteTex()->Set(m_cbvCount + 1);

	if (material.spMetallicRoughnessTex) material.spMetallicRoughnessTex->Set(m_cbvCount + 2);
	else GraphicsDevice::Instance().GetWhiteTex()->Set(m_cbvCount + 2);

	if (material.spEmissiveTex) material.spEmissiveTex->Set(m_cbvCount + 3);
	else GraphicsDevice::Instance().GetBlackTex()->Set(m_cbvCount + 3);
}

void SkinningShader::LoadShaderFile(const std::wstring& filePath)
{
	ID3DInclude* include = D3D_COMPILE_STANDARD_FILE_INCLUDE;
	UINT flag = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
	ID3DBlob* pErrorBlob = nullptr;

	std::wstring format = L".hlsl";
	std::wstring currentPath = L"Asset/Shader/";

	// VS
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
	// HS
	{
		std::wstring fullPath = currentPath + filePath + L"_HS" + format;
		D3DCompileFromFile(fullPath.c_str(), nullptr, include, "main",
			"hs_5_0", flag, 0, &m_pHSBlob, &pErrorBlob);
	}
	// DS
	{
		std::wstring fullPath = currentPath + filePath + L"_DS" + format;
		D3DCompileFromFile(fullPath.c_str(), nullptr, include, "main",
			"ds_5_0", flag, 0, &m_pDSBlob, &pErrorBlob);
	}
	// GS
	{
		std::wstring fullPath = currentPath + filePath + L"_GS" + format;
		D3DCompileFromFile(fullPath.c_str(), nullptr, include, "main",
			"gs_5_0", flag, 0, &m_pGSBlob, &pErrorBlob);
	}
	// PS
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
