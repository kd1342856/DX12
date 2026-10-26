#include "ShaderReloadPanel.h"
#include "../EditorContext.h"
#include "ImGui/imgui.h"
#include "../../../Graphics/Shader/ShaderHotReload/ShaderHotReload.h"

namespace
{
    const ImVec4 kErrorColor(1.0f, 0.45f, 0.4f, 1.0f);
    const ImVec4 kWarningColor(1.0f, 0.8f, 0.3f, 1.0f);

    std::string FormatLocation(const ShaderDiagnostic& diagnostic)
    {
        if (diagnostic.filePath.empty())
        {
            return "";
        }
        if (diagnostic.line <= 0)
        {
            return diagnostic.filePath;
        }
        // Visual Studioの出力ウィンドウと同じ「ファイル(行,列)」の形にする
        return diagnostic.filePath + "(" + std::to_string(diagnostic.line) + "," + std::to_string(diagnostic.column) + ")";
    }

    void OpenFile(const std::string& utf8Path)
    {
        const std::wstring path = std::filesystem::u8path(utf8Path).wstring();
        ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }

    std::string FormatClock(std::chrono::system_clock::time_point time)
    {
        const std::time_t timeValue = std::chrono::system_clock::to_time_t(time);
        std::tm localTime = {};
        localtime_s(&localTime, &timeValue);
        char buffer[16] = {};
        strftime(buffer, sizeof(buffer), "%H:%M:%S", &localTime);
        return buffer;
    }
}

void ShaderReloadPanel::Draw(EditorContext& ctx)
{
    ShaderHotReload& hotReload = ShaderHotReload::Instance();
    const std::vector<ShaderHotReload::ErrorEntry> errors = hotReload.GetErrors();

    // エラーがあるときは見出しの色を変え、閉じていても開く
    if (!errors.empty())
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_Text, kErrorColor);
    }
    const std::string header = errors.empty()
        ? std::string(u8"シェーダーのホットリロード###ShaderHotReload")
        : std::string(u8"シェーダーのホットリロード (エラー ") + std::to_string(errors.size()) + u8"件)###ShaderHotReload";
    const bool isOpen = ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
    if (!errors.empty())
    {
        ImGui::PopStyleColor();
    }
    if (!isOpen)
    {
        return;
    }

    bool isEnabled = hotReload.IsEnabled();
    if (ImGui::Checkbox(u8"保存したら自動でコンパイル", &isEnabled))
    {
        hotReload.SetEnabled(isEnabled);
    }
    ImGui::SameLine();
    if (ImGui::Button(u8"すべて再コンパイル"))
    {
        hotReload.RequestReloadAll();
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip(u8"DXILのキャッシュが当たるものはキャッシュから読むので、変更の無いシェーダーはすぐに終わる。");
    }

    ImGui::TextDisabled(u8"監視中: シェーダー %zu 個、ファイル %zu 個 (Asset/Shader)", hotReload.GetRegisteredCount(), hotReload.GetWatchedFileCount());
    if (hotReload.GetLastReloadCount() > 0)
    {
        ImGui::TextDisabled(u8"前回: %s に %d 個を作り直し (失敗 %d、%.0f ms)",
            FormatClock(hotReload.GetLastReloadTime()).c_str(), hotReload.GetLastReloadCount(), hotReload.GetLastFailedCount(), hotReload.GetLastReloadMilliseconds());
    }

    if (errors.empty())
    {
        return;
    }

    ImGui::TextColored(kErrorColor, u8"コンパイルに失敗したシェーダーは、前のシェーダーのまま描いています");
    for (size_t errorIndex = 0; errorIndex < errors.size(); ++errorIndex)
    {
        const ShaderHotReload::ErrorEntry& entry = errors[errorIndex];
        ImGui::PushID(static_cast<int>(errorIndex));
        ImGui::Separator();
        ImGui::TextColored(kErrorColor, "%s", entry.shaderName.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("%s", FormatClock(entry.time).c_str());

        for (size_t diagnosticIndex = 0; diagnosticIndex < entry.diagnostics.size(); ++diagnosticIndex)
        {
            const ShaderDiagnostic& diagnostic = entry.diagnostics[diagnosticIndex];
            ImGui::PushID(static_cast<int>(diagnosticIndex));
            const std::string location = FormatLocation(diagnostic);
            if (!location.empty())
            {
                // クリックで「ファイル(行,列)」をコピー、ボタンでファイルを開く
                if (ImGui::Selectable(location.c_str(), false, ImGuiSelectableFlags_AllowOverlap))
                {
                    ImGui::SetClipboardText(location.c_str());
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip(u8"クリックで場所をコピー");
                }
                ImGui::SameLine();
                if (ImGui::SmallButton(u8"開く"))
                {
                    OpenFile(diagnostic.filePath);
                }
            }
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextColored(diagnostic.isError ? kErrorColor : kWarningColor, "%s", diagnostic.message.c_str());
            ImGui::PopTextWrapPos();
            ImGui::PopID();
        }
        ImGui::PopID();
    }
}

void ShaderReloadPanel::DrawErrorNotice()
{
    const std::vector<ShaderHotReload::ErrorEntry> errors = ShaderHotReload::Instance().GetErrors();
    if (errors.empty())
    {
        return;
    }

    const ImGuiViewport* pViewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(pViewport->WorkPos.x + pViewport->WorkSize.x - 10.0f, pViewport->WorkPos.y + 10.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.8f);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDocking;
    if (ImGui::Begin("##ShaderErrorNotice", nullptr, flags))
    {
        ImGui::TextColored(kErrorColor, u8"シェーダーのエラー %zu 件 (前のシェーダーで描画中)", errors.size());
        const ShaderHotReload::ErrorEntry& first = errors.front();
        if (!first.diagnostics.empty())
        {
            const ShaderDiagnostic& diagnostic = first.diagnostics.front();
            // 狭い場所なので、フォルダを省いた「ファイル名(行)」と本文の1行目だけを出す
            std::string shortLocation = diagnostic.filePath.empty() ? first.shaderName : std::filesystem::u8path(diagnostic.filePath).filename().u8string();
            if (diagnostic.line > 0)
            {
                shortLocation += "(" + std::to_string(diagnostic.line) + ")";
            }
            const std::string firstLine = diagnostic.message.substr(0, diagnostic.message.find('\n'));
            ImGui::TextUnformatted(shortLocation.c_str());
            ImGui::TextUnformatted(firstLine.c_str());
        }
        ImGui::TextDisabled(u8"詳しくは F1 → Window → Shader Editor");
    }
    ImGui::End();
}
