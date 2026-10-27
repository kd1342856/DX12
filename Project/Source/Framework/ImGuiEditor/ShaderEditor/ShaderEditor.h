#pragma once
#include <vector>
#include <memory>
#include "../IEditorPanel.h"
#include "../EditorContext.h"
#include "../Panels/DebugPanel.h"
#include "../Panels/LightingPanel.h"
#include "../Panels/ShadowPanel.h"
#include "../Panels/SSAOPanel.h"
#include "../Panels/MaterialPanel.h"
#include "../Panels/RendererPanel.h"
#include "../Panels/PostProcessPanel.h"
#include "../Panels/FogPanel.h"
#include "../Panels/ShaderReloadPanel.h"
#include "../Panels/ShaderSettingsPanel.h"

class ShaderEditor
{
public:
    void Initialize();
    void Draw(EditorContext& ctx);
private:
    std::vector<std::unique_ptr<IEditorPanel>> m_Panels;
};
