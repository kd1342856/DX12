#include "PostProcessPanel.h"
#include "../EditorContext.h"
#include "ImGui/imgui.h"
#include "../Widgets/SettingsFieldUI.h"
#include "../EditorPreferences.h"
#include "../EditHistory.h"
#include "../../../Graphics/Shader/ShaderManager/ShaderManager.h"
#include "../../../Graphics/Shader/PhotometricUnits.h"
#include "../../../Graphics/Renderer/TemporalAA.h"
#include "../../../Graphics/Renderer/AutoExposure.h"
#include "../../../Graphics/Renderer/MotionBlur.h"

namespace
{
    // エディターのカメラだけに掛ける表示ガンマ。シーンには保存せず、エディターの設定に保存する
    void DrawEditorDisplayGamma()
    {
        static const FieldMeta s_meta = Field("EditorDisplayGamma", u8"エディターの表示ガンマ",
            u8"エディターのカメラで描くときだけ、暗いところを持ち上げる。ゲームの画面には効かない(ゲームはオプション画面の明るさだけ)。\n"
            u8"シーンには保存せず、Saved/EditorSettings.json に保存する。目安：1.8〜2.2。")
            .Range(1.0f, 3.0f).Section(u8"エディターの見え方");
        ImGui::SeparatorText(s_meta.section);
        if (!FieldWidgets::BeginFieldTable("##editorView"))
        {
            return;
        }
        // Undo/Redoで書き戻せるよう、エディターの設定を対象として登録しておく
        static const bool s_isRegistered = []()
        {
            EditHistory::Instance().RegisterTarget("Editor", [](const std::string& fieldKey, const nlohmann::json& value)
            {
                if (fieldKey != "EditorDisplayGamma" || !value.is_number())
                {
                    return false;
                }
                ShaderManager::Instance().SetEditorDisplayGamma(value.get<float>());
                EditorPreferences::Save();
                return true;
            });
            return true;
        }();
        (void)s_isRegistered;
        static float s_beforeGamma = 2.0f;

        float displayGamma = ShaderManager::Instance().GetEditorDisplayGamma();
        const float beforeDraw = displayGamma;
        const FieldWidgets::EditResult result = FieldWidgets::Number(s_meta, displayGamma, 2.0f);
        if (result.activated && !result.committed)
        {
            s_beforeGamma = beforeDraw;
        }
        if (result.changed)
        {
            ShaderManager::Instance().SetEditorDisplayGamma(displayGamma);
        }
        if (result.committed)
        {
            EditorPreferences::Save();
            const float before = result.activated ? beforeDraw : s_beforeGamma;
            EditHistory::Instance().PushChange({ "Editor", s_meta.key, s_meta.label, before, displayGamma });
        }
        FieldWidgets::EndFieldTable();
    }

    void NotAvailableRow(bool isAvailable)
    {
        if (!isAvailable)
        {
            FieldWidgets::ComputedValue(u8"使えるか", u8"このGPUでは使えません", "");
        }
    }
}

void PostProcessPanel::Draw(EditorContext& ctx)
{
    if (!ImGui::CollapsingHeader(u8"露出とポストプロセス###Post Process Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    DrawEditorDisplayGamma();

    if (!ctx.PostProcess)
    {
        return;
    }
    PostProcessSettings& settings = *ctx.PostProcess;
    SettingsFieldUI::Options options;
    options.afterField = [&settings](const char* key)
    {
        char text[64] = {};
        if (strcmp(key, "ISO") == 0 && settings.UseCameraSettings)
        {
            snprintf(text, sizeof(text), "%.2f EV100", PhotometricUnits::ComputeEV100(settings.Aperture, settings.ShutterSpeed, settings.ISO));
            FieldWidgets::ComputedValue(u8"露出(計算値)", text, u8"絞り・シャッター・ISOから");
        }
        else if (strcmp(key, "EnableAutoExposure") == 0)
        {
            NotAvailableRow(AutoExposure::Instance().IsAvailable());
            if (settings.EnableAutoExposure)
            {
                const AutoExposure& autoExposure = AutoExposure::Instance();
                snprintf(text, sizeof(text), "%.2f EV100", autoExposure.GetEV100());
                char source[64] = {};
                snprintf(source, sizeof(source), u8"(合わせ先 %.2f)", autoExposure.GetTargetEV100());
                FieldWidgets::ComputedValue(u8"今の露出", text, source);
            }
        }
        else if (strcmp(key, "EnableTAA") == 0)
        {
            NotAvailableRow(TemporalAA::Instance().IsAvailable());
        }
        else if (strcmp(key, "EnableMotionBlur") == 0)
        {
            NotAvailableRow(MotionBlur::Instance().IsAvailable());
            if (settings.UseCameraSettings)
            {
                FieldWidgets::ComputedValue(u8"ぼけの長さ", u8"シャッター速度で決まる", "");
            }
        }
    };
    SettingsFieldUI::Draw(settings, "PostProcess", options);
}
