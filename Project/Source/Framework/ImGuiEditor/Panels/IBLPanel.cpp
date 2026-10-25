#include "IBLPanel.h"
#include "../EditorContext.h"
#include "ImGui/imgui.h"

void IBLPanel::Draw(EditorContext& ctx)
{
    if (!ctx.IBL || !ctx.Lighting) return;

    if (ImGui::CollapsingHeader("IBL Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        bool iChanged = false;
        bool lChanged = false;

        // ダミーIBL(空と地面の2色)と、影の中に残す間接光の下限(Indirect Shadow Floor)は、
        // Step 5でDDGIと反射プローブに置き換えたため、もう使っていない
        ImGui::TextDisabled(u8"拡散の環境光はDDGI、鏡面は反射プローブ(ダミーIBLは廃止)");

        lChanged |= ImGui::DragFloat3("Ambient Luminance (nit)", &ctx.Lighting->AmbientLight.x, 0.001f, 0.0f, 1000.0f, "%.3f");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(u8"DDGIのボリューム(家)の外で使う、一様な空の明るさ。拡散と鏡面の両方に使う。");
        }

        lChanged |= ImGui::Checkbox("Indoor Mask (RoomArea)", &ctx.Lighting->EnableIndoorMask);
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(u8"RoomAreaの範囲内には直射の月光が届かないものとして扱う\n(屋根のシャドウマップが届かないときの漏れを防ぐ)。間接光はDDGIが求める。");
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
