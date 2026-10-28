#include "EditorTools.h"
#include "ImGui/imgui.h"
#include "Editor/Editor.h"
#include "../Window/Window.h"
#include "../../Graphics/Shader/ShaderManager/ShaderManager.h"
#include "../../Graphics/Shader/ShaderSettingsIO/ShaderSettingsIO.h"
#include "../Manager/Asset/ModelMeta.h"

namespace
{
    std::string s_pendingWhat;
    std::function<void()> s_pendingAction;
    bool s_isPopupRequested = false;
    // 閉じることを確認し終えた(WM_CLOSEでもう聞かない)
    bool s_isCloseConfirmed = false;
    HWND s_hWnd = nullptr;

    bool s_isGameLookToggled = false;
    bool s_isGameLookActive = false;

    const char* const kPopupId = u8"保存していない変更###UnsavedChanges";
}

void UnsavedChanges::Initialize(HWND hWnd)
{
    s_hWnd = hWnd;
    Window::SetCloseRequestHandler([]()
    {
        if (s_isCloseConfirmed)
        {
            return true;
        }
        Confirm(u8"終了する", []()
        {
            s_isCloseConfirmed = true;
            DestroyWindow(s_hWnd);
        });
        // 聞く必要が無ければConfirmの中で閉じている
        return false;
    });
}

void UnsavedChanges::Confirm(const std::string& what, std::function<void()> action)
{
    if (ShaderSettingsIO::CountUnsavedChanges() == 0 && ModelMetaManager::Instance().CountDirtyModels() == 0)
    {
        action();
        return;
    }
    s_pendingWhat = what;
    s_pendingAction = std::move(action);
    s_isPopupRequested = true;
}

void UnsavedChanges::Update()
{
    if (s_isPopupRequested)
    {
        ImGui::OpenPopup(kPopupId);
        s_isPopupRequested = false;
    }
    const ImGuiViewport* pViewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(pViewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (!ImGui::BeginPopupModal(kPopupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }
    const int shaderChangeCount = ShaderSettingsIO::CountUnsavedChanges();
    const int dirtyModelCount = ModelMetaManager::Instance().CountDirtyModels();
    if (shaderChangeCount > 0)
    {
        ImGui::Text(u8"シェーダーの設定に、保存していない変更が %d 個あります(シーンに保存)。", shaderChangeCount);
    }
    if (dirtyModelCount > 0)
    {
        ImGui::Text(u8"マテリアルを変えて保存していないモデルが %d 個あります(モデルの.metaに保存)。", dirtyModelCount);
    }
    ImGui::Text(u8"「%s」前に、保存しますか？", s_pendingWhat.c_str());
    ImGui::TextDisabled(u8"保存しないと、変えた値は次に開いたときに元に戻ります。");
    ImGui::Spacing();

    auto finish = [](bool isRun)
    {
        std::function<void()> action = std::move(s_pendingAction);
        s_pendingAction = nullptr;
        ImGui::CloseCurrentPopup();
        if (isRun && action)
        {
            action();
        }
    };
    if (ImGui::Button(u8"保存する"))
    {
        // シェーダーの設定はシーンに、マテリアルはモデルの.metaに保存する
        bool isSaved = true;
        if (shaderChangeCount > 0)
        {
            isSaved &= Editor::SaveCurrentScene();
        }
        if (dirtyModelCount > 0)
        {
            isSaved &= ModelMetaManager::Instance().SaveAll();
        }
        if (isSaved)
        {
            finish(true);
        }
        // 保存できなかったら(保存先が無いなど)、閉じずに選び直してもらう。理由はログに出ている
    }
    ImGui::SameLine();
    if (ImGui::Button(u8"保存しない"))
    {
        finish(true);
    }
    ImGui::SameLine();
    if (ImGui::Button(u8"やめる") || ImGui::IsKeyPressed(ImGuiKey_Escape))
    {
        finish(false);
    }
    if (shaderChangeCount > 0 && Editor::GetScenePath().empty())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.4f, 1.0f), u8"このシーンには保存先がないので、保存できません");
    }
    ImGui::EndPopup();
}

bool GameLookPreview::IsToggled()
{
    return s_isGameLookToggled;
}

void GameLookPreview::SetToggled(bool isToggled)
{
    s_isGameLookToggled = isToggled;
}

bool GameLookPreview::IsActive()
{
    return s_isGameLookActive;
}

void GameLookPreview::Update()
{
    // F4はほかのショートカット(F1・F3・F5・F11)と重ならない。入力欄で文字を打っている間は見ない
    const bool isKeyHeld = ImGui::IsKeyDown(ImGuiKey_F4) && !ImGui::GetIO().WantTextInput;
    s_isGameLookActive = s_isGameLookToggled || isKeyHeld;
    ShaderManager::Instance().SetGameLookPreview(s_isGameLookActive);

    if (!s_isGameLookActive || !Editor::GetEditorMode())
    {
        return;
    }
    // 切り替えている間は、画面の隅に出す
    const ImGuiViewport* pViewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(pViewport->WorkPos.x + 10.0f, pViewport->WorkPos.y + pViewport->WorkSize.y - 10.0f), ImGuiCond_Always, ImVec2(0.0f, 1.0f));
    ImGui::SetNextWindowBgAlpha(0.85f);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoDocking;
    if (ImGui::Begin("##GameLookNotice", nullptr, flags))
    {
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.4f, 1.0f), u8"ゲームと同じ見え方");
        ImGui::TextDisabled(isKeyHeld && !s_isGameLookToggled ? u8"F4を離すと戻る" : u8"シェーダーエディターのボタンで戻す");
    }
    ImGui::End();
}
