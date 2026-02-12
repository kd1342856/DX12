#pragma once

// =============================================
// EntityFactory
// Entity生成を一元管理するファクトリクラス
// =============================================
class EntityFactory
{
public:
EntityFactory(ECSCoordinator& coordinator)
: m_rCoordinator(coordinator) {}

// カメラEntity生成
Entity CreateCamera(
const Math::Vector3& position,
float fov,
float aspect,
float nearZ = 0.01f,
float farZ = 1000.0f);

// モデル付きEntity生成(Transform + Model + Shader)
Entity CreateModelEntity(
const std::string& modelPath,
const Math::Vector3& position,
std::shared_ptr<ShaderManager> spShader,
const RenderingSetting& setting);

private:
ECSCoordinator& m_rCoordinator;
};