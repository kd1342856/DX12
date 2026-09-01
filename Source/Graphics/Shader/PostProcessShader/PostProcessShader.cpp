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

void PostProcessShader::Draw(RenderTarget* pSceneRT, RenderTarget* pBloomRT, const CBufferData::PostProcess& postProcessData)
{
	RenderContext context;
	Begin(context);

	int b0 = GetRootParameterIndex(ShaderBindingType::CBV, 0);
	if (b0 != -1) GDF::Instance().BindCBuffer(b0, postProcessData);

	auto srvHeap = GraphicsDevice::Instance().GetDescriptorHeapManager()->GetCBVSRVUAVAllocator();
	
	// Bind Scene HDR texture to t0

	// Bind Bloom texture to t1 (Assuming Root Parameter 2 is another SRV or it's a table? Wait, need to check Root Signature)
	// DX12 Root Signature usually sets a descriptor table that contains continuous descriptors, 
	// OR we set them in different root parameters. Let's see Pipeline.cpp or LitShader for how tables are set up.
	// In the common GraphicsShader root signature, t1 is parameter 2? Or t0 is param 1, t1 is param 2?
	
	int t0 = GetRootParameterIndex(ShaderBindingType::SRV, 0);
	if (t0 != -1 && pSceneRT) m_pDevice->GetCmdList()->SetGraphicsRootDescriptorTable(t0, srvHeap->GetGPUHandle(pSceneRT->GetSRVIndex()));

	int t1 = GetRootParameterIndex(ShaderBindingType::SRV, 1);
	if (t1 != -1 && pBloomRT) m_pDevice->GetCmdList()->SetGraphicsRootDescriptorTable(t1, srvHeap->GetGPUHandle(pBloomRT->GetSRVIndex()));

	Profiler::Instance().AddDrawCall("PostProcess", 1);
	m_pDevice->GetCmdList()->DrawInstanced(3, 1, 0, 0);
}


