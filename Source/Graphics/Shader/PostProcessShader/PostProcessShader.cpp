#include "../../../Pch.h"
#include "../../../Framework/DirectX/Utility/Profiler.h"
#include "PostProcessShader.h"
#include "../../GPUResource/RenderTarget/RenderTarget.h"


void PostProcessShader::Create(GraphicsDevice* pGraphicsDevice)
{
	m_pDevice = pGraphicsDevice;

	m_pProgram = ShaderManager::Instance().LoadShader(L"Asset/Shader/PostProcessShader/PostProcessShader_VS.hlsl", L"Asset/Shader/PostProcessShader/PostProcessShader_PS.hlsl");

	PipelineDesc desc;
	desc.InputLayouts = {};
	desc.Formats = { DXGI_FORMAT_R8G8B8A8_UNORM };
	
	m_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	m_pPipelineState = ShaderManager::Instance().GetPipelineState(m_pProgram, desc);
}

void PostProcessShader::Draw(RenderTarget* pRenderTarget, float exposure)
{
	RenderContext context; // Dummy context if we don't have one globally available here yet.
	Begin(context);

	CBufferData::PostProcess cPostProcess;
	cPostProcess.Exposure = (exposure <= 0.0f) ? 1.0f : exposure;
	GDF::Instance().BindCBuffer(0, cPostProcess);

	// Viewport is bound externally, but PostProcess traditionally forced it.
	// Now we expect Renderer to have bound viewport for the target RT.

	auto srvHeap = GraphicsDevice::Instance().GetDescriptorHeapManager()->GetCBVSRVUAVAllocator();
	m_pDevice->GetCmdList()->SetGraphicsRootDescriptorTable(1, srvHeap->GetGPUHandle(pRenderTarget->GetSRVIndex()));

	Profiler::Instance().AddDrawCall("PostProcess", 1);
	m_pDevice->GetCmdList()->DrawInstanced(3, 1, 0, 0);
}


