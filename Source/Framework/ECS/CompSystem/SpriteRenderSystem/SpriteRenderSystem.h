#pragma once
#include "../System.h"

class SpriteRenderSystem : public SystemBase {
public:
    void Init() {}
    void Update() override;
    void Render();
};
