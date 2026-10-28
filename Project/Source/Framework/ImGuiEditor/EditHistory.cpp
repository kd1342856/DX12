#include "EditHistory.h"
#include "ImGui/imgui.h"
#include "../../Graphics/Shader/ShaderSettingsIO/ShaderSettingsIO.h"

namespace
{
    // 知らせを出しておく時間
    constexpr auto kNoticeDuration = std::chrono::milliseconds(2500);

    // シェーダーの設定のグループ。ShaderSettingsIOで値を書き戻す
    const char* const kSettingsGroups[] = { "Renderer", "Lighting", "Shadow", "PostProcess", "SSAO", "Fog" };
}

EditHistory& EditHistory::Instance()
{
    static EditHistory instance;
    return instance;
}

EditHistory::EditHistory()
{
    for (const char* pGroup : kSettingsGroups)
    {
        const std::string group = pGroup;
        RegisterTarget(group, [group](const std::string& fieldKey, const nlohmann::json& value)
        {
            return ShaderSettingsIO::ApplyField(group, fieldKey, value);
        });
    }
}

void EditHistory::RegisterTarget(const std::string& targetKey, ApplyFunction apply)
{
    m_targets[targetKey] = std::move(apply);
}

std::string EditHistory::FormatJson(const nlohmann::json& value)
{
    char buffer[64] = {};
    // 上書きを外した値(モデルの.metaのマテリアル)
    if (value.is_null())
    {
        return u8"初期値";
    }
    if (value.is_boolean())
    {
        return value.get<bool>() ? u8"オン" : u8"オフ";
    }
    if (value.is_number_integer())
    {
        return std::to_string(value.get<int>());
    }
    if (value.is_number())
    {
        snprintf(buffer, sizeof(buffer), "%.4g", value.get<double>());
        return buffer;
    }
    if (value.is_array())
    {
        std::string text;
        for (const auto& element : value)
        {
            text += (text.empty() ? "" : ", ") + FormatJson(element);
        }
        return text;
    }
    return value.dump();
}

void EditHistory::PushChange(FieldChange change)
{
    if (change.before == change.after)
    {
        return;
    }
    Command command;
    command.description = change.label + " " + FormatJson(change.before) + u8" → " + FormatJson(change.after);
    command.changes.push_back(std::move(change));
    Push(std::move(command));
}

void EditHistory::PushSettingsDiff(const std::string& description, const nlohmann::json& before, const nlohmann::json& after)
{
    Command command;
    command.description = description;
    for (auto groupIt = after.begin(); groupIt != after.end(); ++groupIt)
    {
        auto beforeGroupIt = before.find(groupIt.key());
        for (auto fieldIt = groupIt->begin(); fieldIt != groupIt->end(); ++fieldIt)
        {
            nlohmann::json beforeValue;
            if (beforeGroupIt != before.end() && beforeGroupIt->contains(fieldIt.key()))
            {
                beforeValue = (*beforeGroupIt)[fieldIt.key()];
            }
            if (beforeValue != *fieldIt)
            {
                command.changes.push_back({ groupIt.key(), fieldIt.key(), ShaderSettingsIO::GetFieldLabel(groupIt.key(), fieldIt.key()), beforeValue, *fieldIt });
            }
        }
    }
    if (command.changes.empty())
    {
        return;
    }
    char countText[32] = {};
    snprintf(countText, sizeof(countText), u8"(%zu 個の値)", command.changes.size());
    command.description += countText;
    Push(std::move(command));
}

void EditHistory::Push(Command command)
{
    m_undoStack.push_back(std::move(command));
    while (m_undoStack.size() > kMaxCommands)
    {
        m_undoStack.pop_front();
    }
    // 新しく変えたら、やり直しの側は消す
    m_redoStack.clear();
}

int EditHistory::ApplyCommand(const Command& command, bool isUndo)
{
    int failedCount = 0;
    // 戻すときは、まとめて変えた値を後ろから戻す(同じ値が2回入っていても最初の値に戻るように)
    for (size_t changeIndex = 0; changeIndex < command.changes.size(); ++changeIndex)
    {
        const FieldChange& change = command.changes[isUndo ? command.changes.size() - 1 - changeIndex : changeIndex];
        auto it = m_targets.find(change.targetKey);
        const nlohmann::json& value = isUndo ? change.before : change.after;
        if (it == m_targets.end() || value.is_null() || !it->second(change.fieldKey, value))
        {
            ++failedCount;
        }
    }
    return failedCount;
}

bool EditHistory::Undo()
{
    if (m_undoStack.empty())
    {
        ShowNotice(u8"元に戻す操作がありません");
        return false;
    }
    Command command = std::move(m_undoStack.back());
    m_undoStack.pop_back();
    const int failedCount = ApplyCommand(command, true);
    std::string notice = u8"元に戻しました：" + command.description;
    if (failedCount > 0)
    {
        // シーンを作り直したなどで、戻す先が無くなった値は飛ばす
        notice += u8"(戻せない値を " + std::to_string(failedCount) + u8" 個飛ばしました)";
    }
    ShowNotice(notice);
    m_redoStack.push_back(std::move(command));
    return true;
}

bool EditHistory::Redo()
{
    if (m_redoStack.empty())
    {
        ShowNotice(u8"やり直す操作がありません");
        return false;
    }
    Command command = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    const int failedCount = ApplyCommand(command, false);
    std::string notice = u8"やり直しました：" + command.description;
    if (failedCount > 0)
    {
        notice += u8"(やり直せない値を " + std::to_string(failedCount) + u8" 個飛ばしました)";
    }
    ShowNotice(notice);
    m_undoStack.push_back(std::move(command));
    return true;
}

void EditHistory::Clear()
{
    m_undoStack.clear();
    m_redoStack.clear();
}

void EditHistory::ShowNotice(const std::string& text)
{
    m_notice = text;
    m_noticeTime = std::chrono::steady_clock::now();
}

void EditHistory::Update()
{
    const ImGuiIO& io = ImGui::GetIO();
    // エディターが操作を受けているときだけ(プレイ中にゲームが操作を受けているときは効かない)。
    // 入力欄で文字を打っている間は、入力欄の中の取り消しを優先する
    if (io.WantCaptureKeyboard && !io.WantTextInput && io.KeyCtrl)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_Z, false))
        {
            if (io.KeyShift)
            {
                Redo();
            }
            else
            {
                Undo();
            }
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_Y, false))
        {
            Redo();
        }
    }

    if (m_notice.empty() || std::chrono::steady_clock::now() - m_noticeTime > kNoticeDuration)
    {
        return;
    }
    // 画面の下の真ん中に短く出す
    const ImGuiViewport* pViewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(pViewport->WorkPos.x + pViewport->WorkSize.x * 0.5f, pViewport->WorkPos.y + pViewport->WorkSize.y - 40.0f),
        ImGuiCond_Always, ImVec2(0.5f, 1.0f));
    ImGui::SetNextWindowBgAlpha(0.85f);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoDocking;
    if (ImGui::Begin("##EditHistoryNotice", nullptr, flags))
    {
        ImGui::TextUnformatted(m_notice.c_str());
    }
    ImGui::End();
}
