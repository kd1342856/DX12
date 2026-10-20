#pragma once
#include "../GraphicsShader/GraphicsShader.h"

// Full-screen background for the clear result screen: a pale blue-white mist (the counterpart of
// FogShader, the game-over screen's red fog).
// Same setup as FogShader (no vertex buffer; a single full-screen triangle and a time constant).
class MistShader : public GraphicsShader
{
public:
    void Create(GraphicsDevice* pGraphicsDevice) override;
    void Draw(float time);
};
