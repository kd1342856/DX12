#pragma once
#include "SceneBase.h"

// =============================================
// GameScene
// =============================================
class GameScene : public SceneBase
{
public:
void Init() override;
void Update() override;

private:
std::shared_ptr<ModelData> m_spModel;
Math::Matrix m_mWorld;
Math::Matrix m_mWorld2;
};