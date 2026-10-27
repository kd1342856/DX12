#include "FieldWidgets.h"
#include "ImGui/imgui_internal.h"
#include "../../../Graphics/Shader/PhotometricUnits.h"

namespace
{
    // 仕様書「部品の決まり」の寸法
    constexpr float kKnobRadius = 8.0f;         // つまみ(直径16px)
    constexpr float kKnobRadiusHeld = 10.0f;    // ドラッグ中(直径20px)
    constexpr float kTrackThickness = 3.0f;
    constexpr float kInputWidth = 110.0f;
    constexpr float kMarkerWidth = 3.0f;
    constexpr float kLabelIndent = 8.0f;

    const ImU32 kWarningColor = IM_COL32(235, 110, 70, 255);
    const ImU32 kHighlightColor = IM_COL32(240, 160, 75, 70);

    bool s_isNextRowHighlighted = false;
    const std::function<void()>* s_pContextMenuExtension = nullptr;

    // カメラと同じ段
    constexpr float kShutterStops[] = { 1.0f, 1.0f / 2.0f, 1.0f / 4.0f, 1.0f / 8.0f, 1.0f / 15.0f, 1.0f / 30.0f, 1.0f / 60.0f,
        1.0f / 125.0f, 1.0f / 250.0f, 1.0f / 500.0f, 1.0f / 1000.0f, 1.0f / 2000.0f, 1.0f / 4000.0f };
    constexpr float kApertureStops[] = { 1.0f, 1.4f, 2.0f, 2.8f, 4.0f, 5.6f, 8.0f, 11.0f, 16.0f, 22.0f };

    bool IsLogarithmic(const FieldMeta& meta)
    {
        return (meta.flags & FieldFlags::kLogarithmic) != 0 || meta.kind == FieldKind::Shutter || meta.kind == FieldKind::Aperture;
    }

    // 値 → スライダーの位置(0～1)
    float ValueToRatio(const FieldMeta& meta, float value)
    {
        if (IsLogarithmic(meta) && meta.minValue > 0.0f)
        {
            const float safeValue = (std::max)(value, meta.minValue * 0.5f);
            return (std::log(safeValue) - std::log(meta.minValue)) / (std::log(meta.maxValue) - std::log(meta.minValue));
        }
        return (value - meta.minValue) / (meta.maxValue - meta.minValue);
    }

    // スライダーの位置(0～1) → 値
    float RatioToValue(const FieldMeta& meta, float ratio)
    {
        ratio = std::clamp(ratio, 0.0f, 1.0f);
        if (IsLogarithmic(meta) && meta.minValue > 0.0f)
        {
            return std::exp(std::log(meta.minValue) + (std::log(meta.maxValue) - std::log(meta.minValue)) * ratio);
        }
        return meta.minValue + (meta.maxValue - meta.minValue) * ratio;
    }

