#pragma once

// オプション画面(音量/明るさ)。タイトルの歯車、ゲーム内のESCから開く。
// ImGuiで描くので、ImGui::NewFrame()〜Renderer::EndFrame()の間(各シーンのUpdate内)でDraw()を呼ぶこと。
// ゲーム内のポーズ(音の一時停止・マウスモード切替)は呼び出し側のシーンが開閉の変化を見て行う。
class OptionMenu
{
public:
    // 明るさ(表示ガンマ)の保存先。音量はAudioManagerがAudioSettings.jsonに保存する
    static constexpr const char* kSettingsPath = "Asset/Data/OptionSettings.json";

    static constexpr float kMinBrightnessGamma = 0.5f;
    static constexpr float kMaxBrightnessGamma = 2.5f;
    static constexpr float kDefaultBrightnessGamma = 1.0f;

    static void Open();
    // 閉じる時に設定(音量・明るさ)を保存する
    static void Close();
    static bool IsOpen() { return s_open; }

    // 開いている間だけウィンドウを描く。「閉じる」ボタンが押されるとClose()する
    static void Draw();

    // 起動時に一度呼ぶ(ShaderManager初期化後)
    static void LoadSettings();
    static void SaveSettings();

private:
    static void DrawVolumeTab();
    static void DrawBrightnessTab();
    static void DrawBrightnessPreview(float width);

    static bool s_open;
    static bool s_justOpened;
};
