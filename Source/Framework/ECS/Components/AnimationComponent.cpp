#include "Pch.h"
#include "AnimationComponent.h"
#include "ModelRendererComponent.h"

void AnimationComponent::Update(float deltaTime)
{
	auto& data = GetAnimData().currentAnim;
	if (!data.IsPlaying || data.AnimationIndex < 0) return;

	// ModelRendererComponentからModelDataを取得
	auto pModelComp = GetGameObject()->GetComponent<ModelRendererComponent>();
	if (!pModelComp || !pModelComp->GetData().m_spModelData) return;

	auto pModelData = pModelComp->GetData().m_spModelData;
	const auto& anims = pModelData->GetAnimations();
	if (data.AnimationIndex >= (int)anims.size()) return;

	const auto& animInfo = anims[data.AnimationIndex];

	// タイムラインを進める
	data.ProgressTime += (animInfo.ticksPerSecond * deltaTime * data.Speed);

	if (data.ProgressTime >= animInfo.duration)
	{
		if (data.IsLoop) {
			data.ProgressTime = fmod(data.ProgressTime, animInfo.duration);
		}
		else {
			data.ProgressTime = animInfo.duration;
			data.IsPlaying = false; // ループしない場合は末尾で停止
		}
	}

	// ModelData側に行列の補間更新を指示する
	pModelData->UpdateAnimation(data.AnimationIndex, data.ProgressTime);
}

void AnimationComponent::ImGuiUpdate()
{
	auto& data = GetAnimData().currentAnim;

	auto pModelComp = GetGameObject()->GetComponent<ModelRendererComponent>();
	if (!pModelComp || !pModelComp->GetData().m_spModelData) {
		ImGui::TextColored(ImVec4(1, 0, 0, 1), "Required: ModelRendererComponent");
		return;
	}

	auto pModelData = pModelComp->GetData().m_spModelData;
	const auto& anims = pModelData->GetAnimations();

	if (anims.empty()) {
		ImGui::TextDisabled("No Animations in Model.");
		return;
	}

	ImGui::Separator();
	ImGui::Text("Animation Settings");

	// 1. アニメーション選択
	std::string previewName = (data.AnimationIndex >= 0 && data.AnimationIndex < (int)anims.size())
		? anims[data.AnimationIndex].name : "Select Animation...";

	if (ImGui::BeginCombo("Clip", previewName.c_str()))
	{
		for (int i = 0; i < (int)anims.size(); ++i) {
			bool isSelected = (data.AnimationIndex == i);
			if (ImGui::Selectable(anims[i].name.c_str(), isSelected)) {
				data.AnimationIndex = i;
				data.ProgressTime = 0.0f;
			}
			if (isSelected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	if (data.AnimationIndex < 0 || data.AnimationIndex >= (int)anims.size()) return;
	const auto& currentAnim = anims[data.AnimationIndex];

	// 2. コントロールボタン
	ImGui::Spacing();
	if (data.IsPlaying) {
		if (ImGui::Button("|| Pause")) data.IsPlaying = false;
	}
	else {
		if (ImGui::Button("> Play")) data.IsPlaying = true;
	}

	ImGui::SameLine();
	if (ImGui::Button("[] Stop")) {
		data.IsPlaying = false;
		data.ProgressTime = 0.0f;
		pModelData->ResetAnimation();
	}

	ImGui::SameLine();
	ImGui::Checkbox("Loop", &data.IsLoop);
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("アニメーションの再生をループさせるか設定します");

	// 3. 再生速度
	ImGui::SetNextItemWidth(150.0f);
	ImGui::DragFloat("Speed", &data.Speed, 0.01f, -3.0f, 3.0f, "%.2f");
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("マイナス値にすると逆再生になります");

	// 4. タイムラインスライダー
	ImGui::Spacing();
	ImGui::Text("Timeline (Ticks):");

	if (ImGui::SliderFloat("##Time", &data.ProgressTime, 0.0f, currentAnim.duration, "%.1f Ticks"))
	{
		// ユーザーが手動でスライダーを動かした時、モデル姿勢をリアルタイム更新する
		pModelData->UpdateAnimation(data.AnimationIndex, data.ProgressTime);
	}

	float sec = data.ProgressTime / (currentAnim.ticksPerSecond > 0.0f ? currentAnim.ticksPerSecond : 25.0f);
	float maxSec = currentAnim.duration / (currentAnim.ticksPerSecond > 0.0f ? currentAnim.ticksPerSecond : 25.0f);
	ImGui::TextDisabled("Duration: %.2f / %.2f sec", sec, maxSec);
}

void AnimationComponent::Serialize(nlohmann::json& out) const
{
	auto& data = const_cast<AnimationComponent*>(this)->GetAnimData().currentAnim;
	out["AnimIndex"] = data.AnimationIndex;
	out["Speed"] = data.Speed;
	out["IsLoop"] = data.IsLoop;
	out["IsPlaying"] = data.IsPlaying;
}

void AnimationComponent::Deserialize(const nlohmann::json& in)
{
	auto& data = GetAnimData().currentAnim;
	if (in.contains("AnimIndex")) data.AnimationIndex = in["AnimIndex"];
	if (in.contains("Speed")) data.Speed = in["Speed"];
	if (in.contains("IsLoop")) data.IsLoop = in["IsLoop"];
	if (in.contains("IsPlaying")) data.IsPlaying = in["IsPlaying"];
}