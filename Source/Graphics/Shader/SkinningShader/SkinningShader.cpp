#include "../../../Pch.h"
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
		InputLayout::POSITION, InputLayout::TEXCOORD, InputLayout::NORMAL,
		InputLayout::COLOR, InputLayout::TANGENT,
		InputLayout::SKININDEX, InputLayout::SKINWEIGHT
	};
	setting.Formats = { DXGI_FORMAT_R8G8B8A8_UNORM };
	setting.RTVCount = 1;

	m_upPipeline = std::make_unique<Pipeline>();
	m_upPipeline->SetRenderSettings(pGraphicsDevice, m_upRootSignature.get(), setting.InputLayouts,
		setting.CullMode, setting.BlendMode, setting.PrimitiveTopologyType);
	m_upPipeline->Create({ m_pVSBlob, m_pHSBlob, m_pDSBlob, m_pGSBlob, m_pPSBlob }, setting.Formats,
		setting.IsDepth, setting.IsDepthMask, setting.RTVCount, setting.IsWireFrame);

	// シャドウ用パイプライン作成
	RenderingSetting shadowSetting = setting;
	shadowSetting.Formats = {};
	shadowSetting.RTVCount = 0;
	shadowSetting.CullMode = CullMode::None; // 両面描画で影を落とす
	m_upShadowPipeline = std::make_unique<Pipeline>();
	m_upShadowPipeline->SetRenderSettings(pGraphicsDevice, m_upRootSignature.get(), shadowSetting.InputLayouts,
		shadowSetting.CullMode, shadowSetting.BlendMode, shadowSetting.PrimitiveTopologyType);
	m_upShadowPipeline->Create({ m_pVSBlob, nullptr, nullptr, nullptr, nullptr }, shadowSetting.Formats,
		shadowSetting.IsDepth, shadowSetting.IsDepthMask, shadowSetting.RTVCount, shadowSetting.IsWireFrame);
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
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	rect.right = 1280;
	rect.bottom = 720;

	m_pDevice->GetCmdList()->RSSetViewports(1, &viewport);
	m_pDevice->GetCmdList()->RSSetScissorRects(1, &rect);

	ShaderManager::Instance().BindCameraMatrix(0);
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

		CBufferData::PerDraw cbDraw; cbDraw.mWorld = mWorld; GDF::Instance().BindCBuffer(1, cbDraw);

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
	else GraphicsDevice::Instance().GetNormalTex()->Set(m_cbvCount + 1);

	if (material.spMetallicRoughnessTex) material.spMetallicRoughnessTex->Set(m_cbvCount + 2);
	else GraphicsDevice::Instance().GetWhiteTex()->Set(m_cbvCount + 2);

	if (material.spEmissiveTex) material.spEmissiveTex->Set(m_cbvCount + 3);
	else GraphicsDevice::Instance().GetBlackTex()->Set(m_cbvCount + 3);
}

void SkinningShader::LoadShaderFile(const std::wstring& filePath)
{
	ID3DInclude* include = D3D_COMPILE_STANDARD_FILE_INCLUDE;
	UINT flag = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
	ComPtr<ID3DBlob> pErrorBlob = nullptr;

	std::wstring format = L".hlsl";
	std::wstring currentPath = L"Asset/Shader/" + filePath + L"/";

	// VS
	{
		std::wstring fullPath = currentPath + filePath + L"_VS" + format;
		auto hr = D3DCompileFromFile(fullPath.c_str(), nullptr, include, "main",
			"vs_5_0", flag, 0, &m_pVSBlob, &pErrorBlob);
		if (FAILED(hr))
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
		auto hr = D3DCompileFromFile(fullPath.c_str(), nullptr, include, "main",
			"ps_5_0", flag, 0, &m_pPSBlob, &pErrorBlob);
		if (FAILED(hr))
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

void SkinningShader::BeginShadow()
{
	m_pDevice->GetCmdList()->SetPipelineState(m_upShadowPipeline->GetPipeline());
	m_pDevice->GetCmdList()->SetGraphicsRootSignature(m_upRootSignature->GetRootSignature());
	m_pDevice->GetCmdList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	D3D12_VIEWPORT viewport = {};
	D3D12_RECT rect = {};
	viewport.Width = 4096.0f; // ShadowMapの解像度
	viewport.Height = 4096.0f;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	rect.right = 4096;
	rect.bottom = 4096;

	m_pDevice->GetCmdList()->RSSetViewports(1, &viewport);
	m_pDevice->GetCmdList()->RSSetScissorRects(1, &rect);
}

void SkinningShader::DrawShadowModel(const ModelData& modelData, const Math::Matrix& mWorld, const std::vector<Math::Matrix>& boneMatrices)
{
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

		CBufferData::PerDraw cbDraw; 
		cbDraw.mWorld = nodeWorldMatrices[i]; 
		GDF::Instance().BindCBuffer(1, cbDraw);

		for (const auto& spMesh : node.meshes)
		{
			if (spMesh)
			{
				spMesh->DrawInstanced(spMesh->GetInstanceCount());
			}
		}
	}
}