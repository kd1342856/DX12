#include "PostProcessPanel.h"
#include "../EditorContext.h"
#include "ImGui/imgui.h"
#include "../../../Graphics/Shader/PhotometricUnits.h"
#include "../../../Graphics/Renderer/TemporalAA.h"
#include "../../../Graphics/Renderer/AutoExposure.h"
#include "../../../Graphics/Renderer/MotionBlur.h"

void PostProcessPanel::Draw(EditorContext& ctx)
{
    if (ImGui::CollapsingHeader("Post Process Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ctx.PostProcess)
        {
            bool changed = false;

            if (ImGui::Checkbox("Enable HDR (ACES Filmic ToneMapping)", &ctx.PostProcess->EnableHDR)) changed = true;
            // 露出は前露出としてライトに掛けるので、IsDirtyを待たずに毎フレーム反映される
            ImGui::Checkbox("Use Camera Settings", &ctx.PostProcess->UseCameraSettings);
            if (ctx.PostProcess->UseCameraSettings)
            {
                ImGui::SliderFloat("Aperture (f)", &ctx.PostProcess->Aperture, 1.0f, 22.0f, "f/%.1f");
                float shutterDenominator = 1.0f / (std::max)(ctx.PostProcess->ShutterSpeed, 1e-4f);
                if (ImGui::SliderFloat("Shutter (1/x s)", &shutterDenominator, 1.0f, 4000.0f, "1/%.0f", ImGuiSliderFlags_Logarithmic))
                {
                    ctx.PostProcess->ShutterSpeed = 1.0f / shutterDenominator;
                }
                ImGui::SliderFloat("ISO", &ctx.PostProcess->ISO, 50.0f, 102400.0f, "%.0f", ImGuiSliderFlags_Logarithmic);
                ImGui::Text("EV100 = %.2f", PhotometricUnits::ComputeEV100(ctx.PostProcess->Aperture, ctx.PostProcess->ShutterSpeed, ctx.PostProcess->ISO));
            }
            else
            {
                ImGui::SliderFloat("EV100", &ctx.PostProcess->EV100, -6.0f, 16.0f, "%.2f");
            }
            if (ImGui::SliderFloat("Gamma", &ctx.PostProcess->Gamma, 1.0f, 3.0f, "%.2f")) changed = true;

            ImGui::Separator();
            ImGui::Text("Auto Exposure");
            // 自動露出もCPU側だけで使う設定なので、毎フレーム反映される
            ImGui::Checkbox("Enable Auto Exposure", &ctx.PostProcess->EnableAutoExposure);
            if (!AutoExposure::Instance().IsAvailable())
            {
                ImGui::SameLine();
                ImGui::TextDisabled(u8"(このGPUでは使えません)");
            }
            if (ctx.PostProcess->EnableAutoExposure)
            {
                const AutoExposure& autoExposure = AutoExposure::Instance();
                ImGui::Text("EV100 = %.2f (target %.2f)", autoExposure.GetEV100(), autoExposure.GetTargetEV100());
                ImGui::SliderFloat("Exposure Compensation", &ctx.PostProcess->ExposureCompensation, -4.0f, 4.0f, "%+.2f EV");
                ImGui::DragFloatRange2("EV100 Range", &ctx.PostProcess->AutoExposureMinEV100, &ctx.PostProcess->AutoExposureMaxEV100, 0.1f, -10.0f, 16.0f, "min %.1f", "max %.1f");
                ImGui::DragFloatRange2("Histogram %", &ctx.PostProcess->AutoExposureLowPercent, &ctx.PostProcess->AutoExposureHighPercent, 0.5f, 0.0f, 100.0f, "low %.0f", "high %.0f");
                if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"暗い方から数えてこの範囲の画素だけで平均の明るさを求める。真っ暗な影や光源そのものに露出が引っ張られないようにする。");
                ImGui::SliderFloat("Speed Up", &ctx.PostProcess->AutoExposureSpeedUp, 0.1f, 10.0f, "%.1f");
                ImGui::SliderFloat("Speed Down", &ctx.PostProcess->AutoExposureSpeedDown, 0.1f, 10.0f, "%.1f");
            }

            ImGui::Separator();
            ImGui::Text("Anti-Aliasing");
            // TAAはCPU側だけで使う設定なので、IsDirtyを待たずに毎フレーム反映される
            ImGui::Checkbox("Enable TAA", &ctx.PostProcess->EnableTAA);
            if (!TemporalAA::Instance().IsAvailable())
            {
                ImGui::SameLine();
                ImGui::TextDisabled(u8"(このGPUでは使えません)");
            }
            ImGui::SliderFloat("TAA Current Weight", &ctx.PostProcess->TAACurrentWeight, 0.03f, 0.5f, "%.2f");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"今のフレームを混ぜる割合。小さいほど滑らかでちらつかないが、動いたときに残像が出やすい。");

            ImGui::Separator();
            ImGui::Text("Motion Blur");
            ImGui::Checkbox("Enable Motion Blur", &ctx.PostProcess->EnableMotionBlur);
            if (!MotionBlur::Instance().IsAvailable())
            {
                ImGui::SameLine();
                ImGui::TextDisabled(u8"(このGPUでは使えません)");
            }
            if (ctx.PostProcess->UseCameraSettings)
            {
                ImGui::TextDisabled(u8"ぼけの長さはカメラのシャッター速度で決まる");
            }
            else
            {
                ImGui::SliderFloat("Shutter Angle", &ctx.PostProcess->MotionBlurShutterAngle, 0.0f, 360.0f, "%.0f deg");
            }
            ImGui::SliderFloat("Max Blur (px)", &ctx.PostProcess->MotionBlurMaxPixels, 4.0f, 128.0f, "%.0f");

            ImGui::Separator();
            ImGui::Text("Bloom");
            if (ImGui::SliderFloat("Bloom Threshold", &ctx.PostProcess->BloomThreshold, 0.0f, 5.0f, "%.2f")) changed = true;
            if (ImGui::SliderFloat("Bloom Intensity", &ctx.PostProcess->BloomIntensity, 0.0f, 3.0f, "%.2f")) changed = true;
            if (ImGui::SliderFloat("Bloom Radius", &ctx.PostProcess->BloomRadius, 1.0f, 24.0f, "%.1f")) changed = true;
            if (ImGui::SliderInt("Bloom Iterations", &ctx.PostProcess->BloomIterations, 1, 8)) changed = true;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"半径・反復回数を上げるほど広く柔らかく漏れる。既定値で変化が薄い時はまずここを疑う。");

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
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"平行光の方向とカメラ行列から光源のスクリーン位置を推定し、Bloom結果をラジアルブラーする近似。光源がカメラの後ろにある時は自動的に無効化される。");

            if (changed) {
                ctx.PostProcess->IsDirty = true;
            }
        }
    }
}
