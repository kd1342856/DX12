#include "ShadowPanel.h"
#include "../EditorContext.h"
#include "../../../../Library/ImGui/imgui.h"

void ShadowPanel::Draw(EditorContext& ctx)
{
    if (!ctx.Shadow) return;

    if (ImGui::CollapsingHeader("Shadow Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        bool sChanged = false;

        sChanged |= ImGui::Checkbox("Enable Shadows", &ctx.Shadow->EnableShadows);
        
        sChanged |= ImGui::DragFloat("Shadow Power", &ctx.Shadow->ShadowPower, 0.01f, 0.0f, 10.0f);
        sChanged |= ImGui::DragFloat("Shadow Bias", &ctx.Shadow->ShadowBias, 0.0001f, 0.0f, 0.01f, "%.4f");
        
        ImGui::Text("Cascaded Shadow Maps");
        sChanged |= ImGui::DragFloat4("Cascade Splits", &ctx.Shadow->CascadeSplits.x, 0.01f, 0.01f, 1.0f);

        if (sChanged) ctx.Shadow->IsDirty = true;
    }
}
