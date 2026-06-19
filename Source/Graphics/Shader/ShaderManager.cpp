#include "ShaderManager.h"

void ShaderManager::Init()
{
	m_standardShader.Create(&GDF::Instance().GetGraphicsDevice());
	m_litShader.Create(&GDF::Instance().GetGraphicsDevice());
	m_shadowShader.Create(&GDF::Instance().GetGraphicsDevice());
	m_postProcessShader.Create(&GDF::Instance().GetGraphicsDevice());
	m_skinningShader.Create(&GDF::Instance().GetGraphicsDevice());
}

void ShaderManager::SetCameraMatrix(const Math::Matrix& mView, const Math::Matrix& mProj)
{
	m_mView = mView;
	m_mProj = mProj;
}

void ShaderManager::BindCameraMatrix(int slot)
{
	CBufferData::Camera cCamera;
	cCamera.mView = m_mView;
	cCamera.mInvV = m_mView.Invert();
	cCamera.mProj = m_mProj;
	cCamera.mInvP = m_mProj.Invert();
	cCamera.mVP = m_mView * m_mProj;
	cCamera.mInvVP = cCamera.mVP.Invert();
	cCamera.CamPos = cCamera.mInvV.Translation();
	cCamera.dummy = 0.0f;

	GDF::Instance().BindCBuffer(slot, cCamera);
}
ShaderManager& ShaderManager::Instance()
{
    static ShaderManager instance;
    return instance;
}
