#pragma once

#include "../SceneBase.h"
#include "../../../Framework/ECS/Entity/Entity.h"
#include <string>

class ResultScene : public SceneBase
{
public:
    // ghostType: GhostAI::GhostTypeの値(-1なら不明)。huntsSurvived: ハントを生き延びた回数。
    ResultScene(bool isClear, float timeTaken = 0.0f, int ghostType = -1, int huntsSurvived = 0);

    void Init() override;
    void Update(float deltaTime) override;
    void Unload() override;

private:
    // クリア画面: 青白い霧の背景(MistShader)の上に見出し・ゴーストの正体・クリアタイム・生き延びたハント・評価ランクを順に出す
    void DrawClearReport();
    char CalcRank() const;

    bool m_isClear;
    float m_timeTaken;
    int m_ghostType;
    int m_huntsSurvived;

    float m_time = 0.0f;
    Entity m_bgEntity = 0;
    Entity m_buttonEntity = 0;
    Entity m_deathEntity = 0;
};
