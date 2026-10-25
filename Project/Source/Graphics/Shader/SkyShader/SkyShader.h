#pragma once
#include "../GraphicsShader/GraphicsShader.h"
#include "../../Geometry/Mesh/Mesh.h"

class SkyShader : public GraphicsShader
{
public:
    void Create(class GraphicsDevice* pGraphicsDevice) override;
    
    // Shader initialization
    void Begin(class RenderContext& context) override;
    
    // Draw calls
    void BeginNode(const struct ModelData::Node& node, const Math::Matrix& nodeWorld);
    void BeforeDrawMesh(const Mesh& mesh, const struct Material& material);

private:
    void SetMaterial(const struct Material& material);
};