    template<size_t N>
    float SnapToStop(const float (&stops)[N], float value)
    {
        float best = stops[0];
        float bestDistance = FLT_MAX;
        for (float stop : stops)
        {
            const float distance = std::fabs(std::log(stop) - std::log((std::max)(value, 1e-6f)));
            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = stop;
            }
        }
        return best;
    }

    // 段の並びの中で、valueより大きい(isUp)・小さい次の段
    template<size_t N>
    float NextStop(const float (&stops)[N], float value, bool isUp)
    {
        float result = value;
        float bestDistance = FLT_MAX;
        for (float stop : stops)
        {
            const bool isCandidate = isUp ? (stop > value * 1.001f) : (stop < value * 0.999f);
            const float distance = std::fabs(stop - value);
            if (isCandidate && distance < bestDistance)
            {
                bestDistance = distance;
                result = stop;
            }
        }
        return result;
    }

    // 段のある値は段に吸着させる。整数は丸める
    float SnapValue(const FieldMeta& meta, float value)
    {
        switch (meta.kind)
        {
            case FieldKind::Shutter:
                return SnapToStop(kShutterStops, value);
            case FieldKind::Aperture:
                return SnapToStop(kApertureStops, value);
            case FieldKind::Integer:
                return std::round(value);
            default:
                return value;
        }
    }

    // 上下の矢印キーで1段ずつ増減するときの次の値
    float StepValue(const FieldMeta& meta, float value, bool isUp)
    {
        switch (meta.kind)
        {
            case FieldKind::Shutter:
                return NextStop(kShutterStops, value, isUp);
            case FieldKind::Aperture:
                return NextStop(kApertureStops, value, isUp);
            case FieldKind::Integer:
                return value + (isUp ? 1.0f : -1.0f);
            default:
                break;
        }
        if (IsLogarithmic(meta))
        {
            // 1/3段(2の1/3乗)ずつ
            const float factor = 1.259921f;
            return isUp ? value * factor : value / factor;
        }
        // 範囲の1/100を、1・2・5の切りのいい刻みに丸める
        const float rawStep = (meta.maxValue - meta.minValue) / 100.0f;
        const float magnitude = std::pow(10.0f, std::floor(std::log10((std::max)(rawStep, 1e-6f))));
        const float normalized = rawStep / magnitude;
        const float step = (normalized < 1.5f ? 1.0f : normalized < 3.5f ? 2.0f : 5.0f) * magnitude;
        return value + (isUp ? step : -step);
    }

    bool ParseValue(const FieldMeta& meta, const char* text, float& outValue)
    {
        // 「1/45」のような分数も受け付ける(シャッター速度)
        const char* slash = strchr(text, '/');
        char* end = nullptr;
        if (slash)
        {
            const float numerator = std::strtof(text, &end);
            const float denominator = std::strtof(slash + 1, &end);
            if (denominator == 0.0f)
            {
                return false;
            }
            outValue = numerator / denominator;
            return true;
        }
        const float value = std::strtof(text, &end);
        if (end == text)
        {
            return false;
        }
        outValue = (meta.kind == FieldKind::Integer) ? std::round(value) : value;
        return true;
    }

    ImU32 LinearToDisplayColor(const Math::Vector3& linear)
    {
        auto toSrgb = [](float v)
        {
            v = std::clamp(v, 0.0f, 1.0f);
            return v <= 0.0031308f ? v * 12.92f : 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
        };
        // 輝度1に正規化された色なので、いちばん明るい成分を1にそろえてから表示する
        const float maxComponent = (std::max)({ linear.x, linear.y, linear.z, 1e-4f });
        return ImGui::ColorConvertFloat4ToU32(ImVec4(toSrgb(linear.x / maxComponent), toSrgb(linear.y / maxComponent), toSrgb(linear.z / maxComponent), 1.0f));
    }

    // 丸いつまみのスライダー。標準のSliderFloatは使わない(仕様書「部品の決まり」)
    bool KnobSlider(const FieldMeta& meta, float& value, float width)
    {
        ImGuiWindow* pWindow = ImGui::GetCurrentWindow();
        if (pWindow->SkipItems)
        {
            return false;
        }
        const ImGuiID id = pWindow->GetID("##slider");
        const float height = ImGui::GetFrameHeight();
        const ImVec2 position = pWindow->DC.CursorPos;
        const ImRect bounds(position, ImVec2(position.x + width, position.y + height));
        ImGui::ItemSize(bounds);
        if (!ImGui::ItemAdd(bounds, id))
        {
            return false;
        }

        bool isHovered = false;
        bool isHeld = false;
        ImGui::ButtonBehavior(bounds, id, &isHovered, &isHeld);

        const float trackStartX = bounds.Min.x + kKnobRadiusHeld;
        const float trackEndX = bounds.Max.x - kKnobRadiusHeld;
        const float trackY = (bounds.Min.y + bounds.Max.y) * 0.5f;

        bool isChanged = false;
        if (isHeld)
        {
            const float ratio = (ImGui::GetIO().MousePos.x - trackStartX) / (std::max)(trackEndX - trackStartX, 1.0f);
            const float newValue = SnapValue(meta, RatioToValue(meta, ratio));
            if (newValue != value)
            {
                value = newValue;
                ImGui::MarkItemEdited(id);
                isChanged = true;
            }
        }

        const float rawRatio = ValueToRatio(meta, value);
        const bool isOutOfRange = rawRatio < -0.0001f || rawRatio > 1.0001f;
        const float ratio = std::clamp(rawRatio, 0.0f, 1.0f);
        const float knobX = trackStartX + (trackEndX - trackStartX) * ratio;

        ImDrawList* pDrawList = pWindow->DrawList;
        const ImU32 trackColor = ImGui::GetColorU32(ImGuiCol_FrameBg);
        const ImU32 fillColor = ImGui::GetColorU32(ImGuiCol_SliderGrabActive);
        if (meta.kind == FieldKind::Temperature)
        {
            // 色温度は、線そのものをロウソクの色から昼光の色までのグラデーションにする
            constexpr int kSegmentCount = 24;
            for (int segmentIndex = 0; segmentIndex < kSegmentCount; ++segmentIndex)
            {
                const float startRatio = static_cast<float>(segmentIndex) / kSegmentCount;
                const float endRatio = static_cast<float>(segmentIndex + 1) / kSegmentCount;
                const ImU32 startColor = LinearToDisplayColor(PhotometricUnits::ColorTemperatureToLinearRgb(RatioToValue(meta, startRatio)));
                const ImU32 endColor = LinearToDisplayColor(PhotometricUnits::ColorTemperatureToLinearRgb(RatioToValue(meta, endRatio)));
                const float x0 = trackStartX + (trackEndX - trackStartX) * startRatio;
                const float x1 = trackStartX + (trackEndX - trackStartX) * endRatio;
                pDrawList->AddRectFilledMultiColor(ImVec2(x0, trackY - kTrackThickness), ImVec2(x1, trackY + kTrackThickness), startColor, endColor, endColor, startColor);
            }
        }
        else
        {
            pDrawList->AddRectFilled(ImVec2(trackStartX, trackY - kTrackThickness * 0.5f), ImVec2(trackEndX, trackY + kTrackThickness * 0.5f), trackColor, kTrackThickness);
            pDrawList->AddRectFilled(ImVec2(trackStartX, trackY - kTrackThickness * 0.5f), ImVec2(knobX, trackY + kTrackThickness * 0.5f), fillColor, kTrackThickness);
        }

        // 段のある値は目盛りを打つ
        auto drawTicks = [&](const float* pStops, size_t count)
        {
            for (size_t stopIndex = 0; stopIndex < count; ++stopIndex)
            {
                const float tickX = trackStartX + (trackEndX - trackStartX) * std::clamp(ValueToRatio(meta, pStops[stopIndex]), 0.0f, 1.0f);
                pDrawList->AddLine(ImVec2(tickX, trackY + 4.0f), ImVec2(tickX, trackY + 7.0f), ImGui::GetColorU32(ImGuiCol_TextDisabled));
            }
        };
        if (meta.kind == FieldKind::Shutter)
        {
            drawTicks(kShutterStops, std::size(kShutterStops));
        }
        else if (meta.kind == FieldKind::Aperture)
        {
            drawTicks(kApertureStops, std::size(kApertureStops));
        }

        // つまみ。ドラッグ中は大きくして塗りつぶす。範囲の外の値は端で止めて注意の色にする
        const ImU32 knobColor = isOutOfRange ? kWarningColor
            : ImGui::GetColorU32(isHeld ? ImGuiCol_SliderGrabActive : (isHovered ? ImGuiCol_ButtonHovered : ImGuiCol_SliderGrab));
        if (isHeld)
        {
            pDrawList->AddCircleFilled(ImVec2(knobX, trackY), kKnobRadiusHeld, knobColor);
        }
        else
        {
            pDrawList->AddCircleFilled(ImVec2(knobX, trackY), kKnobRadius, ImGui::GetColorU32(ImGuiCol_WindowBg));
            pDrawList->AddCircleFilled(ImVec2(knobX, trackY), kKnobRadius - 2.0f, knobColor);
        }
        return isChanged;
    }

    struct InputCallbackData
    {
        const FieldMeta* pMeta;
        float* pValue;
        bool isStepped;
    };

    std::string FormatForInput(const FieldMeta& meta, float value)
    {
        return FieldWidgets::FormatValue(meta, value);
    }

    // 上下の矢印キー: 1段ずつ増減し、すぐに値へ入れる(Enterか、ほかをクリックするまでがUndoの1回)
    int InputCallback(ImGuiInputTextCallbackData* pData)
    {
        auto* pUser = static_cast<InputCallbackData*>(pData->UserData);
        if (pData->EventFlag == ImGuiInputTextFlags_CallbackHistory)
        {
            float current = *pUser->pValue;
            ParseValue(*pUser->pMeta, pData->Buf, current);
            const float stepped = StepValue(*pUser->pMeta, current, pData->EventKey == ImGuiKey_UpArrow);
            *pUser->pValue = stepped;
            pUser->isStepped = true;
            const std::string text = FormatForInput(*pUser->pMeta, stepped);
            pData->DeleteChars(0, pData->BufTextLen);
            pData->InsertChars(0, text.c_str());
            pData->SelectAll();
        }
        return 0;
    }

    // 単位つきの入力欄。標準の「Ctrl+クリックで入力」の代わりに、この欄に一本化する
    FieldWidgets::EditResult NumberInput(const FieldMeta& meta, float& value)
    {
        FieldWidgets::EditResult result;
        // 入力中だけ、入力中の文字を持っておく(同時に入力できる欄は1つだけ)
        static ImGuiID s_editingId = 0;
        static char s_editBuffer[64] = {};

        const ImGuiID id = ImGui::GetID("##input");
        const bool wasEditing = (ImGui::GetActiveID() == id && s_editingId == id);
        char localBuffer[64] = {};
        if (!wasEditing)
        {
            snprintf(localBuffer, sizeof(localBuffer), "%s", FormatForInput(meta, value).c_str());
        }
        char* pBuffer = wasEditing ? s_editBuffer : localBuffer;

        InputCallbackData callbackData = { &meta, &value, false };
        ImGui::SetNextItemWidth(kInputWidth);
        ImGui::InputText("##input", pBuffer, 64, ImGuiInputTextFlags_CharsScientific | ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_AutoSelectAll,
            InputCallback, &callbackData);
        if (ImGui::IsItemActivated())
        {
            s_editingId = id;
            memcpy(s_editBuffer, localBuffer, sizeof(s_editBuffer));
            result.activated = true;
        }
        if (callbackData.isStepped)
        {
            result.changed = true;
        }
        // 単位を欄の中に薄く出す
        if (meta.unit && meta.unit[0] != '\0')
        {
            const ImVec2 itemMax = ImGui::GetItemRectMax();
            const ImVec2 itemMin = ImGui::GetItemRectMin();
            const ImVec2 unitSize = ImGui::CalcTextSize(meta.unit);
            const ImGuiStyle& style = ImGui::GetStyle();
            ImGui::GetWindowDrawList()->AddText(ImVec2(itemMax.x - unitSize.x - style.FramePadding.x, itemMin.y + style.FramePadding.y),
                ImGui::GetColorU32(ImGuiCol_TextDisabled), meta.unit);
        }
        if (ImGui::IsItemDeactivated())
        {
            // Enterか、ほかをクリックしたら決定。Escは入力の前の文字に戻るので、値は変わらない
            float parsed = value;
            if (ParseValue(meta, pBuffer, parsed) && parsed != value)
            {
                value = parsed;
                result.changed = true;
            }
            if (result.changed || ImGui::IsItemDeactivatedAfterEdit())
            {
                result.committed = true;
            }
            s_editingId = 0;
        }
        return result;
    }

    std::string DefaultText(const FieldMeta& meta, const std::string& valueText)
    {
        return (meta.unit && meta.unit[0] != '\0') ? valueText + " " + meta.unit : valueText;
    }

    // 1行を描く。左の列に表示名と印、右の列に部品。説明の吹き出しと「初期値に戻す」は共通
    template<typename TDrawControl, typename TReset>
    FieldWidgets::EditResult DrawRow(const FieldMeta& meta, bool isModified, bool isOutOfRange, const std::string& defaultText,
        TDrawControl&& drawControl, TReset&& resetToDefault)
    {
        ImGui::PushID(meta.key);
        ImGui::TableNextRow();
        if (s_isNextRowHighlighted)
        {
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, kHighlightColor);
            s_isNextRowHighlighted = false;
        }

        ImGui::TableSetColumnIndex(0);
        const ImVec2 rowStart = ImGui::GetCursorScreenPos();
        const float frameHeight = ImGui::GetFrameHeight();
        if (isModified)
        {
            ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(rowStart.x, rowStart.y + 2.0f), ImVec2(rowStart.x + kMarkerWidth, rowStart.y + frameHeight - 2.0f),
                FieldWidgets::GetModifiedColor());
        }
        ImGui::SetCursorScreenPos(ImVec2(rowStart.x + kLabelIndent, rowStart.y + ImGui::GetStyle().FramePadding.y));
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(meta.label);
        ImGui::PopTextWrapPos();
        const bool isLabelHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled);
        ImGui::OpenPopupOnItemClick("##default", ImGuiPopupFlags_MouseButtonRight);

        ImGui::TableSetColumnIndex(1);
        ImGui::BeginGroup();
        FieldWidgets::EditResult result = drawControl();
        ImGui::EndGroup();
        const bool isControlHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled);

        if ((isLabelHovered || isControlHovered) && !ImGui::IsAnyItemActive())
        {
            if (ImGui::BeginTooltip())
            {
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28.0f);
                ImGui::TextUnformatted(meta.description);
                ImGui::Spacing();
                if (meta.kind != FieldKind::Toggle && meta.kind != FieldKind::Color && meta.kind != FieldKind::ColorHDR && meta.kind != FieldKind::Direction)
                {
                    const std::string rangeText = FieldWidgets::FormatValue(meta, meta.minValue) + u8" ～ " + FieldWidgets::FormatValue(meta, meta.maxValue);
                    ImGui::TextDisabled(u8"スライダーの範囲: %s", DefaultText(meta, rangeText).c_str());
                }
                ImGui::TextDisabled(u8"初期値: %s", defaultText.c_str());
                if (isOutOfRange)
                {
                    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(kWarningColor), u8"今の値はスライダーの範囲の外(入力欄で入れた値)。絵が崩れることがある");
                }
                ImGui::TextDisabled(u8"右クリックで初期値に戻す");
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }
        }

        if (ImGui::BeginPopup("##default"))
        {
            const std::string itemText = std::string(u8"初期値に戻す (") + defaultText + ")";
            if (ImGui::MenuItem(itemText.c_str(), nullptr, false, isModified))
            {
                resetToDefault();
                result.changed = true;
                result.committed = true;
                result.reset = true;
            }
            if (s_pContextMenuExtension && *s_pContextMenuExtension)
            {
                ImGui::Separator();
                (*s_pContextMenuExtension)();
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();
        return result;
    }

    bool IsSame(const DirectX::XMFLOAT3& lhs, const DirectX::XMFLOAT3& rhs)
    {
        return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
    }
}

