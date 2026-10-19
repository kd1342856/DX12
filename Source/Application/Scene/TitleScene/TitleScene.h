#pragma once

#include "../SceneBase.h"
#include "../../../Framework/ECS/Entity/Entity.h"

class TitleScene : public SceneBase
{
public:
    void Init() override;
    void Update(float deltaTime) override;
    void Unload() override;

private:
    float m_time = 0.0f;
    Entity m_bgEntity = INVALID_ENTITY;
    Entity m_titleEntity = INVALID_ENTITY;
    Entity m_startEntity = INVALID_ENTITY;
    Entity m_exitEntity = INVALID_ENTITY;
    int m_cursorIndex = 0;
};
