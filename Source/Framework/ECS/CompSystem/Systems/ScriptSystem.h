#pragma once
#include "../../ComponentManager.h"
#include "../../ECSCoordinator.h"
#include "../System.h"
#include "../../Components/Data/NativeScriptData.h"

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

    void Update()
    {
        for (auto const& entity : m_entities) {
            auto& scriptData = m_pCoordinator->GetComponent<NativeScriptData>(entity);
            if (scriptData.Instance) {
                scriptData.Instance->Update();
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