#include "../../../Pch.h"
#include "NormalPrepassShader.h"
#include "../../GDF/GDF.h"

void NormalPrepassShader::Create(GraphicsDevice* pGraphicsDevice)
{
	m_pDevice = pGraphicsDevice;

	m_pProgram = ShaderManager::Instance().LoadShader(L"Asset/Shader/NormalPrepassShader/NormalPrepass_VS.hlsl", L"Asset/Shader/NormalPrepassShader/NormalPrepass_PS.hlsl");

	PipelineDesc desc;
	desc.InputLayouts = { InputLayout::POSITION, InputLayout::TEXCOORD, InputLayout::NORMAL, InputLayout::COLOR, InputLayout::TANGENT };
	desc.Formats = { DXGI_FORMAT_R8G8B8A8_UNORM };
	desc.CullMode = CullMode::Back;
	desc.BlendMode = BlendMode::None;
	desc.IsDepthMask = true;

	m_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	m_pPipelineState = ShaderManager::Instance().GetPipelineState(m_pProgram, desc);
}

void NormalPrepassShader::Begin(RenderContext& context)
{
	GraphicsShader::Begin(context);

	int b0 = GetRootParameterIndex(ShaderBindingType::CBV, 0);
	if (b0 != -1) context.BindCamera(b0);
}

void NormalPrepassShader::BeginNode(const ModelData::Node& node, const Math::Matrix& nodeWorld)
{
	CBufferData::PerDraw cbDraw;
	cbDraw.mWorld = nodeWorld;

	int b1 = GetRootParameterIndex(ShaderBindingType::CBV, 1);
	if (b1 != -1) GDF::Instance().BindCBuffer(b1, cbDraw);
}
