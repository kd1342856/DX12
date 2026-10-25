#pragma once

struct LoadModelOption
{
    bool loadAnimation = true;
    bool generateTangent = true;
    bool optimizeMesh = true;
    bool generateCollider = false;
    bool mergeMeshes = true;
    float scale = 1.0f;
};
