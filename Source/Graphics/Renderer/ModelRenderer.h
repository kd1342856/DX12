#pragma once
#include "DrawContext.h"
#include "../Geometry/Model/Model.h"

#include "RenderQueue.h"

class GraphicsShader;

class ModelRenderer {
public:
	// Transitional data container
	const ModelData* pModelData = nullptr;
};
