#include "ShadowPanel.h"
#include "../EditorContext.h"
#include "ImGui/imgui.h"
#include "SettingsWidgets.h"

void ShadowPanel::Draw(EditorContext& ctx)
{
    if (!ctx.Shadow) return;

    if (ImGui::CollapsingHeader("Shadow Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        bool sChanged = false;

        sChanged |= ImGui::Checkbox("Enable Shadows", &ctx.Shadow->EnableShadows);
        SettingsWidgets::DefaultMark(*ctx.Shadow, ctx.Shadow->EnableShadows);
        
        sChanged |= ImGui::DragFloat("Shadow Power", &ctx.Shadow->ShadowPower, 0.01f, 0.0f, 10.0f);
        SettingsWidgets::DefaultMark(*ctx.Shadow, ctx.Shadow->ShadowPower);
        sChanged |= ImGui::DragFloat("Shadow Bias", &ctx.Shadow->ShadowBias, 0.0001f, 0.0f, 0.01f, "%.4f");
        SettingsWidgets::DefaultMark(*ctx.Shadow, ctx.Shadow->ShadowBias);
        
        if (sChanged) ctx.Shadow->IsDirty = true;
    }
}
