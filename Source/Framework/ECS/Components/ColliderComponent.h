#pragma once
#include "../../DirectX/Utility/ClassAssembly.h"
#include "../ComponentBase.h"
#include "../../Object/GameObject.h"
#include "Data/ColliderData.h"
#include "../../Manager/GameManager.h"




#include <string>

class ColliderComponent : public ComponentBase {
public:
    using DataType = ColliderData;

    const char* GetComponentName() const override { return "ColliderComponent"; }

    void RegisterECSData() override {
        ColliderData data{};
        GameManager::Instance().GetECS().AddComponent(GetGameObject()->GetEntityID(), data);
    }

    void ImGuiUpdate() override;
    void Serialize(nlohmann::json& out) const override;
    void Deserialize(const nlohmann::json& in) override;

    ColliderData& GetData() {
        return GameManager::Instance().GetECS().GetComponent<ColliderData>(GetGameObject()->GetEntityID());
    }
};

REGISTER_COMPONENT(ColliderComponent);