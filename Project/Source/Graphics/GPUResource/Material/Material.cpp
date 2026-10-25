#include "../../../Pch.h"
#include "Material.h"
#include "../../Device/GraphicsDevice.h"
#include "../../Shader/ShaderManager/ShaderManager.h"

void Material::Bind(GraphicsDevice* pDevice)
{
	if (!m_pProgram) return;

	// In the future, this will map to PipelineState or Constant Buffer binding
}
