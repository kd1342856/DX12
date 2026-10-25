#pragma once

#include "../SceneBase.h"
#include "../../../Framework/ECS/Entity/Entity.h"

class TitleScene : public SceneBase
{
public:
    // Title BGM. Its volume is set per track in the Sound Manager (registered in Application at startup)
    static constexpr const char* kBGMPath = "Asset/Sound/BGM/shoujorei.wav";
    static constexpr float kBGMDefaultVolume = 0.6f;

    void Init() override;
    void Update(float deltaTime) override;
    void Unload() override;

private:
    float m_time = 0.0f;
    Entity m_bgEntity = INVALID_ENTITY;
    Entity m_titleEntity = INVALID_ENTITY;
    Entity m_startEntity = INVALID_ENTITY;
    Entity m_exitEntity = INVALID_ENTITY;
    // 0: スタート, 1: 終了, 2: オプション(右下の歯車)
    int m_cursorIndex = 0;
    float m_gearAngle = 0.0f;
};
