#include "../../Pch.h"
#include "RenderContext.h"
#include "../GDF/GDF.h"
#include "../Shader/ShaderManager/ShaderManager.h"

void RenderContext::BindCamera(int slot)
{
	CBufferData::Camera cbCamera;
	cbCamera.mView = View;
	cbCamera.mInvV = View.Invert();
	cbCamera.mProj = Projection;
	// Projectionは非可逆になりうる特殊な行列ではない(通常の透視/正射影)前提でInvertを使う。
	// mInvV/mInvP/mVP/mInvVPは以前ここで一切設定されておらず、Math::Matrixの既定コンストラクタ
	// (単位行列)のまま送られていた - g_mInvPを使うシェーダー側の処理(深度のリニア化等、
	// 血痕デカールのフェードやSSAO/SSRの視点空間復元)が総じて誤った変換をしていた原因。
	cbCamera.mInvP = Projection.Invert();
	cbCamera.mVP = View * Projection;
	cbCamera.mInvVP = cbCamera.mVP.Invert();
	cbCamera.CamPos = Math::Vector3::Transform(Math::Vector3::Zero, cbCamera.mInvV);
	cbCamera.dummy = 0.0f;
	GraphicsDevice::Instance().GetFrameConstantBufferAllocator()->BindCachedData(slot, cbCamera, m_cameraCache);
}

void RenderContext::BindLight(int slot)
{
	auto* pAllocator = GraphicsDevice::Instance().GetFrameConstantBufferAllocator();
	if (IsMainView)
	{
		pAllocator->BindCachedData(slot, ShaderManager::Instance().GetLightData(), m_lightCache);
		return;
	}
	// 画面の深度を使う効果を切る
	CBufferData::Light lightData = ShaderManager::Instance().GetLightData();
	lightData.ContactShadowLength = 0.0f;
	pAllocator->BindCachedData(slot, lightData, m_lightCache);
}

void RenderContext::BindSystem(int slot)
{
	GraphicsDevice::Instance().GetFrameConstantBufferAllocator()->BindCachedData(slot, ShaderManager::Instance().GetSystemData(), m_systemCache);
}
