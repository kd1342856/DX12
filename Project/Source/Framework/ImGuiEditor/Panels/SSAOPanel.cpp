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
    if (ImGui::CollapsingHeader(u8"‚·‚«ŠÔ‚Ì‰A‚è###SSAO Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        SettingsFieldUI::Draw(*ctx.SSAO, "SSAO");
    }
}
