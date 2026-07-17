#include "../../../Pch.h"
#include "ShadowShader.h"

#include "../../GDF/GDF.h"

void ShadowShader::Create(GraphicsDevice* pGraphicsDevice)
{
	m_pDevice = pGraphicsDevice;

	m_pProgram = ShaderManager::Instance().LoadShader(L"Asset/Shader/LitShader/LitShader_VS.hlsl", L"Asset/Shader/LitShader/LitShader_PS.hlsl");

	PipelineDesc desc;
	desc.InputLayouts = { InputLayout::POSITION, InputLayout::TEXCOORD, InputLayout::NORMAL, InputLayout::COLOR, InputLayout::TANGENT };
	desc.Formats = {}; // Depth only
	desc.CullMode = CullMode::None;
	
	m_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	m_pPipelineState = ShaderManager::Instance().GetPipelineState(m_pProgram, desc);
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



