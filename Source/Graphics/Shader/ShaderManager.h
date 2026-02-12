#pragma once
#include "RootSignature/RootSignature.h"
#include "Pipeline/Pipeline.h"
//#include "StandardShader/StandardShader.h"

struct RenderingSetting
{
	std::vector<InputLayout> InputLayouts;
	std::vector<DXGI_FORMAT> Formats;
	CullMode CullMode = CullMode::Back;
	BlendMode BlendMode = BlendMode::Alpha;
	PrimitiveTopologyType PrimitiveTopologyType = PrimitiveTopologyType::Triangle;
	bool IsDepth = true;
	bool IsDepthMask = true;
	int RTVCount = 1;
	bool IsWireFrame = false;
};
// =============================================
// ShaderManager
// ?V?F?[?_?[?????R???e?i(?V???O???g??)
// ?e?V?F?[?_?[????????o?[???????
// =============================================
class ShaderManager
{
public:
// ??????(?S?V?F?[?_?[??)
void Init();

// ?J???????Z?b?g(???t???[?????)
void SetCameraMatrix(const Math::Matrix& mView, const Math::Matrix& mProj);

// ?V?F?[?_?[???
//StandardShader m_standardShader;
// SpriteShader m_spriteShader;           // ???????
// PostProcessShader m_postProcessShader; // ???????

private:
ShaderManager() = default;
~ShaderManager() = default;

public:
static ShaderManager& Instance()
{
static ShaderManager instance;
return instance;
}
};