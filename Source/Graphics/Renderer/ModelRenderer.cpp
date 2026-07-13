#include "../../Pch.h"
#include "ModelRenderer.h"
#include "../Shader/GraphicsShader/GraphicsShader.h"

void ModelRenderer::Draw(GraphicsShader& shader, const ModelData& model, const DrawContext& context)
{
	shader.BeginModel(model, context);

	const auto& nodes = model.GetNodes();
	std::vector<Math::Matrix> nodeWorldMatrices(nodes.size());

	for (int i = 0; i < (int)nodes.size(); ++i) {
		const auto& node = nodes[i];
		
		// In this framework, meshes and collisions are already baked to model root space.
		// Applying Assimp's localTransforms causes double transformations and desyncs visual with collisions.
		nodeWorldMatrices[i] = context.World;

		shader.BeginNode(node, nodeWorldMatrices[i]);

		for (const auto& spMesh : node.meshes) {
			if (spMesh) {
				shader.BeforeDrawMesh(*spMesh, spMesh->GetMaterial());
				spMesh->DrawInstanced(spMesh->GetInstanceCount());
			}
		}
	}

	shader.EndModel();
}