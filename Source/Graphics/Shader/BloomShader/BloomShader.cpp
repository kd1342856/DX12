#include "../../../Pch.h"
#include "../../../Framework/DirectX/Utility/Profiler.h"
#include "BloomShader.h"
#include "../../GPUResource/RenderTarget/RenderTarget.h"
#include "../../Renderer/Renderer.h"

void BloomShader::Create(GraphicsDevice* pGraphicsDevice)
{
	m_pDevice = pGraphicsDevice;

	m_pExtractProgram = ShaderManager::Instance().LoadShader(L"Asset/Shader/PostProcessShader/PostProcessShader_VS.hlsl", L"Asset/Shader/BloomShader/BloomExtract_PS.hlsl");
	m_pBlurProgram = ShaderManager::Instance().LoadShader(L"Asset/Shader/PostProcessShader/PostProcessShader_VS.hlsl", L"Asset/Shader/BloomShader/BloomBlur_PS.hlsl");

	PipelineDesc desc;
	desc.InputLayouts = {};
	desc.Formats = { DXGI_FORMAT_R16G16B16A16_FLOAT }; // Bloom targets are HDR
	
	m_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	
	// Set m_pProgram so GetRootParameterIndex works
	m_pProgram = m_pExtractProgram;
	
	m_pExtractPipelineState = ShaderManager::Instance().GetPipelineState(m_pExtractProgram, desc);
	m_pBlurPipelineState = ShaderManager::Instance().GetPipelineState(m_pBlurProgram, desc);
}

void BloomShader::DrawExtract(RenderTarget* pSrcRT, RenderTarget* pDestRT, const CBufferData::PostProcess& postProcessData)
{
	if (!m_pExtractProgram || !pSrcRT || !pDestRT) return;

	m_pDevice->SetRenderTarget(pDestRT);
	Renderer::BindViewport(pDestRT);

	m_pDevice->GetCmdList()->SetPipelineState(m_pExtractPipelineState);
	m_pDevice->GetCmdList()->SetGraphicsRootSignature(m_pExtractProgram->pRootSignature->GetRootSignature());
	m_pDevice->GetCmdList()->IASetPrimitiveTopology(m_topology);

	int b0 = GetRootParameterIndex(ShaderBindingType::CBV, 0);
	if (b0 != -1) GDF::Instance().BindCBuffer(b0, postProcessData);

	auto srvHeap = GraphicsDevice::Instance().GetDescriptorHeapManager()->GetCBVSRVUAVAllocator();
	
	int t0 = GetRootParameterIndex(ShaderBindingType::SRV, 0);
	if (t0 != -1) m_pDevice->GetCmdList()->SetGraphicsRootDescriptorTable(t0, srvHeap->GetGPUHandle(pSrcRT->GetSRVIndex()));

	Profiler::Instance().AddDrawCall("BloomExtract", 1);
	m_pDevice->GetCmdList()->DrawInstanced(3, 1, 0, 0);
}

void BloomShader::DrawBlur(RenderTarget* pSrcRT, RenderTarget* pDestRT, const CBufferData::PostProcess& postProcessData, float dirX, float dirY)
{
	if (!m_pBlurProgram || !pSrcRT || !pDestRT) return;

	m_pDevice->SetRenderTarget(pDestRT);
	Renderer::BindViewport(pDestRT);

	m_pDevice->GetCmdList()->SetPipelineState(m_pBlurPipelineState);
	m_pDevice->GetCmdList()->SetGraphicsRootSignature(m_pBlurProgram->pRootSignature->GetRootSignature());
	m_pDevice->GetCmdList()->IASetPrimitiveTopology(m_topology);

	CBufferData::PostProcess cPostProcess = postProcessData;
	cPostProcess.BlurDirectionX = dirX;
	cPostProcess.BlurDirectionY = dirY;
	int b0 = GetRootParameterIndex(ShaderBindingType::CBV, 0);
	if (b0 != -1) GDF::Instance().BindCBuffer(b0, cPostProcess);

	auto srvHeap = GraphicsDevice::Instance().GetDescriptorHeapManager()->GetCBVSRVUAVAllocator();
	
	int t0 = GetRootParameterIndex(ShaderBindingType::SRV, 0);
	if (t0 != -1) m_pDevice->GetCmdList()->SetGraphicsRootDescriptorTable(t0, srvHeap->GetGPUHandle(pSrcRT->GetSRVIndex()));

	Profiler::Instance().AddDrawCall("BloomBlur", 1);
	m_pDevice->GetCmdList()->DrawInstanced(3, 1, 0, 0);
}
