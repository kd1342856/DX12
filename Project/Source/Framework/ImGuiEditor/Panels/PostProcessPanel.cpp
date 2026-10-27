#include "PostProcessPanel.h"
#include "../EditorContext.h"
#include "ImGui/imgui.h"
#include "SettingsWidgets.h"
#include "../EditorPreferences.h"
#include "../../../Graphics/Shader/ShaderManager/ShaderManager.h"
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
            SettingsWidgets::DefaultMark(*ctx.PostProcess, ctx.PostProcess->EnableHDR);
            // 露出は前露出としてライトに掛けるので、IsDirtyを待たずに毎フレーム反映される
            ImGui::Checkbox("Use Camera Settings", &ctx.PostProcess->UseCameraSettings);
            SettingsWidgets::DefaultMark(*ctx.PostProcess, ctx.PostProcess->UseCameraSettings);
            if (ctx.PostProcess->UseCameraSettings)
            {
                ImGui::SliderFloat("Aperture (f)", &ctx.PostProcess->Aperture, 1.0f, 22.0f, "f/%.1f");
                SettingsWidgets::DefaultMark(*ctx.PostProcess, ctx.PostProcess->Aperture);
                float shutterDenominator = 1.0f / (std::max)(ctx.PostProcess->ShutterSpeed, 1e-4f);
                if (ImGui::SliderFloat("Shutter (1/x s)", &shutterDenominator, 1.0f, 4000.0f, "1/%.0f", ImGuiSliderFlags_Logarithmic))
                {
                    ctx.PostProcess->ShutterSpeed = 1.0f / shutterDenominator;
                }
                SettingsWidgets::DefaultMark(*ctx.PostProcess, ctx.PostProcess->ShutterSpeed);
                ImGui::SliderFloat("ISO", &ctx.PostProcess->ISO, 50.0f, 102400.0f, "%.0f", ImGuiSliderFlags_Logarithmic);
                SettingsWidgets::DefaultMark(*ctx.PostProcess, ctx.PostProcess->ISO);
                ImGui::Text("EV100 = %.2f", PhotometricUnits::ComputeEV100(ctx.PostProcess->Aperture, ctx.PostProcess->ShutterSpeed, ctx.PostProcess->ISO));
            }
            else
            {
                ImGui::SliderFloat("EV100", &ctx.PostProcess->EV100, -6.0f, 16.0f, "%.2f");
                SettingsWidgets::DefaultMark(*ctx.PostProcess, ctx.PostProcess->EV100);
            }
            {
                // エディターのカメラだけに掛ける表示ガンマ。シーンには保存せず、エディターの設定に保存する
                float displayGamma = ShaderManager::Instance().GetEditorDisplayGamma();
                if (ImGui::SliderFloat(u8"エディターの表示ガンマ", &displayGamma, 1.0f, 3.0f, "%.2f"))
                {
                    ShaderManager::Instance().SetEditorDisplayGamma(displayGamma);
                }
                if (ImGui::IsItemDeactivatedAfterEdit())
                {
                    EditorPreferences::Save();
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip(u8"エディターのカメラで描くときだけ暗いところを持ち上げる。ゲームの画面には効かない\n"
                        u8"(ゲームはオプション画面の明るさだけ)。シーンには保存せず、Saved/EditorSettings.json に保存する。");
                }
            }

            ImGui::Separator();
            ImGui::Text("Auto Exposure");
            // 自動露出もCPU側だけで使う設定なので、毎フレーム反映される
            ImGui::Checkbox("Enable Auto Exposure", &ctx.PostProcess->EnableAutoExposure);
            SettingsWidgets::DefaultMark(*ctx.PostProcess, ctx.PostProcess->EnableAutoExposure);
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
                SettingsWidgets::DefaultMark(*ctx.PostProcess, ctx.PostProcess->ExposureCompensation);
                ImGui::DragFloatRange2("EV100 Range", &ctx.PostProcess->AutoExposureMinEV100, &ctx.PostProcess->AutoExposureMaxEV100, 0.1f, -10.0f, 16.0f, "min %.1f", "max %.1f");
                SettingsWidgets::DefaultMark(*ctx.PostProcess, ctx.PostProcess->AutoExposureMinEV100, ctx.PostProcess->AutoExposureMaxEV100);
                ImGui::DragFloatRange2("Histogram %", &ctx.PostProcess->AutoExposureLowPercent, &ctx.PostProcess->AutoExposureHighPercent, 0.5f, 0.0f, 100.0f, "low %.0f", "high %.0f");
                SettingsWidgets::DefaultMark(*ctx.PostProcess, ctx.PostProcess->AutoExposureLowPercent, ctx.PostProcess->AutoExposureHighPercent);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"暗い方から数えてこの範囲の画素だけで平均の明るさを求める。真っ暗な影や光源そのものに露出が引っ張られないようにする。");
                ImGui::SliderFloat("Speed Up", &ctx.PostProcess->AutoExposureSpeedUp, 0.1f, 10.0f, "%.1f");
                SettingsWidgets::DefaultMark(*ctx.PostProcess, ctx.PostProcess->AutoExposureSpeedUp);
                ImGui::SliderFloat("Speed Down", &ctx.PostProcess->AutoExposureSpeedDown, 0.1f, 10.0f, "%.1f");
                SettingsWidgets::DefaultMark(*ctx.PostProcess, ctx.PostProcess->AutoExposureSpeedDown);
            }

            ImGui::Separator();
            ImGui::Text("Anti-Aliasing");
            // TAAはCPU側だけで使う設定なので、IsDirtyを待たずに毎フレーム反映される
            ImGui::Checkbox("Enable TAA", &ctx.PostProcess->EnableTAA);
            SettingsWidgets::DefaultMark(*ctx.PostProcess, ctx.PostProcess->EnableTAA);
            if (!TemporalAA::Instance().IsAvailable())
            {
                ImGui::SameLine();
                ImGui::TextDisabled(u8"(このGPUでは使えません)");
            }
            ImGui::SliderFloat("TAA Current Weight", &ctx.PostProcess->TAACurrentWeight, 0.03f, 0.5f, "%.2f");
            SettingsWidgets::DefaultMark(*ctx.PostProcess, ctx.PostProcess->TAACurrentWeight);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"今のフレームを混ぜる割合。小さいほど滑らかでちらつかないが、動いたときに残像が出やすい。");

            ImGui::Separator();
            ImGui::Text("Motion Blur");
            ImGui::Checkbox("Enable Motion Blur", &ctx.PostProcess->EnableMotionBlur);
            SettingsWidgets::DefaultMark(*ctx.PostProcess, ctx.PostProcess->EnableMotionBlur);
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
                SettingsWidgets::DefaultMark(*ctx.PostProcess, ctx.PostProcess->MotionBlurShutterAngle);
            }
            ImGui::SliderFloat("Max Blur (px)", &ctx.PostProcess->MotionBlurMaxPixels, 4.0f, 128.0f, "%.0f");
            SettingsWidgets::DefaultMark(*ctx.PostProcess, ctx.PostProcess->MotionBlurMaxPixels);

            ImGui::Separator();
            ImGui::Text("Bloom");
            if (ImGui::SliderFloat("Bloom Threshold", &ctx.PostProcess->BloomThreshold, 0.0f, 5.0f, "%.2f")) changed = true;
            SettingsWidgets::DefaultMark(*ctx.PostProcess, ctx.PostProcess->BloomThreshold);
            if (ImGui::SliderFloat("Bloom Intensity", &ctx.PostProcess->BloomIntensity, 0.0f, 3.0f, "%.2f")) changed = true;
            SettingsWidgets::DefaultMark(*ctx.PostProcess, ctx.PostProcess->BloomIntensity);
            if (ImGui::SliderFloat("Bloom Radius", &ctx.PostProcess->BloomRadius, 1.0f, 24.0f, "%.1f")) changed = true;
            SettingsWidgets::DefaultMark(*ctx.PostProcess, ctx.PostProcess->BloomRadius);
            if (ImGui::SliderInt("Bloom Iterations", &ctx.PostProcess->BloomIterations, 1, 8)) changed = true;
            SettingsWidgets::DefaultMark(*ctx.PostProcess, ctx.PostProcess->BloomIterations);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"半径・反復回数を上げるほど広く柔らかく漏れる。既定値で変化が薄い時はまずここを疑う。");

            ImGui::Separator();
            ImGui::Text("Vignette");
            if (ImGui::Checkbox("Enable Vignette", &ctx.PostProcess->EnableVignette)) changed = true;
            SettingsWidgets::DefaultMark(*ctx.PostProcess, ctx.PostProcess->EnableVignette);
            if (ImGui::SliderFloat("Vignette Intensity", &ctx.PostProcess->VignetteIntensity, 0.0f, 3.0f, "%.2f")) changed = true;
            SettingsWidgets::DefaultMark(*ctx.PostProcess, ctx.PostProcess->VignetteIntensity);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"レンズの自然な周辺減光(cos^4則)。画角から求め、1で物理どおり。");

            ImGui::Separator();
            ImGui::Text("Film Grain");
            if (ImGui::Checkbox("Enable Film Grain", &ctx.PostProcess->EnableFilmGrain)) changed = true;
            SettingsWidgets::DefaultMark(*ctx.PostProcess, ctx.PostProcess->EnableFilmGrain);
            if (ImGui::SliderFloat("Film Grain Intensity", &ctx.PostProcess->FilmGrainIntensity, 0.0f, 0.3f, "%.3f")) changed = true;
            SettingsWidgets::DefaultMark(*ctx.PostProcess, ctx.PostProcess->FilmGrainIntensity);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"ISO 800での強さ。ISO感度(Use Camera SettingsのISO)の平方根に比例して強くなり、暗いところほど粒の量は小さい。");

            ImGui::Separator();
            ImGui::Text("Chromatic Aberration");
            if (ImGui::Checkbox("Enable Chromatic Aberration", &ctx.PostProcess->EnableChromaticAberration)) changed = true;
            SettingsWidgets::DefaultMark(*ctx.PostProcess, ctx.PostProcess->EnableChromaticAberration);
            if (ImGui::SliderFloat("Chromatic Aberration Intensity", &ctx.PostProcess->ChromaticAberrationIntensity, 0.0f, 3.0f, "%.2f")) changed = true;
            SettingsWidgets::DefaultMark(*ctx.PostProcess, ctx.PostProcess->ChromaticAberrationIntensity);

            ImGui::Separator();
            ImGui::Text("Depth of Field");
            if (ImGui::Checkbox("Enable DOF", &ctx.PostProcess->EnableDOF)) changed = true;
            SettingsWidgets::DefaultMark(*ctx.PostProcess, ctx.PostProcess->EnableDOF);
            if (ImGui::SliderFloat("DOF Focus Distance", &ctx.PostProcess->DOFFocusDistance, 0.1f, 50.0f, "%.2f")) changed = true;
            SettingsWidgets::DefaultMark(*ctx.PostProcess, ctx.PostProcess->DOFFocusDistance);
            if (ImGui::SliderFloat("DOF Focus Range", &ctx.PostProcess->DOFFocusRange, 0.1f, 30.0f, "%.2f")) changed = true;
            SettingsWidgets::DefaultMark(*ctx.PostProcess, ctx.PostProcess->DOFFocusRange);

            ImGui::Separator();
            ImGui::Text("God Rays");
            if (ImGui::Checkbox("Enable God Rays", &ctx.PostProcess->EnableGodRays)) changed = true;
            SettingsWidgets::DefaultMark(*ctx.PostProcess, ctx.PostProcess->EnableGodRays);
            if (ImGui::SliderFloat("God Rays Density", &ctx.PostProcess->GodRaysDensity, 0.1f, 1.5f, "%.2f")) changed = true;
            SettingsWidgets::DefaultMark(*ctx.PostProcess, ctx.PostProcess->GodRaysDensity);
            if (ImGui::SliderFloat("God Rays Decay", &ctx.PostProcess->GodRaysDecay, 0.8f, 0.999f, "%.3f")) changed = true;
            SettingsWidgets::DefaultMark(*ctx.PostProcess, ctx.PostProcess->GodRaysDecay);
            if (ImGui::SliderFloat("God Rays Weight", &ctx.PostProcess->GodRaysWeight, 0.0f, 2.0f, "%.2f")) changed = true;
            SettingsWidgets::DefaultMark(*ctx.PostProcess, ctx.PostProcess->GodRaysWeight);
            if (ImGui::SliderFloat("God Rays Exposure", &ctx.PostProcess->GodRaysExposure, 0.0f, 3.0f, "%.2f")) changed = true;
            SettingsWidgets::DefaultMark(*ctx.PostProcess, ctx.PostProcess->GodRaysExposure);
            if (ImGui::SliderFloat("God Rays Intensity", &ctx.PostProcess->GodRaysIntensity, 0.0f, 3.0f, "%.2f")) changed = true;
            SettingsWidgets::DefaultMark(*ctx.PostProcess, ctx.PostProcess->GodRaysIntensity);
            if (ImGui::SliderInt("God Rays Samples", &ctx.PostProcess->GodRaysNumSamples, 8, 96)) changed = true;
            SettingsWidgets::DefaultMark(*ctx.PostProcess, ctx.PostProcess->GodRaysNumSamples);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"平行光の方向とカメラ行列から光源のスクリーン位置を推定し、Bloom結果をラジアルブラーする近似。光源がカメラの後ろにある時は自動的に無効化される。");

            if (changed) {
                ctx.PostProcess->IsDirty = true;
            }
        }
    }
}
