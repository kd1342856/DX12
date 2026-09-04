#pragma once
#include "../../Components/Data/NativeScript.h"
#include "../../../DirectX/Utility/Profiler.h"
#include <typeinfo>
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
                // スクリプト種別ごとの内訳(RTTI型名)。ScriptSystem::Updateの合計時間の中で
                // 実際にどのスクリプトがコストを使っているかを見られるようにする。
                PROFILE_CPU_SCOPE(typeid(*scriptData.Instance).name());
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

    void PreDraw()
    {
        for (auto const& entity : m_entities) {
            auto& scriptData = m_pCoordinator->GetComponent<NativeScriptData>(entity);
            if (scriptData.Instance) {
                scriptData.Instance->PreDraw();
            }
        }
    }

    void Draw()
    {
        for (auto const& entity : m_entities) {
            auto& scriptData = m_pCoordinator->GetComponent<NativeScriptData>(entity);
            if (scriptData.Instance) {
                scriptData.Instance->Draw();
            }
        }
    }
};
