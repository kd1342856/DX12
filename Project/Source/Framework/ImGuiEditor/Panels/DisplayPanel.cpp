#include "DisplayPanel.h"
#include "../EditorContext.h"
#include "ImGui/imgui.h"
#include "../Widgets/SettingsFieldUI.h"
#include "../Widgets/FieldWidgets.h"
#include "../../../Graphics/Renderer/QualitySettings.h"
#include "../../../Graphics/Renderer/Renderer.h"
#include "../../../Application/UI/OptionMenu.h"

namespace
{
    const char* const kQualityLabels[] = { u8"低", u8"中", u8"高(標準)", u8"最高" };

    const char* OnOff(bool isOn)
    {
        return isOn ? u8"あり" : u8"なし";
    }

    void HelpMarker(const char* pText)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
        {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28.0f);
            ImGui::TextUnformatted(pText);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    void DrawResolution()
    {
        auto& device = GraphicsDevice::Instance();
        ImGui::SeparatorText(u8"解像度");
        ImGui::Text(u8"ウィンドウ: %u x %u", device.GetBackBufferWidth(), device.GetBackBufferHeight());
        ImGui::Text(u8"描画: %d x %d(倍率 %.2f)", Renderer::GetRenderWidth(), Renderer::GetRenderHeight(), QualitySettings::GetRenderScale());
        HelpMarker(u8"シーンはウィンドウの大きさ×倍率で描き、最後にウィンドウの大きさへ戻す。UIはウィンドウの大きさで描く。\nウィンドウの枠をドラッグして大きさを変えられる。F11で全画面と切り替わる。");

        // 描画解像度の倍率(エディターだけの上書き。プリセットの値を初期値とし、変えた印と「初期値に戻す」を付ける)
        const float presetScale = QualitySettings::GetCurrent().renderScale;
        const bool isOverridden = QualitySettings::GetRenderScaleOverride() > 0.0f;
        float scale = QualitySettings::GetRenderScale();
        static const FieldMeta kScaleMeta = Field("renderScaleOverride", u8"描画解像度の倍率",
            u8"ウィンドウの大きさに対する、シーンを描く解像度の倍率。下げると軽くなるが、ぼやける。\n"
            u8"エディターで重さを確かめるための上書きで、保存しない(再起動すると画質のプリセットの値に戻る)。")
            .Range(0.5f, 1.0f).Ref(u8"等倍(画質「中」以上)", 1.0f).Ref(u8"画質「低」", 0.75f).Ref(u8"半分", 0.5f);
        if (FieldWidgets::BeginFieldTable("##renderScale"))
        {
            FieldWidgets::SetNextModified(isOverridden);
            const FieldWidgets::EditResult result = FieldWidgets::Number(kScaleMeta, scale, presetScale);
            // ドラッグ中に毎フレームRTを作り直さないよう、操作を終えたときだけ反映する
            if (result.reset)
            {
                QualitySettings::RequestRenderScaleOverride(0.0f);
            }
            else if (result.committed)
            {
                QualitySettings::RequestRenderScaleOverride(std::clamp(scale, 0.25f, 2.0f));
            }
            FieldWidgets::EndFieldTable();
        }
    }

    void DrawQuality()
    {
        ImGui::SeparatorText(u8"画質");
        int quality = static_cast<int>(QualitySettings::GetLevel());
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
        if (ImGui::Combo(u8"画質のプリセット", &quality, kQualityLabels, IM_ARRAYSIZE(kQualityLabels)))
        {
            QualitySettings::Request(static_cast<QualityLevel>(quality));
            OptionMenu::SaveSettings();
        }
        HelpMarker(u8"オプション画面の「画質」と同じ値。変えるとオプションの設定(Saved/OptionSettings.json)に保存する。\n基準は「高」(RTX 3060・1080pで目標の性能を満たす設定)。");

        // 今のプリセットの中身(変えられない。値はQualitySettings::GetPreset)
        const QualityPreset& preset = QualitySettings::GetCurrent();
        if (ImGui::BeginTable("##preset", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
        {
            auto row = [](const char* pLabel, const std::string& value)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextDisabled("%s", pLabel);
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(value.c_str());
            };
            char buffer[64] = {};
            snprintf(buffer, sizeof(buffer), "%.2f", preset.renderScale);
            row(u8"描画解像度の倍率", buffer);
            row(u8"月の影", std::to_string(preset.directionalShadowResolution));
            row(u8"スポットライトの影", std::to_string(preset.spotShadowResolution));
            row(u8"点光源の影(キューブの1面)", std::to_string(preset.cubeShadowResolution));
            row(u8"SSAO", OnOff(preset.enableSSAO));
            row(u8"画面の反射(SSR)", OnOff(preset.enableSSR));
            snprintf(buffer, sizeof(buffer), u8"%d 枚(%u)", preset.maxPlanarReflections, preset.planarReflectionResolution);
            row(u8"窓の反射(平面反射)", buffer);
            row(u8"ブルームのぼかしの回数", std::to_string(preset.bloomIterations));
            row(u8"レイトレースの反射", OnOff(preset.raytracedReflections));
            ImGui::EndTable();
        }
    }

    void DrawFrameRate()
    {
        auto& device = GraphicsDevice::Instance();
        ImGui::SeparatorText(u8"fpsの上限");
        const float framerate = ImGui::GetIO().Framerate;
        ImGui::Text(u8"今: %.1f fps(%.2f ms)", framerate, framerate > 0.0f ? 1000.0f / framerate : 0.0f);
        HelpMarker(u8"オプション画面の「フレームレートの上限」と同じ値。変えるとオプションの設定に保存する。\n上限なしは入力の遅延が最も小さいが、画面に横線(ティアリング)が出ることがある。");

        int mode = static_cast<int>(device.GetFrameRateMode());
        bool isChanged = false;
        isChanged |= ImGui::RadioButton(u8"上限なし", &mode, static_cast<int>(GraphicsDevice::FrameRateMode::Unlimited));
        ImGui::SameLine();
        isChanged |= ImGui::RadioButton(u8"垂直同期", &mode, static_cast<int>(GraphicsDevice::FrameRateMode::VSync));
        ImGui::SameLine();
        isChanged |= ImGui::RadioButton(u8"fpsを指定", &mode, static_cast<int>(GraphicsDevice::FrameRateMode::Limited));
        if (isChanged)
        {
            device.SetFrameRateMode(static_cast<GraphicsDevice::FrameRateMode>(mode));
            OptionMenu::SaveSettings();
        }

        ImGui::BeginDisabled(device.GetFrameRateMode() != GraphicsDevice::FrameRateMode::Limited);
        int limit = device.GetFrameRateLimit();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::SliderInt("##frameRateLimit", &limit, OptionMenu::kMinFrameRateLimit, OptionMenu::kMaxFrameRateLimit, u8"%d fps"))
        {
            device.SetFrameRateLimit(limit);
        }
        // ドラッグを終えたときだけ保存する
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            OptionMenu::SaveSettings();
        }
        ImGui::EndDisabled();
    }
}

void DisplayPanel::Draw(EditorContext& ctx)
{
    (void)ctx;
    // 検索中は値のパネルだけを見せる
    if (SettingsFieldUI::IsSearching())
    {
        return;
    }
    if (!ImGui::CollapsingHeader(u8"解像度・画質・fpsの上限###Display"))
    {
        return;
    }
    ImGui::PushID("Display");
    DrawResolution();
    DrawQuality();
    DrawFrameRate();
    ImGui::PopID();
}