ImU32 FieldWidgets::GetModifiedColor()
{
    return IM_COL32(240, 160, 75, 255);
}

void FieldWidgets::SetNextRowHighlight(bool isHighlighted)
{
    s_isNextRowHighlighted = isHighlighted;
}

void FieldWidgets::SetContextMenuExtension(const std::function<void()>* pExtension)
{
    s_pContextMenuExtension = pExtension;
}

bool FieldWidgets::BeginFieldTable(const char* id)
{
    if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp))
    {
        return false;
    }
    ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthStretch, 0.42f);
    ImGui::TableSetupColumn("control", ImGuiTableColumnFlags_WidthStretch, 0.58f);
    return true;
}

void FieldWidgets::EndFieldTable()
{
    ImGui::EndTable();
}

std::string FieldWidgets::FormatValue(const FieldMeta& meta, float value)
{
    char buffer[64] = {};
    switch (meta.kind)
    {
        case FieldKind::Integer:
            snprintf(buffer, sizeof(buffer), "%d", static_cast<int>(std::round(value)));
            return buffer;
        case FieldKind::Shutter:
            if (value > 0.0f && value < 1.0f)
            {
                snprintf(buffer, sizeof(buffer), "1/%.0f", 1.0f / value);
                return buffer;
            }
            break;
        case FieldKind::Aperture:
            snprintf(buffer, sizeof(buffer), "%.1f", value);
            return buffer;
        case FieldKind::Temperature:
            snprintf(buffer, sizeof(buffer), "%.0f", value);
            return buffer;
        default:
            break;
    }
    // 大きな値は指数表記にしない
    if (std::fabs(value) >= 1000.0f)
    {
        snprintf(buffer, sizeof(buffer), "%.0f", value);
    }
    else
    {
        snprintf(buffer, sizeof(buffer), "%.4g", value);
    }
    return buffer;
}

