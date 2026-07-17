#pragma once
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <dxcapi.h>
#include <wrl.h>

#include "../ShaderReflection/ShaderReflection.h"
#include "../RootSignature/RootSignature.h"
#include "../Pipeline/Pipeline.h"

class GraphicsDevice;

struct ShaderProgram {
	Microsoft::WRL::ComPtr<IDxcBlob> pVS;
	Microsoft::WRL::ComPtr<IDxcBlob> pPS;
	std::vector<ShaderBinding> Bindings;
	std::unique_ptr<RootSignature> pRootSignature;
};

class ShaderManager {
public:
	static ShaderManager& Instance();
	void Initialize(GraphicsDevice* pDevice);

	ShaderProgram* LoadShader(const std::wstring& vsPath, const std::wstring& psPath);
	ID3D12PipelineState* GetPipelineState(ShaderProgram* pProgram, const PipelineDesc& desc);

private:
	GraphicsDevice* m_pDevice = nullptr;
	std::unordered_map<std::wstring, std::unique_ptr<ShaderProgram>> m_shaderCache;
	std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID3D12PipelineState>> m_psoCache;
	std::mutex m_mutex;
};
