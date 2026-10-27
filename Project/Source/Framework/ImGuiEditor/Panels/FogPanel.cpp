#include "FogPanel.h"
#include "../EditorContext.h"
#include "ImGui/imgui.h"
#include "../Widgets/SettingsFieldUI.h"
#include "../../../Graphics/Shader/ShaderManager/ShaderManager.h"

void FogPanel::Draw(EditorContext& ctx)
{
    if (!ctx.Fog)
    {
        return;
    }
    if (SettingsFieldUI::PanelHeader(u8"霧とハントの演出###Fog Settings", *ctx.Fog, "Fog"))
    {
        if (!SettingsFieldUI::IsSearching())
        {
            ImGui::TextDisabled(ShaderManager::Instance().IsHuntFogActive() ? u8"今の状態: ハント中" : u8"今の状態: ふだん");
        }
        SettingsFieldUI::Draw(*ctx.Fog, "Fog");
    }
}
