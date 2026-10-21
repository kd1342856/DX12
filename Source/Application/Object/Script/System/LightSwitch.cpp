#include "../../../../Pch.h"
#include "LightSwitch.h"
#include "StimulusSource.h"
#include "../../../../Framework/Object/GameObject.h"
#include "../../../../Framework/Manager/Scene/Scene.h"
#include "../../../../Framework/Manager/Audio/AudioManager.h"
#include <functional>
#include <sstream>

REGISTER_COMPONENT(LightSwitch);

void LightSwitch::Start()
{
    ResolveTargets();
    UpdatePilotLamp();
}

void LightSwitch::Update(float deltaTime)
{
    // 照明側のStartより先に呼ばれた場合や、ゴーストが勝手に点けた場合にも追従させる
    if (!m_resolved) ResolveTargets();
    UpdatePilotLamp();
    UpdateRocker();
}

void LightSwitch::UpdateRocker()
{
    if (!m_pGameObject) return;
    const int want = IsOn() ? 1 : 0;
    if (want == m_rockerState) return;

    // スイッチ本体か子("Model")のうち、モデルとアニメーションを持つもの
    auto& ecs = GameManager::Instance().GetECS();
    ModelRenderData* pModel = nullptr;
    AnimationDataComponent* pAnim = nullptr;
    std::function<bool(GameObject*)> find = [&](GameObject* obj) {
        pModel = ecs.TryGetComponent<ModelRenderData>(obj->GetEntityID());
        pAnim = ecs.TryGetComponent<AnimationDataComponent>(obj->GetEntityID());
        if (pModel && pAnim) return true;
        for (auto& child : obj->GetChildren()) if (find(child.get())) return true;
        return false;
    };
    if (!find(m_pGameObject)) return;
    if (!pModel->m_spModelData || !pModel->m_spModelData->IsLoaded()) return; // 読み込み待ち(次フレームで再試行)

    const auto& anims = pModel->m_spModelData->GetAnimations();
    const char* clipName = want ? "Switch_OFF_to_ON" : "Switch_ON_to_OFF";
    int clip = -1;
    for (int i = 0; i < (int)anims.size(); ++i) {
        if (anims[i].name.find(clipName) != std::string::npos) { clip = i; break; }
    }
    if (clip < 0) clip = want ? 0 : 1; // 名前が取れない場合はglTF内の並び順
    if (clip >= (int)anims.size()) return;

    auto& a = pAnim->currentAnim;
    a.AnimationIndex = clip;
    a.IsLoop = false;
    a.Speed = 1.0f;
    a.StopAtTime = -1.0f;
    a.IsPlaying = true;
    // 初回はクリップの終端から始めて、そのフレームで最終ポーズに止まるようにする
    a.ProgressTime = (m_rockerState < 0) ? (std::max)(anims[clip].duration - 0.002f, 0.0f) : 0.0f;
    m_rockerState = want;
}

void LightSwitch::ResolveTargets()
{
    m_targets.clear();
    if (!m_pGameObject || !m_pGameObject->GetScene()) return;

    std::vector<std::string> names;
    std::stringstream ss(m_targetNames);
    std::string name;
    while (std::getline(ss, name, ',')) {
        size_t b = name.find_first_not_of(' ');
        size_t e = name.find_last_not_of(' ');
        if (b != std::string::npos) names.push_back(name.substr(b, e - b + 1));
    }
    if (names.empty()) { m_resolved = true; return; }

    auto& ecs = GameManager::Instance().GetECS();
    std::function<void(GameObject*)> visit = [&](GameObject* obj) {
        if (std::find(names.begin(), names.end(), obj->GetName()) != names.end()) {
            if (auto* sd = ecs.TryGetComponent<NativeScriptData>(obj->GetEntityID())) {
                if (auto* src = dynamic_cast<StimulusSource*>(sd->Instance.get())) {
                    // 照明本体はスイッチ経由でしか操作できないようにする
                    src->m_interactable = false;
                    src->m_controlledBySwitch = true;
                    src->m_ownerSwitch = this;
                    m_targets.push_back(src);
                }
            }
        }
        for (auto& child : obj->GetChildren()) visit(child.get());
    };
    for (auto& obj : m_pGameObject->GetScene()->GetGameObjects()) visit(obj.get());

    // スクリプトのInstanceがまだ作られていない等で見つからなければ、次のUpdateで再試行する
    m_resolved = !m_targets.empty();
}

