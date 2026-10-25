#pragma once
#include "../Pipeline/Pipeline.h"
#include "../RootSignature/RootSignature.h"
#include "../ShaderManager/ShaderManager.h"
#include "../../Renderer/RenderContext.h"
#include "../../Renderer/DrawContext.h"
#include "../../../Graphics/Device/GraphicsDevice.h"
#include "../../Renderer/EnvironmentBRDF.h"

class ModelData;
struct Node;
class Mesh;
class Material;

class GraphicsShader {
public:
	virtual ~GraphicsShader() = default;

	virtual void Create(GraphicsDevice* pDevice) = 0;

	virtual void Begin(RenderContext& context)
	{
		auto* cmd = m_pDevice->GetCmdList();
		if (m_pPipelineState) cmd->SetPipelineState(m_pPipelineState);
		if (m_pProgram && m_pProgram->pRootSignature) cmd->SetGraphicsRootSignature(m_pProgram->pRootSignature->GetRootSignature());
		cmd->IASetPrimitiveTopology(m_topology);
	}

	virtual void BeginModel(const ModelData& model, const DrawContext& drawContext) {}
	virtual void BeginNode(const ModelData::Node& node, const Math::Matrix& nodeWorld) {}
	virtual void BeforeDrawMesh(const Mesh& mesh, const Material& material) {}
	virtual void EndModel() {}

	virtual void End() {}

	int GetRootParameterIndex(ShaderBindingType type, uint32_t bindPoint, uint32_t space = 0) const
	{
		if (!m_pProgram) return -1;
		int index = 0;
		for (const auto& b : m_pProgram->Bindings) {
			if (b.Type == ShaderBindingType::Sampler) continue;
			if (b.Type == type && b.BindPoint == bindPoint && b.Space == space) {
				return index;
			}
			index++;
		}
		return -1;
	}

protected:
	// 局所光源の影(t13: 狭いスポットの2D配列、t14: 点光源と広いスポットのキューブ配列)を束縛する。
	// inc_Lighting.hlsliの影の関数を使うシェーダーのBegin()で呼ぶ
	void BindLocalShadowMaps()
	{
		BindShadowMapArray(13, m_pDevice->GetSpotShadowMaps());
		BindShadowMapArray(14, m_pDevice->GetCubeShadowMaps());
	}

	// 環境BRDFのLUT(t21)を束縛する。inc_Lighting.hlsliのInitializeSurfaceBRDFを使うシェーダーのBegin()で呼ぶ。
	// LUTをまだ作っていなければ黒を渡し、シェーダー側で解析的な近似に切り替える
	void BindEnvironmentBRDF()
	{
		const int rootIndex = GetRootParameterIndex(ShaderBindingType::SRV, EnvironmentBRDF::kShaderRegister);
		if (rootIndex == -1)
		{
			return;
		}
		const int srvIndex = EnvironmentBRDF::Instance().GetSRVIndex();
		if (srvIndex == -1)
		{
			m_pDevice->GetBlackTex()->Set(rootIndex);
			return;
		}
		auto handle = m_pDevice->GetDescriptorHeapManager()->GetCBVSRVUAVAllocator()->GetGPUHandle(srvIndex);
		m_pDevice->GetCmdList()->SetGraphicsRootDescriptorTable(rootIndex, handle);
	}

	GraphicsDevice* m_pDevice = nullptr;
	ShaderProgram* m_pProgram = nullptr;
	ID3D12PipelineState* m_pPipelineState = nullptr;
	D3D12_PRIMITIVE_TOPOLOGY m_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

private:
	void BindShadowMapArray(uint32_t bindPoint, const ShadowMapArray* pShadowMaps)
	{
		int rootIndex = GetRootParameterIndex(ShaderBindingType::SRV, bindPoint);
		if (rootIndex == -1 || !pShadowMaps || pShadowMaps->GetSRVNumber() == -1)
		{
			return;
		}
		auto handle = m_pDevice->GetDescriptorHeapManager()->GetCBVSRVUAVAllocator()->GetGPUHandle(pShadowMaps->GetSRVNumber());
		m_pDevice->GetCmdList()->SetGraphicsRootDescriptorTable(rootIndex, handle);
	}
};