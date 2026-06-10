#include "ColliderComponent.h"
#include <string>

void ColliderComponent::ImGuiUpdate() {
    auto& data = GetData();
    
    ImGui::Checkbox("Use Model Bounds", &data.m_useModelBounds);
    ImGui::Checkbox("Is Trigger (No Physics Response)", &data.m_isTrigger);
    ImGui::Checkbox("Is Static (Immovable Object)", &data.m_isStatic);

    if (true) {
        if (ImGui::Button("Add Shape")) {
            data.m_shapes.push_back(ColliderShape{});
        }
        
        ImGui::Separator();
        for (size_t i = 0; i < data.m_shapes.size(); ++i) {
            auto& shape = data.m_shapes[i];
            std::string header = "Shape " + std::to_string(i);
            if (ImGui::TreeNode(header.c_str())) {
                
                const char* typeNames[] = { "AABB", "OBB", "Sphere" };
                int typeIdx = static_cast<int>(shape.type);
                if (ImGui::Combo("Type", &typeIdx, typeNames, 3)) {
                    shape.type = static_cast<ColliderType>(typeIdx);
                }

                ImGui::DragFloat3("Offset", &shape.offset.x, 0.01f);

                if (shape.type == ColliderType::Sphere) {
                    ImGui::DragFloat("Radius", &shape.radius, 0.01f, 0.001f, 1000.0f);
                } else {
                    ImGui::DragFloat3("Extents (Half Size)", &shape.extents.x, 0.01f, 0.001f, 1000.0f);
                }

                if (ImGui::Button("Delete")) {
                    data.m_shapes.erase(data.m_shapes.begin() + i);
                    ImGui::TreePop();
                    break;
                }
                ImGui::TreePop();
            }
        }
    }
}

void ColliderComponent::Serialize(nlohmann::json& out) const {
    auto& data = const_cast<ColliderComponent*>(this)->GetData();
    out["UseModelBounds"] = data.m_useModelBounds;
    out["IsTrigger"] = data.m_isTrigger;
    out["IsStatic"] = data.m_isStatic;
    
    nlohmann::json shapesArray = nlohmann::json::array();
    for (const auto& s : data.m_shapes) {
        nlohmann::json shapeJson;
        shapeJson["Type"] = static_cast<int>(s.type);
        shapeJson["OffsetX"] = s.offset.x;
        shapeJson["OffsetY"] = s.offset.y;
        shapeJson["OffsetZ"] = s.offset.z;
        shapeJson["ExtentsX"] = s.extents.x;
        shapeJson["ExtentsY"] = s.extents.y;
        shapeJson["ExtentsZ"] = s.extents.z;
        shapeJson["Radius"] = s.radius;
        shapesArray.push_back(shapeJson);
    }
    out["Shapes"] = shapesArray;
}

void ColliderComponent::Deserialize(const nlohmann::json& in) {
    auto& data = GetData();
    if (in.contains("UseModelBounds")) {
        data.m_useModelBounds = in["UseModelBounds"];
    }
    
    if (in.contains("Shapes") && in["Shapes"].is_array()) {
        data.m_shapes.clear();
        for (const auto& shapeJson : in["Shapes"]) {
            ColliderShape s;
            if (shapeJson.contains("Type")) s.type = static_cast<ColliderType>(shapeJson["Type"]);
            if (shapeJson.contains("OffsetX")) s.offset.x = shapeJson["OffsetX"];
            if (shapeJson.contains("OffsetY")) s.offset.y = shapeJson["OffsetY"];
            if (shapeJson.contains("OffsetZ")) s.offset.z = shapeJson["OffsetZ"];
            if (shapeJson.contains("ExtentsX")) s.extents.x = shapeJson["ExtentsX"];
            if (shapeJson.contains("ExtentsY")) s.extents.y = shapeJson["ExtentsY"];
            if (shapeJson.contains("ExtentsZ")) s.extents.z = shapeJson["ExtentsZ"];
            if (shapeJson.contains("Radius")) s.radius = shapeJson["Radius"];
            data.m_shapes.push_back(s);
        }
    }
}