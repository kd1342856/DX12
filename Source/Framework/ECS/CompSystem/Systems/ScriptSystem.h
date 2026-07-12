#pragma once
#include "../../Components/Data/NativeScript.h"
#pragma once

class ScriptSystem : public SystemBase
{
public:
    void Awake()
    {
        for (auto const& entity : m_entities) {
            auto& scriptData = m_pCoordinator->GetComponent<NativeScriptData>(entity);
            if (scriptData.Instance) {
                scriptData.Instance->Awake();
            }
        }
    }

    void Start()
    {
        for (auto const& entity : m_entities) {
            auto& scriptData = m_pCoordinator->GetComponent<NativeScriptData>(entity);
            if (scriptData.Instance) {
                scriptData.Instance->Start();
            }
        }
    }

    void Update(float deltaTime) override
    {
        for (auto const& entity : m_entities) {
            auto& scriptData = m_pCoordinator->GetComponent<NativeScriptData>(entity);
            if (scriptData.Instance) {
                scriptData.Instance->Update(deltaTime);
            }
        }
    }

    void PostUpdate()
    {
        for (auto const& entity : m_entities) {
            auto& scriptData = m_pCoordinator->GetComponent<NativeScriptData>(entity);
            if (scriptData.Instance) {
                scriptData.Instance->PostUpdate();
            }
        }
    }
};