bool LightSwitch::IsOn() const
{
    for (auto* src : m_targets) {
        if (src->IsActive()) return true;
    }
    return false;
}

void LightSwitch::Toggle()
{
    if (!m_resolved) ResolveTargets();
    // 1つでも点いていれば全部消す、全部消えていれば全部点ける
    SetOn(!IsOn());
}

void LightSwitch::SetOn(bool on)
{
    if (!m_resolved) ResolveTargets();
    for (auto* src : m_targets) src->SetActive(on);
    PlaySwitchSound();
    UpdatePilotLamp();
    UpdateRocker();
}

void LightSwitch::PlaySwitchSound() const
{
    AudioManager::Instance().PlaySEAt("Asset/Sound/SE/kabe_switch.wav", GetWorldPosition(), m_soundVolume);
}

void LightSwitch::UpdatePilotLamp()
{
    if (!m_pGameObject) return;
    auto& ecs = GameManager::Instance().GetECS();
    const Math::Vector3 glow = IsOn() ? Math::Vector3(1.0f, 0.45f, 0.15f) * 6.0f : Math::Vector3::Zero;
    std::function<void(GameObject*)> apply = [&](GameObject* obj) {
        if (auto* model = ecs.TryGetComponent<ModelRenderData>(obj->GetEntityID())) {
            model->m_glowColor = glow;
            model->m_glowMaterial = m_pilotMaterial;
        }
        for (auto& child : obj->GetChildren()) apply(child.get());
    };
    apply(m_pGameObject);
}

Math::Vector3 LightSwitch::GetWorldPosition() const
{
    if (!m_pGameObject) return Math::Vector3::Zero;
    auto& ecs = GameManager::Instance().GetECS();
    auto* pTrans = ecs.TryGetComponent<TransformData>(m_pGameObject->GetEntityID());
    if (!pTrans) return Math::Vector3::Zero;
    return pTrans->m_worldMatrix.Translation();
}

void LightSwitch::Serialize(nlohmann::json& out) const
{
    out["Targets"] = m_targetNames;
    out["PilotMaterial"] = m_pilotMaterial;
    out["Interactable"] = m_interactable;
    out["SoundVolume"] = m_soundVolume;
}

void LightSwitch::Deserialize(const nlohmann::json& in)
{
    if (in.contains("Targets")) m_targetNames = in["Targets"].get<std::string>();
    if (in.contains("PilotMaterial")) m_pilotMaterial = in["PilotMaterial"].get<std::string>();
    if (in.contains("Interactable")) m_interactable = in["Interactable"];
    if (in.contains("SoundVolume")) m_soundVolume = in["SoundVolume"];
}

void LightSwitch::ImGuiUpdate()
{
    char buf[256] = {};
    strncpy_s(buf, m_targetNames.c_str(), _TRUNCATE);
    if (ImGui::InputText(u8"つなぐ照明", buf, sizeof(buf))) {
        m_targetNames = buf;
        m_resolved = false;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"操作する照明のGameObject名(カンマ区切りで複数可)");
    ImGui::Text(u8"接続中の照明: %d  状態: %s", (int)m_targets.size(), IsOn() ? u8"ON" : u8"OFF");
    if (ImGui::Button(u8"切り替え")) Toggle();
    ImGui::Checkbox(u8"Fキーで操作できる", &m_interactable);
    ImGui::SliderFloat(u8"音量", &m_soundVolume, 0.0f, 1.0f);
}
