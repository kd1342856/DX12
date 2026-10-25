#include "../../Pch.h"
#include "OptionMenu.h"
#include "../../Framework/Manager/Audio/AudioManager.h"
#include "../../Graphics/Shader/ShaderManager/ShaderManager.h"
#include "../../Framework/System/AppPaths/AppPaths.h"
#include "../../Graphics/Renderer/QualitySettings.h"
#include "../../Graphics/Renderer/TemporalAA.h"
#include "../../Graphics/Renderer/MotionBlur.h"
#include <imgui.h>
#include <algorithm>

bool OptionMenu::s_open = false;
bool OptionMenu::s_justOpened = false;

namespace
{
    // 0~1の音量を0~100%のスライダーで編集する。変更されたらtrue
    bool VolumeSlider(const char* label, float& volume)
    {
        int percent = static_cast<int>(volume * 100.0f + 0.5f);
        if (ImGui::SliderInt(label, &percent, 0, 100, "%d%%")) {
            volume = percent / 100.0f;
            return true;
        }
        return false;
    }

    // OptionSettings.jsonでのfpsの上限の決め方の名前
    const char* ToSettingName(GraphicsDevice::FrameRateMode mode)
    {
        switch (mode)
        {
        case GraphicsDevice::FrameRateMode::VSync:
            return "vsync";
        case GraphicsDevice::FrameRateMode::Limited:
            return "limited";
        default:
            return "unlimited";
        }
    }

    // OptionSettings.jsonでの画質の名前
    const char* kQualityNames[] = { "low", "medium", "high", "ultra" };

    GraphicsDevice::FrameRateMode FromSettingName(const std::string& name)
    {
        if (name == "vsync")
        {
            return GraphicsDevice::FrameRateMode::VSync;
        }
        if (name == "limited")
        {
            return GraphicsDevice::FrameRateMode::Limited;
        }
        return GraphicsDevice::FrameRateMode::Unlimited;
    }
}

void OptionMenu::Open()
{
    s_open = true;
    s_justOpened = true;
}

void OptionMenu::Close()
{
    if (!s_open) return;
    s_open = false;
    SaveSettings();
    AudioManager::Instance().SaveSettings();
}

void OptionMenu::LoadSettings()
{
    std::filesystem::path settingsPath = AppPaths::GetSavedFilePath(kSettingsFileName);
    std::ifstream in(settingsPath);
    if (!in.is_open()) return;
    try {
        nlohmann::json j;
        in >> j;
        auto& pp = ShaderManager::Instance().GetPostProcessSettings();
        if (j.contains("brightnessGamma")) {
            pp.BrightnessGamma = std::clamp(j["brightnessGamma"].get<float>(), kMinBrightnessGamma, kMaxBrightnessGamma);
            pp.IsDirty = true;
        }
        auto& device = GraphicsDevice::Instance();
        if (j.contains("frameRateMode"))
        {
            device.SetFrameRateMode(FromSettingName(j["frameRateMode"].get<std::string>()));
        }
        if (j.contains("frameRateLimit"))
        {
            device.SetFrameRateLimit(std::clamp(j["frameRateLimit"].get<int>(), kMinFrameRateLimit, kMaxFrameRateLimit));
        }
        if (j.contains("qualityLevel"))
        {
            const std::string name = j["qualityLevel"].get<std::string>();
            for (int levelIndex = 0; levelIndex < 4; ++levelIndex)
            {
                if (name == kQualityNames[levelIndex])
                {
                    QualitySettings::Request(static_cast<QualityLevel>(levelIndex));
                }
            }
        }
        if (j.contains("antiAliasing"))
        {
            pp.EnableTAA = j["antiAliasing"].get<bool>();
        }
        if (j.contains("motionBlur"))
        {
            pp.EnableMotionBlur = j["motionBlur"].get<bool>();
        }
    }
    catch (const std::exception& e) {
        Logger::Instance().AddLog(Logger::LogLevel::Warning, "OptionMenu: Failed to load %s (%s)", AppPaths::ToUtf8(settingsPath).c_str(), e.what());
    }
}

