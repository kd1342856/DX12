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
    if (SettingsFieldUI::PanelHeader(u8"ŒŽ‚Ì‰e###Shadow Settings", *ctx.Shadow, "Shadow"))
    {
        SettingsFieldUI::Draw(*ctx.Shadow, "Shadow");
    }
}
