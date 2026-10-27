#include "SSAOPanel.h"
#include "../EditorContext.h"
#include "ImGui/imgui.h"
#include "../Widgets/SettingsFieldUI.h"

void SSAOPanel::Draw(EditorContext& ctx)
{
    if (!ctx.SSAO)
    {
        return;
    }
    if (SettingsFieldUI::PanelHeader(u8"‚·‚«ŠÔ‚Ì‰A‚è###SSAO Settings", *ctx.SSAO, "SSAO"))
    {
        SettingsFieldUI::Draw(*ctx.SSAO, "SSAO");
    }
}
