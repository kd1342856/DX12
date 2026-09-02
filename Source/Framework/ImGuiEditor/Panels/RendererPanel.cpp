#include "RendererPanel.h"
#include "../EditorContext.h"
#include "../../../../Library/ImGui/imgui.h"
#include "../../../Graphics/Device/GraphicsDevice.h"

#ifdef _DEBUG
namespace
{
    // Live-recreating the D3D12 device (swapchain, every mesh/texture/PSO, ImGui, ...) to
    // apply a debug-layer change without restarting is a lot of invasive, risky work for
    // what it buys. Spawning a fresh copy of the process and quitting this one gets the
    // same "one click, no manual restart" experience with none of that risk.
    void RelaunchApplication()
    {
        wchar_t exePath[MAX_PATH];
        if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) == 0) return;

        // Explicitly pass this process's *current* working directory rather than leaving
        // lpCurrentDirectory null and trusting inheritance. This process resolves things
        // like dxcompiler.dll via a CWD-relative search, so whatever CWD got it running is
        // known-good right now - capture and reuse it instead of assuming the child would
        // end up with the same one.
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

        // Same thread as the Win32 message loop (Window::ProcessMessage), so this is picked
        // up on its next PeekMessage and runs the normal shutdown path cleanly.
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

            if (changed) {
                ctx.Renderer->IsDirty = true;
            }
        }

#ifdef _DEBUG
        ImGui::Separator();
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
