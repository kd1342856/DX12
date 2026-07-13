#pragma once
#include "DrawContext.h"
#include "../Geometry/Model/Model.h"

class GraphicsShader;

class ModelRenderer {
public:
	static void Draw(GraphicsShader& shader, const ModelData& model, const DrawContext& context);
};
