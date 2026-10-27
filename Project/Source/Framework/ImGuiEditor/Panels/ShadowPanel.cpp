#include "ShadowPanel.h"
#include "../EditorContext.h"
#include "ImGui/imgui.h"
#include "../Widgets/SettingsFieldUI.h"

void ShadowPanel::Draw(EditorContext& ctx)
{
    if (!ctx.Shadow)
    {
        return;
    }
    if (ImGui::CollapsingHeader(u8"ŒŽ‚Ì‰e###Shadow Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        SettingsFieldUI::Draw(*ctx.Shadow, "Shadow");
    }
}
