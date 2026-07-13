#include "../../../Pch.h"
#include "StandardShader.h"
#include "../ShaderCompiler/ShaderCompiler.h"
#include "../../../Framework/DirectX/GDF/GDF.h"

void StandardShader::Create(GraphicsDevice* pGraphicsDevice)
{
	m_pDevice = pGraphicsDevice;

	auto vsBlob = ShaderCompiler::CompileVS(L"Asset/Shader/Study_VS.hlsl", "main");
	auto psBlob = ShaderCompiler::CompilePS(L"Asset/Shader/Study_PS.hlsl", "main");

	std::vector<DescriptorRange> ranges = {
		{ RangeType::CBV, 0, 1, 0 },
		{ RangeType::CBV, 1, 1, 0 },
		{ RangeType::SRV, 0, 1, 0 },
		{ RangeType::SRV, 1, 1, 0 },
		{ RangeType::SRV, 2, 1, 0 },
		{ RangeType::SRV, 3, 1, 0 }
	};
	m_cbvCount = 2;

	m_rootSignature = std::make_unique<RootSignature>();
	m_rootSignature->Create(pGraphicsDevice, ranges);

	PipelineDesc desc;
	desc.pBlobs = { vsBlob.Get(), nullptr, nullptr, nullptr, psBlob.Get() };
	desc.InputLayouts = { InputLayout::POSITION, InputLayout::TEXCOORD, InputLayout::COLOR, InputLayout::NORMAL, InputLayout::TANGENT };
	desc.Formats = { DXGI_FORMAT_R8G8B8A8_UNORM };
	desc.pRootSignature = m_rootSignature.get();
	desc.TopologyType = PrimitiveTopologyType::Triangle;

	m_pipeline = std::make_unique<Pipeline>();
	m_pipeline->Create(pGraphicsDevice, desc);
}

void StandardShader::Begin(RenderContext& context)
{
	GraphicsShader::Begin(context);
	context.BindCamera(0);
}

void StandardShader::BeginNode(const ModelData::Node& node, const Math::Matrix& nodeWorld)
{
	CBufferData::PerDraw cbDraw;
	cbDraw.mWorld = nodeWorld;
	GDF::Instance().BindCBuffer(1, cbDraw);
}

void StandardShader::BeforeDrawMesh(const Mesh& mesh, const Material& material)
{
	SetMaterial(material);
}

void StandardShader::SetMaterial(const Material& material)
{
	CBufferData::Material cbMat = {};
	cbMat.BaseColor = material.BaseColor;
	cbMat.EmissiveColor = Math::Vector4(material.Emissive.x, material.Emissive.y, material.Emissive.z, 1.0f);
	cbMat.Metallic = material.Metallic;
	cbMat.Smoothness = material.Roughness; // Mapping roughness to smoothness
	GDF::Instance().BindCBuffer(2, cbMat);

	if (material.spBaseColorTex) material.spBaseColorTex->Set(m_cbvCount);
	else GraphicsDevice::Instance().GetWhiteTex()->Set(m_cbvCount);
}



