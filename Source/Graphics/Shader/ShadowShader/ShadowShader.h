#pragma once
#include "../GraphicsShader/GraphicsShader.h"

class ShadowShader : public GraphicsShader {
public:
	virtual void Create(GraphicsDevice* pGraphicsDevice) override;
	virtual void Begin(RenderContext& context) override;
	virtual void BeginNode(const ModelData::Node& node, const Math::Matrix& nodeWorld) override;
};
