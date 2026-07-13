#include "../../../Pch.h"
#include "SkinningShader.h"
#include "../ShaderCompiler/ShaderCompiler.h"
#include "../../Renderer/ModelRenderer.h"
#include "../../../Framework/DirectX/GDF/GDF.h"

void SkinningShader::Create(GraphicsDevice* pGraphicsDevice)
{
	m_pDevice = pGraphicsDevice;

	auto vsBlob = ShaderCompiler::CompileVS(L"Asset/Shader/SkinningShader/SkinningShader_VS.hlsl", "main");
	auto psBlob = ShaderCompiler::CompilePS(L"Asset/Shader/SkinningShader/SkinningShader_PS.hlsl", "main");

	std::vector<DescriptorRange> ranges = {
		{ RangeType::CBV, 0, 1, 0 },
		{ RangeType::CBV, 1, 1, 0 },
		{ RangeType::CBV, 2, 1, 0 },
		{ RangeType::SRV, 0, 1, 0 },
		{ RangeType::SRV, 1, 1, 0 },
		{ RangeType::SRV, 2, 1, 0 },
		{ RangeType::SRV, 3, 1, 0 }
	};
	m_cbvCount = 3; 

	m_rootSignature = std::make_unique<RootSignature>();
	m_rootSignature->Create(pGraphicsDevice, ranges);

	PipelineDesc desc;
	desc.pBlobs = { vsBlob.Get(), nullptr, nullptr, nullptr, psBlob.Get() };
	desc.InputLayouts = {
		InputLayout::POSITION, InputLayout::TEXCOORD, InputLayout::NORMAL,
		InputLayout::COLOR, InputLayout::TANGENT,
		InputLayout::SKININDEX, InputLayout::SKINWEIGHT
	};
	desc.Formats = { DXGI_FORMAT_R8G8B8A8_UNORM };
	desc.pRootSignature = m_rootSignature.get();
	desc.TopologyType = PrimitiveTopologyType::Triangle;

	m_pipeline = std::make_unique<Pipeline>();
	m_pipeline->Create(pGraphicsDevice, desc);

	PipelineDesc shadowDesc = desc;
	shadowDesc.pBlobs = { vsBlob.Get(), nullptr, nullptr, nullptr, nullptr };
	shadowDesc.Formats = {};
	shadowDesc.CullMode = CullMode::None;
	
	m_upShadowPipeline = std::make_unique<Pipeline>();
	m_upShadowPipeline->Create(pGraphicsDevice, shadowDesc);
}

void SkinningShader::Begin(RenderContext& context)
{
	GraphicsShader::Begin(context);
	context.BindCamera(0);
}

void SkinningShader::BeginModel(const ModelData& model, const DrawContext& drawContext)
{
	m_pCurrentDrawContext = &drawContext;

	if (drawContext.BoneMatrices) {
		CBufferData::Bones cbBones;
		for (size_t i = 0; i < drawContext.BoneMatrices->size() && i < 256; ++i) {
			cbBones.mBones[i] = (*drawContext.BoneMatrices)[i];
		}
		GDF::Instance().BindCBuffer(2, cbBones);
	}
}

void SkinningShader::BeginNode(const ModelData::Node& node, const Math::Matrix& nodeWorld)
{
	CBufferData::PerDraw cbDraw; 
	cbDraw.mWorld = nodeWorld; 
	GDF::Instance().BindCBuffer(1, cbDraw);
}

void SkinningShader::BeforeDrawMesh(const Mesh& mesh, const Material& material)
{
	SetMaterial(material);
}

void SkinningShader::SetMaterial(const Material& material)
{
	if (material.spBaseColorTex) material.spBaseColorTex->Set(m_cbvCount);
	else GraphicsDevice::Instance().GetWhiteTex()->Set(m_cbvCount);

	if (material.spNormalTex) material.spNormalTex->Set(m_cbvCount + 1);
	else GraphicsDevice::Instance().GetNormalTex()->Set(m_cbvCount + 1);

	if (material.spMetallicRoughnessTex) material.spMetallicRoughnessTex->Set(m_cbvCount + 2);
	else GraphicsDevice::Instance().GetWhiteTex()->Set(m_cbvCount + 2);

	if (material.spEmissiveTex) material.spEmissiveTex->Set(m_cbvCount + 3);
	else GraphicsDevice::Instance().GetBlackTex()->Set(m_cbvCount + 3);
}

void SkinningShader::BeginShadow(RenderContext& context)
{
	m_pDevice->GetCmdList()->SetPipelineState(m_upShadowPipeline->GetPipeline());
	m_pDevice->GetCmdList()->SetGraphicsRootSignature(m_rootSignature->GetRootSignature());
	m_pDevice->GetCmdList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	D3D12_VIEWPORT viewport = {};
	D3D12_RECT rect = {};
	viewport.Width = 4096.0f; // ShadowMapの解像度
	viewport.Height = 4096.0f;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	rect.right = 4096;
	rect.bottom = 4096;
	m_pDevice->GetCmdList()->RSSetViewports(1, &viewport);
	m_pDevice->GetCmdList()->RSSetScissorRects(1, &rect);

	context.BindCamera(0);
}

void SkinningShader::DrawShadowModel(const ModelData& modelData, const DrawContext& context)
{
	// Shadow drawing using ModelRenderer but with shadow pipeline
	// This is a temporary solution until shadow passes are unified
	ModelRenderer::Draw(*this, modelData, context);
}
