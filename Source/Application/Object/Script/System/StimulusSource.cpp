#include "../../../../Pch.h"
#include "StimulusSource.h"
#include "../../../../Framework/Object/GameObject.h"
#include "LightSwitch.h"
#include "../../../../Framework/Manager/Audio/AudioManager.h"
#include <functional>

REGISTER_COMPONENT(StimulusSource);

void StimulusSource::Start()
{
    // シーン読み込み時の状態(m_isActive)をライトにも反映しておく
    ApplyToLights();
}

void StimulusSource::Update(float deltaTime)
{
    if (m_flickerTimer <= 0.0f) return;

    m_flickerTimer -= deltaTime;
    if (m_flickerTimer <= 0.0f || !m_isActive) {
        // 点滅終了(または途中で消灯された): 本来の点灯状態に戻す
        m_flickerTimer = 0.0f;
        ApplyToLights();
        return;
    }

    m_flickerToggleTimer -= deltaTime;
    if (m_flickerToggleTimer <= 0.0f) {
        m_flickerLit = !m_flickerLit;
        // 消えている時間は短め、点いている時間は不規則にして電気が不安定な感じを出す
        m_flickerToggleTimer = m_flickerLit ? Random::Instance().Range(0.03f, 0.25f)
                                            : Random::Instance().Range(0.02f, 0.12f);
        SetLightsEnabled(m_flickerLit);
    }
}

void StimulusSource::Flicker(float duration)
{
    if (m_type != StimulusType::Light || !m_isActive) return;
    if (m_flickerTimer <= 0.0f) {
        m_flickerToggleTimer = 0.0f; // 次のUpdateですぐ一度消える
        m_flickerLit = true;
    }
    m_flickerTimer = (std::max)(m_flickerTimer, duration);
}

void StimulusSource::SetActive(bool active)
{
    if (m_isActive == active) return;
    m_isActive = active;
    m_flickerTimer = 0.0f;
    ApplyToLights();
}

void StimulusSource::ApplyToLights()
{
    SetLightsEnabled(m_isActive);
}

void StimulusSource::SetLightsEnabled(bool enabled)
{
    if (m_type != StimulusType::Light || !m_pGameObject) return;

    // ランプのモデル構成によってはライトが子オブジェクト側に付いているため、子階層も辿る
    auto& ecs = GameManager::Instance().GetECS();
    std::function<void(GameObject*)> apply = [&](GameObject* obj) {
        if (auto* pLight = ecs.TryGetComponent<PointLightData>(obj->GetEntityID())) {
            pLight->m_enabled = enabled;
        }
        if (auto* pSpot = ecs.TryGetComponent<SpotLightData>(obj->GetEntityID())) {
            pSpot->m_enabled = enabled;
        }
        for (auto& child : obj->GetChildren()) {
            apply(child.get());
        }
    };
    apply(m_pGameObject);
}

void StimulusSource::PlayToggleSound() const
{
    if (m_controlledBySwitch && m_ownerSwitch) {
        m_ownerSwitch->PlaySwitchSound();
        return;
    }
    const char* path = "Asset/Sound/SE/kabe_switch.wav"; // ラジカセ等(専用の音がまだない)
    if (m_type == StimulusType::Light) {
        path = m_isActive ? "Asset/Sound/SE/keikoutou_on.wav" : "Asset/Sound/SE/keikoutou_off.wav";
    }
    AudioManager::Instance().PlaySEAt(path, GetWorldPosition(), m_soundVolume);
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
    out["RoomLight"]    = m_isRoomLight;
    out["SoundVolume"]  = m_soundVolume;
}

void StimulusSource::Deserialize(const nlohmann::json& in)
{
    if (in.contains("StimulusType")) m_type = static_cast<StimulusType>((int)in["StimulusType"]);
    if (in.contains("Active"))       m_isActive = in["Active"];
    if (in.contains("Radius"))       m_radius = in["Radius"];
    if (in.contains("Interactable")) m_interactable = in["Interactable"];
    if (in.contains("RoomLight"))    m_isRoomLight = in["RoomLight"];
    if (in.contains("SoundVolume"))  m_soundVolume = in["SoundVolume"];
}

void StimulusSource::ImGuiUpdate()
{
    int current = static_cast<int>(m_type);
    const char* names[] = { u8"光 (照明)", u8"音 (ラジオ)" };
    if (ImGui::Combo(u8"刺激の種類", &current, names, IM_ARRAYSIZE(names))) {
        m_type = static_cast<StimulusType>(current);
    }
    bool active = m_isActive;
    if (ImGui::Checkbox(u8"有効", &active)) {
        SetActive(active);
    }
    ImGui::DragFloat(u8"半径", &m_radius, 0.1f, 0.5f, 50.0f);
    ImGui::Checkbox(u8"Fキーで操作できる", &m_interactable);
    ImGui::SliderFloat(u8"音量", &m_soundVolume, 0.0f, 1.0f);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"本体(紐スイッチ)をon/offした時の音量。壁スイッチ接続中はスイッチ側の音量を使う");
    if (m_type == StimulusType::Light) {
        ImGui::Checkbox(u8"部屋の照明", &m_isRoomLight);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"部屋の電気(天井照明等)か。ランプ等の小さな明かりはオフにする。\n影女の退治条件はオンの照明だけを数える。");
    }
}
