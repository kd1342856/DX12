#include "../../../Pch.h"
#include "ShaderManager.h"
#include "../ShaderCompiler/ShaderCompiler.h"
#include "../ShaderHotReload/ShaderHotReload.h"
#include "../PhotometricUnits.h"
#include "../../Renderer/AutoExposure.h"
#include "../../Renderer/VolumetricFog.h"
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
	m_PostProcessSettings = {};
	m_DebugSettings = {};
	m_FogSettings = {};

	m_SystemData = {};
	m_PostProcessData = {};

	m_LightData = {};
	m_LightData.SL_Count = 0;
	m_LightData.PL_Count = 0;
	m_LightData.AmbientLight = { 0.35f, 0.35f, 0.35f };
	m_LightData.DL_Dir = { 0.0f, -1.0f, 1.0f };
	m_LightData.DL_ShadowBias = 0.001f;
	m_LightData.DL_Color = { 0.6f, 0.6f, 0.65f };
	m_LightData.DL_ShadowPower = 2.5f;

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

		float dx = m_LightingSettings.DirectionalLightDir.x;
		float dy = m_LightingSettings.DirectionalLightDir.y;
		float dz = m_LightingSettings.DirectionalLightDir.z;
		float len = sqrtf(dx * dx + dy * dy + dz * dz);
		if (len > 0.00001f) {
			dx /= len; dy /= len; dz /= len;
		}
		m_LightData.DL_Dir = { dx, dy, dz };

		// 照度(lx)。露出はUpdateFogCBで毎フレーム掛ける
		const Math::Vector3 dlTint(m_LightingSettings.DirectionalLightColor.x, m_LightingSettings.DirectionalLightColor.y, m_LightingSettings.DirectionalLightColor.z);
		const Math::Vector3 dlColor = PhotometricUnits::MakeLightColor(dlTint, m_LightingSettings.DirectionalLightTemperature) * m_LightingSettings.DirectionalLightIntensity;
		m_baseDLColor = { dlColor.x, dlColor.y, dlColor.z };

		m_LightingSettings.IsDirty = false;
	}
}

void ShaderManager::UpdateShadowCB()
{
	if (m_ShadowSettings.IsDirty)
	{
		m_LightData.DL_ShadowPower = m_ShadowSettings.EnableShadows ? m_ShadowSettings.ShadowPower : 0.0f;
		m_LightData.DL_ShadowBias = m_ShadowSettings.ShadowBias;
		m_ShadowSettings.IsDirty = false;
	}
}

