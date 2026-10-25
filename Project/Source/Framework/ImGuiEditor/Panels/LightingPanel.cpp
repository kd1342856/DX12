#include "LightingPanel.h"
#include "../EditorContext.h"
#include "../../../../Library/ImGui/imgui.h"
#include <cmath>

void LightingPanel::Draw(EditorContext& ctx)
{
    if (!ctx.Lighting) return;

    if (ImGui::CollapsingHeader("Lighting Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        bool changed = false;

        ImGui::Text("Directional Light");
        
        changed |= ImGui::DragFloat3("Direction", &ctx.Lighting->DirectionalLightDir.x, 0.01f, -1.0f, 1.0f);
        if (changed)
        {
            float length = std::sqrt(
                ctx.Lighting->DirectionalLightDir.x * ctx.Lighting->DirectionalLightDir.x +
                ctx.Lighting->DirectionalLightDir.y * ctx.Lighting->DirectionalLightDir.y +
                ctx.Lighting->DirectionalLightDir.z * ctx.Lighting->DirectionalLightDir.z
            );
            if (length > 0.0001f) {
                ctx.Lighting->DirectionalLightDir.x /= length;
                ctx.Lighting->DirectionalLightDir.y /= length;
                ctx.Lighting->DirectionalLightDir.z /= length;
            }
        }

        changed |= ImGui::ColorEdit3("Color", &ctx.Lighting->DirectionalLightColor.x);
        changed |= ImGui::DragFloat("Intensity", &ctx.Lighting->DirectionalLightIntensity, 0.01f, 0.0f, 100.0f);

        if (changed)
        {
            ctx.Lighting->IsDirty = true;
        }
    }
}