void OptionMenu::SaveSettings()
{
    nlohmann::json j;
    j["brightnessGamma"] = ShaderManager::Instance().GetPostProcessSettings().BrightnessGamma;
    j["frameRateMode"] = ToSettingName(GraphicsDevice::Instance().GetFrameRateMode());
    j["frameRateLimit"] = GraphicsDevice::Instance().GetFrameRateLimit();
    j["qualityLevel"] = kQualityNames[static_cast<int>(QualitySettings::GetLevel())];
    j["antiAliasing"] = ShaderManager::Instance().GetPostProcessSettings().EnableTAA;
    j["motionBlur"] = ShaderManager::Instance().GetPostProcessSettings().EnableMotionBlur;
    std::filesystem::path settingsPath = AppPaths::GetSavedFilePath(kSettingsFileName);
    std::ofstream o(settingsPath);
    if (o.is_open()) {
        o << std::setw(4) << j << std::endl;
        Logger::Instance().AddLog(Logger::LogLevel::Info, "OptionMenu: Saved %s", AppPaths::ToUtf8(settingsPath).c_str());
    }
}

void OptionMenu::Draw()
{
    if (!s_open) return;

    // 画面の高さ720を基準に、解像度/全画面に合わせて拡大する
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float scale = vp->Size.y / 720.0f;
    const ImVec2 windowSize(640.0f * scale, 560.0f * scale);

    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);
    if (s_justOpened) {
        ImGui::SetNextWindowFocus();
        s_justOpened = false;
    }

    // ホラーの雰囲気に合わせた暗い配色(赤を差し色)
    ImGui::PushStyleColor(ImGuiCol_WindowBg,         ImVec4(0.04f, 0.03f, 0.03f, 0.94f));
    ImGui::PushStyleColor(ImGuiCol_Border,           ImVec4(0.45f, 0.08f, 0.08f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_Tab,              ImVec4(0.18f, 0.06f, 0.06f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_TabHovered,       ImVec4(0.55f, 0.12f, 0.12f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_TabSelected,      ImVec4(0.42f, 0.08f, 0.08f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg,          ImVec4(0.14f, 0.08f, 0.08f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,   ImVec4(0.24f, 0.10f, 0.10f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,    ImVec4(0.30f, 0.10f, 0.10f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,       ImVec4(0.70f, 0.15f, 0.15f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.90f, 0.20f, 0.20f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_Button,           ImVec4(0.25f, 0.07f, 0.07f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,    ImVec4(0.50f, 0.12f, 0.12f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,     ImVec4(0.65f, 0.15f, 0.15f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_CheckMark,        ImVec4(0.90f, 0.25f, 0.25f, 1.00f));
    constexpr int kStyleColorCount = 14;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.0f * scale, 20.0f * scale));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f * scale, 6.0f * scale));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f * scale, 12.0f * scale));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
    constexpr int kStyleVarCount = 4;
    ImGui::PushFont(nullptr, 20.0f * scale);

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar;
    if (ImGui::Begin("##OptionMenu", nullptr, flags))
    {
        ImGui::PushFont(nullptr, 30.0f * scale);
        ImGui::TextUnformatted(u8"オプション");
        ImGui::PopFont();
        ImGui::Separator();

        if (ImGui::BeginTabBar("##OptionTabs"))
        {
            if (ImGui::BeginTabItem(u8"  音量  ")) {
                DrawVolumeTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(u8"  明るさ  ")) {
                DrawBrightnessTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(u8"  画面  "))
            {
                DrawDisplayTab();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        // 閉じるボタンはウィンドウ下端に固定
        const ImVec2 buttonSize(160.0f * scale, 40.0f * scale);
        ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() - buttonSize.x) * 0.5f,
                                   ImGui::GetWindowHeight() - buttonSize.y - 20.0f * scale));
        if (ImGui::Button(u8"閉じる", buttonSize)) {
            Close();
        }
        ImGui::SameLine();
        ImGui::TextDisabled(u8"(ESC)");
    }
    ImGui::End();

    ImGui::PopFont();
    ImGui::PopStyleVar(kStyleVarCount);
    ImGui::PopStyleColor(kStyleColorCount);
}

void OptionMenu::DrawVolumeTab()
{
    auto& audio = AudioManager::Instance();
    ImGui::Spacing();

    const float labelWidth = ImGui::CalcTextSize(u8"マスター音量").x + ImGui::GetStyle().ItemInnerSpacing.x;
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - labelWidth);

    float master = audio.GetMasterVolume();
    if (VolumeSlider(u8"マスター音量", master)) audio.SetMasterVolume(master);

    float bgm = audio.GetBGMBusVolume();
    if (VolumeSlider(u8"BGM", bgm)) audio.SetBGMBusVolume(bgm);

    float se = audio.GetSEVolume();
    if (VolumeSlider(u8"効果音", se)) audio.SetSEVolume(se);

    ImGui::PopItemWidth();

    bool muted = audio.IsMuted();
    if (ImGui::Checkbox(u8"ミュート", &muted)) audio.SetMuted(muted);

    ImGui::Spacing();
    ImGui::TextDisabled(u8"効果音の音量は、次に鳴った音から反映されます。");
}

void OptionMenu::DrawBrightnessTab()
{
    auto& pp = ShaderManager::Instance().GetPostProcessSettings();
    ImGui::Spacing();

    const float buttonWidth = ImGui::CalcTextSize(u8"初期値に戻す").x + ImGui::GetStyle().FramePadding.x * 2.0f;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - buttonWidth - ImGui::GetStyle().ItemSpacing.x);
    if (ImGui::SliderFloat("##BrightnessGamma", &pp.BrightnessGamma, kMinBrightnessGamma, kMaxBrightnessGamma, u8"ガンマ %.2f")) {
        pp.IsDirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button(u8"初期値に戻す")) {
        pp.BrightnessGamma = kDefaultBrightnessGamma;
        pp.IsDirty = true;
    }

    DrawBrightnessPreview(ImGui::GetContentRegionAvail().x);

    ImGui::TextWrapped(u8"左の霊がほとんど見えず、真ん中の霊がかすかに見える明るさがおすすめです。");
}

void OptionMenu::DrawDisplayTab()
{
    auto& device = GraphicsDevice::Instance();
    ImGui::Spacing();

    // 画質(影の解像度・SSAO・SSR・平面反射・描画解像度などをまとめて切り替える)
    const char* kQualityLabels[] = { u8"低", u8"中", u8"高(標準)", u8"最高" };
    int quality = static_cast<int>(QualitySettings::GetLevel());
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
    if (ImGui::Combo(u8"画質", &quality, kQualityLabels, IM_ARRAYSIZE(kQualityLabels)))
    {
        QualitySettings::Request(static_cast<QualityLevel>(quality));
    }

    // アンチエイリアス(TAA)とモーションブラー。どちらもシェーダーモデル6.6に対応したGPUでだけ使える
    auto& postProcess = ShaderManager::Instance().GetPostProcessSettings();
    ImGui::BeginDisabled(!TemporalAA::Instance().IsAvailable());
    ImGui::Checkbox(u8"アンチエイリアス(TAA)", &postProcess.EnableTAA);
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!MotionBlur::Instance().IsAvailable());
    ImGui::Checkbox(u8"モーションブラー", &postProcess.EnableMotionBlur);
    ImGui::EndDisabled();
    ImGui::Spacing();
    ImGui::TextUnformatted(u8"フレームレートの上限");

    int mode = static_cast<int>(device.GetFrameRateMode());
    bool isChanged = false;
    isChanged |= ImGui::RadioButton(u8"上限なし", &mode, static_cast<int>(GraphicsDevice::FrameRateMode::Unlimited));
    isChanged |= ImGui::RadioButton(u8"垂直同期(モニターのリフレッシュレート)", &mode, static_cast<int>(GraphicsDevice::FrameRateMode::VSync));
    isChanged |= ImGui::RadioButton(u8"fpsを指定", &mode, static_cast<int>(GraphicsDevice::FrameRateMode::Limited));
    if (isChanged)
    {
        device.SetFrameRateMode(static_cast<GraphicsDevice::FrameRateMode>(mode));
    }

    const bool isLimited = (device.GetFrameRateMode() == GraphicsDevice::FrameRateMode::Limited);
    ImGui::BeginDisabled(!isLimited);
    int limit = device.GetFrameRateLimit();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::SliderInt("##FrameRateLimit", &limit, kMinFrameRateLimit, kMaxFrameRateLimit, u8"%d fps"))
    {
        device.SetFrameRateLimit(limit);
    }
    ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::TextDisabled(u8"上限なしは最も入力の遅延が小さく、画面の横線(ティアリング)が出ることがあります。");
}

