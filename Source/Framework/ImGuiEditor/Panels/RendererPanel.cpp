#include "RendererPanel.h"
#include "../EditorContext.h"
#include "../../../../Library/ImGui/imgui.h"
#include "../../../Graphics/Device/GraphicsDevice.h"
#include "../../ECS/CompSystem/Systems/RenderSystem.h"

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

        // lpCurrentDirectoryをnullのままにして継承に任せるのではなく、このプロセスの
        // *現在の*作業ディレクトリを明示的に渡す。このプロセスはdxcompiler.dll等を
        // CWD相対の検索で解決しているので、今動けているそのCWDは確実に正しい -
        // 子プロセスも同じになると仮定するのではなく、それを捕まえて使い回す。
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
    if (ImGui::CollapsingHeader("Renderer Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ctx.Renderer)
        {
            bool changed = false;

            if (ImGui::Checkbox("Enable PBR", &ctx.Renderer->EnablePBR)) changed = true;

            ImGui::Separator();
            ImGui::Text("SSR (Screen Space Reflection)");
            if (ImGui::Checkbox("Enable SSR", &ctx.Renderer->EnableSSR)) changed = true;
            if (ImGui::SliderFloat("SSR Step Size", &ctx.Renderer->SSRStepSize, 0.05f, 1.5f, "%.2f")) changed = true;
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("平面反射(専用の反射カメラ)が設定されていないガラス面のフォールバック。\nOpaqueパスの深度/カラーだけでレイマーチするため、Opaque材質自身には効かない(既知の制限)。");
            }

            if (changed) {
                ctx.Renderer->IsDirty = true;
            }
        }

        ImGui::Separator();
        ImGui::Text("Culling (debug)");
        ImGui::Checkbox("Frustum Culling", &RenderSystem::s_enableFrustumCulling);
        ImGui::Checkbox("Room Culling", &RenderSystem::s_enableRoomCulling);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(
                "Turn these off to check whether something disappearing/not rendering is\n"
                "caused by culling or is unrelated to it. Takes effect immediately.");
        }

#ifdef _DEBUG
        ImGui::Separator();
        // このセッションで実際に動いているものの実測値。GraphicsDevice::Init()で
        // 一度だけ決まる - 下のチェックボックスとは独立しているので、fpsから
        // 推測する必要が無い。これがチェックボックスと食い違っている場合、
        // チェックボックスは*次回*起動時に何が起こるかを表している(ツールチップ参照) -
        // ライブには決して反映されない。
        ImGui::Text("Debug Layer Active This Session: %s", GraphicsDevice::IsDebugLayerActive() ? "ON" : "OFF");

        static bool s_debugLayerRequested = GraphicsDevice::IsDebugLayerRequested();
        bool debugLayerChanged = ImGui::Checkbox("D3D12 Debug Layer", &s_debugLayerRequested);
        if (debugLayerChanged)
        {
            GraphicsDevice::SetDebugLayerRequested(s_debugLayerRequested);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(
                "The D3D12 debug layer can only be enabled/disabled before the device is\n"
                "created, so it can't apply live - restart the app to pick up the change.\n"
                "Turning it off gets Debug-build performance much closer to Release; turn it\n"
                "back on when you actually need to catch a GPU-side bug.");
        }

        bool pendingChange = s_debugLayerRequested != GraphicsDevice::IsDebugLayerActive();
        if (!pendingChange) ImGui::BeginDisabled();
        if (ImGui::Button("Apply & Restart"))
        {
            RelaunchApplication();
        }
        if (!pendingChange) ImGui::EndDisabled();
        if (pendingChange)
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "restart needed to apply");
        }
#endif
    }
}