void ShaderManager::UpdatePostProcessCB()
{
	if (m_PostProcessSettings.IsDirty)
	{
		m_SystemData.EnableHDR = m_PostProcessSettings.EnableHDR ? 1 : 0;

		m_PostProcessData.EnableHDR = m_PostProcessSettings.EnableHDR ? 1 : 0;
		// 露出はライトの値に前もって掛けてある(前露出)ので、ポストでは掛けない
		m_PostProcessData.Exposure = 1.0f;
		m_PostProcessData.BloomThreshold = m_PostProcessSettings.BloomThreshold;
		m_PostProcessData.BloomIntensity = m_PostProcessSettings.BloomIntensity;
		m_PostProcessData.BlurRadius = m_PostProcessSettings.BloomRadius;

		m_PostProcessData.VignetteIntensity = m_PostProcessSettings.EnableVignette ? m_PostProcessSettings.VignetteIntensity : 0.0f;


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

	// オプション画面の明るさはIsDirtyを立てずに変わるので、毎フレーム掛け直す
	ApplyDisplayGamma();

	// フィルムグレインはISO感度に連動する(感度を上げるほど粒が荒れる。ISO 800で設定の強さ)。
	// カメラの設定はIsDirtyを立てずに変わるので、毎フレーム求める
	m_PostProcessData.FilmGrainIntensity = m_PostProcessSettings.EnableFilmGrain
		? m_PostProcessSettings.FilmGrainIntensity * std::sqrt((std::max)(m_PostProcessSettings.ISO, 1.0f) / 800.0f)
		: 0.0f;

	// HDR出力のときは、モニターの最大の輝度までトーンマップする
	{
		const GraphicsDevice& device = GraphicsDevice::Instance();
		m_PostProcessData.HDRPeakRatio = device.IsHDROutput() ? (std::max)(device.GetDisplayMaxLuminance() / device.GetPaperWhiteNits(), 1.0f) : 0.0f;
	}

	// フィルムグレインのアニメーション用に経過時間を毎フレーム進める(IsDirtyに関係なく)。
	m_PostProcessData.Time += GameTimer::Instance().DeltaTime();
}

void ShaderManager::ApplyDisplayGamma()
{
	const float gamma = m_isEditorView ? m_editorDisplayGamma : 1.0f;
	m_SystemData.Gamma = gamma;
	m_PostProcessData.Gamma = gamma;
	m_PostProcessData.BrightnessGamma = m_isEditorView ? 1.0f : m_PostProcessSettings.BrightnessGamma;
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
	m_fogExtinction = m_FogSettings.EnableFog
		? (m_FogSettings.FogDensity + (m_FogSettings.HuntFogDensity - m_FogSettings.FogDensity) * m_huntBlend)
		: 0.0f;
	// ボリュメトリックフォグを使うときは、シェーダーの中の距離フォグは掛けない(二重にならないように)
	const bool isVolumetricFog = m_FogSettings.EnableVolumetricFog && VolumetricFog::Instance().IsAvailable();
	m_LightData.DistanceFogDensity = isVolumetricFog ? 0.0f : m_fogExtinction;

	// Screen darkening: scales ambient + directional light down toward black as the hunt blend
	// rises, layered on top of whatever LightingSettings/UpdateLightingCB last computed.
	float darken = m_FogSettings.EnableHuntDarken ? (1.0f - m_FogSettings.HuntDarkenAmount * m_huntBlend) : 1.0f;
	// 前露出: 物理単位(lx・nit)の値に露出を掛けてから送る。EV100はいつでも変えられるので毎フレーム掛け直す
	const float exposure = GetExposure();
	m_SystemData.Exposure = exposure;
	const float lightScale = darken * exposure;
	m_LightData.AmbientLight = { m_baseAmbientLight.x * lightScale, m_baseAmbientLight.y * lightScale, m_baseAmbientLight.z * lightScale };
	m_LightData.DL_Color = { m_baseDLColor.x * lightScale, m_baseDLColor.y * lightScale, m_baseDLColor.z * lightScale };
	// 霧の色は、霧が散乱して返す光の輝度(nit)として扱う
	m_LightData.DistanceFogColor = { m_FogSettings.FogColor.x * exposure, m_FogSettings.FogColor.y * exposure, m_FogSettings.FogColor.z * exposure };
	m_LightData.SkyLuminance = m_LightingSettings.SkyLuminance * exposure;
	m_LightData.ContactShadowLength = m_RendererSettings.EnableContactShadows ? m_RendererSettings.ContactShadowLength : 0.0f;
}

float ShaderManager::GetManualEV100() const
{
	const PostProcessSettings& settings = m_PostProcessSettings;
	return settings.UseCameraSettings
		? PhotometricUnits::ComputeEV100(settings.Aperture, settings.ShutterSpeed, settings.ISO)
		: settings.EV100;
}

float ShaderManager::GetExposure() const
{
	const AutoExposure& autoExposure = AutoExposure::Instance();
	const float ev100 = autoExposure.IsActive() ? autoExposure.GetEV100() : GetManualEV100();
	return PhotometricUnits::ExposureFromEV100(ev100);
}

void ShaderManager::BuildProgram(ShaderProgram& program, Microsoft::WRL::ComPtr<IDxcBlob> cpVS, Microsoft::WRL::ComPtr<IDxcBlob> cpPS)
{
	program.pVS = cpVS;
	program.pPS = cpPS;
	program.vsHash = 0;
	program.psHash = 0;
	if (program.pVS)
	{
		program.vsHash = ShaderCompiler::HashBytes(program.pVS->GetBufferPointer(), program.pVS->GetBufferSize());
	}
	if (program.pPS)
	{
		program.psHash = ShaderCompiler::HashBytes(program.pPS->GetBufferPointer(), program.pPS->GetBufferSize());
	}

	auto vsBindings = ShaderReflection::Reflect(program.pVS);
	auto psBindings = ShaderReflection::Reflect(program.pPS);

	// Merge bindings
	std::vector<ShaderBinding> mergedBindings = vsBindings;
	for (const auto& pb : psBindings)
	{
		bool found = false;
		for (const auto& vb : mergedBindings)
		{
			if (vb.Type == pb.Type && vb.BindPoint == pb.BindPoint && vb.Space == pb.Space)
			{
				found = true;
				break;
			}
		}
		if (!found)
		{
			mergedBindings.push_back(pb);
		}
	}

	// Sort bindings to maintain deterministic root signature order
	std::sort(mergedBindings.begin(), mergedBindings.end(), [](const ShaderBinding& a, const ShaderBinding& b)
	{
		if (a.Type != b.Type)
		{
			return a.Type < b.Type;
		}
		return a.BindPoint < b.BindPoint;
	});

	program.Bindings = mergedBindings;

	// Build RootSignature
	std::vector<DescriptorRange> ranges;
	for (const auto& b : mergedBindings)
	{
		if (b.Type == ShaderBindingType::Sampler)
		{
			continue; // Handled as static samplers in RootSignature::Create for now
		}

		DescriptorRange range;
		if (b.Type == ShaderBindingType::CBV)
		{
			range.Type = RangeType::CBV;
		}
		else if (b.Type == ShaderBindingType::SRV)
		{
			range.Type = RangeType::SRV;
		}
		else if (b.Type == ShaderBindingType::UAV)
		{
			range.Type = RangeType::UAV;
		}

		range.Register = b.BindPoint;
		range.Count = b.BindCount;
		range.Space = b.Space;
		ranges.push_back(range);
	}

	program.pRootSignature = std::make_unique<RootSignature>();
	program.pRootSignature->Create(m_pDevice, ranges);
}

namespace
{
	// エラーの一覧やログに出すシェーダーの名前(UTF-8)
	std::string MakeShaderName(const std::wstring& path, const std::wstring& entryPoint, const std::wstring& profile)
	{
		return AppPaths::ToUtf8(std::filesystem::path(path).filename()) + " (" + AppPaths::ToUtf8(entryPoint) + ", " + AppPaths::ToUtf8(profile) + ")";
	}

	// VSとPSのコンパイル。結果をホットリロードのエラーの一覧に記録する
	bool CompileProgramStages(const ShaderProgram& program, ShaderCompileResult& outVS, ShaderCompileResult& outPS, std::vector<std::wstring>& outDependencies)
	{
		const bool isVSOk = ShaderCompiler::TryCompileShader(program.vsPath, L"main", L"vs_6_0", outVS);
		ShaderHotReload::Instance().ReportResult(MakeShaderName(program.vsPath, L"main", L"vs_6_0"), outVS, isVSOk);
		outDependencies = outVS.dependencies;

		bool isPSOk = true;
		if (!program.psPath.empty())
		{
			isPSOk = ShaderCompiler::TryCompileShader(program.psPath, L"main", program.psProfile, outPS);
			ShaderHotReload::Instance().ReportResult(MakeShaderName(program.psPath, L"main", program.psProfile), outPS, isPSOk);
			outDependencies.insert(outDependencies.end(), outPS.dependencies.begin(), outPS.dependencies.end());
		}
		return isVSOk && isPSOk;
	}

	bool HasSameBindings(const std::vector<ShaderBinding>& lhs, const std::vector<ShaderBinding>& rhs)
	{
		if (lhs.size() != rhs.size())
		{
			return false;
		}
		for (size_t bindingIndex = 0; bindingIndex < lhs.size(); ++bindingIndex)
		{
			const ShaderBinding& a = lhs[bindingIndex];
			const ShaderBinding& b = rhs[bindingIndex];
			if (a.Type != b.Type || a.BindPoint != b.BindPoint || a.BindCount != b.BindCount || a.Space != b.Space)
			{
				return false;
			}
		}
		return true;
	}
}

ShaderProgram* ShaderManager::LoadShader(const std::wstring& vsPath, const std::wstring& psPath, const std::wstring& psProfile)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	std::wstring key = vsPath + L"|" + psPath + L"|" + psProfile;
	if (m_shaderCache.find(key) != m_shaderCache.end())
	{
		return m_shaderCache[key].get();
	}

	auto program = std::make_unique<ShaderProgram>();
	program->vsPath = vsPath;
	program->psPath = psPath;
	program->psProfile = psProfile;

	ShaderCompileResult vsResult;
	ShaderCompileResult psResult;
	std::vector<std::wstring> dependencies;
	CompileProgramStages(*program, vsResult, psResult, dependencies);
	// 起動時の読み込みはこれまでどおり、コンパイルできなければ止める(検証だけの失敗はそのまま使う)
	if (!vsResult.cpBlob || (!psPath.empty() && !psResult.cpBlob))
	{
		for (const ShaderCompileResult* pResult : { &vsResult, &psResult })
		{
			if (!pResult->log.empty())
			{
				OutputDebugStringA(pResult->log.c_str());
			}
		}
		throw std::runtime_error("Shader compile failed");
	}

	{
		char msg[512];
		sprintf_s(msg, "VS size=%zu, path=%ls\n", vsResult.cpBlob->GetBufferSize(), vsPath.c_str());
		OutputDebugStringA(msg);
	}

	BuildProgram(*program, vsResult.cpBlob, psResult.cpBlob);

	ShaderProgram* pProgram = program.get();
	const std::string name = MakeShaderName(psPath.empty() ? vsPath : psPath, L"main", psPath.empty() ? L"vs_6_0" : psProfile);
	ShaderHotReload::Instance().Register(pProgram, name, dependencies, [this, pProgram]()
	{
		return ReloadProgram(pProgram);
	});

	m_shaderCache[key] = std::move(program);
	return pProgram;
}

