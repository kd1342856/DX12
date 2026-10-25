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
        lChanged |= ImGui::SliderFloat("Indirect Shadow Floor", &ctx.Lighting->IndirectShadowFloor, 0.0f, 1.0f,
            "%.2f");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Ambient/IBL kept in areas the moon/sun shadow doesn't reach.\n0 = pitch black indoors, 1 = no darkening (old behavior).");
        }

        lChanged |= ImGui::Checkbox("Indoor Mask (RoomArea)", &ctx.Lighting->EnableIndoorMask);
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(u8"RoomAreaの範囲内は外の光(月明かり・環境光・空の反射光)を受けず、\n月の影の中と同じ扱い(Indirect Shadow Floorまで暗くなる)にする。");
        }
        lChanged |= ImGui::SliderFloat("Indoor Margin", &ctx.Lighting->IndoorMargin, 0.0f, 2.0f, "%.2f m");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(u8"RoomAreaの範囲をこの分だけ広げて判定する(境界上の壁・床も室内扱いにするため)。");
        }

        if (iChanged) ctx.IBL->IsDirty = true;
        if (lChanged) ctx.Lighting->IsDirty = true;
    }

    if (ctx.SSAO && ImGui::CollapsingHeader("SSAO Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Enable SSAO", &ctx.SSAO->EnableSSAO);
        ImGui::SliderFloat("SSAO Radius", &ctx.SSAO->Radius, 0.05f, 3.0f, "%.2f");
        ImGui::SliderFloat("SSAO Bias", &ctx.SSAO->Bias, 0.0f, 0.3f, "%.3f");
        ImGui::SliderFloat("SSAO Power", &ctx.SSAO->Power, 0.2f, 4.0f, "%.2f");
        ImGui::SliderFloat("SSAO Intensity", &ctx.SSAO->Intensity, 0.0f, 2.0f, "%.2f");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(u8"画面空間で近くのジオメトリ同士の隙間を暗くする近似(環境光にのみ影響)。\nSkinned/Skyメッシュはこの計算には含まれない(既知の制限)。");
        }
    }
}
