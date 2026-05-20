#pragma once
#include "../ComponentBase.h"

struct PostProcessData
{
    float m_exposure = 1.0f;
};

class PostProcessComponent : public ComponentBase
{
public:
    const char* GetComponentName() const override { return "PostProcessComponent"; }
    void ImGuiUpdate() override;
    
    PostProcessData& GetData() { return m_data; }
    const PostProcessData& GetData() const { return m_data; }

private:
    PostProcessData m_data;
};