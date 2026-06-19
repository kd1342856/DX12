#pragma once
#include "RootSignature/RootSignature.h"
#include "Pipeline/Pipeline.h"
#include "StandardShader/StandardShader.h"
#include "LitShader/LitShader.h"
#include "ShadowShader/ShadowShader.h"
#include "PostProcessShader/PostProcessShader.h"
#include "SkinningShader/SkinningShader.h"

struct RenderingSetting
{
	std::vector<InputLayout> InputLayouts;
	std::vector<DXGI_FORMAT> Formats;
	CullMode CullMode = CullMode::Back;
	BlendMode BlendMode = BlendMode::None;
	PrimitiveTopologyType PrimitiveTopologyType = PrimitiveTopologyType::Triangle;
	bool IsDepth = true;
	bool IsDepthMask = true;
	int RTVCount = 1;
	bool IsWireFrame = false;
};

class ShaderManager
{
public:
	void Init();
	void SetCameraMatrix(const Math::Matrix& mView, const Math::Matrix& mProj);
	void BindCameraMatrix(int slot = 0);

	void SetLightData(const CBufferData::Light& lightData)
	{
		m_cbLight = lightData;
	}

	void BindLightData(int slot)
	{
		GDF::Instance().BindCBuffer(slot, m_cbLight);
	}

	StandardShader m_standardShader;
	LitShader m_litShader;
	ShadowShader m_shadowShader;
	PostProcessShader m_postProcessShader;
	SkinningShader m_skinningShader;

private:
	ShaderManager() = default;
	~ShaderManager() = default;

	// カメラのビュー行列と射影行列を保持するメンバー変数
	Math::Matrix m_mView;
	Math::Matrix m_mProj;
	CBufferData::Light m_cbLight;

public:
	static ShaderManager& Instance();
};
