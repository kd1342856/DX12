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
            if (ImGui::SliderFloat("Bloom Radius", &ctx.PostProcess->BloomRadius, 1.0f, 24.0f, "%.1f")) changed = true;
            if (ImGui::SliderInt("Bloom Iterations", &ctx.PostProcess->BloomIterations, 1, 8)) changed = true;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("半径・反復回数を上げるほど広く柔らかく漏れる。既定値で変化が薄い時はまずここを疑う。");

            ImGui::Separator();
            ImGui::Text("Vignette");
            if (ImGui::Checkbox("Enable Vignette", &ctx.PostProcess->EnableVignette)) changed = true;
            if (ImGui::SliderFloat("Vignette Intensity", &ctx.PostProcess->VignetteIntensity, 0.0f, 3.0f, "%.2f")) changed = true;
            if (ImGui::SliderFloat("Vignette Smoothness", &ctx.PostProcess->VignetteSmoothness, 0.0f, 1.0f, "%.2f")) changed = true;

            ImGui::Separator();
            ImGui::Text("Film Grain");
            if (ImGui::Checkbox("Enable Film Grain", &ctx.PostProcess->EnableFilmGrain)) changed = true;
            if (ImGui::SliderFloat("Film Grain Intensity", &ctx.PostProcess->FilmGrainIntensity, 0.0f, 0.3f, "%.3f")) changed = true;

            ImGui::Separator();
            ImGui::Text("Chromatic Aberration");
            if (ImGui::Checkbox("Enable Chromatic Aberration", &ctx.PostProcess->EnableChromaticAberration)) changed = true;
            if (ImGui::SliderFloat("Chromatic Aberration Intensity", &ctx.PostProcess->ChromaticAberrationIntensity, 0.0f, 3.0f, "%.2f")) changed = true;

            ImGui::Separator();
            ImGui::Text("Depth of Field");
            if (ImGui::Checkbox("Enable DOF", &ctx.PostProcess->EnableDOF)) changed = true;
            if (ImGui::SliderFloat("DOF Focus Distance", &ctx.PostProcess->DOFFocusDistance, 0.1f, 50.0f, "%.2f")) changed = true;
            if (ImGui::SliderFloat("DOF Focus Range", &ctx.PostProcess->DOFFocusRange, 0.1f, 30.0f, "%.2f")) changed = true;

            ImGui::Separator();
            ImGui::Text("God Rays");
            if (ImGui::Checkbox("Enable God Rays", &ctx.PostProcess->EnableGodRays)) changed = true;
            if (ImGui::SliderFloat("God Rays Density", &ctx.PostProcess->GodRaysDensity, 0.1f, 1.5f, "%.2f")) changed = true;
            if (ImGui::SliderFloat("God Rays Decay", &ctx.PostProcess->GodRaysDecay, 0.8f, 0.999f, "%.3f")) changed = true;
            if (ImGui::SliderFloat("God Rays Weight", &ctx.PostProcess->GodRaysWeight, 0.0f, 2.0f, "%.2f")) changed = true;
            if (ImGui::SliderFloat("God Rays Exposure", &ctx.PostProcess->GodRaysExposure, 0.0f, 3.0f, "%.2f")) changed = true;
            if (ImGui::SliderFloat("God Rays Intensity", &ctx.PostProcess->GodRaysIntensity, 0.0f, 3.0f, "%.2f")) changed = true;
            if (ImGui::SliderInt("God Rays Samples", &ctx.PostProcess->GodRaysNumSamples, 8, 96)) changed = true;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("平行光の方向とカメラ行列から光源のスクリーン位置を推定し、Bloom結果をラジアルブラーする近似。光源がカメラの後ろにある時は自動的に無効化される。");

            if (changed) {
                ctx.PostProcess->IsDirty = true;
            }
        }
    }
}
