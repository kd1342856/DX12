#include "ShaderSettingsPanel.h"
#include "../EditorContext.h"
#include "ImGui/imgui.h"
#include "../Widgets/FieldWidgets.h"
#include "../Editor/Editor.h"
#include "../EditHistory.h"
#include "../EditorTools.h"
#include "../Widgets/SettingsFieldUI.h"
#include "../../../Graphics/Shader/ShaderSettingsIO/ShaderSettingsIO.h"

namespace
{
    const ImVec4 kErrorColor(1.0f, 0.45f, 0.4f, 1.0f);
    const ImVec4 kOkColor(0.55f, 0.85f, 0.55f, 1.0f);
    const char* const kSlotNames[2] = { "A", "B" };

    // 変えた値の一覧に出すグループの名前(パネルの見出しと同じ)
    const char* GetGroupDisplayName(const std::string& group)
    {
        if (group == "Renderer")
        {
            return u8"描画";
        }
        if (group == "Lighting")
        {
            return u8"月の光と空";
        }
        if (group == "Shadow")
        {
            return u8"月の影";
        }
        if (group == "PostProcess")
        {
            return u8"露出とポストプロセス";
        }
        if (group == "SSAO")
        {
            return u8"すき間の陰り";
        }
        if (group == "Fog")
        {
            return u8"霧とハントの演出";
        }
        return group.c_str();
    }
}

void ShaderSettingsPanel::Draw(EditorContext& ctx)
{
    // 検索中は、値のパネルだけを出す
    if (SettingsFieldUI::IsSearching())
    {
        return;
    }
    if (!ImGui::CollapsingHeader(u8"設定の保存・プリセット・A/B比較", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }
    // Undo/Redo。ショートカットは EditHistory::Update が受ける
    EditHistory& history = EditHistory::Instance();
    ImGui::BeginDisabled(!history.CanUndo());
    if (ImGui::Button(u8"元に戻す(Ctrl+Z)"))
    {
        history.Undo();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!history.CanRedo());
    if (ImGui::Button(u8"やり直す(Ctrl+Y)"))
    {
        history.Redo();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    // エディターでは見えたのにゲームでは真っ暗、を防ぐ。F4を押している間も同じ
    bool isGameLook = GameLookPreview::IsToggled();
    if (ImGui::Checkbox(u8"ゲームと同じ見え方", &isGameLook))
    {
        GameLookPreview::SetToggled(isGameLook);
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
    {
        ImGui::SetTooltip(u8"エディターの表示ガンマを外し、ゲームと同じガンマとプレイヤーの明るさで表示する。\nF4を押している間だけ切り替えることもできる。");
    }

    const int unsavedCount = ShaderSettingsIO::CountUnsavedChanges();
    if (unsavedCount > 0)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.4f, 1.0f), u8"シーンに保存していない変更: %d 個", unsavedCount);
    }

    DrawSceneAndDefaults();
    DrawChangedList();
    ImGui::Separator();
    DrawPresets();
    ImGui::Separator();
    DrawABCompare();

    if (!m_message.empty())
    {
        ImGui::TextColored(m_isMessageError ? kErrorColor : kOkColor, "%s", m_message.c_str());
    }
}

