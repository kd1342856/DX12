#pragma once
#include "../ECS/ECS.h"
#include "../ECS/Components/Data/ColliderData.h"
#include "../Manager/CollisionManager.h"
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
    static GameManager& Instance()
    {
        static GameManager instance;
        return instance;
    }

    // static破棄時に false になる生存フラグ
    static bool IsInstanceAlive() { return s_alive; }

    // 初期化（アプリ起動時に一度だけ呼ぶ）
    void Init()
    {
        m_ecs.Init();
        // ECSに全コンポーネント型を登録
        m_ecs.RegisterComponent<TransformData>();
        m_ecs.RegisterComponent<CameraData>();
        m_ecs.RegisterComponent<ModelRenderData>();
        m_ecs.RegisterComponent<ShaderData>();
        m_ecs.RegisterComponent<AnimationDataComponent>();
        m_ecs.RegisterComponent<ColliderData>();
        CollisionManager::Instance().Init();
    }

    // ECS取得
    ECSCoordinator& GetECS() { return m_ecs; }

    // ClassAssembly取得（型登録の一本化）
    ClassAssembly& GetClassAssembly() { return ClassAssembly::Instance(); }

private:
    GameManager() {}
    ~GameManager() { s_alive = false; }
    GameManager(const GameManager&) = delete;
    GameManager& operator=(const GameManager&) = delete;

    ECSCoordinator m_ecs;

    // シャットダウン時の二重アクセス防止フラグ
    static bool s_alive;
};