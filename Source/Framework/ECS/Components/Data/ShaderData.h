#pragma once
#include "../../../../Graphics/Shader/Pipeline/Pipeline.h"
#include <string>
#include <vector>

struct ShaderData
{
	PipelineDesc m_pipelineDesc;
	std::vector<RangeType> m_rangeTypes;
	std::wstring m_shaderFilePath;
};
