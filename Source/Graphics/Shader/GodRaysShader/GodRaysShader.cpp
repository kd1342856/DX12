#include "../../../Pch.h"
#include "../../../Framework/DirectX/Utility/Profiler.h"
#include "GodRaysShader.h"
#include "../../GPUResource/RenderTarget/RenderTarget.h"
#include "../../Renderer/Renderer.h"

void GodRaysShader::Create(GraphicsDevice* pGraphicsDevice)
{
	m_pDevice = pGraphicsDevice;

	m_pProgram = ShaderManager::Instance().LoadShader(L"Asset/Shader/PostProcessShader/PostProcessShader_VS.hlsl", L"Asset/Shader/GodRaysShader/GodRays_PS.hlsl");

	PipelineDesc desc;
	desc.InputLayouts = {};
	desc.Formats = { DXGI_FORMAT_R16G16B16A16_FLOAT };

	m_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	m_pPipelineState = ShaderManager::Instance().GetPipelineState(m_pProgram, desc);
}

void GodRaysShader::Draw(RenderTarget* pSrcRT, RenderTarget* pDestRT, const CBufferData::PostProcess& postProcessData)
{
	if (!m_pProgram || !pSrcRT || !pDestRT) return;

	m_pDevice->SetRenderTarget(pDestRT);
	Renderer::BindViewport(pDestRT);

	m_pDevice->GetCmdList()->SetPipelineState(m_pPipelineState);
	m_pDevice->GetCmdList()->SetGraphicsRootSignature(m_pProgram->pRootSignature->GetRootSignature());
	m_pDevice->GetCmdList()->IASetPrimitiveTopology(m_topology);

	int b0 = GetRootParameterIndex(ShaderBindingType::CBV, 0);
	if (b0 != -1) GDF::Instance().BindCBuffer(b0, postProcessData);

	auto srvHeap = GraphicsDevice::Instance().GetDescriptorHeapManager()->GetCBVSRVUAVAllocator();

	int t0 = GetRootParameterIndex(ShaderBindingType::SRV, 0);
	if (t0 != -1) m_pDevice->GetCmdList()->SetGraphicsRootDescriptorTable(t0, srvHeap->GetGPUHandle(pSrcRT->GetSRVIndex()));

	Profiler::Instance().AddDrawCall("GodRays", 1);
	m_pDevice->GetCmdList()->DrawInstanced(3, 1, 0, 0);
}
