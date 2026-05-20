#pragma once
#include "../ComponentBase.h"
#include "Data/AnimationData.h"
#include "../../Object/GameObject.h"

class AnimationComponent : public ComponentBase
{
public:
	// ECS登録用のDataTypeエイリアス
	using DataType = AnimationDataComponent;

	AnimationComponent() {}
	~AnimationComponent() {}

	const char* GetComponentName() const override { return "AnimationComponent"; }

	void RegisterECSData() override 
	{
		AnimationDataComponent data{};
		GameManager::Instance().GetECS().AddComponent(GetGameObject()->GetEntityID(), data);
	}

	void Awake() override {}

	void Update(float deltaTime);
	void ImGuiUpdate() override;

	void Serialize(nlohmann::json& out) const override;
	void Deserialize(const nlohmann::json& in) override;

	AnimationDataComponent& GetAnimData() 
	{
		return GameManager::Instance().GetECS().GetComponent<AnimationDataComponent>(GetGameObject()->GetEntityID());
	}
private:

};
