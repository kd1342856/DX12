#include "../../../Pch.h"
#include "ShadowShader.h"
#include "../ShaderCompiler/ShaderCompiler.h"
#include "../../../Framework/DirectX/GDF/GDF.h"

void ShadowShader::Create(GraphicsDevice* pGraphicsDevice)
{
	m_pDevice = pGraphicsDevice;

	auto vsBlob = ShaderCompiler::CompileVS(L"Asset/Shader/LitShader/LitShader_VS.hlsl", "ShadowCasterVS");
	auto psBlob = ShaderCompiler::CompilePS(L"Asset/Shader/LitShader/LitShader_PS.hlsl", "ShadowCasterPS");

	std::vector<DescriptorRange> ranges = {
		{ RangeType::CBV, 0, 1, 0 }, // cbCamera
		{ RangeType::CBV, 1, 1, 0 }  // cbWorld
	};

	m_rootSignature = std::make_unique<RootSignature>();
	m_rootSignature->Create(pGraphicsDevice, ranges);

	PipelineDesc desc;
	desc.pBlobs = { vsBlob.Get(), nullptr, nullptr, nullptr, nullptr };
	desc.InputLayouts = { InputLayout::POSITION, InputLayout::TEXCOORD, InputLayout::NORMAL, InputLayout::COLOR, InputLayout::TANGENT };
	desc.Formats = {}; // Depth only
	desc.CullMode = CullMode::None;
	desc.pRootSignature = m_rootSignature.get();
	desc.TopologyType = PrimitiveTopologyType::Triangle;

	m_pipeline = std::make_unique<Pipeline>();
	m_pipeline->Create(pGraphicsDevice, desc);
}

void ShadowShader::Begin(RenderContext& context)
{
	GraphicsShader::Begin(context);
	context.BindCamera(0);
}

void ShadowShader::BeginNode(const ModelData::Node& node, const Math::Matrix& nodeWorld)
{
	CBufferData::PerDraw cbDraw;
	cbDraw.mWorld = nodeWorld;
	GDF::Instance().BindCBuffer(1, cbDraw);
}



