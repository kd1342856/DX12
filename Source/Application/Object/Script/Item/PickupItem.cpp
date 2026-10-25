#include "../../../../Pch.h"
#include "PickupItem.h"
#include "../../../../Framework/Object/GameObject.h"
#include "../../../../Framework/DirectX/Utility/Logger.h"
#include "../Player/Player.h"
#include "../../../../Framework/Manager/Collision/CollisionManager.h"

REGISTER_COMPONENT(PickupItem);

void PickupItem::Serialize(nlohmann::json& out) const
{
    out["ItemType"] = static_cast<int>(m_itemType);
}

void PickupItem::Deserialize(const nlohmann::json& in)
{
    if (in.contains("ItemType"))
    {
        m_itemType = static_cast<ItemType>((int)in["ItemType"]);
    }
}

void PickupItem::ImGuiUpdate()
{
    int current = static_cast<int>(m_itemType) - 2; // enum starts at 2 (Incense)
    const char* names[] = {"Incense (SmudgeStick)", "Amulet (Ofuda)", "FlashLight"};
    if (ImGui::Combo("Item Type", &current, names, IM_ARRAYSIZE(names)))
    {
        m_itemType = static_cast<ItemType>(current + 2);
    }
    ImGui::Text("[Debug] Collected: %s", m_collected ? "true" : "false");
}

void PickupItem::Update(float deltaTime)
{
    if (m_isFlying)
    {
        UpdateFlight(deltaTime);
    }
    if (m_itemType == ItemType::FlashLight)
    {
        SyncLight(deltaTime);
    }
}

void PickupItem::SyncLight(float deltaTime)
{
    if (!m_pGameObject)
    {
        return;
    }
    auto& ecs = GameManager::Instance().GetECS();
    Entity e = m_pGameObject->GetEntityID();
    // Collect() destroys the entity from inside Player::Update, but ScriptSystem can still call this
    // script's Update afterwards, with the entity already invalid: never touch components then.
    if (m_collected || e == INVALID_ENTITY || !ecs.IsAlive(e))
    {
        return;
    }
    SpotLightData* pLight = ecs.TryGetComponent<SpotLightData>(e);
    if (!pLight)
    {
        // Added lazily so every FlashLight pickup (placed or thrown) has a light that follows its Transform.
        SpotLightData light;
        light.m_color = {1.0f, 0.97f, 0.9f};
        light.m_intensity = 6.0f;
        light.m_range = 15.0f;
        light.m_innerAngle = 12.0f;
        light.m_outerAngle = 22.0f;
        light.m_localOffset = {0.0f, 0.06f, 0.15f};
        light.m_localDirection = {0.0f, 0.0f, 1.0f};
        light.m_glowMaterial = "Glass"; // only the lens glows, not the whole body
        light.m_enabled = m_lightOn;
        ecs.AddComponent(e, light);
        return;
    }
    pLight->m_enabled = m_lightOn;
    // Ghost interference only flickers the look (m_flickerOff); the light itself stays on.
    bool flickerOff = m_lightFlicker.Update(deltaTime);
    pLight->m_flickerOff = m_lightOn && flickerOff;
}

void PickupItem::Launch(const Math::Vector3& velocity)
{
    m_velocity = velocity;
    m_isFlying = true;
    m_flightTime = 0.0f;
    m_spinSpeed = Random::Instance().Range(-6.0f, 6.0f);
}

void PickupItem::UpdateFlight(float deltaTime)
{
    if (!m_pGameObject)
    {
        return;
    }
    auto* pTrans = GameManager::Instance().GetECS().TryGetComponent<TransformData>(m_pGameObject->GetEntityID());
    if (!pTrans)
    {
        m_isFlying = false;
        return;
    }

    const float kGravity = 9.8f;
    const float kWallClearance = 0.1f;   // keep the item this far off a wall it hits
    const float kWallRestitution = 0.3f; // fraction of horizontal speed kept (reversed) after hitting a wall
    const float kFloorRestitution = 0.3f;
    const float kFloorFriction = 0.5f; // fraction of horizontal speed kept on each floor bounce
    const float kRestSpeed = 1.0f;     // below this vertical impact speed it stops bouncing and settles
    const float kMaxFlightTime = 5.0f; // safety net in case it never finds a floor

    deltaTime = (std::min)(deltaTime, 0.05f); // avoid tunnelling through thin geometry on a hitch
    auto& cm = CollisionManager::Instance();
    Math::Vector3 pos = pTrans->m_position;

    m_velocity.y -= kGravity * deltaTime;
    m_flightTime += deltaTime;
    pTrans->m_rotation.y += m_spinSpeed * deltaTime;

    // Horizontal move, stopping (and bouncing back a little) at walls.
    Math::Vector3 horiz(m_velocity.x, 0.0f, m_velocity.z);
    float horizStep = horiz.Length() * deltaTime;
    if (horizStep > 0.0001f)
    {
        Math::Vector3 dir = horiz / horiz.Length();
        RaycastHit wall = cm.RaycastAgainstMesh(pos + Math::Vector3(0, 0.05f, 0), dir, horizStep + kWallClearance, "Stage");
        if (wall.hit)
        {
            float allowed = (std::max)(0.0f, wall.distance - kWallClearance);
            pos += dir * allowed;
            m_velocity.x *= -kWallRestitution;
            m_velocity.z *= -kWallRestitution;
        }
        else
        {
            pos += dir * horizStep;
        }
    }

    // Vertical move, landing on the floor.
    float fallStep = m_velocity.y * deltaTime;
    const float probeUp = 0.3f;
    RaycastHit floor = cm.RaycastAgainstMesh(pos + Math::Vector3(0, probeUp, 0), Math::Vector3(0, -1, 0),
                                             probeUp + (std::max)(0.0f, -fallStep) + 0.01f, "Stage");
    float floorY = floor.hit ? (pos.y + probeUp - floor.distance) : -FLT_MAX;

    if (m_velocity.y <= 0.0f && floor.hit && pos.y + fallStep <= floorY)
    {
        pos.y = floorY;
        if (-m_velocity.y > kRestSpeed)
        {
            m_velocity.y = -m_velocity.y * kFloorRestitution;
            m_velocity.x *= kFloorFriction;
            m_velocity.z *= kFloorFriction;
            m_spinSpeed *= kFloorFriction;
        }
        else
        {
            m_isFlying = false;
            m_velocity = Math::Vector3::Zero;
        }
    }
    else
    {
        pos.y += fallStep;
    }

    if (m_flightTime > kMaxFlightTime)
    {
        m_isFlying = false;
        Logger::Instance().AddLog(Logger::LogLevel::Warning, "[PickupItem] Thrown item never landed - stopped it mid-air.");
    }

    pTrans->m_position = pos;
}

void PickupItem::Collect(Player* player)
{
    if (m_collected || !player)
    {
        return;
    }

    player->AddItem(m_itemType);
    if (m_itemType == ItemType::FlashLight)
    {
        player->SetFlashLightOn(m_lightOn); // keep it lit if it was
    }

    m_collected = true;
    Logger::Instance().AddLog(Logger::LogLevel::Info, "[PickupItem] Item collected via F key.");
    if (m_pGameObject)
    {
        m_pGameObject->Destroy();
    }
}
