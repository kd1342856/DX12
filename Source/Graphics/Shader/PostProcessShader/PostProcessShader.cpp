#include "../../../Pch.h"
#include "../../../Framework/DirectX/Utility/Profiler.h"
#include "PostProcessShader.h"
#include "../../Buffer/RenderTarget/RenderTarget.h"
#include "../ShaderCompiler/ShaderCompiler.h"

void PostProcessShader::Create(GraphicsDevice* pGraphicsDevice)
{
	m_pDevice = pGraphicsDevice;

	auto vsBlob = ShaderCompiler::CompileVS(L"Asset/Shader/PostProcessShader/PostProcessShader_VS.hlsl", "VS");
	auto psBlob = ShaderCompiler::CompilePS(L"Asset/Shader/PostProcessShader/PostProcessShader_PS.hlsl", "PS");

	std::vector<DescriptorRange> ranges = {
		{ RangeType::CBV, 0, 1, 0 },
		{ RangeType::SRV, 0, 1, 0 }
	};

	m_rootSignature = std::make_unique<RootSignature>();
	m_rootSignature->Create(pGraphicsDevice, ranges);

	PipelineDesc desc;
	desc.pBlobs = { vsBlob.Get(), nullptr, nullptr, nullptr, psBlob.Get() };
	desc.InputLayouts = {}; // Empty for full screen quad usually
	desc.Formats = { DXGI_FORMAT_R8G8B8A8_UNORM };
	desc.IsDepth = false;
	desc.IsDepthMask = false;
	desc.CullMode = CullMode::None;
	desc.BlendMode = BlendMode::None;
	desc.pRootSignature = m_rootSignature.get();
	desc.TopologyType = PrimitiveTopologyType::Triangle;

	m_pipeline = std::make_unique<Pipeline>();
	m_pipeline->Create(pGraphicsDevice, desc);
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

	auto srvHeap = GraphicsDevice::Instance().GetCBVSRVUAVHeap();
	m_pDevice->GetCmdList()->SetGraphicsRootDescriptorTable(1, srvHeap->GetGPUHandle(pRenderTarget->GetSRVIndex()));

	Profiler::Instance().AddDrawCall("PostProcess", 1);
	m_pDevice->GetCmdList()->DrawInstanced(3, 1, 0, 0);
}


