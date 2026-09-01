#pragma once

class SpriteRenderSystem : public SystemBase {
public:
    void Init() {}
    void Update(float deltaTime) override;
    void Render();
};
