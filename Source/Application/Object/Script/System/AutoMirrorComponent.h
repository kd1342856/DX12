#pragma once

#include "../../../../Framework/ECS/Components/Data/NativeScript.h"
#include <string>

class AutoMirrorComponent : public NativeScript {
public:
    void Start() override;
    void ImGuiUpdate() override;
    void Serialize(nlohmann::json& out) const override;
    void Deserialize(const nlohmann::json& in) override;
    void GenerateMirrors();

    // Keywords
    std::string m_targetKeywords = "Window,OldGrass,OldGlass";
    Math::Vector3 m_localNormal = { 0.0f, 0.0f, 1.0f }; // Normal axis
    
    // Notes
};
