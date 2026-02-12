#include "EntityFactory.h"

Entity EntityFactory::CreateCamera(
const Math::Vector3& position,
float fov,
float aspect,
float nearZ,
float farZ)
{
Entity cameraEntity = m_rCoordinator.CreateEntity();

CameraComponent cCamera;
cCamera.m_fov = fov;
cCamera.m_nearZ = nearZ;
cCamera.m_farZ = farZ;
cCamera.m_viewMatrix = Math::Matrix::CreateTranslation(position);
cCamera.m_projMatrix = DirectX::XMMatrixPerspectiveFovLH(
DirectX::XMConvertToRadians(fov), aspect, nearZ, farZ);
m_rCoordinator.AddComponent(cameraEntity, cCamera);

return cameraEntity;
}

Entity EntityFactory::CreateModelEntity(
const std::string& modelPath,
const Math::Vector3& position,
std::shared_ptr<ShaderManager> spShader,
const RenderingSetting& setting)
{
Entity entity = m_rCoordinator.CreateEntity();

// Transform
TransformComponent cTransform;
cTransform.m_position = position;
cTransform.m_worldMatrix = Math::Matrix::CreateTranslation(position);
m_rCoordinator.AddComponent(entity, cTransform);

// Model
ModelComponent cModel;
cModel.m_spModelData = std::make_shared<ModelData>();
cModel.m_spModelData->Load(modelPath);
cModel.m_filePath = modelPath;
m_rCoordinator.AddComponent(entity, cModel);

// Shader
ShaderComponent cShaderComp;
cShaderComp.m_spShader = spShader;
cShaderComp.m_renderingSetting = setting;
m_rCoordinator.AddComponent(entity, cShaderComp);

return entity;
}