void ShaderSettingsPanel::DrawSceneAndDefaults()
{
    const int modifiedCount = ShaderSettingsIO::CountDifferences(ShaderSettingsIO::Capture(), ShaderSettingsIO::CaptureDefaults());
    ImGui::Text(u8"初期値から変えた値: %d 個", modifiedCount);
    ImGui::SameLine();
    ImGui::BeginDisabled(modifiedCount == 0);
    if (ImGui::SmallButton(u8"すべて初期値に戻す"))
    {
        ImGui::OpenPopup("##ResetAllSettings");
    }
    ImGui::EndDisabled();
    if (ImGui::BeginPopupModal("##ResetAllSettings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text(u8"%d 個の値を初期値に戻します。", modifiedCount);
        ImGui::TextDisabled(u8"今の値を残したいときは、先にプリセットかA/Bに記録してください。");
        if (ImGui::Button(u8"初期値に戻す"))
        {
            const nlohmann::json before = ShaderSettingsIO::Capture();
            ShaderSettingsIO::Apply(ShaderSettingsIO::CaptureDefaults());
            EditHistory::Instance().PushSettingsDiff(u8"すべて初期値に戻す", before, ShaderSettingsIO::Capture());
            SetMessage(u8"すべて初期値に戻しました", false);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(u8"やめる"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // 印の見本
    ImGui::TextDisabled(u8"左に");
    ImGui::SameLine(0.0f, 2.0f);
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(FieldWidgets::GetModifiedColor()), "|");
    ImGui::SameLine(0.0f, 2.0f);
    ImGui::TextDisabled(u8"の印がある値は初期値から変えた値。右クリックで初期値に戻せる");

    const std::string& scenePath = Editor::GetScenePath();
    ImGui::Text(u8"シーン: %s", scenePath.empty() ? u8"(保存先なし)" : scenePath.c_str());
    ImGui::SameLine();
    ImGui::BeginDisabled(scenePath.empty());
    if (ImGui::SmallButton(u8"シーンに保存"))
    {
        if (Editor::SaveCurrentScene())
        {
            SetMessage(u8"シーンに保存しました: " + scenePath, false);
        }
        else
        {
            SetMessage(u8"シーンを保存できませんでした(ログを見てください)", true);
        }
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
        ImGui::SetTooltip(u8"File > Save Scene と同じ。ライト・影・霧・ポストプロセスなどの値もシーンのJSONに入り、\nシーンを開くと読み込まれる。画質設定とオプション画面が決める値(SSR・SSAO・TAAなど)は入らない。");
    }
}

void ShaderSettingsPanel::DrawChangedList()
{
    // 初期値から変えた値を、パネルをまたいで1つのリストにする。クリックでその値へ飛び、行ごとに初期値に戻せる
    const nlohmann::json current = ShaderSettingsIO::Capture();
    const nlohmann::json defaults = ShaderSettingsIO::CaptureDefaults();
    if (ShaderSettingsIO::CountDifferences(current, defaults) == 0 || !ImGui::TreeNodeEx(u8"変えた値の一覧", ImGuiTreeNodeFlags_SpanAvailWidth))
    {
        return;
    }
    if (ImGui::BeginTable("##changedList", 3, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("name", ImGuiTableColumnFlags_WidthStretch, 0.45f);
        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch, 0.45f);
        ImGui::TableSetupColumn("reset", ImGuiTableColumnFlags_WidthFixed);
        for (auto groupIt = current.begin(); groupIt != current.end(); ++groupIt)
        {
            const std::string group = groupIt.key();
            const nlohmann::json& defaultGroup = defaults[group];
            for (auto fieldIt = groupIt->begin(); fieldIt != groupIt->end(); ++fieldIt)
            {
                const std::string key = fieldIt.key();
                if (!defaultGroup.contains(key) || defaultGroup[key] == *fieldIt)
                {
                    continue;
                }
                const std::string label = ShaderSettingsIO::GetFieldLabel(group, key);
                ImGui::PushID((group + "/" + key).c_str());
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                const std::string rowText = std::string(GetGroupDisplayName(group)) + u8" ・ " + label;
                if (ImGui::Selectable(rowText.c_str(), false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap))
                {
                    SettingsFieldUI::RequestFocus(group, key);
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
                {
                    ImGui::SetTooltip(u8"クリックでその値まで飛ぶ");
                }
                ImGui::TableSetColumnIndex(1);
                ImGui::TextDisabled("%s", EditHistory::FormatJson(defaultGroup[key]).c_str());
                ImGui::SameLine();
                ImGui::Text(u8"→ %s", EditHistory::FormatJson(*fieldIt).c_str());
                ImGui::TableSetColumnIndex(2);
                if (ImGui::SmallButton(u8"初期値に戻す"))
                {
                    const nlohmann::json before = *fieldIt;
                    ShaderSettingsIO::ApplyField(group, key, defaultGroup[key]);
                    EditHistory::Instance().PushChange({ group, key, label, before, defaultGroup[key] });
                }
                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }
    ImGui::TreePop();
}

void ShaderSettingsPanel::DrawPresets()
{
    ImGui::TextUnformatted(u8"プリセット (Asset/Data/ShaderPreset)");

    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::BeginCombo("##Preset", m_selectedPreset.empty() ? u8"(選ぶ)" : m_selectedPreset.c_str()))
    {
        const std::vector<std::string> presets = ShaderSettingsIO::ListPresets();
        if (presets.empty())
        {
            ImGui::TextDisabled(u8"まだありません");
        }
        for (const std::string& name : presets)
        {
            if (ImGui::Selectable(name.c_str(), name == m_selectedPreset))
            {
                m_selectedPreset = name;
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            {
                const std::string description = ShaderSettingsIO::GetPresetDescription(name);
                if (!description.empty())
                {
                    ImGui::BeginTooltip();
                    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28.0f);
                    ImGui::TextUnformatted(description.c_str());
                    ImGui::PopTextWrapPos();
                    ImGui::EndTooltip();
                }
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(m_selectedPreset.empty());
    if (ImGui::Button(u8"読み込む"))
    {
        // 保存していない変更があれば、読み込む前に聞く(読み込み自体はUndoで戻せる)
        const std::string presetName = m_selectedPreset;
        UnsavedChanges::Confirm(u8"プリセットを読み込む", [this, presetName]()
        {
            std::string error;
            const nlohmann::json before = ShaderSettingsIO::Capture();
            if (ShaderSettingsIO::LoadPreset(presetName, error))
            {
                EditHistory::Instance().PushSettingsDiff(u8"プリセットの読み込み: " + presetName, before, ShaderSettingsIO::Capture());
                SetMessage(u8"プリセットを読み込みました: " + presetName, false);
            }
            else
            {
                SetMessage(error, true);
            }
        });
    }
    ImGui::EndDisabled();

    if (!m_selectedPreset.empty())
    {
        // 説明は選んだときだけ読む(毎フレームのファイルの読み込みを避ける)
        if (m_describedPreset != m_selectedPreset)
        {
            m_describedPreset = m_selectedPreset;
            m_selectedDescription = ShaderSettingsIO::GetPresetDescription(m_selectedPreset);
        }
        if (!m_selectedDescription.empty())
        {
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextDisabled("%s", m_selectedDescription.c_str());
            ImGui::PopTextWrapPos();
        }
    }

    ImGui::SetNextItemWidth(200.0f);
    ImGui::InputTextWithHint("##PresetName", u8"新しいプリセットの名前", m_presetName, sizeof(m_presetName));
    ImGui::SameLine();
    const std::string name = m_presetName;
    const bool exists = ShaderSettingsIO::HasPreset(name);
    ImGui::BeginDisabled(name.empty());
    if (ImGui::Button(exists ? u8"上書き保存" : u8"保存"))
    {
        std::string error;
        if (ShaderSettingsIO::SavePreset(name, error))
        {
            SetMessage(u8"プリセットを保存しました: " + name, false);
            m_selectedPreset = name;
        }
        else
        {
            SetMessage(error, true);
        }
    }
    ImGui::EndDisabled();
}

void ShaderSettingsPanel::DrawABCompare()
{
    ImGui::TextUnformatted(u8"A/B比較");
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip(u8"今の値をAかBに記録し、ボタンで切り替えて見比べる。\n表示した後に変えた値はスロットには残らないので、残すときはもう一度記録する。");
    }

    const nlohmann::json current = ShaderSettingsIO::Capture();
    for (int slotIndex = 0; slotIndex < 2; ++slotIndex)
    {
        ImGui::PushID(slotIndex);
        const bool hasSlot = !m_slots[slotIndex].is_null();
        if (ImGui::Button((std::string(kSlotNames[slotIndex]) + u8"に記録").c_str()))
        {
            m_slots[slotIndex] = current;
            m_shownSlot = slotIndex;
            SetMessage(std::string(u8"今の値を") + kSlotNames[slotIndex] + u8"に記録しました", false);
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!hasSlot);
        const bool isShown = (m_shownSlot == slotIndex);
        if (isShown)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        }
        if (ImGui::Button((std::string(kSlotNames[slotIndex]) + u8"を表示").c_str()))
        {
            const nlohmann::json before = ShaderSettingsIO::Capture();
            ShaderSettingsIO::Apply(m_slots[slotIndex]);
            EditHistory::Instance().PushSettingsDiff(std::string(kSlotNames[slotIndex]) + u8"を表示", before, ShaderSettingsIO::Capture());
            m_shownSlot = slotIndex;
        }
        if (isShown)
        {
            ImGui::PopStyleColor();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (!hasSlot)
        {
            ImGui::TextDisabled(u8"未記録");
        }
        else
        {
            const int difference = ShaderSettingsIO::CountDifferences(m_slots[slotIndex], current);
            if (difference == 0)
            {
                ImGui::TextDisabled(isShown ? u8"表示中" : u8"今の値と同じ");
            }
            else
            {
                ImGui::TextDisabled(u8"今の値と %d 個違う", difference);
            }
        }
        ImGui::PopID();
    }
    if (!m_slots[0].is_null() && !m_slots[1].is_null())
    {
        ImGui::TextDisabled(u8"AとBで違う値: %d 個", ShaderSettingsIO::CountDifferences(m_slots[0], m_slots[1]));
    }
}

void ShaderSettingsPanel::SetMessage(const std::string& message, bool isError)
{
    m_message = message;
    m_isMessageError = isError;
}
