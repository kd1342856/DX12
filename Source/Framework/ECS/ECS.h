#pragma once

// =============================================
// ECS
// =============================================

// ECS
#include "Entity/Entity.h"
#include "ComponentManager.h"
#include "CompSystem/System.h"
#include "ECSCoordinator.h"

// �R���|�[�l���g�f�[�^��`�iRenderSystem.h���O�ɃC���N���[�h�j
#include "Components/Data/TransformData.h"
#include "Components/Data/ModelRenderData.h"
#include "Components/Data/CameraData.h"
#include "Components/Data/ShaderData.h"
#include "Components/Data/AnimationData.h"
#include "Components/Data/SpriteData.h"
#include "Components/Data/ColliderData.h"
#include "Components/Data/NativeScriptData.h"
#include "Components/Data/PointLightData.h"

#include "CompSystem/Systems/RenderSystem.h"