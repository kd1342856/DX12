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

void PostProcessShader::Draw(RenderTarget* pSceneRT, RenderTarget* pBloomRT, const CBufferData::PostProcess& postProcessData,
	RenderTarget* pDofRT, RenderTarget* pGodRaysRT)
{
	RenderContext context;
	Begin(context);

	int b0 = GetRootParameterIndex(ShaderBindingType::CBV, 0);
	if (b0 != -1) GDF::Instance().BindCBuffer(b0, postProcessData);

	auto srvHeap = GraphicsDevice::Instance().GetDescriptorHeapManager()->GetCBVSRVUAVAllocator();

	// t0: シーンHDR
	int t0 = GetRootParameterIndex(ShaderBindingType::SRV, 0);
	if (t0 != -1 && pSceneRT) m_pDevice->GetCmdList()->SetGraphicsRootDescriptorTable(t0, srvHeap->GetGPUHandle(pSceneRT->GetSRVIndex()));

	// t1: Bloom合成結果
	int t1 = GetRootParameterIndex(ShaderBindingType::SRV, 1);
	if (t1 != -1 && pBloomRT) m_pDevice->GetCmdList()->SetGraphicsRootDescriptorTable(t1, srvHeap->GetGPUHandle(pBloomRT->GetSRVIndex()));

	// t2: DOF用にぼかし済みのシーンコピー (DOF無効時は何もバインドしない - シェーダー側もg_EnableDOFで分岐)
	int t2 = GetRootParameterIndex(ShaderBindingType::SRV, 2);
	if (t2 != -1 && pDofRT) m_pDevice->GetCmdList()->SetGraphicsRootDescriptorTable(t2, srvHeap->GetGPUHandle(pDofRT->GetSRVIndex()));

	// t3: シーン深度 (DOFのCoC計算用) - pSceneRT自身の専用深度バッファを読む
	int t3 = GetRootParameterIndex(ShaderBindingType::SRV, 3);
	if (t3 != -1 && pSceneRT && pSceneRT->GetDepthSRVIndex() != -1) m_pDevice->GetCmdList()->SetGraphicsRootDescriptorTable(t3, srvHeap->GetGPUHandle(pSceneRT->GetDepthSRVIndex()));

	// t4: God Rays結果
	int t4 = GetRootParameterIndex(ShaderBindingType::SRV, 4);
	if (t4 != -1 && pGodRaysRT) m_pDevice->GetCmdList()->SetGraphicsRootDescriptorTable(t4, srvHeap->GetGPUHandle(pGodRaysRT->GetSRVIndex()));

	Profiler::Instance().AddDrawCall("PostProcess", 1);
	m_pDevice->GetCmdList()->DrawInstanced(3, 1, 0, 0);
}