FieldWidgets::EditResult FieldWidgets::Number(const FieldMeta& meta, float& value, float defaultValue)
{
    const float ratio = ValueToRatio(meta, value);
    const bool isOutOfRange = ratio < -0.0001f || ratio > 1.0001f;
    return DrawRow(meta, value != defaultValue, isOutOfRange, DefaultText(meta, FormatValue(meta, defaultValue)), [&]()
    {
        EditResult result;
        const float sliderWidth = (std::max)(ImGui::GetContentRegionAvail().x - kInputWidth - ImGui::GetStyle().ItemSpacing.x, 40.0f);
        if (KnobSlider(meta, value, sliderWidth))
        {
            result.changed = true;
        }
        // スライダーの上でも右クリックで「初期値に戻す」を開く
        ImGui::OpenPopupOnItemClick("##default", ImGuiPopupFlags_MouseButtonRight);
        result.activated |= ImGui::IsItemActivated();
        result.committed |= ImGui::IsItemDeactivatedAfterEdit();
        ImGui::SameLine();
        const EditResult inputResult = NumberInput(meta, value);
        result.changed |= inputResult.changed;
        result.activated |= inputResult.activated;
        result.committed |= inputResult.committed;
        return result;
    }, [&]()
    {
        value = defaultValue;
    });
}

