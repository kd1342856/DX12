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
	InitializeShaderSettings();
}

void ShaderManager::InitializeShaderSettings()
{
	m_RendererSettings = {};
	m_LightingSettings = {};
	m_ShadowSettings = {};
	m_IBLSettings = {};
	m_PostProcessSettings = {};
	m_DebugSettings = {};
	m_FogSettings = {};

	m_SystemData = {};
	m_PostProcessData = {};

	m_LightData = {};
	m_LightData.SL_Count = 0;
	m_LightData.PL_Count = 0;
	m_LightData.AmbientLight = { 0.35f, 0.35f, 0.35f };
	m_LightData.IndirectShadowFloor = 0.05f;
	m_LightData.DL_Dir = { 0.0f, -1.0f, 1.0f };
	m_LightData.DL_ShadowBias = 0.001f;
	m_LightData.DL_Color = { 0.6f, 0.6f, 0.65f };
	m_LightData.DL_ShadowPower = 2.5f;
	m_LightData.DL_CascadeSplits = { 0.1f, 0.3f, 0.6f, 1.0f };

	m_baseAmbientLight = m_LightData.AmbientLight;
	m_baseDLColor = m_LightData.DL_Color;

	m_huntActiveCount = 0;
	m_huntBlend = 0.0f;
	m_LightData.DistanceFogColor = { m_FogSettings.FogColor.x, m_FogSettings.FogColor.y, m_FogSettings.FogColor.z };
	m_LightData.DistanceFogDensity = m_FogSettings.FogDensity;
}

void ShaderManager::UpdateConstantBuffers()
{
	UpdateRendererCB();
	UpdateLightingCB();
	UpdateShadowCB();
	UpdateIBLCB();
	UpdatePostProcessCB();
	UpdateDebugCB();
	UpdateFogCB();
}

void ShaderManager::UpdateRendererCB()
{
	if (m_RendererSettings.IsDirty)
	{
		m_SystemData.EnableSSR = m_RendererSettings.EnableSSR ? 1 : 0;
		m_SystemData.SSRStepSize = m_RendererSettings.SSRStepSize;

		m_RendererSettings.IsDirty = false;
	}
}

void ShaderManager::UpdateLightingCB()
{
	if (m_LightingSettings.IsDirty)
	{
		// Stored as the "base" values rather than written to m_LightData directly - UpdateFogCB
		// re-derives m_LightData.AmbientLight/DL_Color from these every frame, scaled by the
		// hunt-darken multiplier, so a stale write here can't fight with that.
		m_baseAmbientLight = { m_LightingSettings.AmbientLight.x, m_LightingSettings.AmbientLight.y, m_LightingSettings.AmbientLight.z };
		m_LightData.IndirectShadowFloor = m_LightingSettings.IndirectShadowFloor;

		float dx = m_LightingSettings.DirectionalLightDir.x;
		float dy = m_LightingSettings.DirectionalLightDir.y;
		float dz = m_LightingSettings.DirectionalLightDir.z;
		float len = sqrtf(dx * dx + dy * dy + dz * dz);
		if (len > 0.00001f) {
			dx /= len; dy /= len; dz /= len;
		}
		m_LightData.DL_Dir = { dx, dy, dz };

		m_baseDLColor = {
			m_LightingSettings.DirectionalLightColor.x * m_LightingSettings.DirectionalLightIntensity,
			m_LightingSettings.DirectionalLightColor.y * m_LightingSettings.DirectionalLightIntensity,
			m_LightingSettings.DirectionalLightColor.z * m_LightingSettings.DirectionalLightIntensity
		};

		m_LightingSettings.IsDirty = false;
	}
}

