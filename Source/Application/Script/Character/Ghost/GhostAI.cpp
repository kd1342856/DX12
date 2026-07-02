#include "../../../../Pch.h"
#include "GhostAI.h"
#include "../../../../Framework/Manager/NavMeshManager.h"
#include "../../../../Framework/Manager/Scene/SceneManager.h"
#include "../../../../Framework/Manager/Scene/Scene.h"
#include "../../../Scene/TitleScene/TitleScene.h"
#include "../../../../Framework/Manager/Animation/AnimationManager.h"


REGISTER_COMPONENT(GhostAI);

void GhostAI::Awake()
{
}

void GhostAI::Start()
{
    m_changeDirTimer = 0.0f;
    SetState(GhostAI::State::Wander);
}

void GhostAI::SetState(GhostAI::State state)
{
    m_currentState = state;
    switch (state)
    {
    case GhostAI::State::Idle:
        AnimationManager::Instance().PlayAnimation(GetGameObject()->GetEntityID(), m_animIdle, true);
        break;
    case GhostAI::State::Wander:
        AnimationManager::Instance().PlayAnimation(GetGameObject()->GetEntityID(), m_animWander, true);
        break;
    case GhostAI::State::Hunt:
        AnimationManager::Instance().PlayAnimation(GetGameObject()->GetEntityID(), m_animHunt, true, 1.5f);
        break;
    case GhostAI::State::Dead:
        AnimationManager::Instance().PlayAnimation(GetGameObject()->GetEntityID(), m_animDead, false);
        break;
    }
}

void GhostAI::Update()
{
    if (m_isExorcised) return;

    auto&      ecs    = GameManager::Instance().GetECS();
    int        entity = GetGameObject()->GetEntityID();
    auto&      cTrans = ecs.GetComponent<TransformData>(entity);
    const auto& navMgr = NavMeshManager::Instance();

    Math::Vector3 playerPos;
    bool          playerFound = false;
    {
        auto currentScene = SceneManager::Instance().GetCurrentScene();
        auto scene        = std::dynamic_pointer_cast<Scene>(currentScene);
        if (scene)
        {
            for (auto& obj : scene->GetGameObjects())
            {
                if (obj->GetName() == "Player")
                {
                    playerPos   = ecs.GetComponent<TransformData>(obj->GetEntityID()).m_position;
                    playerFound = true;
                    break;
                }
            }
        }
    }

    if (playerFound)
    {
        const bool inDetect = NavMeshManager::Instance().IsInRange(cTrans.m_position, playerPos, m_detectionRadius);
        const bool inLose   = NavMeshManager::Instance().IsInRange(cTrans.m_position, playerPos, m_loseRadius);

        if (m_currentState == GhostAI::State::Wander && inDetect)
        {
            NavMeshManager::Instance().ClearPath(entity);
            SetState(GhostAI::State::Hunt);
        }
        else if (m_currentState == GhostAI::State::Hunt && !inLose)
        {
            NavMeshManager::Instance().ClearPath(entity);
            SetState(GhostAI::State::Wander);
        }
    }
    else if (m_currentState == GhostAI::State::Hunt)
    {
        NavMeshManager::Instance().ClearPath(entity);
        SetState(GhostAI::State::Wander);
    }

    const float deltaTime = GameTimer::Instance().DeltaTime();

    if (m_currentState == GhostAI::State::Wander)
    {
        m_changeDirTimer -= deltaTime;
        if (m_changeDirTimer <= 0.0f)
        {
            float randX  = Random::Instance().Range(-1.0f, 1.0f);
            float randZ  = Random::Instance().Range(-1.0f, 1.0f);
            m_moveDir    = Math::Vector3(randX, 0.0f, randZ);
            if (m_moveDir.LengthSquared() > 0.0f) m_moveDir.Normalize();
            m_changeDirTimer = Random::Instance().Range(1.0f, 3.0f);
        }

        cTrans.m_position += m_moveDir * m_moveSpeed * deltaTime;
        if (m_moveDir.LengthSquared() > 0.001f)
            cTrans.m_rotation.y = atan2f(m_moveDir.x, m_moveDir.z);
    }
    else if (m_currentState == GhostAI::State::Hunt && playerFound)
    {
        Math::Vector3 newPos = NavMeshManager::Instance().MoveToward(
            entity,
            cTrans.m_position,
            playerPos,
            m_huntSpeed,
            deltaTime,
            m_pathUpdateInterval);

        Math::Vector3 moveDir = newPos - cTrans.m_position;
        moveDir.y = 0;
        if (moveDir.LengthSquared() > 0.001f)
        {
            moveDir.Normalize();
            cTrans.m_rotation.y = atan2f(moveDir.x, moveDir.z);
        }

        cTrans.m_position = newPos;
    }
}

void GhostAI::PostUpdate()
{
}

