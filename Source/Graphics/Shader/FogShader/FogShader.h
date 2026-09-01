#pragma once
#include "../GraphicsShader/GraphicsShader.h"

class FogShader : public GraphicsShader
{
public:
    void Create(GraphicsDevice* pGraphicsDevice) override;
    void Draw(float time);
};
