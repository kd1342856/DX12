#include "IBLPanel.h"
#include "../EditorContext.h"
#include "../../../../Library/ImGui/imgui.h"

void IBLPanel::Draw(EditorContext& ctx)
{
    if (!ctx.IBL || !ctx.Lighting) return;

    if (ImGui::CollapsingHeader("IBL Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        bool iChanged = false;
        bool lChanged = false;

        iChanged |= ImGui::Checkbox("Enable IBL", &ctx.IBL->EnableIBL);
        iChanged |= ImGui::DragFloat("IBL Intensity", &ctx.IBL->IBLIntensity, 0.01f, 0.0f, 10.0f);
        
        lChanged |= ImGui::ColorEdit3("Ambient Light", &ctx.Lighting->AmbientLight.x);

        if (iChanged) ctx.IBL->IsDirty = true;
        if (lChanged) ctx.Lighting->IsDirty = true;
    }
}
