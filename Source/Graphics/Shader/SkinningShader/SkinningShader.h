#pragma once
#include "../GraphicsShader/GraphicsShader.h"

class SkinningShader : public GraphicsShader {
public:
	virtual void Create(GraphicsDevice* pGraphicsDevice) override;
	virtual void Begin(RenderContext& context) override;
	virtual void BeginModel(const ModelData& model, const DrawContext& drawContext) override;
	virtual void BeginNode(const ModelData::Node& node, const Math::Matrix& nodeWorld) override;
	virtual void BeforeDrawMesh(const Mesh& mesh, const Material& material) override;

	// Note: ShadowPass could be a separate shader (e.g. SkinningShadowShader) 
	// or we can use m_upShadowPipeline in a separate BeginShadow() if we don't separate it yet.
	// For now, let's keep BeginShadow/DrawShadowModel using ModelRenderer too if possible.
	void BeginShadow(RenderContext& context);

private:
	void SetMaterial(const Material& material);

	ID3D12PipelineState* m_pShadowPipelineState = nullptr;
	
	// Temporary state during DrawModel
	const DrawContext* m_pCurrentDrawContext = nullptr;
};