Microsoft::WRL::ComPtr<ID3D12PipelineState> ShaderManager::CreatePipelineState(const ShaderProgram& program, const PipelineDesc& desc, bool usePS, const std::wstring& libraryName)
{
	Pipeline pipeline;
	// Always use the program's root signature
	PipelineDesc copyDesc = desc;
	copyDesc.pRootSignature = program.pRootSignature.get();
	if (usePS)
	{
		// Normal pass: bind both VS and PS
		copyDesc.pBlobs = { program.pVS.Get(), nullptr, nullptr, nullptr, program.pPS.Get() };
	}
	else
	{
		// Depth Only pass (ShadowShader etc.): set PS to nullptr -> depth-only PSO
		copyDesc.pBlobs = { program.pVS.Get(), nullptr, nullptr, nullptr, nullptr };
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
	return pipeline.GetPipeline();
}

PipelineStateRef ShaderManager::GetPipelineState(ShaderProgram* pProgram, const PipelineDesc& desc)
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
	for (auto fmt : desc.Formats)
	{
		key += std::to_wstring((uint32_t)fmt) + L",";
	}
	key += L"|";

	// Encode InputLayouts
	for (auto il : desc.InputLayouts)
	{
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
	std::wstring descKey = key;
	key = std::to_wstring((uint64_t)pProgram) + L"|" + key;

	auto it = m_psoCache.find(key);
	if (it != m_psoCache.end())
	{
		return PipelineStateRef(std::addressof(it->second.cpPipelineState));
	}

	// unordered_mapの要素は再ハッシュでも動かないので、要素のアドレスを参照として返せる
	PipelineCacheEntry& entry = m_psoCache[key];
	entry.cpPipelineState = CreatePipelineState(*pProgram, desc, bUsePS, libraryName);
	entry.libraryName = libraryName;
	entry.pProgram = pProgram;
	entry.desc = desc;
	entry.desc.pBlobs.clear(); // 使うのはbUsePSだけ。古いブロブのポインタを残さない
	entry.usePS = bUsePS;
	entry.descKey = descKey;

	// ComPtrの&は中身を解放するReleaseAndGetAddressOfなので、入れ物のアドレスはstd::addressofで取る
	return PipelineStateRef(std::addressof(entry.cpPipelineState));
}

bool ShaderManager::ReloadProgram(ShaderProgram* pProgram)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	ShaderCompileResult vsResult;
	ShaderCompileResult psResult;
	std::vector<std::wstring> dependencies;
	const bool isCompiled = CompileProgramStages(*pProgram, vsResult, psResult, dependencies);
	// 失敗しても依存するファイルは更新する(新しく足したインクルードを直したときにも作り直せるように)
	ShaderHotReload::Instance().UpdateDependencies(pProgram, dependencies);
	if (!isCompiled)
	{
		return false;
	}

	ShaderProgram newProgram;
	newProgram.vsPath = pProgram->vsPath;
	newProgram.psPath = pProgram->psPath;
	newProgram.psProfile = pProgram->psProfile;
	BuildProgram(newProgram, vsResult.cpBlob, psResult.cpBlob);
	if (!newProgram.pRootSignature || !newProgram.pRootSignature->GetRootSignature())
	{
		Logger::Instance().AddLog(Logger::LogLevel::Error, u8"[ShaderHotReload] ルートシグネチャを作れませんでした: %ls", pProgram->psPath.empty() ? pProgram->vsPath.c_str() : pProgram->psPath.c_str());
		return false;
	}
	if (!HasSameBindings(newProgram.Bindings, pProgram->Bindings))
	{
		// ルートパラメーターの番号を作成時に控えているシェーダーは、正しく描けないことがある
		Logger::Instance().AddLog(Logger::LogLevel::Warning, u8"[ShaderHotReload] リソースのバインドが変わりました。表示がおかしければ再起動してください: %ls", pProgram->psPath.empty() ? pProgram->vsPath.c_str() : pProgram->psPath.c_str());
	}

	// 先にすべてのPSOを作り、1つでも失敗したら何も差し替えない
	std::vector<std::pair<PipelineCacheEntry*, Microsoft::WRL::ComPtr<ID3D12PipelineState>>> newPipelineStates;
	std::vector<std::wstring> newLibraryNames;
	for (auto& [key, entry] : m_psoCache)
	{
		if (entry.pProgram != pProgram)
		{
			continue;
		}
		const std::wstring libraryName = ToHex(newProgram.vsHash) + L"|" + ToHex(newProgram.psHash) + L"|" + entry.descKey;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> cpPipelineState = CreatePipelineState(newProgram, entry.desc, entry.usePS, libraryName);
		if (!cpPipelineState)
		{
			Logger::Instance().AddLog(Logger::LogLevel::Error, u8"[ShaderHotReload] PSOを作れませんでした: %ls", pProgram->psPath.empty() ? pProgram->vsPath.c_str() : pProgram->psPath.c_str());
			return false;
		}
		newPipelineStates.emplace_back(&entry, cpPipelineState);
		newLibraryNames.push_back(libraryName);
	}

	// GPUは止まっているので、古いものはここで解放してよい
	*pProgram = std::move(newProgram);
	for (size_t entryIndex = 0; entryIndex < newPipelineStates.size(); ++entryIndex)
	{
		newPipelineStates[entryIndex].first->cpPipelineState = newPipelineStates[entryIndex].second;
		newPipelineStates[entryIndex].first->libraryName = newLibraryNames[entryIndex];
	}
	return true;
}
