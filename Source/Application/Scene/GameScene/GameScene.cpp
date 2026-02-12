#include "GameScene.h"

void GameScene::Init()
{
// ƒ‚ƒfƒ‹“Ç‚İ‚İ
m_spModel = std::make_shared<ModelData>();
m_spModel->Load("Asset/Model/Cube/Cube.gltf");

// ‰ŠúˆÊ’u
m_mWorld = Math::Matrix::Identity;
m_mWorld2 = Math::Matrix::CreateTranslation(0, 0, 1);
}

void GameScene::Update()
{
// ƒJƒƒ‰İ’è
Math::Matrix mView = Math::Matrix::CreateTranslation(0, 0, 3);
Math::Matrix mProj = DirectX::XMMatrixPerspectiveFovLH(
DirectX::XMConvertToRadians(60.0f), 1280.0f / 720.0f, 0.01f, 1000.0f);
ShaderManager::Instance().SetCameraMatrix(mView, mProj);

// Cube1(‰ñ“])
m_mWorld *= Math::Matrix::CreateRotationY(0.001f);
ShaderManager::Instance().m_standardShader.DrawModel(*m_spModel, m_mWorld);

// Cube2(ŒÅ’è)
ShaderManager::Instance().m_standardShader.DrawModel(*m_spModel, m_mWorld2);
}