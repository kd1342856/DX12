#include "../../../Pch.h"
#include "../../../Framework/DirectX/Utility/Profiler.h"
#include "SSAOShader.h"
#include "../../GPUResource/RenderTarget/RenderTarget.h"
#include "../../Renderer/Renderer.h"
#include "../../GDF/GDF.h"

void SSAOShader::Create(GraphicsDevice* pGraphicsDevice)
{
	m_pDevice = pGraphicsDevice;

	m_pProgram = ShaderManager::Instance().LoadShader(L"Asset/Shader/SSAOShader/SSAO_VS.hlsl", L"Asset/Shader/SSAOShader/SSAO_PS.hlsl");

	PipelineDesc desc;
	desc.InputLayouts = {};
	desc.Formats = { DXGI_FORMAT_R16G16B16A16_FLOAT };
	desc.IsDepth = false;

	m_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	m_pPipelineState = ShaderManager::Instance().GetPipelineState(m_pProgram, desc);
}

void SSAOShader::Draw(RenderTarget* pNormalRT, RenderTarget* pDestRT, const Params& params)
{
	if (!m_pProgram || !pNormalRT || !pDestRT) return;

	m_pDevice->SetRenderTarget(pDestRT);
	Renderer::BindViewport(pDestRT);

	m_pDevice->GetCmdList()->SetPipelineState(m_pPipelineState);
	m_pDevice->GetCmdList()->SetGraphicsRootSignature(m_pProgram->pRootSignature->GetRootSignature());
	m_pDevice->GetCmdList()->IASetPrimitiveTopology(m_topology);

	// b0: カメラ行列(g_mP/g_mInvP)。今描画しているシーンのカメラをそのまま使う。
	int b0 = GetRootParameterIndex(ShaderBindingType::CBV, 0);
	if (b0 != -1) Renderer::GetContext().BindCamera(b0);

	int b2 = GetRootParameterIndex(ShaderBindingType::CBV, 2);
	if (b2 != -1)
	{
		CBufferData::SSAO cbSSAO;
		cbSSAO.Radius = params.Radius;
		cbSSAO.Bias = params.Bias;
		cbSSAO.Power = params.Power;
		cbSSAO.Intensity = params.Intensity;
		GDF::Instance().BindCBuffer(b2, cbSSAO);
	}

	auto srvHeap = GraphicsDevice::Instance().GetDescriptorHeapManager()->GetCBVSRVUAVAllocator();

	int t0 = GetRootParameterIndex(ShaderBindingType::SRV, 0);
	if (t0 != -1) m_pDevice->GetCmdList()->SetGraphicsRootDescriptorTable(t0, srvHeap->GetGPUHandle(pNormalRT->GetSRVIndex()));

	int t1 = GetRootParameterIndex(ShaderBindingType::SRV, 1);
	if (t1 != -1 && pNormalRT->GetDepthSRVIndex() != -1) m_pDevice->GetCmdList()->SetGraphicsRootDescriptorTable(t1, srvHeap->GetGPUHandle(pNormalRT->GetDepthSRVIndex()));

	Profiler::Instance().AddDrawCall("SSAO", 1);
	m_pDevice->GetCmdList()->DrawInstanced(3, 1, 0, 0);
}