void GhostAI::PreDraw()
{
}

void GhostAI::Draw()
{
}

void GhostAI::Serialize(nlohmann::json& out) const
{
    out["moveSpeed"]          = m_moveSpeed;
    out["huntSpeed"]          = m_huntSpeed;
    out["detectionRadius"]    = m_detectionRadius;
    out["loseRadius"]         = m_loseRadius;
    out["pathUpdateInterval"] = m_pathUpdateInterval;
    out["animIdle"]           = m_animIdle;
    out["animWander"]         = m_animWander;
    out["animHunt"]           = m_animHunt;
    out["animDead"]           = m_animDead;
}

void GhostAI::Deserialize(const nlohmann::json& in)
{
    if (in.contains("moveSpeed"))          m_moveSpeed          = in["moveSpeed"];
    if (in.contains("huntSpeed"))          m_huntSpeed          = in["huntSpeed"];
    if (in.contains("detectionRadius"))    m_detectionRadius    = in["detectionRadius"];
    if (in.contains("loseRadius"))         m_loseRadius         = in["loseRadius"];
    if (in.contains("pathUpdateInterval")) m_pathUpdateInterval = in["pathUpdateInterval"];
    if (in.contains("animIdle"))           m_animIdle           = in["animIdle"];
    if (in.contains("animWander"))         m_animWander         = in["animWander"];
    if (in.contains("animHunt"))           m_animHunt           = in["animHunt"];
    if (in.contains("animDead"))           m_animDead           = in["animDead"];
}

void GhostAI::ImGuiUpdate()
{
    ImGui::DragFloat("Wander Speed",         &m_moveSpeed,          0.1f, 0.1f, 20.0f);
    ImGui::DragFloat("Hunt Speed",           &m_huntSpeed,          0.1f, 0.1f, 20.0f);
    ImGui::DragFloat("Detection Radius",     &m_detectionRadius,    0.5f, 1.0f, 50.0f);
    ImGui::DragFloat("Lose Radius",          &m_loseRadius,         0.5f, 1.0f, 50.0f);
    ImGui::DragFloat("Path Update Interval", &m_pathUpdateInterval, 0.05f, 0.1f, 2.0f);

    if (ImGui::Button("Test Exorcise"))
        Exorcise();

    ImGui::Separator();
    ImGui::Text("Animation Settings");

    auto& ecs    = GameManager::Instance().GetECS();
    auto  entity = GetGameObject()->GetEntityID();

    std::shared_ptr<ModelData> spModel = nullptr;
    std::function<void(std::shared_ptr<GameObject>)> findModel = [&](std::shared_ptr<GameObject> obj)
    {
        if (spModel) return;
        if (ecs.HasComponent<ModelRenderData>(obj->GetEntityID()))
        {
            auto& md = ecs.GetComponent<ModelRenderData>(obj->GetEntityID());
            if (md.m_spModelData) spModel = md.m_spModelData;
        }
        for (auto& child : obj->GetChildren())
            findModel(child);
    };
    findModel(GetGameObject()->shared_from_this());

    if (spModel)
    {
        const auto& animations = spModel->GetAnimations();
        if (!animations.empty())
        {
            std::vector<const char*> animNames;
            for (const auto& anim : animations)
                animNames.push_back(anim.name.c_str());

            auto drawCombo = [&](const char* label, int& idx)
            {
                if (idx < 0)                     idx = 0;
                if (idx >= (int)animNames.size()) idx = (int)animNames.size() - 1;
                ImGui::Combo(label, &idx, animNames.data(), (int)animNames.size());
            };

            drawCombo("Idle Anim",   m_animIdle);
            drawCombo("Wander Anim", m_animWander);
            drawCombo("Hunt Anim",   m_animHunt);
            drawCombo("Dead Anim",   m_animDead);
        }
        else
        {
            ImGui::Text("Model has no animations.");
        }
    }
    else
    {
        ImGui::Text("No ModelRenderData found.");
        ImGui::InputInt("Idle Anim",   &m_animIdle);
        ImGui::InputInt("Wander Anim", &m_animWander);
        ImGui::InputInt("Hunt Anim",   &m_animHunt);
        ImGui::InputInt("Dead Anim",   &m_animDead);
    }
}

void GhostAI::OnCollisionEnter(GameObject* other)
{
    if (m_currentState == GhostAI::State::Dead) return;
    if (other && other->GetName() == "Player")
    {
        SceneManager::Instance().ChangeScene(std::make_shared<TitleScene>());
    }
}

void GhostAI::OnCollisionStay(GameObject* other)
{
}

void GhostAI::Exorcise()
{
    if (m_isExorcised) return;
    m_isExorcised = true;
    NavMeshManager::Instance().ClearPath(GetGameObject()->GetEntityID());
    SetState(GhostAI::State::Dead);
}
