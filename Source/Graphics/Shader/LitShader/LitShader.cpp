#include "../../../Pch.h"
#include "LitShader.h"
#include "../ShaderCompiler/ShaderCompiler.h"
#include "../../../Framework/DirectX/GDF/GDF.h"
#include "../../Renderer/RenderContext.h"

void LitShader::Create(GraphicsDevice* pGraphicsDevice)
{
	m_pDevice = pGraphicsDevice;

	auto vsBlob = ShaderCompiler::CompileVS(L"Asset/Shader/LitShader/LitShader_VS.hlsl", "VS");
	auto psBlob = ShaderCompiler::CompilePS(L"Asset/Shader/LitShader/LitShader_PS.hlsl", "PS");

	std::vector<DescriptorRange> ranges = {
		{ RangeType::CBV, 0, 1, 0 }, // b0
		{ RangeType::CBV, 1, 1, 0 }, // b1
		{ RangeType::CBV, 2, 1, 0 }, // b2
		{ RangeType::CBV, 3, 1, 0 }, // b3
		{ RangeType::CBV, 4, 1, 0 }, // b4
		{ RangeType::SRV, 0, 1, 0 }, // t0
		{ RangeType::SRV, 1, 1, 0 }, // t1
		{ RangeType::SRV, 2, 1, 0 }, // t2
		{ RangeType::SRV, 3, 1, 0 }, // t3
		{ RangeType::SRV, 4, 1, 0 }, // t4
		{ RangeType::SRV, 5, 1, 0 }, // t5
		{ RangeType::SRV, 6, 1, 0 }, // t6
		{ RangeType::SRV, 7, 1, 0 }, // t7
		{ RangeType::SRV, 8, 1, 0 }  // t8
	};
	m_cbvCount = 5;

	m_rootSignature = std::make_unique<RootSignature>();
	m_rootSignature->Create(pGraphicsDevice, ranges);

	PipelineDesc desc;
	desc.pBlobs = { vsBlob.Get(), nullptr, nullptr, nullptr, psBlob.Get() };
	desc.InputLayouts = { InputLayout::POSITION, InputLayout::TEXCOORD, InputLayout::NORMAL, InputLayout::COLOR, InputLayout::TANGENT };
	desc.Formats = { DXGI_FORMAT_R8G8B8A8_UNORM };
	desc.pRootSignature = m_rootSignature.get();
	desc.TopologyType = PrimitiveTopologyType::Triangle;

	m_pipeline = std::make_unique<Pipeline>();
	m_pipeline->Create(pGraphicsDevice, desc);
}

void LitShader::Begin(RenderContext& context)
{
	GraphicsShader::Begin(context);
	context.BindCamera(0);
	context.BindLight(3);

	// Bind ShadowMap to t7 (index 12)
	auto* pShadowMap = m_pDevice->GetShadowMap();
	if (pShadowMap && pShadowMap->GetSRVNumber() != -1)
	{
		auto handle = m_pDevice->GetCBVSRVUAVHeap()->GetGPUHandle(pShadowMap->GetSRVNumber());
		m_pDevice->GetCmdList()->SetGraphicsRootDescriptorTable(12, handle);
	}
}

void LitShader::BeginNode(const ModelData::Node& node, const Math::Matrix& nodeWorld)
{
	CBufferData::PerDraw cbDraw;
	cbDraw.mWorld = nodeWorld;
	GDF::Instance().BindCBuffer(1, cbDraw);
}

void LitShader::BeforeDrawMesh(const Mesh& mesh, const Material& material)
{
	SetMaterial(material);
}

void LitShader::SetMaterial(const Material& material)
{
	CBufferData::Material cbMat = {};
	cbMat.BaseColor = material.BaseColor;
	cbMat.EmissiveColor = Math::Vector4(material.Emissive.x, material.Emissive.y, material.Emissive.z, 1.0f);
	cbMat.Metallic = material.Metallic;
	cbMat.Smoothness = material.Roughness; // Mapping roughness to smoothness
	GDF::Instance().BindCBuffer(2, cbMat);

	if (material.spBaseColorTex) material.spBaseColorTex->Set(m_cbvCount);
	else GraphicsDevice::Instance().GetWhiteTex()->Set(m_cbvCount);

	if (material.spNormalTex) material.spNormalTex->Set(m_cbvCount + 1);
	else GraphicsDevice::Instance().GetNormalTex()->Set(m_cbvCount + 1);

	if (material.spMetallicRoughnessTex) material.spMetallicRoughnessTex->Set(m_cbvCount + 2);
	else GraphicsDevice::Instance().GetWhiteTex()->Set(m_cbvCount + 2);

	if (material.spEmissiveTex) material.spEmissiveTex->Set(m_cbvCount + 3);
	else GraphicsDevice::Instance().GetBlackTex()->Set(m_cbvCount + 3);

	// TODO: ShadowMap binding should ideally be done by RenderContext once per frame, not per material.
	// We will leave slot m_cbvCount+4 (SRV 4) for ShadowMap to be bound by Renderer.
}



