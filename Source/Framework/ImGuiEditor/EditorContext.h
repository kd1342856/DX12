#pragma once
#include "../../Graphics/Shader/ShaderSettings.h"

class GameObject;

struct EditorContext
{
    RendererSettings*    Renderer = nullptr;
    LightingSettings*    Lighting = nullptr;
    ShadowSettings*      Shadow = nullptr;
    IBLSettings*         IBL = nullptr;
    PostProcessSettings* PostProcess = nullptr;
    DebugSettings*       Debug = nullptr;
    FogSettings*         Fog = nullptr;
    
    GameObject*          SelectedObject = nullptr;
};
