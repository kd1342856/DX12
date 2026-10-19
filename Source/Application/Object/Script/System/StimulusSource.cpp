#include "../../../../Pch.h"
#include "StimulusSource.h"
#include "../../../../Framework/Object/GameObject.h"
#include <functional>

REGISTER_COMPONENT(StimulusSource);

void StimulusSource::Start()
{
    // シーン読み込み時の状態(m_isActive)をライトにも反映しておく
    ApplyToLights();
}

void StimulusSource::SetActive(bool active)
{
    if (m_isActive == active) return;
    m_isActive = active;
    ApplyToLights();
}

void StimulusSource::ApplyToLights()
{
    if (m_type != StimulusType::Light || !m_pGameObject) return;

    // ランプのモデル構成によってはライトが子オブジェクト側に付いているため、子階層も辿る
    auto& ecs = GameManager::Instance().GetECS();
    std::function<void(GameObject*)> apply = [&](GameObject* obj) {
        if (auto* pLight = ecs.TryGetComponent<PointLightData>(obj->GetEntityID())) {
            pLight->m_enabled = m_isActive;
        }
        if (auto* pSpot = ecs.TryGetComponent<SpotLightData>(obj->GetEntityID())) {
            pSpot->m_enabled = m_isActive;
        }
        for (auto& child : obj->GetChildren()) {
            apply(child.get());
        }
    };
    apply(m_pGameObject);
}

Math::Vector3 StimulusSource::GetWorldPosition() const
{
    if (!m_pGameObject) return Math::Vector3::Zero;
    auto& ecs = GameManager::Instance().GetECS();
    auto* pTrans = ecs.TryGetComponent<TransformData>(m_pGameObject->GetEntityID());
    if (!pTrans) return Math::Vector3::Zero;
    return pTrans->m_worldMatrix.Translation();
}

StimulusSource* StimulusSource::FindNearestActive(StimulusType type, const Math::Vector3& pos)
{
    auto& ecs = GameManager::Instance().GetECS();
    StimulusSource* best = nullptr;
    float bestDist = FLT_MAX;

    for (auto& scriptData : ecs.GetComponentArray<NativeScriptData>()) {
        auto* src = dynamic_cast<StimulusSource*>(scriptData.Instance.get());
        if (!src || !src->m_isActive || src->m_type != type) continue;

        float dist = Math::Vector3::Distance(pos, src->GetWorldPosition());
        if (dist > src->m_radius || dist >= bestDist) continue;
        best = src;
        bestDist = dist;
    }
    return best;
}

void StimulusSource::Serialize(nlohmann::json& out) const
{
    out["StimulusType"] = static_cast<int>(m_type);
    out["Active"]       = m_isActive;
    out["Radius"]       = m_radius;
    out["Interactable"] = m_interactable;
}

void StimulusSource::Deserialize(const nlohmann::json& in)
{
    if (in.contains("StimulusType")) m_type = static_cast<StimulusType>((int)in["StimulusType"]);
    if (in.contains("Active"))       m_isActive = in["Active"];
    if (in.contains("Radius"))       m_radius = in["Radius"];
    if (in.contains("Interactable")) m_interactable = in["Interactable"];
}

void StimulusSource::ImGuiUpdate()
{
    int current = static_cast<int>(m_type);
    const char* names[] = { "Light (Lamp)", "Sound (Radio)" };
    if (ImGui::Combo("Stimulus Type", &current, names, IM_ARRAYSIZE(names))) {
        m_type = static_cast<StimulusType>(current);
    }
    bool active = m_isActive;
    if (ImGui::Checkbox("Active", &active)) {
        SetActive(active);
    }
    ImGui::DragFloat("Radius", &m_radius, 0.1f, 0.5f, 50.0f);
    ImGui::Checkbox("Interactable (F key)", &m_interactable);
}
