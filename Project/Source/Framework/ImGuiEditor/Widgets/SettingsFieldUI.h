#pragma once
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include "FieldWidgets.h"

//=============================
//
// VisitFieldsのUI用のVisitor
//
//   設定の構造体を渡すと、VisitFieldsに並べた順にセクションごとの表を作り、
//   「基本」の値を出して、「詳細」の値は折りたたみの中に出す。値の描き方はFieldMeta::kindで決まり、
//   パネルごとに描き方を書かない(仕様書「5. 部品の決まり」)。
//
//=============================
namespace SettingsFieldUI
{
    struct Options
    {
        // 値を変えたときに呼ぶ(パストレーサーの積み上げを捨てる、など)
        std::function<void(const char* key)> onChanged;
        // その値の行の後に、計算した値などを足す(表の中なので FieldWidgets::ComputedValue を使う)
        std::function<void(const char* key)> afterField;
    };

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

    namespace Detail
    {
        inline FieldWidgets::EditResult DrawField(const FieldMeta& meta, float& value, const float& defaultValue)
        {
            return FieldWidgets::Number(meta, value, defaultValue);
        }

        inline FieldWidgets::EditResult DrawField(const FieldMeta& meta, int& value, const int& defaultValue)
        {
            return FieldWidgets::Integer(meta, value, defaultValue);
        }

        inline FieldWidgets::EditResult DrawField(const FieldMeta& meta, bool& value, const bool& defaultValue)
        {
            return FieldWidgets::Toggle(meta, value, defaultValue);
        }

        inline FieldWidgets::EditResult DrawField(const FieldMeta& meta, DirectX::XMFLOAT3& value, const DirectX::XMFLOAT3& defaultValue)
        {
            if (meta.kind == FieldKind::Direction)
            {
                return FieldWidgets::Direction(meta, value, defaultValue);
            }
            return FieldWidgets::Color(meta, value, defaultValue);
        }

        struct Entry
        {
            FieldMeta meta;
            std::function<FieldWidgets::EditResult()> draw;
        };
    }

    // 値を変えたらtrue。targetKeyは表のIDと、Undo/Redoで値を探すときの名前(PostProcessなど)
    template<typename TSettings>
    bool Draw(TSettings& settings, const char* targetKey, const Options& options = {})
    {
        // 1回目: 値を集める(オンオフの値は、ほかの値の条件に使うので控える)
        std::vector<Detail::Entry> entries;
        std::unordered_map<std::string, const bool*> toggles;
        settings.VisitFields([&](const FieldMeta& meta, auto& field)
        {
            using FieldType = std::remove_reference_t<decltype(field)>;
            if constexpr (std::is_same_v<FieldType, bool>)
            {
                toggles[meta.key] = &field;
            }
            auto* pField = &field;
            const auto* pDefault = &GetDefaultField(settings, field);
            entries.push_back({ meta, [meta, pField, pDefault]()
            {
                return Detail::DrawField(meta, *pField, *pDefault);
            } });
        });

        auto isActive = [&](const FieldMeta& meta)
        {
            if (!meta.condition || meta.condition[0] == '\0')
            {
                return true;
            }
            auto it = toggles.find(meta.condition);
            return it == toggles.end() || *it->second == meta.conditionValue;
        };

        bool isAnyChanged = false;
        auto drawEntry = [&](const Detail::Entry& entry)
        {
            const bool active = isActive(entry.meta);
            if (!active && entry.meta.hideWhenInactive)
            {
                return;
            }
            ImGui::BeginDisabled(!active);
            const FieldWidgets::EditResult result = entry.draw();
            ImGui::EndDisabled();
            if (result.changed)
            {
                SetDirty(settings, nullptr);
                isAnyChanged = true;
                if (options.onChanged)
                {
                    options.onChanged(entry.meta.key);
                }
            }
            if (options.afterField)
            {
                options.afterField(entry.meta.key);
            }
        };

        // 2回目: セクションごとに、基本の値を出し、詳細の値は折りたたみの中に出す
        ImGui::PushID(targetKey);
        size_t sectionStart = 0;
        while (sectionStart < entries.size())
        {
            size_t sectionEnd = sectionStart + 1;
            while (sectionEnd < entries.size() && (entries[sectionEnd].meta.section == nullptr || entries[sectionEnd].meta.section[0] == '\0'))
            {
                ++sectionEnd;
            }
            const char* pSection = entries[sectionStart].meta.section;
            ImGui::PushID(static_cast<int>(sectionStart));
            if (pSection && pSection[0] != '\0')
            {
                ImGui::SeparatorText(pSection);
            }
            if (FieldWidgets::BeginFieldTable("##basic"))
            {
                for (size_t entryIndex = sectionStart; entryIndex < sectionEnd; ++entryIndex)
                {
                    if ((entries[entryIndex].meta.flags & FieldFlags::kAdvanced) == 0)
                    {
                        drawEntry(entries[entryIndex]);
                    }
                }
                FieldWidgets::EndFieldTable();
            }

            bool hasAdvanced = false;
            for (size_t entryIndex = sectionStart; entryIndex < sectionEnd; ++entryIndex)
            {
                const FieldMeta& meta = entries[entryIndex].meta;
                if ((meta.flags & FieldFlags::kAdvanced) != 0 && !(meta.hideWhenInactive && !isActive(meta)))
                {
                    hasAdvanced = true;
                }
            }
            if (hasAdvanced && ImGui::TreeNodeEx(u8"詳細", ImGuiTreeNodeFlags_SpanAvailWidth))
            {
                if (FieldWidgets::BeginFieldTable("##advanced"))
                {
                    for (size_t entryIndex = sectionStart; entryIndex < sectionEnd; ++entryIndex)
                    {
                        if ((entries[entryIndex].meta.flags & FieldFlags::kAdvanced) != 0)
                        {
                            drawEntry(entries[entryIndex]);
                        }
                    }
                    FieldWidgets::EndFieldTable();
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
            sectionStart = sectionEnd;
        }
        ImGui::PopID();
        return isAnyChanged;
    }
}
