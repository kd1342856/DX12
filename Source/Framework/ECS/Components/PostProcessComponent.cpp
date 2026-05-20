#include "Pch.h"
#include "PostProcessComponent.h"
#include "../../ImGuiEditor/ImGui/imgui.h"

void PostProcessComponent::ImGuiUpdate()
{
    ImGui::SliderFloat("Exposure", &m_data.m_exposure, 0.1f, 5.0f);
}

REGISTER_COMPONENT(PostProcessComponent);