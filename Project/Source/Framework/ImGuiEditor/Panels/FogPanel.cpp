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
    if (ImGui::CollapsingHeader(u8"霧とハントの演出###Fog Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::TextDisabled(ShaderManager::Instance().IsHuntFogActive() ? u8"今の状態: ハント中" : u8"今の状態: ふだん");
        SettingsFieldUI::Draw(*ctx.Fog, "Fog");
    }
}
