#pragma once

#include "../SceneBase.h"

class TitleScene : public SceneBase
{
public:
    void Init() override;
    void Update(float deltaTime) override;
};
