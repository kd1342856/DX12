#include "../../../Pch.h"
#include "ShaderManager.h"
#include "../ShaderCompiler/ShaderCompiler.h"
#include <algorithm>

ShaderManager& ShaderManager::Instance()
{
	static ShaderManager instance;
	return instance;
}

void ShaderManager::Initialize(GraphicsDevice* pDevice)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_pDevice = pDevice;
}

ShaderProgram* ShaderManager::LoadShader(const std::wstring& vsPath, const std::wstring& psPath)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	std::wstring key = vsPath + L"|" + psPath;
	if (m_shaderCache.find(key) != m_shaderCache.end()) {
		return m_shaderCache[key].get();
	}

	auto program = std::make_unique<ShaderProgram>();

	program->pVS = ShaderCompiler::CompileVS(vsPath, L"main");
	if (program->pVS) {
		char msg[512];
		sprintf_s(msg, "VS size=%zu, path=%ls\n", program->pVS->GetBufferSize(), vsPath.c_str());
		OutputDebugStringA(msg);
	} else {
		char msg[512];
		sprintf_s(msg, "VS is null after CompileVS, path=%ls\n", vsPath.c_str());
		OutputDebugStringA(msg);
	}

	if (psPath.length() > 0) {
		program->pPS = ShaderCompiler::CompilePS(psPath, L"main");
	}

	auto vsBindings = ShaderReflection::Reflect(program->pVS);
	auto psBindings = ShaderReflection::Reflect(program->pPS);

	// Merge bindings
	std::vector<ShaderBinding> mergedBindings = vsBindings;
	for (const auto& pb : psBindings) {
		bool found = false;
		for (const auto& vb : mergedBindings) {
			if (vb.Type == pb.Type && vb.BindPoint == pb.BindPoint && vb.Space == pb.Space) {
				found = true;
				break;
			}
		}
		if (!found) {
			mergedBindings.push_back(pb);
		}
	}

	// Sort bindings to maintain deterministic root signature order
	std::sort(mergedBindings.begin(), mergedBindings.end(), [](const ShaderBinding& a, const ShaderBinding& b) {
		if (a.Type != b.Type) return a.Type < b.Type;
		return a.BindPoint < b.BindPoint;
	});

	program->Bindings = mergedBindings;

	// Build RootSignature
	std::vector<DescriptorRange> ranges;
	for (const auto& b : mergedBindings) {
		if (b.Type == ShaderBindingType::Sampler) continue; // Handled as static samplers in RootSignature::Create for now

		DescriptorRange range;
		if (b.Type == ShaderBindingType::CBV) range.Type = RangeType::CBV;
		else if (b.Type == ShaderBindingType::SRV) range.Type = RangeType::SRV;
		else if (b.Type == ShaderBindingType::UAV) range.Type = RangeType::UAV;
		
		range.Register = b.BindPoint;
		range.Count = b.BindCount;
		range.Space = b.Space;
		ranges.push_back(range);
	}

	program->pRootSignature = std::make_unique<RootSignature>();
	program->pRootSignature->Create(m_pDevice, ranges);

	m_shaderCache[key] = std::move(program);
	return m_shaderCache[key].get();
}

ID3D12PipelineState* ShaderManager::GetPipelineState(ShaderProgram* pProgram, const PipelineDesc& desc)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	// Generate hash for desc
	std::wstring key = std::to_wstring((uint64_t)pProgram) + L"|";
	key += std::to_wstring((uint32_t)desc.TopologyType) + L"|";
	key += std::to_wstring((uint32_t)desc.BlendMode) + L"|";
	key += std::to_wstring((uint32_t)desc.CullMode) + L"|";
	key += std::to_wstring(desc.IsDepth) + L"|";
	key += std::to_wstring(desc.IsDepthMask) + L"|";
	key += std::to_wstring(desc.IsWireFrame) + L"|";
	
	// Encode Formats
	for (auto fmt : desc.Formats) {
		key += std::to_wstring((uint32_t)fmt) + L",";
	}
	key += L"|";
	
	// Encode InputLayouts
	for (auto il : desc.InputLayouts) {
		key += std::to_wstring((uint32_t)il) + L",";
	}

	
	if (m_psoCache.find(key) != m_psoCache.end()) {
		return m_psoCache[key].Get();
	}

	Pipeline pipeline;
	// Override root signature
	PipelineDesc copyDesc = desc;
	copyDesc.pRootSignature = pProgram->pRootSignature.get();
	copyDesc.pBlobs = { pProgram->pVS.Get(), nullptr, nullptr, nullptr, pProgram->pPS.Get() };

	pipeline.Create(m_pDevice, copyDesc);
	m_psoCache[key] = pipeline.GetPipeline();

	return m_psoCache[key].Get();
}