void OptionMenu::DrawBrightnessPreview(float width)
{
    const auto& pp = ShaderManager::Instance().GetPostProcessSettings();
    // ゲーム画面の最終合成(PostProcessShader_PS)と同じ変換(リニア → sRGB → 見た目用のGamma → 明るさ)をCPUで再現し、
    // 「この設定でゲーム内の暗がりがどう見えるか」をその場で確認できるようにする
    auto linearToSrgb = [](float v)
    {
        v = std::clamp(v, 0.0f, 1.0f);
        return v <= 0.0031308f ? v * 12.92f : 1.055f * powf(v, 1.0f / 2.4f) - 0.055f;
    };
    auto srgbToLinear = [](float v)
    {
        return v <= 0.04045f ? v / 12.92f : powf((v + 0.055f) / 1.055f, 2.4f);
    };
    auto toDisplay = [&](float linearValue) {
        float v = linearToSrgb(linearValue);
        v = powf(v, 1.0f / pp.Gamma);
        return powf(std::clamp(v, 0.0f, 1.0f), 1.0f / (std::max)(pp.BrightnessGamma, 0.01f));
    };
    // 見本の明るさ(下のkLevels・kMaxLevel)は、明るさが初期値のときの画面上の値(sRGB)で決めている。
    // ゲーム内のリニアの値に戻してから、ゲームと同じ変換を通す
    auto gray = [&](float displayValue) {
        const float d = toDisplay(srgbToLinear(displayValue));
        return ImGui::GetColorU32(ImVec4(d, d, d, 1.0f));
    };

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float scale = ImGui::GetFontSize() / 20.0f;
    const float height = width * 0.46f;
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const ImVec2 p1(p0.x + width, p0.y + height);
    const ImU32 labelColor = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    const float labelSize = 16.0f * scale;

    // 背景はゲーム内の真っ暗な部屋(黒)
    dl->AddRectFilled(p0, p1, gray(0.0f));

    // 暗さの異なる3体の霊(頭+裾が波打つ胴体)
    constexpr float kLevels[3] = { 0.004f, 0.02f, 0.08f };
    const char* kLabels[3] = { u8"見えない", u8"かすかに見える", u8"はっきり見える" };
    const float r = height * 0.14f;
    for (int i = 0; i < 3; ++i) {
        const ImU32 col = gray(kLevels[i]);
        const ImVec2 head(p0.x + width * (i * 2 + 1) / 6.0f, p0.y + height * 0.24f);
        dl->AddCircleFilled(head, r, col, 32);
        dl->AddRectFilled(ImVec2(head.x - r, head.y), ImVec2(head.x + r, head.y + r * 1.5f), col);
        for (int k = 0; k < 3; ++k) {
            const float x = head.x - r + r * (2.0f / 3.0f) * (k + 0.5f);
            dl->AddCircleFilled(ImVec2(x, head.y + r * 1.5f), r / 3.0f, col, 16);
        }

        const ImVec2 textSize = ImGui::CalcTextSize(kLabels[i]);
        const float textW = textSize.x * labelSize / ImGui::GetFontSize();
        dl->AddText(nullptr, labelSize, ImVec2(head.x - textW * 0.5f, head.y + r * 2.1f), labelColor, kLabels[i]);
    }

    // 暗部の階調(黒→暗いグレーを16段階)。隣同士の段差が見分けられるほど暗部がつぶれていない
    constexpr int kSteps = 16;
    constexpr float kMaxLevel = 0.25f;
    const float barTop = p0.y + height * 0.76f;
    const float barBottom = p1.y - height * 0.06f;
    const float margin = width * 0.04f;
    const float stepW = (width - margin * 2.0f) / kSteps;
    for (int i = 0; i < kSteps; ++i) {
        const float x = p0.x + margin + stepW * i;
        dl->AddRectFilled(ImVec2(x, barTop), ImVec2(x + stepW, barBottom), gray(kMaxLevel * i / (kSteps - 1)));
    }
    dl->AddText(nullptr, labelSize, ImVec2(p0.x + margin, barTop - labelSize - 4.0f * scale), labelColor, u8"暗部の階調");

    dl->AddRect(p0, p1, ImGui::GetColorU32(ImGuiCol_Border));
    ImGui::Dummy(ImVec2(width, height));
}
