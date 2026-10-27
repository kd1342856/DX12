#include "DebugPanel.h"
#include "../EditorContext.h"
#include "ImGui/imgui.h"
#include "../Widgets/SettingsFieldUI.h"

namespace
{
    struct LegendItem
    {
        ImVec4 color; // sRGB
        const char* label;
    };

    // 色の見本を1行に並べる。LitShader_PS.hlslのデバッグビューの色と合わせる
    void DrawLegend(const LegendItem* pItems, size_t count)
    {
        for (size_t itemIndex = 0; itemIndex < count; ++itemIndex)
        {
            ImGui::ColorButton(pItems[itemIndex].label, pItems[itemIndex].color, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker, ImVec2(14.0f, 14.0f));
            ImGui::SameLine(0.0f, 4.0f);
            ImGui::TextUnformatted(pItems[itemIndex].label);
            if (itemIndex + 1 < count)
            {
                ImGui::SameLine(0.0f, 12.0f);
            }
        }
    }

    const DebugViewItem& FindItem(DebugView view)
    {
        for (const DebugViewItem& item : DebugViewNames)
        {
            if (item.Value == view)
            {
                return item;
            }
        }
        return DebugViewNames[0];
    }
}

void DebugPanel::Draw(EditorContext& ctx)
{
    // 検索中は、検索できる値の無いパネルを隠す
    if (SettingsFieldUI::IsSearching())
    {
        return;
    }
    if (!ctx.Debug)
    {
        return;
    }

    if (ImGui::CollapsingHeader(u8"デバッグビュー###Debug Views", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const DebugViewItem& current = FindItem(ctx.Debug->CurrentDebugView);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##debugView", current.Name))
        {
            for (const DebugViewItem& item : DebugViewNames)
            {
                const bool isSelected = (ctx.Debug->CurrentDebugView == item.Value);
                if (ImGui::Selectable(item.Name, isSelected))
                {
                    ctx.Debug->CurrentDebugView = item.Value;
                    ctx.Debug->IsDirty = true;
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
                {
                    ImGui::BeginTooltip();
                    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28.0f);
                    ImGui::TextUnformatted(item.Description);
                    ImGui::PopTextWrapPos();
                    ImGui::EndTooltip();
                }
                if (isSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("%s", current.Description);
        ImGui::PopTextWrapPos();
        if (current.isRawOutput)
        {
            ImGui::TextDisabled(u8"値をそのまま表示している(露出・トーンマッピング・霧・SSR・モーションブラーは掛けない)");
        }
        if (current.Value != DebugView::Final)
        {
            ImGui::TextDisabled(u8"スキニングのモデルと空は、デバッグビューに対応していない(ふつうの表示のまま)");
        }

        // 色の意味の見本
        if (current.Value == DebugView::LuminanceFalseColor)
        {
            static const LegendItem kItems[] =
            {
                { ImVec4(0.0f, 0.0f, 0.0f, 1.0f), u8"-6段より暗い" }, { ImVec4(0.25f, 0.0f, 0.5f, 1.0f), u8"-6〜-4" },
                { ImVec4(0.0f, 0.2f, 0.9f, 1.0f), u8"-4〜-2" }, { ImVec4(0.0f, 0.7f, 0.9f, 1.0f), u8"-2〜-1" },
            };
            static const LegendItem kItems2[] =
            {
                { ImVec4(0.2f, 0.8f, 0.2f, 1.0f), u8"適正(±1段)" }, { ImVec4(0.95f, 0.9f, 0.1f, 1.0f), u8"+1〜+2" },
                { ImVec4(1.0f, 0.5f, 0.0f, 1.0f), u8"+2〜+4" }, { ImVec4(1.0f, 0.0f, 0.0f, 1.0f), u8"+4段より明るい(白飛び)" },
            };
            DrawLegend(kItems, std::size(kItems));
            DrawLegend(kItems2, std::size(kItems2));
        }
        else if (current.Value == DebugView::AlbedoCheck)
        {
            static const LegendItem kItems[] =
            {
                { ImVec4(0.6f, 0.6f, 0.6f, 1.0f), u8"範囲内(輝度で表示)" }, { ImVec4(0.0f, 0.3f, 1.0f, 1.0f), u8"暗すぎる" },
                { ImVec4(1.0f, 0.1f, 0.1f, 1.0f), u8"明るすぎる" }, { ImVec4(0.8f, 0.0f, 0.8f, 1.0f), u8"暗すぎる金属" },
            };
            DrawLegend(kItems, std::size(kItems));
        }
        else if (current.Value == DebugView::MipLevel)
        {
            static const LegendItem kItems[] =
            {
                { ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "0" }, { ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "1" }, { ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "2" },
                { ImVec4(0.0f, 0.9f, 0.0f, 1.0f), "3" }, { ImVec4(0.0f, 0.9f, 0.9f, 1.0f), "4" }, { ImVec4(0.0f, 0.3f, 1.0f, 1.0f), "5" },
                { ImVec4(0.6f, 0.0f, 1.0f, 1.0f), u8"6以上" },
            };
            DrawLegend(kItems, std::size(kItems));
        }
    }

    // 反射RTの生確認は「Window > Preview Camera」に移した(PreviewCameraWindow.cpp)。
    // CameraDataにDebug Preview Cameraフラグを立てたカメラのTransformを動かして
    // 好きな角度から見られるので、エディタのフリーカメラの動きに引きずられる問題が無い。
}
