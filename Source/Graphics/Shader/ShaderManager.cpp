#include "ShaderManager.h"

void ShaderManager::Init()
{
m_standardShader.Create(&GDF::Instance().GetGraphicsDevice());
}

void ShaderManager::SetCameraMatrix(const Math::Matrix& mView, const Math::Matrix& mProj)
{
CBufferData::Camera cbCamera;
cbCamera.mView = mView;
cbCamera.mProj = mProj;
GDF::Instance().BindCBuffer(0, cbCamera);
}