void ShaderManager::UpdateShadowCB()
{
	if (m_ShadowSettings.IsDirty)
	{
		m_LightData.DL_ShadowPower = m_ShadowSettings.EnableShadows ? m_ShadowSettings.ShadowPower : 0.0f;
		m_LightData.DL_ShadowBias = m_ShadowSettings.ShadowBias;
		m_LightData.DL_CascadeSplits = {
			m_ShadowSettings.CascadeSplits.x,
			m_ShadowSettings.CascadeSplits.y,
			m_ShadowSettings.CascadeSplits.z,
			m_ShadowSettings.CascadeSplits.w
		};
		m_ShadowSettings.IsDirty = false;
	}
}

void ShaderManager::UpdateIBLCB()
{
	if (m_IBLSettings.IsDirty)
	{
		// IBL Data mapping will go here when CBufferData is updated
		m_IBLSettings.IsDirty = false;
	}
}

void ShaderManager::UpdatePostProcessCB()
{
	if (m_PostProcessSettings.IsDirty)
	{
		m_SystemData.EnableHDR = m_PostProcessSettings.EnableHDR ? 1 : 0;
		m_SystemData.Exposure = m_PostProcessSettings.Exposure;
		m_SystemData.Gamma = m_PostProcessSettings.Gamma;
		m_SystemData.ScreenWidth = 1280.0f;
		m_SystemData.ScreenHeight = 720.0f;

		m_PostProcessData.EnableHDR = m_PostProcessSettings.EnableHDR ? 1 : 0;
		m_PostProcessData.Exposure = m_PostProcessSettings.Exposure;
		m_PostProcessData.Gamma = m_PostProcessSettings.Gamma;
		m_PostProcessData.BloomThreshold = m_PostProcessSettings.BloomThreshold;
		m_PostProcessData.BloomIntensity = m_PostProcessSettings.BloomIntensity;
		m_PostProcessData.BlurRadius = m_PostProcessSettings.BloomRadius;

		m_PostProcessData.VignetteIntensity = m_PostProcessSettings.EnableVignette ? m_PostProcessSettings.VignetteIntensity : 0.0f;
		m_PostProcessData.VignetteSmoothness = m_PostProcessSettings.VignetteSmoothness;

		m_PostProcessData.FilmGrainIntensity = m_PostProcessSettings.EnableFilmGrain ? m_PostProcessSettings.FilmGrainIntensity : 0.0f;

		m_PostProcessData.ChromaticAberrationIntensity = m_PostProcessSettings.EnableChromaticAberration ? m_PostProcessSettings.ChromaticAberrationIntensity : 0.0f;

		m_PostProcessData.EnableDOF = m_PostProcessSettings.EnableDOF ? 1 : 0;
		m_PostProcessData.DOFFocusDistance = m_PostProcessSettings.DOFFocusDistance;
		m_PostProcessData.DOFFocusRange = m_PostProcessSettings.DOFFocusRange;

		m_PostProcessData.GodRaysDensity = m_PostProcessSettings.GodRaysDensity;
		m_PostProcessData.GodRaysDecay = m_PostProcessSettings.GodRaysDecay;
		m_PostProcessData.GodRaysWeight = m_PostProcessSettings.GodRaysWeight;
		m_PostProcessData.GodRaysExposure = m_PostProcessSettings.GodRaysExposure;
		m_PostProcessData.GodRaysIntensity = m_PostProcessSettings.GodRaysIntensity;
		m_PostProcessData.GodRaysNumSamples = static_cast<uint32_t>(std::max(1, m_PostProcessSettings.GodRaysNumSamples));

		// EnableGodRays自体はSetGodRaysLightScreenPos側で毎フレーム確定させる(このIsDirtyブロックは
		// 設定変更時にしか通らないため、ここで書くと光源位置の更新と1フレームずれてしまう)。

		m_PostProcessSettings.IsDirty = false;
	}

	// フィルムグレインのアニメーション用に経過時間を毎フレーム進める(IsDirtyに関係なく)。
	m_PostProcessData.Time += GameTimer::Instance().DeltaTime();
}

