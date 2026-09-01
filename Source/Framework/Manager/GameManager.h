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
// ECSCoordinator �Ɗe System �̈ꌳ�Ǘ�
// Application ���[�v���� Update() ���ĂԂ����őS System ������
// =============================================
class GameManager
{
public:
    // �V���O���g���C���X�^���X�擾
    static GameManager& Instance();

    // static �j����� false �ɂȂ鐶���t���O
    static bool IsInstanceAlive() { return s_alive; }

    // �A�v���N������1�񂾂��Ă�
    // Component �^�o�^ + �S System �o�^ + Signature �ݒ�
    void Init();

    void Update(float deltaTime, class Scene* pScene);


    // ECS �擾
    ECSCoordinator& GetECS() { return m_ecs; }

    // ClassAssembly �擾�i�^�o�^�̈�{���j
    ClassAssembly& GetClassAssembly() { return ClassAssembly::Instance(); }

    // System �A�N�Z�T�i�K�v�ȏꍇ�̂݁j
    std::shared_ptr<RenderSystem>       GetRenderSystem()       const { return m_spRenderSystem; }
    std::shared_ptr<SpriteRenderSystem> GetSpriteRenderSystem() const { return m_spSpriteRenderSystem; }
    std::shared_ptr<CameraSystem>       GetCameraSystem()       const { return m_spCameraSystem; }
    std::shared_ptr<ScriptSystem>       GetScriptSystem()       const { return m_spScriptSystem; }

    void Shutdown();

private:
    GameManager() {}
    ~GameManager() { s_alive = false; }
    GameManager(const GameManager&) = delete;
    GameManager& operator=(const GameManager&) = delete;

    ECSCoordinator m_ecs;

    // �풓 System�iScene �ؑ֌������������j
    std::shared_ptr<ScriptSystem>       m_spScriptSystem;
    std::shared_ptr<TransformSystem>    m_spTransformSystem;
    std::shared_ptr<CameraSystem>       m_spCameraSystem;
    std::shared_ptr<AnimationSystem>    m_spAnimationSystem;
    std::shared_ptr<RenderSystem>       m_spRenderSystem;
    std::shared_ptr<SpriteRenderSystem> m_spSpriteRenderSystem;

    // �V���b�g�_�E���̓�d�A�N�Z�X�h�~�t���O
    static bool s_alive;

    // Init()�̓�d�Ăяo���h�~(GameScene::Init()���������ĕ�����Ă΂�Ă�
    // ECS���󂳂Ȃ��悤�ɂ��邽�߂̃K�[�h)
    bool m_isInitialized = false;
};
