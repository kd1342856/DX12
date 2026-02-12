#pragma once

// =============================================
// ECS集約ヘッダー
// =============================================

// ECSコア
#include "Entity/Entity.h"
#include "Component/Component.h"
#include "CompSystem/System.h"
#include "ECSCoordinator.h"

// コンポーネント定義
#include "Component/Components/TransformComponent.h"
#include "Component/Components/ModelComponent.h"
#include "Component/Components/CameraComponent.h"
#include "Component/Components/ShaderComponent.h"

// システム定義
#include "CompSystem/Systems/RenderSystem.h"

// ファクトリ
#include "Factory/EntityFactory.h"