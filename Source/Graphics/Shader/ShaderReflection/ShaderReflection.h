#pragma once
#include <d3d12.h>
#include <dxcapi.h>
#include <d3d12shader.h>
#include <wrl.h>
#include <string>
#include <vector>

enum class ShaderBindingType {
	CBV,
	SRV,
	UAV,
	Sampler
};

struct ShaderBinding {
	std::string Name;
	ShaderBindingType Type;
	uint32_t BindPoint;
	uint32_t BindCount;
	uint32_t Space;
};

class ShaderReflection
{
public:
	static std::vector<ShaderBinding> Reflect(Microsoft::WRL::ComPtr<IDxcBlob> pShaderBlob);
};