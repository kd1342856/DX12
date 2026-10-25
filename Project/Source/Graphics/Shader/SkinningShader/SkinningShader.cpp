#include "../../../Pch.h"
#include "SkinningShader.h"

#include "../../Renderer/ModelRenderer.h"
#include "../../GDF/GDF.h"
#include "../../Renderer/RenderContext.h"
#include "../../../Framework/Manager/Asset/TextureManager.h"

void SkinningShader::Create(GraphicsDevice* pGraphicsDevice)
{
	m_pDevice = pGraphicsDevice;

	m_pProgram = ShaderManager::Instance().LoadShader(L"Asset/Shader/SkinningShader/SkinningShader_VS.hlsl", L"Asset/Shader/SkinningShader/SkinningShader_PS.hlsl");

	PipelineDesc desc;
	desc.InputLayouts = {
		InputLayout::POSITION, InputLayout::TEXCOORD, InputLayout::NORMAL,
		InputLayout::COLOR, InputLayout::TANGENT,
		InputLayout::SKININDEX, InputLayout::SKINWEIGHT
	};
	desc.Formats = { DXGI_FORMAT_R16G16B16A16_FLOAT };
	desc.CullMode = CullMode::None; // 反射の鏡像描画でカリングが反転しても描画されるようにNoneにする
	
	m_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	m_pPipelineState = ShaderManager::Instance().GetPipelineState(m_pProgram, desc);

	PipelineDesc shadowDesc = desc;
	// Shadow pipeline (depth only)
	shadowDesc.Formats = {};
	shadowDesc.CullMode = CullMode::None;
	shadowDesc.pBlobs = { m_pProgram->pVS.Get(), nullptr, nullptr, nullptr, nullptr };
	// RootSignature will be overriden inside GetPipelineState
	m_pShadowPipelineState = ShaderManager::Instance().GetPipelineState(m_pProgram, shadowDesc);
}

void SkinningShader::Begin(RenderContext& context)
{
	GraphicsShader::Begin(context);
	
	int b0 = GetRootParameterIndex(ShaderBindingType::CBV, 0);
	if (b0 != -1) context.BindCamera(b0);

	// ライティング(LitShaderと同じ光源と露出)
	int b3 = GetRootParameterIndex(ShaderBindingType::CBV, 3);
	if (b3 != -1)
	{
		context.BindLight(b3);
	}
	int b4 = GetRootParameterIndex(ShaderBindingType::CBV, 4);
	if (b4 != -1)
	{
		context.BindSystem(b4);
	}

	// 平行光のシャドウマップ(t7)
	int t7 = GetRootParameterIndex(ShaderBindingType::SRV, 7);
	if (t7 != -1)
	{
		auto* pShadowMap = m_pDevice->GetShadowMap();
		if (pShadowMap && pShadowMap->GetSRVNumber() != -1)
		{
			auto handle = m_pDevice->GetDescriptorHeapManager()->GetCBVSRVUAVAllocator()->GetGPUHandle(pShadowMap->GetSRVNumber());
			m_pDevice->GetCmdList()->SetGraphicsRootDescriptorTable(t7, handle);
		}
	}

	// 局所光源の影(t13・t14)
	BindLocalShadowMaps();
	// 環境BRDFのLUT(t21)、反射プローブ(t22)、DDGI(t23・t24)
	BindEnvironmentBRDF();
	BindReflectionProbes();
	BindDDGI();
	// コンタクトシャドウの画面の深度(t25)
	BindContactShadowDepth();
}

void SkinningShader::BeginModel(const ModelData& model, const DrawContext& drawContext)
{
	m_pCurrentDrawContext = &drawContext;

	if (drawContext.BoneMatrices) {
		CBufferData::Bones cbBones = {};
		for (size_t i = 0; i < drawContext.BoneMatrices->size() && i < 256; ++i) {
			cbBones.mBones[i] = (*drawContext.BoneMatrices)[i];
		}
		
		int b2 = GetRootParameterIndex(ShaderBindingType::CBV, 2);
		if (b2 != -1) GDF::Instance().BindCBuffer(b2, cbBones);
	}
}

