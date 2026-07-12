#pragma once
#include "../Manager/Collision/CollisionManager.h"
#include "../System/JobSystem/JobSystem.h"
#include "../DirectX/Utility/ClassAssembly.h"
class RenderSystem;
class SpriteRenderSystem;
class TransformSystem;
class CameraSystem;
class AnimationSystem;
class ScriptSystem;
class Scene;

// =============================================
// GameManager
// ECSCoordinator と各 System の一元管理
// Application ループから Update() を呼ぶだけで全 System が動く
// =============================================
class GameManager
{
public:
    // シングルトンインスタンス取得
    static GameManager& Instance();

    // static 破棄後に false になる生存フラグ
    static bool IsInstanceAlive() { return s_alive; }

    // アプリ起動時に1回だけ呼ぶ
    // Component 型登録 + 全 System 登録 + Signature 設定
    void Init();

    void Update(float deltaTime, class Scene* pScene);


    // ECS 取得
    ECSCoordinator& GetECS() { return m_ecs; }

    // ClassAssembly 取得（型登録の一本化）
    ClassAssembly& GetClassAssembly() { return ClassAssembly::Instance(); }

    // System アクセサ（必要な場合のみ）
    std::shared_ptr<RenderSystem>       GetRenderSystem()       const { return m_spRenderSystem; }
    std::shared_ptr<SpriteRenderSystem> GetSpriteRenderSystem() const { return m_spSpriteRenderSystem; }
    std::shared_ptr<CameraSystem>       GetCameraSystem()       const { return m_spCameraSystem; }

private:
    GameManager() {}
    ~GameManager() { JobSystem::Instance().Shutdown(); s_alive = false; }
    GameManager(const GameManager&) = delete;
    GameManager& operator=(const GameManager&) = delete;

    ECSCoordinator m_ecs;

    // 常駐 System（Scene 切替後も生き続ける）
    std::shared_ptr<ScriptSystem>       m_spScriptSystem;
    std::shared_ptr<TransformSystem>    m_spTransformSystem;
    std::shared_ptr<CameraSystem>       m_spCameraSystem;
    std::shared_ptr<AnimationSystem>    m_spAnimationSystem;
    std::shared_ptr<RenderSystem>       m_spRenderSystem;
    std::shared_ptr<SpriteRenderSystem> m_spSpriteRenderSystem;

    // シャットダウンの二重アクセス防止フラグ
    static bool s_alive;
};
