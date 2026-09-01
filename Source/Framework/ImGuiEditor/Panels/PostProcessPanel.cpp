#include "PostProcessPanel.h"
#include "../EditorContext.h"
#include "../../../../Library/ImGui/imgui.h"

void PostProcessPanel::Draw(EditorContext& ctx)
{
    if (ImGui::CollapsingHeader("Post Process Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ctx.PostProcess)
        {
            bool changed = false;

            if (ImGui::Checkbox("Enable HDR (ACES Filmic ToneMapping)", &ctx.PostProcess->EnableHDR)) changed = true;
            if (ImGui::SliderFloat("Exposure", &ctx.PostProcess->Exposure, 0.1f, 10.0f, "%.2f")) changed = true;
            if (ImGui::SliderFloat("Gamma", &ctx.PostProcess->Gamma, 1.0f, 3.0f, "%.2f")) changed = true;

            ImGui::Separator();
            ImGui::Text("Bloom");
            if (ImGui::SliderFloat("Bloom Threshold", &ctx.PostProcess->BloomThreshold, 0.0f, 5.0f, "%.2f")) changed = true;
            if (ImGui::SliderFloat("Bloom Intensity", &ctx.PostProcess->BloomIntensity, 0.0f, 3.0f, "%.2f")) changed = true;

            if (changed) {
                ctx.PostProcess->IsDirty = true;
            }
        }
    }
}
