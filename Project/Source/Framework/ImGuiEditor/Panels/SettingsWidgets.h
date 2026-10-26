#pragma once
#include <cstring>
#include <string>
#include <tuple>
#include "ImGui/imgui.h"

//=============================
//
// シェーダーエディターのパネルで使う、設定の値の補助
//
//   ウィジェットの直後に DefaultMark(*ctx.Renderer, ctx.Renderer->SSRStepSize) のように呼ぶと、
//     ・初期値(構造体の既定値)から変えた値は、ウィジェットの左に印を付ける
//     ・右クリックのメニューで「初期値に戻す」(初期値も表示する)
//   ができる。初期値は構造体の既定値で、フィールドの位置(オフセット)から引くので、値ごとに書き足すものは無い。
//
//=============================
namespace SettingsWidgets
{
    // 初期値から変えた値の印の色
    inline ImU32 GetModifiedColor()
    {
        return IM_COL32(240, 160, 75, 255);
    }

    template<typename TSettings>
    const TSettings& GetDefaults()
    {
        static const TSettings s_defaults{};
        return s_defaults;
    }

    // settingsの中のfieldと同じ位置にある、初期値の側のフィールド
    template<typename TSettings, typename TField>
    const TField& GetDefaultField(const TSettings& settings, const TField& field)
    {
        const size_t offset = static_cast<size_t>(reinterpret_cast<const char*>(&field) - reinterpret_cast<const char*>(&settings));
        IM_ASSERT(offset + sizeof(TField) <= sizeof(TSettings) && "fieldはsettingsのメンバーを渡すこと");
        return *reinterpret_cast<const TField*>(reinterpret_cast<const char*>(&GetDefaults<TSettings>()) + offset);
    }

    inline bool IsSame(bool lhs, bool rhs)
    {
        return lhs == rhs;
    }

    inline bool IsSame(int lhs, int rhs)
    {
        return lhs == rhs;
    }

    inline bool IsSame(float lhs, float rhs)
    {
        return lhs == rhs;
    }

    inline bool IsSame(const DirectX::XMFLOAT3& lhs, const DirectX::XMFLOAT3& rhs)
    {
        return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
    }

    inline bool IsSame(const DirectX::XMFLOAT4& lhs, const DirectX::XMFLOAT4& rhs)
    {
        return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z && lhs.w == rhs.w;
    }

    inline std::string FormatValue(bool value)
    {
        return value ? "ON" : "OFF";
    }

    inline std::string FormatValue(int value)
    {
        return std::to_string(value);
    }

    inline std::string FormatValue(float value)
    {
        char buffer[32] = {};
        snprintf(buffer, sizeof(buffer), "%g", value);
        return buffer;
    }

    inline std::string FormatValue(const DirectX::XMFLOAT3& value)
    {
        return FormatValue(value.x) + ", " + FormatValue(value.y) + ", " + FormatValue(value.z);
    }

    inline std::string FormatValue(const DirectX::XMFLOAT4& value)
    {
        return FormatValue(value.x) + ", " + FormatValue(value.y) + ", " + FormatValue(value.z) + ", " + FormatValue(value.w);
    }

    template<typename TSettings>
    void SetDirty(TSettings& settings, decltype(settings.IsDirty)*)
    {
        settings.IsDirty = true;
    }

    // IsDirtyを持たない設定(SSAOSettings)は毎フレーム読まれるので何もしない
    template<typename TSettings>
    void SetDirty(TSettings&, ...)
    {
    }

    // 直前のウィジェットに、初期値から変えた印と、右クリックの「初期値に戻す」を付ける。
    // 1つのウィジェットで2つの値を編集するとき(範囲のスライダーなど)は、両方を渡す。戻したらtrue(IsDirtyも立てる)
    template<typename TSettings, typename... TFields>
    bool DefaultMark(TSettings& settings, TFields&... fields)
    {
        const bool isModified = (!IsSame(fields, GetDefaultField(settings, fields)) || ...);
        if (isModified)
        {
            // ウィジェットの左の余白に細い縦線を引く
            const ImVec2 itemMin = ImGui::GetItemRectMin();
            const ImVec2 itemMax = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(itemMin.x - 6.0f, itemMin.y + 1.0f), ImVec2(itemMin.x - 3.0f, itemMax.y - 1.0f), GetModifiedColor());
        }

        // ColorEdit3やDragFloat3などはグループとして描かれ、直前の項目にIDが無いので、メニューのIDは値のアドレスから作る
        char popupId[32] = {};
        const size_t firstOffset = static_cast<size_t>(reinterpret_cast<const char*>(&std::get<0>(std::tie(fields...))) - reinterpret_cast<const char*>(&settings));
        snprintf(popupId, sizeof(popupId), "##Default%zu_%p", firstOffset, static_cast<const void*>(&settings));
        bool isReset = false;
        if (ImGui::BeginPopupContextItem(popupId))
        {
            std::string defaultText;
            ((defaultText += (defaultText.empty() ? "" : " / ") + FormatValue(GetDefaultField(settings, fields))), ...);
            const std::string label = std::string(u8"初期値に戻す (") + defaultText + ")";
            if (ImGui::MenuItem(label.c_str(), nullptr, false, isModified))
            {
                ((fields = GetDefaultField(settings, fields)), ...);
                SetDirty(settings, nullptr);
                isReset = true;
            }
            ImGui::EndPopup();
        }
        return isReset;
    }
}
