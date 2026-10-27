#include "RendererPanel.h"
#include "../EditorContext.h"
#include "ImGui/imgui.h"
#include "../Widgets/SettingsFieldUI.h"
#include "../../../Graphics/Device/GraphicsDevice.h"
#include "../../ECS/CompSystem/Systems/RenderSystem.h"
#include "../../../Graphics/Renderer/PathTracer.h"

#ifdef _DEBUG
namespace
{
    // D3D12デバイス(スワップチェーン、全メッシュ/テクスチャ/PSO、ImGui等)をライブ再作成して
    // デバッグレイヤーの変更を再起動無しで反映するのは、得られるものの割に侵襲的で
    // リスクの大きい作業。プロセスの新しいコピーを起動してこちらを終了する方が、
    // そのリスク無しで同じ「ワンクリックで手動再起動不要」という体験を得られる。
    void RelaunchApplication()
    {
        wchar_t exePath[MAX_PATH];
        if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) == 0) return;

        // 子プロセスもAppPaths::Initializeで自分の場所からProjectフォルダを見つけ直すが、
        // 念のため、このプロセスの現在の作業ディレクトリ(=Projectフォルダ)をそのまま渡す。
        wchar_t cwd[MAX_PATH];
        DWORD cwdLen = GetCurrentDirectoryW(MAX_PATH, cwd);
        const wchar_t* pCwd = (cwdLen > 0 && cwdLen < MAX_PATH) ? cwd : nullptr;

        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = {};
        if (CreateProcessW(exePath, nullptr, nullptr, nullptr, FALSE, 0, nullptr, pCwd, &si, &pi))
        {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }

        // Win32メッセージループ(Window::ProcessMessage)と同じスレッドなので、次の
        // PeekMessageで拾われて、通常のシャットダウン処理がきれいに走る。
        PostQuitMessage(0);
    }
}
#endif

void RendererPanel::Draw(EditorContext& ctx)
{
    if (!ctx.Renderer || !SettingsFieldUI::PanelHeader(u8"描画###Renderer Settings", *ctx.Renderer, "Renderer"))
    {
        return;
    }

    {
        SettingsFieldUI::Options options;
        options.onChanged = [](const char* key)
        {
            // 反射の回数を変えたら、足し続けた絵を捨てて最初から足し直す
            if (strcmp(key, "PathTracerMaxBounces") == 0)
            {
                PathTracer::Instance().ResetAccumulation();
            }
        };
        options.afterField = [&ctx](const char* key)
        {
            if (strcmp(key, "EnableReferencePathTracer") != 0)
            {
                return;
            }
            if (!PathTracer::Instance().IsAvailable())
            {
                FieldWidgets::ComputedValue(u8"使えるか", u8"このGPUでは使えません", "");
            }
            else if (ctx.Renderer->EnableReferencePathTracer)
            {
                char samples[32] = {};
                snprintf(samples, sizeof(samples), "%u", PathTracer::Instance().GetSampleCount());
                FieldWidgets::ComputedValue(u8"足した枚数", samples, u8"(カメラを動かすと最初から)");
            }
        };
        SettingsFieldUI::Draw(*ctx.Renderer, "Renderer", options);
    }
    if (SettingsFieldUI::IsSearching())
    {
        return;
    }

    ImGui::SeparatorText(u8"見え方の切り分け(デバッグ用)");
    ImGui::Checkbox(u8"画面の外の物を描かない(視錐台カリング)", &RenderSystem::s_enableFrustumCulling);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
    {
        ImGui::SetTooltip(u8"切ると、物が消える・描かれない原因がカリングかどうかを確かめられる。すぐに効く。保存しない。");
    }
    ImGui::Checkbox(u8"見えない部屋の物を描かない(部屋のカリング)", &RenderSystem::s_enableRoomCulling);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
    {
        ImGui::SetTooltip(u8"切ると、物が消える・描かれない原因がカリングかどうかを確かめられる。すぐに効く。保存しない。");
    }

#ifdef _DEBUG
    ImGui::SeparatorText(u8"D3D12のデバッグレイヤー");
    // このセッションで実際に動いているものの実測値。GraphicsDevice::Init()で一度だけ決まり、
    // 下のチェックボックスとは独立している。食い違っているときは、チェックボックスは次回の起動のときの値
    ImGui::Text(u8"今の起動: %s", GraphicsDevice::IsDebugLayerActive() ? u8"使っている" : u8"使っていない");

    static bool s_debugLayerRequested = GraphicsDevice::IsDebugLayerRequested();
    if (ImGui::Checkbox(u8"次の起動からデバッグレイヤーを使う", &s_debugLayerRequested))
    {
        GraphicsDevice::SetDebugLayerRequested(s_debugLayerRequested);
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
    {
        ImGui::SetTooltip(u8"デバッグレイヤーはデバイスを作る前にしか切り替えられないので、再起動で反映する。\n"
            u8"切るとDebugビルドの速さがReleaseに近づく。GPU側の不具合を探すときだけ使う。");
    }

    const bool isPendingChange = s_debugLayerRequested != GraphicsDevice::IsDebugLayerActive();
    ImGui::BeginDisabled(!isPendingChange);
    if (ImGui::Button(u8"反映して再起動"))
    {
        RelaunchApplication();
    }
    ImGui::EndDisabled();
    if (isPendingChange)
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), u8"再起動すると反映される");
    }
#endif
}
