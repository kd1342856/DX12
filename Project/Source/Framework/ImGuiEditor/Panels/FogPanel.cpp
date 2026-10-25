#include "FogPanel.h"
#include "../EditorContext.h"
#include "../../../../Library/ImGui/imgui.h"
#include "../../../Graphics/Shader/ShaderManager/ShaderManager.h"

void FogPanel::Draw(EditorContext& ctx)
{
    if (!ctx.Fog) return;

    if (ImGui::CollapsingHeader("Fog Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        bool changed = false;

        ImGui::Text("Distance Fog");
        changed |= ImGui::Checkbox("Enable Fog", &ctx.Fog->EnableFog);
        changed |= ImGui::ColorEdit3("Fog Color", &ctx.Fog->FogColor.x);
        changed |= ImGui::DragFloat("Density (Normal)", &ctx.Fog->FogDensity, 0.001f, 0.0f, 1.0f, "%.3f");
        changed |= ImGui::DragFloat("Density (Hunt)", &ctx.Fog->HuntFogDensity, 0.001f, 0.0f, 1.0f, "%.3f");

        ImGui::Separator();
        ImGui::Text("Hunt Screen Darkening");
        changed |= ImGui::Checkbox("Enable Darkening", &ctx.Fog->EnableHuntDarken);
        changed |= ImGui::SliderFloat("Darken Amount", &ctx.Fog->HuntDarkenAmount, 0.0f, 1.0f, "%.2f");

        ImGui::Separator();
        changed |= ImGui::DragFloat("Hunt Transition Speed", &ctx.Fog->HuntTransitionSpeed, 0.01f, 0.01f, 10.0f, "%.2f");
        ImGui::TextDisabled("(fraction/sec; 1.0 = full ramp in ~1s)");

        ImGui::TextDisabled(ShaderManager::Instance().IsHuntFogActive() ? "State: Hunting" : "State: Normal");

        if (changed) ctx.Fog->IsDirty = true;
    }
}
