#pragma once
#include <chrono>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include "FieldWidgets.h"
#include "../EditHistory.h"
#include "../EditorPreferences.h"

//=============================
//
// VisitFieldsのUI用のVisitor
//
//   設定の構造体を渡すと、VisitFieldsに並べた順にセクションごとの表を作り、
//   「基本」の値を出して、「詳細」の値は折りたたみの中に出す。値の描き方はFieldMeta::kindで決まり、
//   パネルごとに描き方を書かない(仕様書「5. 部品の決まり」)。
//   検索・変えた値の一覧からの移動・よく使う値のピン留め・Undo/Redoの記録もここで行う。
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
        // シェーダーエディターの検索で絞り込むか(インスペクターのライトなどはfalse)
        bool isSearchable = true;
    };

    // 検索と「変えた値の一覧」からの移動の状態。シェーダーエディターに1つ
    struct UIState
    {
        std::string search;      // 小文字にした検索語。空なら検索していない
        std::string focusTarget; // 一覧から飛ぶ先
        std::string focusKey;
        bool isFocusScrolled = false;
        std::chrono::steady_clock::time_point focusTime;
    };

    inline UIState& State()
    {
        static UIState s_state;
        return s_state;
    }

    inline bool IsSearching()
    {
        return !State().search.empty();
    }

    // 検索語を設定する(大文字と小文字は区別しない)
    inline void SetSearch(const std::string& text)
    {
        std::string lower = text;
        for (char& ch : lower)
        {
            if (ch >= 'A' && ch <= 'Z')
            {
                ch = static_cast<char>(ch - 'A' + 'a');
            }
        }
        State().search = lower;
    }

    // 「変えた値の一覧」から、その値まで飛ぶ(パネルと詳細を開き、スクロールして行を一瞬強調する)
    inline void RequestFocus(const std::string& target, const std::string& key)
    {
        UIState& state = State();
        state.focusTarget = target;
        state.focusKey = key;
        state.isFocusScrolled = false;
        state.focusTime = std::chrono::steady_clock::now();
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
            if (meta.kind == FieldKind::Direction || meta.kind == FieldKind::Vector)
            {
                return FieldWidgets::Direction(meta, value, defaultValue);
            }
            return FieldWidgets::Color(meta, value, defaultValue);
        }

        // Undo/Redoに記録する値の形(ShaderSettingsIOと同じ)
        inline nlohmann::json ToJson(bool value)
        {
            return value;
        }

        inline nlohmann::json ToJson(int value)
        {
            return value;
        }

        inline nlohmann::json ToJson(float value)
        {
            return value;
        }

        inline nlohmann::json ToJson(const DirectX::XMFLOAT3& value)
        {
            return nlohmann::json::array({ value.x, value.y, value.z });
        }

        inline void FromJson(const nlohmann::json& json, bool& outValue)
        {
            if (json.is_boolean())
            {
                outValue = json.get<bool>();
            }
        }

        inline void FromJson(const nlohmann::json& json, int& outValue)
        {
            if (json.is_number())
            {
                outValue = json.get<int>();
            }
        }

        inline void FromJson(const nlohmann::json& json, float& outValue)
        {
            if (json.is_number())
            {
                outValue = json.get<float>();
            }
        }

        inline void FromJson(const nlohmann::json& json, DirectX::XMFLOAT3& outValue)
        {
            if (json.is_array() && json.size() >= 3)
            {
                outValue = { json[0].get<float>(), json[1].get<float>(), json[2].get<float>() };
            }
        }

        // 操作を始めたときの値。操作を終えたときに、変更前としてUndoに積む
        inline std::unordered_map<std::string, nlohmann::json>& PendingBefore()
        {
            static std::unordered_map<std::string, nlohmann::json> s_pending;
            return s_pending;
        }

        inline bool ContainsLower(const char* pText, const std::string& lowerQuery)
        {
            if (!pText)
            {
                return false;
            }
            std::string lower = pText;
            for (char& ch : lower)
            {
                if (ch >= 'A' && ch <= 'Z')
                {
                    ch = static_cast<char>(ch - 'A' + 'a');
                }
            }
            return lower.find(lowerQuery) != std::string::npos;
        }

        struct Entry
        {
            FieldMeta meta;
            std::function<FieldWidgets::EditResult()> draw;
            std::function<nlohmann::json()> capture;
        };

        struct Collected
        {
            std::vector<Entry> entries;
            std::unordered_map<std::string, const bool*> toggles;

            bool IsActive(const FieldMeta& meta) const
            {
                if (!meta.condition || meta.condition[0] == '\0')
                {
                    return true;
                }
                auto it = toggles.find(meta.condition);
                return it == toggles.end() || *it->second == meta.conditionValue;
            }

            bool IsShown(const FieldMeta& meta) const
            {
                return !(meta.hideWhenInactive && !IsActive(meta));
            }
        };

        template<typename TSettings>
        Collected Collect(TSettings& settings)
        {
            Collected collected;
            settings.VisitFields([&](const FieldMeta& meta, auto& field)
            {
                using FieldType = std::remove_reference_t<decltype(field)>;
                if constexpr (std::is_same_v<FieldType, bool>)
                {
                    collected.toggles[meta.key] = &field;
                }
                auto* pField = &field;
                const auto* pDefault = &GetDefaultField(settings, field);
                collected.entries.push_back({ meta, [meta, pField, pDefault]()
                {
                    return DrawField(meta, *pField, *pDefault);
                }, [pField]()
                {
                    return ToJson(*pField);
                } });
            });
            return collected;
        }

        // 検索に当たるか(表示名・英語のキー・見出し)
        inline bool MatchesSearch(const FieldMeta& meta, const char* pSection)
        {
            const std::string& query = State().search;
            return query.empty() || ContainsLower(meta.label, query) || ContainsLower(meta.key, query) || ContainsLower(pSection, query);
        }

        inline bool IsFocused(const char* targetKey, const FieldMeta& meta)
        {
            const UIState& state = State();
            return state.focusTarget == targetKey && state.focusKey == meta.key;
        }

        // 1行を描く。Undoの記録・ピン留めのメニュー・一覧から飛んだときの強調もここ
        template<typename TSettings>
        bool DrawEntry(TSettings& settings, const char* targetKey, const Collected& collected, const Entry& entry, const Options& options,
            const std::function<void()>& extraMenu)
        {
            const bool active = collected.IsActive(entry.meta);
            if (!collected.IsShown(entry.meta))
            {
                return false;
            }

            // 一覧から飛んできた行は、1.5秒だけ強調し、最初の1回だけスクロールする
            UIState& state = State();
            const bool isFocused = IsFocused(targetKey, entry.meta) && std::chrono::steady_clock::now() - state.focusTime < std::chrono::milliseconds(1500);
            FieldWidgets::SetNextRowHighlight(isFocused);

            // 右クリックのメニューに「よく使う値」を足す
            const std::string target = targetKey;
            const std::string key = entry.meta.key;
            // よく使う値に入れられるのはシェーダーの設定だけ(シーンの物は「種類#番号」で、シーンごとに変わるため)
            const bool isPinnable = target.find('#') == std::string::npos;
            const std::function<void()> pinMenu = [&]()
            {
                const bool isPinned = EditorPreferences::IsPinned(target, key);
                if (isPinnable && ImGui::MenuItem(isPinned ? u8"よく使う値から外す" : u8"よく使う値に追加"))
                {
                    EditorPreferences::TogglePin(target, key);
                }
                if (extraMenu)
                {
                    extraMenu();
                }
            };
            FieldWidgets::SetContextMenuExtension(&pinMenu);

            ImGui::BeginDisabled(!active);
            // オンオフや「初期値に戻す」は描く中で値が変わるので、変更前の値を描く前に控える
            const nlohmann::json beforeDraw = entry.capture();
            const FieldWidgets::EditResult result = entry.draw();
            ImGui::EndDisabled();
            FieldWidgets::SetContextMenuExtension(nullptr);

            if (isFocused && !state.isFocusScrolled)
            {
                ImGui::SetScrollHereY(0.4f);
                state.isFocusScrolled = true;
            }

            // Undo/Redo: 操作の開始で変更前を控え、終わったら1回分として積む
            const std::string pendingKey = target + "/" + key;
            auto& pending = PendingBefore();
            if (result.activated && !result.committed)
            {
                pending[pendingKey] = beforeDraw;
            }
            if (result.committed)
            {
                auto it = pending.find(pendingKey);
                const nlohmann::json before = (it != pending.end()) ? it->second : beforeDraw;
                if (it != pending.end())
                {
                    pending.erase(it);
                }
                EditHistory::Instance().PushChange({ target, key, entry.meta.label, before, entry.capture() });
            }
            if (result.changed)
            {
                SetDirty(settings, nullptr);
                if (options.onChanged)
                {
                    options.onChanged(entry.meta.key);
                }
            }
            if (options.afterField)
            {
                options.afterField(entry.meta.key);
            }
            return result.changed;
        }
    }

    // 値1つをJSONから書き込む(シーンの物のUndo/Redo用)。見つからなければfalse
    template<typename TSettings>
    bool ApplyFieldJson(TSettings& settings, const std::string& key, const nlohmann::json& value)
    {
        bool isFound = false;
        settings.VisitFields([&](const FieldMeta& meta, auto& field)
        {
            if (!isFound && key == meta.key)
            {
                isFound = true;
                Detail::FromJson(value, field);
            }
        });
        if (isFound)
        {
            SetDirty(settings, nullptr);
        }
        return isFound;
    }

    // 検索に当たる値があるか(検索していなければtrue)
    template<typename TSettings>
    bool HasSearchMatch(TSettings& settings)
    {
        if (!IsSearching())
        {
            return true;
        }
        const Detail::Collected collected = Detail::Collect(settings);
        const char* pSection = "";
        for (const Detail::Entry& entry : collected.entries)
        {
            if (entry.meta.section && entry.meta.section[0] != '\0')
            {
                pSection = entry.meta.section;
            }
            if (collected.IsShown(entry.meta) && Detail::MatchesSearch(entry.meta, pSection))
            {
                return true;
            }
        }
        return false;
    }

    // パネルの見出し。検索中は当たる値が無いパネルを隠して当たるパネルを開き、一覧から飛ぶ先のパネルも開く
    template<typename TSettings>
    bool PanelHeader(const char* label, TSettings& settings, const char* targetKey)
    {
        if (!HasSearchMatch(settings))
        {
            return false;
        }
        if (IsSearching() || (State().focusTarget == targetKey && !State().isFocusScrolled))
        {
            ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        }
        return ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen);
    }

    // 値を変えたらtrue。targetKeyは表のIDと、Undo/Redoで値を探すときの名前(PostProcessなど)
    template<typename TSettings>
    bool Draw(TSettings& settings, const char* targetKey, const Options& options = {})
    {
        const Detail::Collected collected = Detail::Collect(settings);
        const std::vector<Detail::Entry>& entries = collected.entries;
        const UIState& state = State();

        bool isAnyChanged = false;
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

            // 検索中は当たる値だけを出す
            auto isListed = [&](size_t entryIndex)
            {
                const FieldMeta& meta = entries[entryIndex].meta;
                return collected.IsShown(meta) && (!options.isSearchable || Detail::MatchesSearch(meta, pSection));
            };
            bool hasBasic = false;
            bool hasAdvanced = false;
            bool isFocusInAdvanced = false;
            for (size_t entryIndex = sectionStart; entryIndex < sectionEnd; ++entryIndex)
            {
                if (!isListed(entryIndex))
                {
                    continue;
                }
                const bool isAdvanced = (entries[entryIndex].meta.flags & FieldFlags::kAdvanced) != 0;
                (isAdvanced ? hasAdvanced : hasBasic) = true;
                if (isAdvanced && Detail::IsFocused(targetKey, entries[entryIndex].meta) && !state.isFocusScrolled)
                {
                    isFocusInAdvanced = true;
                }
            }
            if (!hasBasic && !hasAdvanced)
            {
                sectionStart = sectionEnd;
                continue;
            }

            ImGui::PushID(static_cast<int>(sectionStart));
            if (pSection && pSection[0] != '\0')
            {
                ImGui::SeparatorText(pSection);
            }
            if (hasBasic && FieldWidgets::BeginFieldTable("##basic"))
            {
                for (size_t entryIndex = sectionStart; entryIndex < sectionEnd; ++entryIndex)
                {
                    if ((entries[entryIndex].meta.flags & FieldFlags::kAdvanced) == 0 && isListed(entryIndex))
                    {
                        isAnyChanged |= Detail::DrawEntry(settings, targetKey, collected, entries[entryIndex], options, nullptr);
                    }
                }
                FieldWidgets::EndFieldTable();
            }

            // 検索中と、一覧から詳細の値へ飛ぶときは、詳細を開く
            if (hasAdvanced && ((options.isSearchable && IsSearching()) || isFocusInAdvanced))
            {
                ImGui::SetNextItemOpen(true, ImGuiCond_Always);
            }
            if (hasAdvanced && ImGui::TreeNodeEx(u8"詳細", ImGuiTreeNodeFlags_SpanAvailWidth))
            {
                if (FieldWidgets::BeginFieldTable("##advanced"))
                {
                    for (size_t entryIndex = sectionStart; entryIndex < sectionEnd; ++entryIndex)
                    {
                        if ((entries[entryIndex].meta.flags & FieldFlags::kAdvanced) != 0 && isListed(entryIndex))
                        {
                            isAnyChanged |= Detail::DrawEntry(settings, targetKey, collected, entries[entryIndex], options, nullptr);
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

    // 値1つだけを、呼び出し側で開いた表の中に描く(よく使う値のグループ用)。見つからなければfalse
    template<typename TSettings>
    bool DrawSingle(TSettings& settings, const char* targetKey, const std::string& key, const std::function<void()>& extraMenu)
    {
        const Detail::Collected collected = Detail::Collect(settings);
        for (const Detail::Entry& entry : collected.entries)
        {
            if (key == entry.meta.key)
            {
                ImGui::PushID(targetKey);
                Detail::DrawEntry(settings, targetKey, collected, entry, Options{}, extraMenu);
                ImGui::PopID();
                return true;
            }
        }
        return false;
    }
}
