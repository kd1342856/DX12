#pragma once
#include "../GraphicsShader/GraphicsShader.h"

class LitShader : public GraphicsShader {
public:
	virtual void Create(GraphicsDevice* pGraphicsDevice) override;
	virtual void Begin(RenderContext& context) override;
	virtual void BeginNode(const ModelData::Node& node, const Math::Matrix& nodeWorld) override;
	virtual void BeforeDrawMesh(const Mesh& mesh, const Material& material) override;

private:
	void SetMaterial(const Material& material);
	int m_cbvCount = 0;
};