FieldWidgets::EditResult FieldWidgets::Integer(const FieldMeta& meta, int& value, int defaultValue)
{
    float floatValue = static_cast<float>(value);
    FieldMeta integerMeta = meta;
    integerMeta.kind = FieldKind::Integer;
    const EditResult result = Number(integerMeta, floatValue, static_cast<float>(defaultValue));
    value = static_cast<int>(std::round(floatValue));
    return result;
}

FieldWidgets::EditResult FieldWidgets::Toggle(const FieldMeta& meta, bool& value, bool defaultValue)
{
    return DrawRow(meta, value != defaultValue, false, defaultValue ? "ON" : "OFF", [&]()
    {
        EditResult result;
        if (ImGui::Checkbox("##toggle", &value))
        {
            result.changed = true;
            result.activated = true;
            result.committed = true;
        }
        return result;
    }, [&]()
    {
        value = defaultValue;
    });
}

FieldWidgets::EditResult FieldWidgets::Color(const FieldMeta& meta, DirectX::XMFLOAT3& value, const DirectX::XMFLOAT3& defaultValue)
{
    char defaultText[64] = {};
    snprintf(defaultText, sizeof(defaultText), "%.3g, %.3g, %.3g", defaultValue.x, defaultValue.y, defaultValue.z);
    return DrawRow(meta, !IsSame(value, defaultValue), false, defaultText, [&]()
    {
        EditResult result;
        ImGuiColorEditFlags flags = ImGuiColorEditFlags_Float | ImGuiColorEditFlags_DisplayRGB;
        if (meta.kind == FieldKind::ColorHDR)
        {
            flags |= ImGuiColorEditFlags_HDR;
        }
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::ColorEdit3("##color", &value.x, flags))
        {
            result.changed = true;
        }
        result.activated = ImGui::IsItemActivated();
        result.committed = ImGui::IsItemDeactivatedAfterEdit();
        return result;
    }, [&]()
    {
        value = defaultValue;
    });
}

