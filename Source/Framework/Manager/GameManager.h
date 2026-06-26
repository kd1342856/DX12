#pragma once
#include "../ECS/ECS.h"
#include "../ECS/Components/Data/TransformData.h"
#include "../ECS/Components/Data/CameraData.h"
#include "../ECS/Components/Data/ModelRenderData.h"
#include "../ECS/Components/Data/ShaderData.h"
#include "../ECS/Components/Data/AnimationData.h"
#include "../ECS/Components/Data/ColliderData.h"
#include "../ECS/Components/Data/NativeScriptData.h"
#include "../ECS/Components/Data/SpriteData.h"
#include "../Manager/CollisionManager.h"
#include "../System/JobSystem/JobSystem.h"
#include "../DirectX/Utility/ClassAssembly.h"

// =============================================
// GameManager
// ECSCoordinator と ClassAssembly を一元管理する
// マネージャー層のシングルトン
// =============================================
class GameManager
{
public:
    // シングルトンインスタンス取得
    static GameManager& Instance();

    // static破棄時に false になる生存フラグ
    static bool IsInstanceAlive() { return s_alive; }

    // 初期化（アプリ起動時に一度だけ呼ぶ）
        void Init()
    {
        m_ecs.Init();
        // ECS?SR|[lg^o^
        m_ecs.RegisterComponent<TransformData>();
        m_ecs.RegisterComponent<CameraData>();
        m_ecs.RegisterComponent<ModelRenderData>();
        m_ecs.RegisterComponent<ShaderData>();
        m_ecs.RegisterComponent<AnimationDataComponent>();
        m_ecs.RegisterComponent<ColliderData>();
        m_ecs.RegisterComponent<NativeScriptData>();
        m_ecs.RegisterComponent<std::shared_ptr<NativeScript>>();
        m_ecs.RegisterComponent<SpriteData>();
        CollisionManager::Instance().Init();
		JobSystem::Instance().Init();
    }

    // ECS取得
    ECSCoordinator& GetECS() { return m_ecs; }

    // ClassAssembly取得（型登録の一本化）
    ClassAssembly& GetClassAssembly() { return ClassAssembly::Instance(); }

private:
    GameManager() {}
    ~GameManager() { JobSystem::Instance().Shutdown(); s_alive = false; }
    GameManager(const GameManager&) = delete;
    GameManager& operator=(const GameManager&) = delete;

    ECSCoordinator m_ecs;

    // シャットダウン時の二重アクセス防止フラグ
    static bool s_alive;
};

