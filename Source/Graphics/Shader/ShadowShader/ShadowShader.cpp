#include "../../../Pch.h"
#include "ShadowShader.h"

void ShadowShader::Create(GraphicsDevice* pGraphicsDevice)
{
	m_pDevice = pGraphicsDevice;
	LoadShaderFile(L"LitShader");

	// Registers for ShadowShader:
	// CBV0: cbPerCamera (b0)  (繝ｩ繧､繝医・陦悟・繧貞・繧後ｋ)
	// CBV1: cbPerDraw (b1)

	std::vector<RangeType> rangeTypes = {
		RangeType::CBV, RangeType::CBV
	};

	m_upRootSignature = std::make_unique<RootSignature>();
	m_upRootSignature->Create(pGraphicsDevice, rangeTypes, m_cbvCount);

	RenderingSetting setting = {};
	setting.InputLayouts = {
		InputLayout::POSITION, InputLayout::TEXCOORD
	};
	setting.Formats = {}; // 豺ｱ蠎ｦ縺ｮ縺ｿ
	setting.RTVCount = 0;
	setting.IsDepth = true;
	setting.IsDepthMask = true;
	setting.CullMode = CullMode::Front; // 繧ｷ繝｣繝峨え繧｢繧ｯ繝榊ｯｾ遲・	setting.BlendMode = BlendMode::None;

	m_upPipeline = std::make_unique<Pipeline>();
	m_upPipeline->SetRenderSettings(pGraphicsDevice, m_upRootSignature.get(), setting.InputLayouts,
		setting.CullMode, setting.BlendMode, setting.PrimitiveTopologyType);
	m_upPipeline->Create({ m_pVSBlob.Get(), nullptr, nullptr, nullptr, nullptr }, setting.Formats,
		setting.IsDepth, setting.IsDepthMask, setting.RTVCount, setting.IsWireFrame);
}

void ShadowShader::LoadShaderFile(const std::wstring& filePath)
{
	ID3DInclude* include = D3D_COMPILE_STANDARD_FILE_INCLUDE;
	// デバッグビルドは最適化スキップ、リリースビルドは最高速最適化
#ifdef _DEBUG
	UINT flag = D3DCOMPILE_DEBUG;
#else
	UINT flag = D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
	ComPtr<ID3DBlob> pErrorBlob = nullptr;

	std::wstring format = L".hlsl";
	std::wstring currentPath = L"Asset/Shader/LitShader/";

	// VS
	{
		std::wstring fullPath = currentPath + filePath + L"_VS" + format;
		auto hResult = D3DCompileFromFile(fullPath.c_str(), nullptr, include, "ShadowCasterVS",
			"vs_5_0", flag, 0, &m_pVSBlob, &pErrorBlob);
		if (FAILED(hResult))
		{
			if (pErrorBlob) OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
			assert(0 && "頂点シェーダーのコンパイルに失敗しました");
			return;
		}
	}
	// PS
	{
		std::wstring fullPath = currentPath + filePath + L"_PS" + format;
		auto hResult = D3DCompileFromFile(fullPath.c_str(), nullptr, include, "ShadowCasterPS",
			"ps_5_0", flag, 0, &m_pPSBlob, &pErrorBlob);
		if (FAILED(hResult))
		{
			if (pErrorBlob) OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
			assert(0 && "ピクセルシェーダーのコンパイルに失敗しました");
			return;
		}
	}
}

void ShadowShader::Begin()
{
	DepthStencil* pShadowMap = m_pDevice->GetShadowMap();

	pShadowMap->TransitionTo(m_pDevice->GetCmdList(), D3D12_RESOURCE_STATE_DEPTH_WRITE);

	m_pDevice->GetCmdList()->SetPipelineState(m_upPipeline->GetPipeline());
	m_pDevice->GetCmdList()->SetGraphicsRootSignature(m_upRootSignature->GetRootSignature());
	m_pDevice->GetCmdList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	D3D12_VIEWPORT viewport = {};
	D3D12_RECT rect = {};
	viewport.Width = 4096.0f; // ShadowMap縺ｮ隗｣蜒丞ｺｦ
	viewport.Height = 4096.0f;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	rect.right = 4096;
	rect.bottom = 4096;

	m_pDevice->GetCmdList()->RSSetViewports(1, &viewport);
	m_pDevice->GetCmdList()->RSSetScissorRects(1, &rect);

	auto dsvH = m_pDevice->GetDSVHeap()->GetCPUHandle(pShadowMap->GetDSVNumber());
	m_pDevice->GetCmdList()->OMSetRenderTargets(0, nullptr, false, &dsvH);

	pShadowMap->ClearBuffer();
}

void ShadowShader::End()
{
	DepthStencil* pShadowMap = m_pDevice->GetShadowMap();
	pShadowMap->TransitionTo(m_pDevice->GetCmdList(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

}

void ShadowShader::DrawModel(const ModelData& modelData, const Math::Matrix& mWorld)
{
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

void ShadowShader::DrawMesh(const Mesh& mesh)
{
	mesh.DrawInstanced(mesh.GetInstanceCount());
}
