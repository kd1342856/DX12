#include "ShaderManager.h"

void ShaderManager::Init()
{
	m_standardShader.Create(&GDF::Instance().GetGraphicsDevice());
	m_litShader.Create(&GDF::Instance().GetGraphicsDevice());
	m_postProcessShader.Create(&GDF::Instance().GetGraphicsDevice());
}

void ShaderManager::SetCameraMatrix(const Math::Matrix& mView, const Math::Matrix& mProj)
{
	m_standardShader.Begin();

	CBufferData::Camera cbCamera;
	cbCamera.mView = mView;
	cbCamera.mInvV = mView.Invert();
	cbCamera.mProj = mProj;
	cbCamera.mInvP = mProj.Invert();
	cbCamera.mVP = mView * mProj;
	cbCamera.mInvVP = cbCamera.mVP.Invert();
	cbCamera.CamPos = cbCamera.mInvV.Translation();
	cbCamera.dummy = 0.0f;
	
	GDF::Instance().BindCBuffer(0, cbCamera);
}