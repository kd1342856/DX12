#include "../../../Pch.h"
#include "../../Manager/Audio/AudioManager.h"

namespace
{
    constexpr const char* kSoundRoot = "Asset/Sound";

    struct SoundFileEntry
    {
        std::string path;   // "Asset/Sound/SE/door_open.wav" (UTF-8, '/'区切り)
        std::string folder; // "SE" / "BGM" 等(kSoundRoot直下のフォルダ名)
        std::string fileName;
    };

    std::string ToUtf8(const std::wstring& w)
    {
        if (w.empty())
        {
            return {};
        }
        int size = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
        std::string s(size, '\0');
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), size, nullptr, nullptr);
        return s;
    }

    // Asset/Sound以下の.wavを列挙する(毎フレームは重いので、開いた時とRefresh時だけ)
    std::vector<SoundFileEntry> ScanSoundFiles()
    {
        std::vector<SoundFileEntry> files;
        std::error_code ec;
        if (!std::filesystem::exists(kSoundRoot, ec))
        {
            return files;
        }
        for (auto& e : std::filesystem::recursive_directory_iterator(kSoundRoot, ec))
        {
            if (!e.is_regular_file())
            {
                continue;
            }
            std::wstring ext = e.path().extension().wstring();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
            if (ext != L".wav")
            {
                continue;
            }

            SoundFileEntry entry;
            entry.path = ToUtf8(e.path().generic_wstring());
            entry.fileName = ToUtf8(e.path().filename().wstring());
            auto rel = std::filesystem::relative(e.path().parent_path(), kSoundRoot, ec);
            entry.folder = (ec || rel.empty() || rel == ".") ? "" : ToUtf8(rel.generic_wstring());
            files.push_back(std::move(entry));
        }
        std::sort(files.begin(), files.end(), [](const SoundFileEntry& a, const SoundFileEntry& b)
        { return a.path < b.path; });
        return files;
    }

    const char* StateName(DirectX::SoundState state)
    {
        switch (state)
        {
        case DirectX::SoundState::PLAYING:
            return "Playing";
        case DirectX::SoundState::PAUSED:
            return "Paused";
        default:
            return "Stopped";
        }
    }
}

void Editor::DrawSoundManager()
{
    if (!ImGui::Begin("Sound Manager", &s_showSoundManager))
    {
        ImGui::End();
        return;
    }

    auto& audio = AudioManager::Instance();

    // 音量の変更は、スライダーを離した時点で自動保存する(Saveボタンの押し忘れで消えないように)
    auto autoSave = [&]()
    { if (ImGui::IsItemDeactivatedAfterEdit()){ audio.SaveSettings();
} };

    // --- 音量バス ---
    ImGui::SeparatorText(u8"音量");
    bool muted = audio.IsMuted();
    if (ImGui::Checkbox("Mute", &muted))
    {
        audio.SetMuted(muted);
        audio.SaveSettings();
    }

    float master = audio.GetMasterVolume();
    if (ImGui::SliderFloat("Master", &master, 0.0f, 1.0f))
    {
        audio.SetMasterVolume(master);
    }
    autoSave();
    float bgm = audio.GetBGMBusVolume();
    if (ImGui::SliderFloat("BGM", &bgm, 0.0f, 1.0f))
    {
        audio.SetBGMBusVolume(bgm);
    }
    autoSave();
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip(u8"全BGM共通。下の「曲ごとの音量」に掛け合わされる");
    }
    float se = audio.GetSEVolume();
    if (ImGui::SliderFloat("SE", &se, 0.0f, 1.0f))
    {
        audio.SetSEVolume(se);
    }
    autoSave();
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip(u8"各スクリプト(Player/LightSwitch等)の音量に掛け合わされる");
    }

    // --- 曲ごとの音量 ---
    ImGui::SeparatorText(u8"曲ごとの音量");
    for (const auto& path : audio.GetBGMTrackPaths())
    {
        const std::string& label = audio.GetBGMTrackLabel(path);
        const std::string fileName = path.substr(path.find_last_of("/\\") + 1);
        const std::string text = label.empty() ? fileName : label + " (" + fileName + ")";
        ImGui::PushID(path.c_str());
        float vol = audio.GetBGMTrackVolume(path);
        if (ImGui::SliderFloat(text.c_str(), &vol, 0.0f, 1.0f))
        {
            audio.SetBGMTrackVolume(path, vol);
        }
        autoSave();
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", path.c_str());
        }
        ImGui::PopID();
    }

    if (ImGui::Button("Save"))
    {
        audio.SaveSettings();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload"))
    {
        audio.LoadSettings();
    }
    ImGui::SameLine();
    ImGui::TextDisabled(u8"%s (変更は自動保存)", AudioManager::kSettingsPath);

    // --- BGM ---
    ImGui::SeparatorText("BGM");
    const std::string& current = audio.GetCurrentBGM();
    const DirectX::SoundState bgmState = audio.GetBGMState();
    ImGui::Text("Current: %s", current.empty() ? "(none)" : current.c_str());
    ImGui::Text("State: %s", StateName(bgmState));
    ImGui::BeginDisabled(current.empty());
    if (bgmState == DirectX::SoundState::PLAYING)
    {
        if (ImGui::Button("Pause"))
        {
            audio.PauseBGM();
        }
    }
    else
    {
        if (ImGui::Button("Resume"))
        {
            audio.ResumeBGM();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop"))
    {
        audio.StopBGM();
    }
    ImGui::EndDisabled();

    // --- サウンドファイル一覧(試聴) ---
    ImGui::SeparatorText(u8"サウンド一覧");
    static std::vector<SoundFileEntry> s_files;
    static bool s_scanned = false;
    static float s_previewVolume = 1.0f;
    static char s_filter[64] = {};
    if (!s_scanned || ImGui::Button("Refresh"))
    {
        s_files = ScanSoundFiles();
        s_scanned = true;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputTextWithHint("##filter", "Filter", s_filter, sizeof(s_filter));
    ImGui::SetNextItemWidth(160.0f);
    ImGui::SliderFloat("Preview Volume", &s_previewVolume, 0.0f, 1.0f);

    const auto loaded = audio.GetLoadedSoundPaths();
    auto isLoaded = [&](const std::string& path)
    {
        return std::find(loaded.begin(), loaded.end(), path) != loaded.end();
    };

    if (ImGui::BeginTable("SoundFiles", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable,
                          ImVec2(0.0f, 260.0f)))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Folder", ImGuiTableColumnFlags_WidthFixed, 50.0f);
        ImGui::TableSetupColumn("File", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Loaded", ImGuiTableColumnFlags_WidthFixed, 50.0f);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableHeadersRow();

        for (const auto& f : s_files)
        {
            if (s_filter[0] && f.path.find(s_filter) == std::string::npos)
            {
                continue;
            }

            ImGui::PushID(f.path.c_str());
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(f.folder.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(f.fileName.c_str());
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", f.path.c_str());
            }
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(isLoaded(f.path) ? "Yes" : "-");
            ImGui::TableNextColumn();
            if (ImGui::SmallButton("Play"))
            {
                audio.PlaySE(f.path, s_previewVolume);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("As BGM"))
            {
                audio.StopBGM(); // 同じ曲でも頭から鳴らし直す
                audio.PlayBGM(f.path, s_previewVolume);
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::TextDisabled(u8"Loadedが付かないファイルは読み込みに失敗している可能性あり(Consoleを確認。中身がMP3等のWAV以外だと読めない)");

    ImGui::End();
}
