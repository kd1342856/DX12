#pragma once
#include "../EditorContext.h"

class NavMeshEditor
{
public:
    void Initialize();
    void Draw(EditorContext& ctx);
    
private:
    bool m_placementMode = false;
};
