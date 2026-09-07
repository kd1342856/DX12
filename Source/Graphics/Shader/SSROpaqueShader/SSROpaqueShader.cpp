#include "../../../Pch.h"
#include "../../../Framework/DirectX/Utility/Profiler.h"
#include "SSROpaqueShader.h"
#include "../../GPUResource/RenderTarget/RenderTarget.h"
#include "../../Renderer/Renderer.h"
#include "../../GDF/GDF.h"

void SSROpaqueShader::Create(GraphicsDevice* pGraphicsDevice)
{
	m_pDevice = pGraphicsDevice;

	// VSはSSAOShaderと全く同じ(カメラcbufferのみ使う、cbPostProcessと競合しないフルスクリーン
	// 三角形用VS)なので使い回す。
	m_pProgram = ShaderManager::Instance().LoadShader(L"Asset/Shader/SSAOShader/SSAO_VS.hlsl", L"Asset/Shader/SSROpaqueShader/SSROpaque_PS.hlsl");

	PipelineDesc desc;
	desc.InputLayouts = {};
	desc.Formats = { DXGI_FORMAT_R16G16B16A16_FLOAT }; // シーンHDRと同じフォーマットへ加算合成する
	desc.BlendMode = BlendMode::Add;
	desc.IsDepth = false;

	m_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	m_pPipelineState = ShaderManager::Instance().GetPipelineState(m_pProgram, desc);
}

void SSROpaqueShader::Draw(RenderTarget* pNormalRT, RenderTarget* pSceneOpaqueCopy, RenderTarget* pDestRT, const Params& params)
{
	if (!m_pProgram || !pNormalRT || !pSceneOpaqueCopy || !pDestRT) return;

	m_pDevice->SetRenderTarget(pDestRT);
	Renderer::BindViewport(pDestRT);

	m_pDevice->GetCmdList()->SetPipelineState(m_pPipelineState);
	m_pDevice->GetCmdList()->SetGraphicsRootSignature(m_pProgram->pRootSignature->GetRootSignature());
	m_pDevice->GetCmdList()->IASetPrimitiveTopology(m_topology);

	int b0 = GetRootParameterIndex(ShaderBindingType::CBV, 0);
	if (b0 != -1) Renderer::GetContext().BindCamera(b0);

	int b2 = GetRootParameterIndex(ShaderBindingType::CBV, 2);
	if (b2 != -1)
	{
		CBufferData::SSROpaque cb;
		cb.StepSize = params.StepSize;
		cb.Intensity = params.Intensity;
		GDF::Instance().BindCBuffer(b2, cb);
	}

	auto srvHeap = GraphicsDevice::Instance().GetDescriptorHeapManager()->GetCBVSRVUAVAllocator();

	int t0 = GetRootParameterIndex(ShaderBindingType::SRV, 0);
	if (t0 != -1) m_pDevice->GetCmdList()->SetGraphicsRootDescriptorTable(t0, srvHeap->GetGPUHandle(pNormalRT->GetSRVIndex()));

	int t1 = GetRootParameterIndex(ShaderBindingType::SRV, 1);
	if (t1 != -1 && pNormalRT->GetDepthSRVIndex() != -1) m_pDevice->GetCmdList()->SetGraphicsRootDescriptorTable(t1, srvHeap->GetGPUHandle(pNormalRT->GetDepthSRVIndex()));

	int t2 = GetRootParameterIndex(ShaderBindingType::SRV, 2);
	if (t2 != -1) m_pDevice->GetCmdList()->SetGraphicsRootDescriptorTable(t2, srvHeap->GetGPUHandle(pSceneOpaqueCopy->GetSRVIndex()));

	Profiler::Instance().AddDrawCall("SSROpaque", 1);
	m_pDevice->GetCmdList()->DrawInstanced(3, 1, 0, 0);
}
