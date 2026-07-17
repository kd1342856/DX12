#pragma once
#include "../Pipeline/Pipeline.h"
#include "../RootSignature/RootSignature.h"
#include "../ShaderManager/ShaderManager.h"
#include "../../Renderer/RenderContext.h"
#include "../../Renderer/DrawContext.h"
#include "../../../Graphics/Device/GraphicsDevice.h"

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

protected:
	GraphicsDevice* m_pDevice = nullptr;
	ShaderProgram* m_pProgram = nullptr;
	ID3D12PipelineState* m_pPipelineState = nullptr;
	D3D12_PRIMITIVE_TOPOLOGY m_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
};