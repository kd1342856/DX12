#include "../../../Pch.h"
#include "ShaderManager.h"
#include "../ShaderCompiler/ShaderCompiler.h"
#include <algorithm>

namespace
{
	std::filesystem::path GetPipelineLibraryPath()
	{
		std::filesystem::path dir = AppPaths::GetSavedDir() / L"ShaderCache";
		std::error_code errorCode;
		std::filesystem::create_directories(dir, errorCode);
		// DebugとReleaseはシェーダーの中身が違い、保存のたびに今回使ったPSOだけで作り直すので、
		// 同じファイルだと交互に起動するたびに互いのPSOを消してしまう。構成ごとにファイルを分ける
#ifdef _DEBUG
		return dir / L"PipelineLibrary_Debug.bin";
#else
		return dir / L"PipelineLibrary_Release.bin";
#endif
	}

	std::wstring ToHex(uint64_t value)
	{
		wchar_t buffer[17] = {};
		swprintf_s(buffer, L"%016llx", static_cast<unsigned long long>(value));
		return buffer;
	}
}

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
	OpenPipelineLibrary();
}

void ShaderManager::OpenPipelineLibrary()
{
	m_cpPipelineLibrary.Reset();
	m_pipelineLibraryData.clear();
	if (!m_pDevice || !m_pDevice->GetDevice())
	{
		return;
	}
	ID3D12Device8* pD3DDevice = m_pDevice->GetDevice();

	const std::filesystem::path libraryPath = GetPipelineLibraryPath();
	{
		std::ifstream ifs(libraryPath, std::ios::binary | std::ios::ate);
		if (ifs.is_open())
		{
			const std::streamsize size = ifs.tellg();
			if (size > 0)
			{
				m_pipelineLibraryData.resize(static_cast<size_t>(size));
				ifs.seekg(0);
				if (!ifs.read(reinterpret_cast<char*>(m_pipelineLibraryData.data()), size))
				{
					m_pipelineLibraryData.clear();
				}
			}
		}
	}

	if (!m_pipelineLibraryData.empty())
	{
		HRESULT hr = pD3DDevice->CreatePipelineLibrary(m_pipelineLibraryData.data(), m_pipelineLibraryData.size(), IID_PPV_ARGS(&m_cpPipelineLibrary));
		if (FAILED(hr))
		{
			// ドライバーの更新やGPUの交換の後はD3D12_ERROR_DRIVER_VERSION_MISMATCHなどで失敗する。空から作り直す
			Logger::Instance().AddLog(Logger::LogLevel::Info, u8"PSOライブラリを読み込めないため作り直します (hr=0x%08X)", static_cast<unsigned int>(hr));
			m_cpPipelineLibrary.Reset();
			m_pipelineLibraryData.clear();
		}
	}

	if (!m_cpPipelineLibrary)
	{
		if (FAILED(pD3DDevice->CreatePipelineLibrary(nullptr, 0, IID_PPV_ARGS(&m_cpPipelineLibrary))))
		{
			// 対応していないドライバーでは、ライブラリを使わずに毎回PSOを作成する
			Logger::Instance().AddLog(Logger::LogLevel::Warning, u8"このドライバーはPSOライブラリに対応していません");
			m_cpPipelineLibrary.Reset();
		}
	}
}

