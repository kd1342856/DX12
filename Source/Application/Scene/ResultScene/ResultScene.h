#pragma once

#include "../SceneBase.h"
#include "../../../Framework/ECS/Entity/Entity.h"
#include <string>

class ResultScene : public SceneBase
{
public:
    ResultScene(bool isClear, float timeTaken = 0.0f, const std::string& ghostName = "", int reward = 0);

    void Init() override;
    void Update(float deltaTime) override;
    void Unload() override;

private:
    bool m_isClear;
    float m_timeTaken;
    std::string m_ghostName;
    int m_reward;

    float m_time = 0.0f;
    Entity m_bgEntity = 0;
    Entity m_buttonEntity = 0;
    Entity m_deathEntity = 0;
};
