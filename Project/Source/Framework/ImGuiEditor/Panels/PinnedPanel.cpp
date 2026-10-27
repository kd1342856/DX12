#include "PinnedPanel.h"
#include "../EditorContext.h"
#include "ImGui/imgui.h"
#include "../Widgets/SettingsFieldUI.h"
#include "../EditorPreferences.h"

namespace
{
    // ピン留めの対象(PostProcessなど)から設定を探して1行描く。見つからなければfalse
    bool DrawPinnedField(EditorContext& ctx, const EditorPreferences::Pin& pin, const std::function<void()>& extraMenu)
    {
        const char* pTarget = pin.target.c_str();
        if (pin.target == "Renderer" && ctx.Renderer)
        {
            return SettingsFieldUI::DrawSingle(*ctx.Renderer, pTarget, pin.key, extraMenu);
        }
        if (pin.target == "Lighting" && ctx.Lighting)
        {
            return SettingsFieldUI::DrawSingle(*ctx.Lighting, pTarget, pin.key, extraMenu);
        }
        if (pin.target == "Shadow" && ctx.Shadow)
        {
            return SettingsFieldUI::DrawSingle(*ctx.Shadow, pTarget, pin.key, extraMenu);
        }
        if (pin.target == "PostProcess" && ctx.PostProcess)
        {
            return SettingsFieldUI::DrawSingle(*ctx.PostProcess, pTarget, pin.key, extraMenu);
        }
        if (pin.target == "SSAO" && ctx.SSAO)
        {
            return SettingsFieldUI::DrawSingle(*ctx.SSAO, pTarget, pin.key, extraMenu);
        }
        if (pin.target == "Fog" && ctx.Fog)
        {
            return SettingsFieldUI::DrawSingle(*ctx.Fog, pTarget, pin.key, extraMenu);
        }
        return false;
    }
}

void PinnedPanel::Draw(EditorContext& ctx)
{
    // メニューから並べ替え・外すと一覧が変わるので、写しを回す
    const std::vector<EditorPreferences::Pin> pins = EditorPreferences::GetPins();
    if (pins.empty() || SettingsFieldUI::IsSearching())
    {
        return;
    }
    if (!ImGui::CollapsingHeader(u8"よく使う値###Pinned", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }
    ImGui::PushID("Pinned");
    if (FieldWidgets::BeginFieldTable("##pinned"))
    {
        for (size_t pinIndex = 0; pinIndex < pins.size(); ++pinIndex)
        {
            const EditorPreferences::Pin pin = pins[pinIndex];
            ImGui::PushID(static_cast<int>(pinIndex));
            const std::function<void()> extraMenu = [pin, pinIndex, count = pins.size()]()
            {
                if (ImGui::MenuItem(u8"上へ", nullptr, false, pinIndex > 0))
                {
                    EditorPreferences::MovePin(pin.target, pin.key, -1);
                }
                if (ImGui::MenuItem(u8"下へ", nullptr, false, pinIndex + 1 < count))
                {
                    EditorPreferences::MovePin(pin.target, pin.key, 1);
                }
            };
            if (!DrawPinnedField(ctx, pin, extraMenu))
            {
                // 値の名前が変わったなどで見つからない。外せるようにだけしておく
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextDisabled(u8"見つからない値: %s / %s", pin.target.c_str(), pin.key.c_str());
                ImGui::TableSetColumnIndex(1);
                if (ImGui::SmallButton(u8"外す"))
                {
                    EditorPreferences::TogglePin(pin.target, pin.key);
                }
            }
            ImGui::PopID();
        }
        FieldWidgets::EndFieldTable();
    }
    ImGui::PopID();
}