void ShaderManager::SavePipelineLibrary()
{
	std::lock_guard<std::mutex> lock(m_mutex);

	Logger::Instance().AddLog(Logger::LogLevel::Info, u8"シェーダー: キャッシュから %d / コンパイル %d、PSO: ライブラリから %d / 新規作成 %d",
		ShaderCompiler::GetCacheHitCount(), ShaderCompiler::GetCacheMissCount(), m_psoLoadedCount, m_psoCreatedCount);

	if (!m_isPipelineLibraryDirty || !m_cpPipelineLibrary || !m_pDevice)
	{
		return;
	}

	// 今回使ったPSOだけで新しいライブラリを作って保存する。
	// 読み込んだライブラリに書き足していくと、シェーダーを変えるたびに古いPSOが溜まり続けるため
	Microsoft::WRL::ComPtr<ID3D12PipelineLibrary> cpNewLibrary;
	if (FAILED(m_pDevice->GetDevice()->CreatePipelineLibrary(nullptr, 0, IID_PPV_ARGS(&cpNewLibrary))))
	{
		return;
	}
	for (const auto& [key, entry] : m_psoCache)
	{
		if (entry.cpPipelineState)
		{
			// 中身が同じPSOが別のキーで2回来ると名前が重なりE_INVALIDARGになるが、1つ入っていれば十分なので無視する
			cpNewLibrary->StorePipeline(entry.libraryName.c_str(), entry.cpPipelineState.Get());
		}
	}

	const SIZE_T serializedSize = cpNewLibrary->GetSerializedSize();
	std::vector<uint8_t> serialized(serializedSize);
	if (serializedSize == 0 || FAILED(cpNewLibrary->Serialize(serialized.data(), serializedSize)))
	{
		return;
	}

	// 一時ファイルに書いてから置き換え、書き込みの途中で終了しても壊れたファイルを残さない
	const std::filesystem::path libraryPath = GetPipelineLibraryPath();
	std::filesystem::path tempPath = libraryPath;
	tempPath += L".tmp";
	{
		std::ofstream ofs(tempPath, std::ios::binary | std::ios::trunc);
		if (!ofs.is_open())
		{
			return;
		}
		ofs.write(reinterpret_cast<const char*>(serialized.data()), static_cast<std::streamsize>(serialized.size()));
		if (!ofs)
		{
			return;
		}
	}
	std::error_code errorCode;
	std::filesystem::rename(tempPath, libraryPath, errorCode);
	if (errorCode)
	{
		std::filesystem::remove(tempPath, errorCode);
		return;
	}

	// 古いライブラリを解放してから、そのライブラリが参照していたデータを捨てる
	m_cpPipelineLibrary = cpNewLibrary;
	m_pipelineLibraryData.clear();
	m_isPipelineLibraryDirty = false;
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
		m_PostProcessData.BrightnessGamma = m_PostProcessSettings.BrightnessGamma;
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

	if (program->pVS)
	{
		program->vsHash = ShaderCompiler::HashBytes(program->pVS->GetBufferPointer(), program->pVS->GetBufferSize());
	}
	if (program->pPS)
	{
		program->psHash = ShaderCompiler::HashBytes(program->pPS->GetBufferPointer(), program->pPS->GetBufferSize());
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
	// PSOの中身を決めるdescの値はすべてキーに含める(ディスクのライブラリでも同じキーを名前に使う)
	std::wstring key;
	key += std::to_wstring((uint32_t)desc.TopologyType) + L"|";
	key += std::to_wstring((uint32_t)desc.BlendMode) + L"|";
	key += std::to_wstring((uint32_t)desc.CullMode) + L"|";
	key += std::to_wstring(desc.IsDepth) + L"|";
	key += std::to_wstring(desc.IsDepthMask) + L"|";
	key += std::to_wstring(desc.IsWireFrame) + L"|";
	key += std::to_wstring((uint32_t)desc.DepthFunc) + L"|";
	key += std::to_wstring(desc.DepthBias) + L"|";

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

	// ライブラリでの名前は、起動ごとに変わるポインタの代わりにシェーダーの中身のハッシュで区別する
	std::wstring libraryName = ToHex(pProgram->vsHash) + L"|" + ToHex(pProgram->psHash) + L"|" + key;
	key = std::to_wstring((uint64_t)pProgram) + L"|" + key;

	if (m_psoCache.find(key) != m_psoCache.end()) {
		return m_psoCache[key].cpPipelineState.Get();
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

	pipeline.Create(m_pDevice, copyDesc, m_cpPipelineLibrary.Get(), libraryName.c_str());
	if (pipeline.IsLoadedFromLibrary())
	{
		++m_psoLoadedCount;
	}
	else if (pipeline.GetPipeline())
	{
		++m_psoCreatedCount;
		m_isPipelineLibraryDirty = true;
	}
	m_psoCache[key] = { pipeline.GetPipeline(), libraryName };

	return m_psoCache[key].cpPipelineState.Get();
}