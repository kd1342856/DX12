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
    Entity m_bgEntity = 0;
    Entity m_titleEntity = 0;
    Entity m_startEntity = 0;
    Entity m_exitEntity = 0;
    int m_cursorIndex = 0;
};
