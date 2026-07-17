#include "../../../Pch.h"
#include "ShaderReflection.h"

#pragma comment(lib, "dxguid.lib")

std::vector<ShaderBinding> ShaderReflection::Reflect(Microsoft::WRL::ComPtr<IDxcBlob> pShaderBlob)
{
	std::vector<ShaderBinding> bindings;
	if (!pShaderBlob) return bindings;

	Microsoft::WRL::ComPtr<IDxcUtils> pUtils;
	if (FAILED(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils)))) return bindings;

	DxcBuffer reflectionBuffer;
	reflectionBuffer.Ptr = pShaderBlob->GetBufferPointer();
	reflectionBuffer.Size = pShaderBlob->GetBufferSize();
	reflectionBuffer.Encoding = DXC_CP_ACP;

	Microsoft::WRL::ComPtr<ID3D12ShaderReflection> pReflection;
	if (FAILED(pUtils->CreateReflection(&reflectionBuffer, IID_PPV_ARGS(&pReflection)))) return bindings;

	D3D12_SHADER_DESC shaderDesc;
	pReflection->GetDesc(&shaderDesc);

	for (UINT i = 0; i < shaderDesc.BoundResources; ++i)
	{
		D3D12_SHADER_INPUT_BIND_DESC bindDesc;
		pReflection->GetResourceBindingDesc(i, &bindDesc);

		ShaderBinding binding;
		binding.Name = bindDesc.Name;
		binding.BindPoint = bindDesc.BindPoint;
		binding.BindCount = bindDesc.BindCount;
		binding.Space = bindDesc.Space;

		if (bindDesc.Type == D3D_SIT_CBUFFER) {
			binding.Type = ShaderBindingType::CBV;
		}
		else if (bindDesc.Type == D3D_SIT_TEXTURE || bindDesc.Type == D3D_SIT_TBUFFER || bindDesc.Type == D3D_SIT_STRUCTURED || bindDesc.Type == D3D_SIT_BYTEADDRESS) {
			binding.Type = ShaderBindingType::SRV;
		}
		else if (bindDesc.Type == D3D_SIT_UAV_RWTYPED || bindDesc.Type == D3D_SIT_UAV_RWSTRUCTURED || bindDesc.Type == D3D_SIT_UAV_RWBYTEADDRESS || bindDesc.Type == D3D_SIT_UAV_APPEND_STRUCTURED || bindDesc.Type == D3D_SIT_UAV_CONSUME_STRUCTURED || bindDesc.Type == D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER) {
			binding.Type = ShaderBindingType::UAV;
		}
		else if (bindDesc.Type == D3D_SIT_SAMPLER) {
			binding.Type = ShaderBindingType::Sampler;
		}
		else {
			continue; // unsupported type
		}

		bindings.push_back(binding);
	}

	return bindings;
}