FieldWidgets::EditResult FieldWidgets::Direction(const FieldMeta& meta, DirectX::XMFLOAT3& value, const DirectX::XMFLOAT3& defaultValue)
{
    char defaultText[64] = {};
    snprintf(defaultText, sizeof(defaultText), "%.3g, %.3g, %.3g", defaultValue.x, defaultValue.y, defaultValue.z);
    return DrawRow(meta, !IsSame(value, defaultValue), false, defaultText, [&]()
    {
        EditResult result;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::DragFloat3("##direction", &value.x, 0.01f, meta.minValue, meta.maxValue, "%.3f"))
        {
            // 向きなので長さを1にそろえる
            const float length = std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
            if (length > 0.0001f)
            {
                value = { value.x / length, value.y / length, value.z / length };
            }
            result.changed = true;
        }
        result.activated = ImGui::IsItemActivated();
        result.committed = ImGui::IsItemDeactivatedAfterEdit();
        return result;
    }, [&]()
    {
        value = defaultValue;
    });
}

void FieldWidgets::ComputedValue(const char* label, const char* valueText, const char* sourceText)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + kLabelIndent);
    ImGui::TextDisabled("%s", label);
    ImGui::TableSetColumnIndex(1);
    ImGui::TextUnformatted(valueText);
    ImGui::SameLine();
    ImGui::TextDisabled("%s", sourceText);
}
