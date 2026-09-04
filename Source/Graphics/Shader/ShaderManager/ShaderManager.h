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
#include "../ShaderSettings.h"
#include "../../GPUResource/CBufferData/CBufferData.h"

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

	void InitializeShaderSettings();
	void UpdateConstantBuffers();

	RendererSettings& GetRendererSettings() { return m_RendererSettings; }
	LightingSettings& GetLightingSettings() { return m_LightingSettings; }
	ShadowSettings& GetShadowSettings() { return m_ShadowSettings; }
	IBLSettings& GetIBLSettings() { return m_IBLSettings; }
	PostProcessSettings& GetPostProcessSettings() { return m_PostProcessSettings; }
	SSAOSettings& GetSSAOSettings() { return m_SSAOSettings; }
	DebugSettings& GetDebugSettings() { return m_DebugSettings; }
	FogSettings& GetFogSettings() { return m_FogSettings; }

	const CBufferData::System& GetSystemData() const { return m_SystemData; }
	const CBufferData::PostProcess& GetPostProcessData() const { return m_PostProcessData; }
	const CBufferData::Light& GetLightData() const { return m_LightData; }
	// LightSystemがポイントライト配列(PL/PL_Count)を毎フレーム書き込むための可変アクセス。
	CBufferData::Light& GetMutableLightData() { return m_LightData; }

	void SetDirectionalLightMatrix(const Math::Matrix& m) { m_LightData.DL_mLightVP[0] = m; }

	// 最も近いポイントライト(g_PL[0])の簡易シャドウ(単一パースペクティブ)用データ。
	// enabled=falseの時はシェーダー側で影計算そのものをスキップする。
	void SetPointLightShadowData(const Math::Matrix& vp, float bias, bool enabled)
	{
		m_LightData.PL0_ShadowVP = vp;
		m_LightData.PL0_ShadowBias = bias;
		m_LightData.PL0_ShadowEnabled = enabled ? 1 : 0;
	}

	// Increments/decrements the count of ghosts currently hunting. While the count is > 0
	// the fog density and screen darkening both blend toward their "hunt" values (a counter,
	// not a bool, so overlapping hunts from multiple ghosts are still tracked correctly).
	void SetHuntActive(bool active)
	{
		m_huntActiveCount += active ? 1 : -1;
		if (m_huntActiveCount < 0) m_huntActiveCount = 0;
	}
	bool IsHuntFogActive() const { return m_huntActiveCount > 0; }

	// ���ʔ���(���K���X��)�p: ���˃J������View*Proj�s��ƗL���t���O��ݒ肷��B
	// �K���X�̃s�N�Z���V�F�[�_�[�͂���Ń��[���h���W���ē��e���A
	// g_planarReflectionMap�̐�����UV�����߂�B
	void SetReflectionData(const Math::Matrix& viewProj, bool hasReflection)
	{
		m_SystemData.mReflectionVP = viewProj;
		m_SystemData.HasReflection = hasReflection ? 1 : 0;
	}

	// DOFの深度リニア化に使うアクティブカメラのNear/Far。RenderScene呼び出し側(GameScene)が
	// 毎フレーム、実際に描画に使ったカメラの値で更新する。
	void SetCameraNearFar(float nearZ, float farZ)
	{
		m_PostProcessData.CameraNear = nearZ;
		m_PostProcessData.CameraFar = farZ;
	}

	// God Raysの光源スクリーン座標。GameScene側で平行光の方向とカメラ行列から毎フレーム計算する。
	// isValid=false(光源がカメラの後ろ等)の時はGodRays設定が有効でも今フレームは描画しない。
	// EnableGodRaysもここで確定させる(UpdateConstantBuffers()はDoPostProcessより前に走るため、
	// IsDirty待ちにすると1フレーム遅れてしまう)。
	void SetGodRaysLightScreenPos(float u, float v, bool isValid)
	{
		m_PostProcessData.GodRaysLightU = u;
		m_PostProcessData.GodRaysLightV = v;
		m_PostProcessData.EnableGodRays = (m_PostProcessSettings.EnableGodRays && isValid) ? 1 : 0;
	}

	ShaderProgram* LoadShader(const std::wstring& vsPath, const std::wstring& psPath);
	ID3D12PipelineState* GetPipelineState(ShaderProgram* pProgram, const PipelineDesc& desc);

private:
	GraphicsDevice* m_pDevice = nullptr;
	std::unordered_map<std::wstring, std::unique_ptr<ShaderProgram>> m_shaderCache;
	std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID3D12PipelineState>> m_psoCache;
	std::mutex m_mutex;

	void UpdateRendererCB();
	void UpdateLightingCB();
	void UpdateShadowCB();
	void UpdateIBLCB();
	void UpdatePostProcessCB();
	void UpdateDebugCB();
	void UpdateFogCB();

	RendererSettings m_RendererSettings;
	LightingSettings m_LightingSettings;
	ShadowSettings m_ShadowSettings;
	IBLSettings m_IBLSettings;
	PostProcessSettings m_PostProcessSettings;
	SSAOSettings m_SSAOSettings;
	DebugSettings m_DebugSettings;
	FogSettings m_FogSettings;

	CBufferData::System m_SystemData;
	CBufferData::PostProcess m_PostProcessData;
	CBufferData::Light m_LightData;

	int m_huntActiveCount = 0;
	float m_huntBlend = 0.0f; // 0 = normal, 1 = full hunt; smoothed toward its target each frame

	// Lighting values as set by LightingSettings, before the hunt-darken multiplier is applied.
	// UpdateFogCB reapplies the multiplier on top of these every frame, so darkening never has to
	// fight with (or get clobbered by) UpdateLightingCB's own IsDirty-gated writes to m_LightData.
	Math::Vector3 m_baseAmbientLight = { 0.35f, 0.35f, 0.35f };
	Math::Vector3 m_baseDLColor = { 0.6f, 0.6f, 0.65f };
};