void SkinningShader::BeginNode(const ModelData::Node& node, const Math::Matrix& nodeWorld)
{
	CBufferData::PerDraw cbDraw; 
	cbDraw.mWorld = nodeWorld; 
	
	int b1 = GetRootParameterIndex(ShaderBindingType::CBV, 1);
	if (b1 != -1) GDF::Instance().BindCBuffer(b1, cbDraw);
}

void SkinningShader::BeforeDrawMesh(const Mesh& mesh, const Material& material)
{
	SetMaterial(material);
}

void SkinningShader::SetMaterial(const Material& material)
{
	int t0 = GetRootParameterIndex(ShaderBindingType::SRV, 0);
	if (t0 != -1) {
		auto* pTex = TextureManager::Instance().Get(material.BaseColor.handle);
		if (pTex && material.BaseColor.valid && pTex->IsReady()) pTex->Set(t0);
		else GraphicsDevice::Instance().GetWhiteTex()->Set(t0);
	}

	int t1 = GetRootParameterIndex(ShaderBindingType::SRV, 1);
	if (t1 != -1) {
		auto* pTex = TextureManager::Instance().Get(material.Normal.handle);
		if (pTex && material.Normal.valid && pTex->IsReady()) pTex->Set(t1);
		else GraphicsDevice::Instance().GetNormalTex()->Set(t1);
	}

	int t2 = GetRootParameterIndex(ShaderBindingType::SRV, 2);
	if (t2 != -1) {
		auto* pTex = TextureManager::Instance().Get(material.MetallicRoughness.handle);
		if (pTex && material.MetallicRoughness.valid && pTex->IsReady()) pTex->Set(t2);
		else GraphicsDevice::Instance().GetWhiteTex()->Set(t2);
	}

	int t3 = GetRootParameterIndex(ShaderBindingType::SRV, 3);
	if (t3 != -1) {
		auto* pTex = TextureManager::Instance().Get(material.Emissive.handle);
		if (pTex && material.Emissive.valid && pTex->IsReady()) pTex->Set(t3);
		else GraphicsDevice::Instance().GetBlackTex()->Set(t3);
	}

	// SkinningShader.hlsl might not have t4 for occlusion or CBV 2 for MaterialConstants yet,
	// but I'll add them if they are in the root parameter index list for consistency.
	int t4 = GetRootParameterIndex(ShaderBindingType::SRV, 4);
	if (t4 != -1) {
		auto* pTex = TextureManager::Instance().Get(material.Occlusion.handle);
		if (pTex && material.Occlusion.valid && pTex->IsReady()) pTex->Set(t4);
		else GraphicsDevice::Instance().GetWhiteTex()->Set(t4);
	}

	// マテリアルはb5(b2は骨の行列、b3とb4はライトとシステム)
	int b5 = GetRootParameterIndex(ShaderBindingType::CBV, 5);
	if (b5 != -1)
	{
		GDF::Instance().BindCBuffer(b5, material.Constants);
	}
}

void SkinningShader::BeginShadow(RenderContext& context)
{
	if (m_pShadowPipelineState) m_pDevice->GetCmdList()->SetPipelineState(m_pShadowPipelineState);
	if (m_pProgram && m_pProgram->pRootSignature) m_pDevice->GetCmdList()->SetGraphicsRootSignature(m_pProgram->pRootSignature->GetRootSignature());
	m_pDevice->GetCmdList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// ビューポートは呼び出し側(影の描画)が描き込む深度テクスチャの大きさで設定済み。
	// (以前はここで4096に固定していたため、2048の平行光の影ではスキニングのモデルだけ縮尺がずれていた)

	int b0 = GetRootParameterIndex(ShaderBindingType::CBV, 0);
	if (b0 != -1) context.BindCamera(b0);
}


