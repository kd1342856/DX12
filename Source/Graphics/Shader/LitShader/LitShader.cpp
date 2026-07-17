#include "../../../Pch.h"
#include "LitShader.h"

#include "../../GDF/GDF.h"
#include "../../Renderer/RenderContext.h"

void LitShader::Create(GraphicsDevice* pGraphicsDevice)
{
	m_pDevice = pGraphicsDevice;

	m_pProgram = ShaderManager::Instance().LoadShader(L"Asset/Shader/LitShader/LitShader_VS.hlsl", L"Asset/Shader/LitShader/LitShader_PS.hlsl");

	PipelineDesc desc;
	desc.InputLayouts = { InputLayout::POSITION, InputLayout::TEXCOORD, InputLayout::NORMAL, InputLayout::COLOR, InputLayout::TANGENT };
	desc.Formats = { DXGI_FORMAT_R8G8B8A8_UNORM };
	
	m_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	m_pPipelineState = ShaderManager::Instance().GetPipelineState(m_pProgram, desc);
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
		auto handle = m_pDevice->GetDescriptorHeapManager()->GetCBVSRVUAVAllocator()->GetGPUHandle(pShadowMap->GetSRVNumber());
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

	if (material.spBaseColorTex) material.spBaseColorTex->Set(3);
	else GraphicsDevice::Instance().GetWhiteTex()->Set(3);

	if (material.spNormalTex) material.spNormalTex->Set(3 + 1);
	else GraphicsDevice::Instance().GetNormalTex()->Set(3 + 1);

	if (material.spMetallicRoughnessTex) material.spMetallicRoughnessTex->Set(3 + 2);
	else GraphicsDevice::Instance().GetWhiteTex()->Set(3 + 2);

	if (material.spEmissiveTex) material.spEmissiveTex->Set(3 + 3);
	else GraphicsDevice::Instance().GetBlackTex()->Set(3 + 3);

	// TODO: ShadowMap binding should ideally be done by RenderContext once per frame, not per material.
	// We will leave slot 3+4 (SRV 4) for ShadowMap to be bound by Renderer.
}



