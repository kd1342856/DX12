#include "BufferViewPanel.h"
#include "../EditorContext.h"
#include "ImGui/imgui.h"
#include "../Widgets/SettingsFieldUI.h"
#include "../Widgets/FieldWidgets.h"
#include "../../../Graphics/Renderer/BufferViewer.h"
#include "../../../Graphics/Renderer/Renderer.h"

namespace
{
    // サムネイルの幅がこれより狭くならないよう、列の数を決める
    constexpr float kMinThumbnailWidth = 140.0f;

    ImTextureID ToTextureID(int imGuiSrvIndex)
    {
        // ImTextureIDの型はImGuiの版で違うので、PreviewCameraWindowと同じく変換する
        return (ImTextureID)GraphicsDevice::Instance().GetImGuiSRVGPUHandle(imGuiSrvIndex).ptr;
    }

    void DrawBufferTooltip(BufferViewer::Buffer buffer)
    {
        const auto& info = BufferViewer::GetInfo(buffer);
        int width = 0;
        int height = 0;
        BufferViewer::Instance().GetSourceSize(buffer, width, height);
        ImGui::BeginTooltip();
        ImGui::TextUnformatted(info.pLabel);
        ImGui::TextUnformatted(info.pDescription);
        ImGui::TextDisabled(u8"元の大きさ %d x %d", width, height);
        ImGui::EndTooltip();
    }
}

void BufferViewPanel::Draw(EditorContext& ctx)
{
    (void)ctx;
    // 検索中は値のパネルだけを見せる
    if (SettingsFieldUI::IsSearching())
    {
        return;
    }
    if (!ImGui::CollapsingHeader(u8"バッファの一覧###BufferView"))
    {
        // 閉じている間は、変換を止める
        return;
    }
    ImGui::PushID("BufferView");
    auto& viewer = BufferViewer::Instance();
    if (!viewer.IsAvailable())
    {
        ImGui::TextDisabled(u8"このGPUではコンピュートシェーダー(シェーダーモデル6.6)が使えないので出せません");
        ImGui::PopID();
        return;
    }
    viewer.Request();
    if (!viewer.HasContent())
    {
        ImGui::TextDisabled(u8"準備しています…");
        ImGui::PopID();
        return;
    }
    ImGui::TextDisabled(u8"クリックすると下に大きく出す。マウスを乗せると説明が出る");

    // 画面と同じ縦横の比で並べる
    const float aspect = static_cast<float>(Renderer::GetRenderHeight()) / static_cast<float>((std::max)(Renderer::GetRenderWidth(), 1));
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const int columnCount = std::clamp(static_cast<int>((availableWidth + spacing) / (kMinThumbnailWidth + spacing)), 1, BufferViewer::kBufferCount);
    const float thumbnailWidth = (std::max)((availableWidth - spacing * (columnCount - 1)) / columnCount, 32.0f);
    const ImVec2 thumbnailSize(thumbnailWidth, thumbnailWidth * aspect);

    for (int bufferIndex = 0; bufferIndex < BufferViewer::kBufferCount; ++bufferIndex)
    {
        const auto buffer = static_cast<BufferViewer::Buffer>(bufferIndex);
        const bool isSelected = (viewer.GetSelected() == buffer);
        if (bufferIndex % columnCount != 0)
        {
            ImGui::SameLine();
        }
        ImGui::BeginGroup();
        if (isSelected)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, FieldWidgets::GetModifiedColor());
        }
        ImGui::TextUnformatted(BufferViewer::GetInfo(buffer).pLabel);
        if (isSelected)
        {
            ImGui::PopStyleColor();
        }
        ImGui::Image(ToTextureID(viewer.GetThumbnailImGuiSRVIndex(buffer)), thumbnailSize);
        if (ImGui::IsItemClicked())
        {
            viewer.SetSelected(buffer);
        }
        if (ImGui::IsItemHovered())
        {
            DrawBufferTooltip(buffer);
        }
        if (isSelected)
        {
            ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), FieldWidgets::GetModifiedColor(), 0.0f, 0, 2.0f);
        }
        ImGui::EndGroup();
    }

    ImGui::Separator();
    const auto selected = viewer.GetSelected();
    int sourceWidth = 0;
    int sourceHeight = 0;
    viewer.GetSourceSize(selected, sourceWidth, sourceHeight);
    ImGui::Text(u8"%s(元の大きさ %d x %d)", BufferViewer::GetInfo(selected).pLabel, sourceWidth, sourceHeight);
    ImGui::TextWrapped("%s", BufferViewer::GetInfo(selected).pDescription);
    const float largeWidth = ImGui::GetContentRegionAvail().x;
    ImGui::Image(ToTextureID(viewer.GetLargeImGuiSRVIndex()), ImVec2(largeWidth, largeWidth * aspect));
    ImGui::PopID();
}