void ShaderManager::UpdateDebugCB()
{
	if (m_DebugSettings.IsDirty)
	{
		m_SystemData.DebugView = static_cast<int>(m_DebugSettings.CurrentDebugView);
		m_DebugSettings.IsDirty = false;
	}
}

void ShaderManager::UpdateFogCB()
{
	if (m_FogSettings.IsDirty)
	{
		m_LightData.DistanceFogColor = { m_FogSettings.FogColor.x, m_FogSettings.FogColor.y, m_FogSettings.FogColor.z };
		m_FogSettings.IsDirty = false;
	}

	// Smoothly interpolate a single 0..1 blend every frame toward 1 while a ghost is hunting,
	// or back toward 0 otherwise. Both the fog density and the light-darkening below are derived
	// from this same value so they ramp up/down together. Not gated behind IsDirty, since this
	// needs to keep updating purely from the ghost's hunting state changing, without any ImGui
	// edit happening.
	float target = IsHuntFogActive() ? 1.0f : 0.0f;
	float dt = GameTimer::Instance().DeltaTime();
	float speed = m_FogSettings.HuntTransitionSpeed;
	if (speed <= 0.0f)
	{
		m_huntBlend = target;
	}
	else
	{
		float maxStep = speed * dt;
		float diff = target - m_huntBlend;
		if (fabsf(diff) <= maxStep) m_huntBlend = target;
		else m_huntBlend += (diff > 0.0f ? maxStep : -maxStep);
	}

	// Distance fog: present at FogDensity from the start of a match, growing to HuntFogDensity
	// while a ghost is hunting.
	m_LightData.DistanceFogDensity = m_FogSettings.EnableFog
		? (m_FogSettings.FogDensity + (m_FogSettings.HuntFogDensity - m_FogSettings.FogDensity) * m_huntBlend)
		: 0.0f;

	// Screen darkening: scales ambient + directional light down toward black as the hunt blend
	// rises, layered on top of whatever LightingSettings/UpdateLightingCB last computed.
	float darken = m_FogSettings.EnableHuntDarken ? (1.0f - m_FogSettings.HuntDarkenAmount * m_huntBlend) : 1.0f;
	m_LightData.AmbientLight = { m_baseAmbientLight.x * darken, m_baseAmbientLight.y * darken, m_baseAmbientLight.z * darken };
	m_LightData.DL_Color = { m_baseDLColor.x * darken, m_baseDLColor.y * darken, m_baseDLColor.z * darken };
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

	// Generate cache key from desc parameters
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

	// Add PS usage flag to cache key
	// so that depth-only PSO (PS=nullptr) is cached separately from normal PSO (PS=valid)
	bool bUsePS = true;
	if (!desc.pBlobs.empty() && desc.pBlobs.size() >= 5 && desc.pBlobs[4] == nullptr)
	{
		// desc explicitly sets PS blob to nullptr -> Depth Only PSO (e.g. ShadowShader)
		bUsePS = false;
	}
	key += L"|PS=";
	key += std::to_wstring((int)bUsePS);

	if (m_psoCache.find(key) != m_psoCache.end()) {
		return m_psoCache[key].Get();
	}

	Pipeline pipeline;
	// Always use the program's root signature
	PipelineDesc copyDesc = desc;
	copyDesc.pRootSignature = pProgram->pRootSignature.get();
	if (bUsePS)
	{
		// Normal pass: bind both VS and PS
		copyDesc.pBlobs = { pProgram->pVS.Get(), nullptr, nullptr, nullptr, pProgram->pPS.Get() };
	}
	else
	{
		// Depth Only pass (ShadowShader etc.): set PS to nullptr -> depth-only PSO
		copyDesc.pBlobs = { pProgram->pVS.Get(), nullptr, nullptr, nullptr, nullptr };
	}

	pipeline.Create(m_pDevice, copyDesc);
	m_psoCache[key] = pipeline.GetPipeline();

	return m_psoCache[key].Get();
}