#pragma once
#include "../ECS.h"
#include "../../Manager/GameManager.h"
#include "../../../Graphics/Buffer/CBufferAllocator/CBufferData/CBufferData.h"
#include "RenderSystem.h" // for setting cbLight or we just let RenderSystem pull from somewhere?

// Actually, it's better to just pass LightSystem's collected lights to RenderSystem
