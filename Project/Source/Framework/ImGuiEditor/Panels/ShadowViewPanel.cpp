#include "ShadowViewPanel.h"
#include "../EditorContext.h"
#include "ImGui/imgui.h"
#include "../Widgets/SettingsFieldUI.h"
#include "../../../Graphics/Renderer/ShadowViewer.h"
#include "../../../Graphics/Renderer/QualitySettings.h"

namespace
{
    // サムネイルの幅がこれより狭くならないよう、列の数を決める
    constexpr float kMinThumbnailWidth = 130.0f;

    void DrawImage(int imGuiSrvIndex, const ImVec2& size)
    {
        // ImTextureIDの型はImGuiの版で違うので、PreviewCameraWindowと同じく変換する
        ImGui::Image((ImTextureID)GraphicsDevice::Instance().GetImGuiSRVGPUHandle(imGuiSrvIndex).ptr, size);
    }

    // 枠の使い方(どのライトか、このフレームで使ったか、描き直しが要るか)
    void DrawSlotStatus(const LocalShadowManager::Slot& slot)
    {
        if (slot.lightKey == UINT32_MAX)
        {
            ImGui::TextDisabled(u8"空き");
            return;
        }
        ImGui::Text(u8"ライト #%u", slot.lightKey);
        ImGui::TextDisabled(u8"範囲 %.1f m", slot.range);
        ImGui::TextDisabled("%s", slot.isRequested ? (slot.isValid ? u8"使用中・キャッシュ" : u8"使用中・描き直し") : u8"このフレームは未使用");
    }

    // 枠のサムネイルを、パネルの幅に合わせて並べる
    template<typename TGetIndex>
    void DrawSlots(const std::vector<LocalShadowManager::Slot>& slots, int slotCount, float aspect, TGetIndex getImGuiSrvIndex)
    {
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const int columnCount = std::clamp(static_cast<int>((availableWidth + spacing) / (kMinThumbnailWidth * aspect + spacing)), 1, slotCount);
        const float width = (std::max)((availableWidth - spacing * (columnCount - 1)) / columnCount, 32.0f);
        for (int slotIndex = 0; slotIndex < slotCount && slotIndex < static_cast<int>(slots.size()); ++slotIndex)
        {
            if (slotIndex % columnCount != 0)
            {
                ImGui::SameLine();
            }
            ImGui::BeginGroup();
            ImGui::Text(u8"枠 %d", slotIndex);
            DrawImage(getImGuiSrvIndex(slotIndex), ImVec2(width, width / aspect));
            DrawSlotStatus(slots[slotIndex]);
            ImGui::EndGroup();
        }
    }
}

void ShadowViewPanel::Draw(EditorContext& ctx)
{
    (void)ctx;
    // 検索中は値のパネルだけを見せる
    if (SettingsFieldUI::IsSearching())
    {
        return;
    }
    if (!ImGui::CollapsingHeader(u8"影の中身###ShadowView"))
    {
        // 閉じている間は、変換を止める
        return;
    }
    ImGui::PushID("ShadowView");
    auto& viewer = ShadowViewer::Instance();
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
    ImGui::TextDisabled(u8"近いほど白く、何も無い所(届く範囲の外)は黒。解像度は画質の設定で変わる");

    const QualityPreset& preset = QualitySettings::GetCurrent();
    ImGui::SeparatorText(u8"月の影");
    const float directionalWidth = (std::min)(ImGui::GetContentRegionAvail().x, 256.0f);
    DrawImage(viewer.GetDirectionalImGuiSRVIndex(), ImVec2(directionalWidth, directionalWidth));
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::Text(u8"%u x %u", preset.directionalShadowResolution, preset.directionalShadowResolution);
    ImGui::TextDisabled(u8"平行投影。屋根や壁が月の光をさえぎる");
    ImGui::EndGroup();

    auto& shadowManager = LocalShadowManager::Instance();
    const std::string spotTitle = std::string(u8"スポットライトの影(2D配列、") + std::to_string(preset.spotShadowResolution) + u8")";
    ImGui::SeparatorText(spotTitle.c_str());
    DrawSlots(shadowManager.GetSpotSlots(), ShadowViewer::kSpotCount, 1.0f, [&](int slotIndex)
    {
        return viewer.GetSpotImGuiSRVIndex(slotIndex);
    });

    const std::string cubeTitle = std::string(u8"点光源と広いスポットの影(キューブ配列、1面 ") + std::to_string(preset.cubeShadowResolution) + u8")";
    ImGui::SeparatorText(cubeTitle.c_str());
    ImGui::TextDisabled(u8"6面を並べている(上の段 +X・-X・+Y、下の段 -Y・+Z・-Z)");
    DrawSlots(shadowManager.GetCubeSlots(), ShadowViewer::kCubeCount, 1.5f, [&](int slotIndex)
    {
        return viewer.GetCubeImGuiSRVIndex(slotIndex);
    });
    ImGui::PopID();
}
