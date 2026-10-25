#include "../../Pch.h"
#include "LocalShadowManager.h"

namespace
{
    // 位置や向きがこれ以上変わったら影を描き直す
    constexpr float kPositionEpsilon = 0.001f;
    constexpr float kDirectionEpsilon = 0.0001f;

    // 左手系の視点行列。キューブマップの各面の向きの規約(D3D)に合わせるためLHで作る
    Math::Matrix MakeLookAtLH(const Math::Vector3& eye, const Math::Vector3& forward, const Math::Vector3& up)
    {
        return DirectX::XMMatrixLookToLH(eye, forward, up);
    }
}

LocalShadowManager& LocalShadowManager::Instance()
{
    static LocalShadowManager instance;
    return instance;
}

LocalShadowManager::LocalShadowManager()
{
    m_spotSlots.resize(kSpotShadowCount);
    m_cubeSlots.resize(kCubeShadowCount);
}

void LocalShadowManager::InvalidateAll()
{
    for (Slot& slot : m_spotSlots)
    {
        slot.isValid = false;
    }
    for (Slot& slot : m_cubeSlots)
    {
        slot.isValid = false;
    }
}

void LocalShadowManager::BeginFrame()
{
    ++m_frame;
    for (Slot& slot : m_spotSlots)
    {
        slot.isRequested = false;
    }
    for (Slot& slot : m_cubeSlots)
    {
        slot.isRequested = false;
    }
}

int LocalShadowManager::AcquireSlot(std::vector<Slot>& slots, uint32_t lightKey)
{
    // 1) 前から同じライトが持っている枠
    for (size_t slotIndex = 0; slotIndex < slots.size(); ++slotIndex)
    {
        if (slots[slotIndex].lightKey == lightKey)
        {
            return slots[slotIndex].isRequested ? -1 : static_cast<int>(slotIndex);
        }
    }

    // 2) このフレームでまだ使われていない枠のうち、最も長く使われていないもの(空きを含む)
    int bestIndex = -1;
    uint64_t oldestFrame = UINT64_MAX;
    for (size_t slotIndex = 0; slotIndex < slots.size(); ++slotIndex)
    {
        const Slot& slot = slots[slotIndex];
        if (slot.isRequested)
        {
            continue;
        }
        const uint64_t usedFrame = (slot.lightKey == UINT32_MAX) ? 0 : slot.lastRequestedFrame;
        if (usedFrame < oldestFrame)
        {
            oldestFrame = usedFrame;
            bestIndex = static_cast<int>(slotIndex);
        }
    }
    if (bestIndex >= 0)
    {
        slots[bestIndex].lightKey = lightKey;
        slots[bestIndex].isValid = false;
    }
    return bestIndex;
}

int LocalShadowManager::RequestSpotShadow(uint32_t lightKey, const Math::Vector3& position, const Math::Vector3& direction,
    float outerAngleRadians, float range, Math::Matrix& outViewProj)
{
    const int slotIndex = AcquireSlot(m_spotSlots, lightKey);
    if (slotIndex < 0)
    {
        return -1;
    }
    Slot& slot = m_spotSlots[slotIndex];
    slot.isRequested = true;
    slot.lastRequestedFrame = m_frame;

    const float fov = outerAngleRadians * 2.0f + kSpotShadowFovMargin * 2.0f;
    const bool isChanged =
        (slot.position - position).LengthSquared() > kPositionEpsilon * kPositionEpsilon ||
        (slot.direction - direction).LengthSquared() > kDirectionEpsilon ||
        std::fabs(slot.fovRadians - fov) > 1e-4f ||
        std::fabs(slot.range - range) > 1e-4f;
    if (isChanged || !slot.isValid)
    {
        slot.isValid = slot.isValid && !isChanged;
        slot.position = position;
        slot.direction = direction;
        slot.fovRadians = fov;
        slot.range = range;
        const Math::Vector3 up = (std::fabs(direction.y) < 0.99f) ? Math::Vector3(0.0f, 1.0f, 0.0f) : Math::Vector3(1.0f, 0.0f, 0.0f);
        slot.view[0] = MakeLookAtLH(position, direction, up);
        slot.projection = DirectX::XMMatrixPerspectiveFovLH(fov, 1.0f, kShadowNear, (std::max)(range, kShadowNear * 2.0f));
    }
    outViewProj = slot.view[0] * slot.projection;
    return slotIndex;
}

int LocalShadowManager::RequestCubeShadow(uint32_t lightKey, const Math::Vector3& position, float range)
{
    const int slotIndex = AcquireSlot(m_cubeSlots, lightKey);
    if (slotIndex < 0)
    {
        return -1;
    }
    Slot& slot = m_cubeSlots[slotIndex];
    slot.isRequested = true;
    slot.lastRequestedFrame = m_frame;

    const bool isChanged =
        (slot.position - position).LengthSquared() > kPositionEpsilon * kPositionEpsilon ||
        std::fabs(slot.range - range) > 1e-4f;
    if (isChanged || !slot.isValid)
    {
        slot.isValid = slot.isValid && !isChanged;
        slot.position = position;
        slot.range = range;

        // D3Dのキューブマップの面の順(+X,-X,+Y,-Y,+Z,-Z)と、それぞれの上方向
        static const Math::Vector3 kFaceDirections[6] = {
            { 1.0f, 0.0f, 0.0f }, { -1.0f, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f }, { 0.0f, -1.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f },
        };
        static const Math::Vector3 kFaceUps[6] = {
            { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f },
            { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f, 1.0f },
            { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f },
        };
        for (int faceIndex = 0; faceIndex < 6; ++faceIndex)
        {
            slot.view[faceIndex] = MakeLookAtLH(position, kFaceDirections[faceIndex], kFaceUps[faceIndex]);
        }
        slot.projection = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV2, 1.0f, kShadowNear, GetCubeFar(range));
    }
    return slotIndex;
}
