#include "RendererPanel.h"
#include "../EditorContext.h"
#include "../../../../Library/ImGui/imgui.h"

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
    }
}
