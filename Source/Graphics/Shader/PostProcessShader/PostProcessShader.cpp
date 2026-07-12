#include "../../../Pch.h"
#include "../../../Framework/DirectX/Utility/Profiler.h"
#include "PostProcessShader.h"
#include "../../Buffer/RenderTarget/RenderTarget.h"

void PostProcessShader::Create(GraphicsDevice* pGraphicsDevice)
{
	m_pDevice = pGraphicsDevice;
	LoadShaderFile(L"PostProcessShader");

	// RangeType for SRV (Texture to process)
	std::vector<RangeType> rangeTypes = { RangeType::CBV, RangeType::SRV };

	m_upRootSignature = std::make_unique<RootSignature>();
	m_upRootSignature->Create(pGraphicsDevice, rangeTypes, m_cbvCount);

	RenderingSetting setting = {};
	setting.Formats = { DXGI_FORMAT_R8G8B8A8_UNORM };
	setting.IsDepth = false;
	setting.IsDepthMask = false;
	setting.CullMode = CullMode::None;
	setting.BlendMode = BlendMode::None;

	m_upPipeline = std::make_unique<Pipeline>();
	m_upPipeline->SetRenderSettings(pGraphicsDevice, m_upRootSignature.get(), setting.InputLayouts,
		setting.CullMode, setting.BlendMode, setting.PrimitiveTopologyType);
	m_upPipeline->Create({ m_pVSBlob, nullptr, nullptr, nullptr, m_pPSBlob }, setting.Formats,
		setting.IsDepth, setting.IsDepthMask, setting.RTVCount, setting.IsWireFrame);
}

void PostProcessShader::Draw(RenderTarget* pRenderTarget, float exposure)
{
	m_pDevice->GetCmdList()->SetPipelineState(m_upPipeline->GetPipeline());
	m_pDevice->GetCmdList()->SetGraphicsRootSignature(m_upRootSignature->GetRootSignature());
	m_pDevice->GetCmdList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 定数バッファをバインチE
	CBufferData::PostProcess cPostProcess;
	cPostProcess.Exposure = (exposure <= 0.0f) ? 1.0f : exposure;
	GDF::Instance().BindCBuffer(0, cPostProcess);

	D3D12_VIEWPORT viewport = {};
	viewport.Width = 1280.0f;
	viewport.Height = 720.0f;
	D3D12_RECT rect = { 0, 0, 1280, 720 };
	m_pDevice->GetCmdList()->RSSetViewports(1, &viewport);
	m_pDevice->GetCmdList()->RSSetScissorRects(1, &rect);

	auto srvHeap = GraphicsDevice::Instance().GetCBVSRVUAVHeap();
	m_pDevice->GetCmdList()->SetGraphicsRootDescriptorTable(1, srvHeap->GetGPUHandle(pRenderTarget->GetSRVIndex()));

	// Draw full screen quad (3 vertices)
	Profiler::Instance().AddDrawCall("PostProcess", 1);
	m_pDevice->GetCmdList()->DrawInstanced(3, 1, 0, 0);
}

void PostProcessShader::LoadShaderFile(const std::wstring& filePath)
{
	ID3DInclude* include = D3D_COMPILE_STANDARD_FILE_INCLUDE;
#if _DEBUG
	UINT flag = D3DCOMPILE_DEBUG;
#else
	UINT flag = 0;
#endif
	ID3DBlob* pErrorBlob = nullptr;

	std::wstring baseFullPath = L"Asset/Shader/PostProcessShader/" + filePath;

	std::wstring vsPath = baseFullPath + L"_VS.hlsl";
	auto hResult = D3DCompileFromFile(vsPath.c_str(), nullptr, include, "VS", "vs_5_0", flag, 0, &m_pVSBlob, &pErrorBlob);
	if (FAILED(hResult)) assert(0 && "頂点シェーダーのコンパイルに失敗しました");

	std::wstring psPath = baseFullPath + L"_PS.hlsl";
	hResult = D3DCompileFromFile(psPath.c_str(), nullptr, include, "PS", "ps_5_0", flag, 0, &m_pPSBlob, &pErrorBlob);
	if (FAILED(hResult)) assert(0 && "ピクセルシェーダーのコンパイルに失敗しました");
}