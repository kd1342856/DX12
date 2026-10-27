#include "LightingPanel.h"
#include "../EditorContext.h"
#include "ImGui/imgui.h"
#include "../Widgets/SettingsFieldUI.h"

void LightingPanel::Draw(EditorContext& ctx)
{
    if (!ctx.Lighting)
    {
        return;
    }
    if (ImGui::CollapsingHeader(u8"月の光と空###Lighting Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::TextDisabled(u8"拡散の環境光はDDGI、鏡面は反射プローブが求める。ここは月・空・家の外の光");
        SettingsFieldUI::Draw(*ctx.Lighting, "Lighting");
    }
}
