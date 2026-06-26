import os

def update_gamescene_h():
    path = r'c:\GitHub\DX12\Source\Application\Scene\GameScene\GameScene.h'
    with open(path, 'r', encoding='cp932') as f:
        content = f.read()

    target = '''    bool m_fullscreenGame = false;
    bool m_isCameraDragging = false;'''
    rep = '''    bool m_fullscreenGame = false;
    bool m_isCameraDragging = false;
    bool m_flashlightOn = false;'''
    
    if "bool m_flashlightOn = false;" not in content:
        content = content.replace(target, rep)

    with open(path, 'w', encoding='cp932') as f:
        f.write(content)

def update_gamescene_cpp():
    path = r'c:\GitHub\DX12\Source\Application\Scene\GameScene\GameScene.cpp'
    with open(path, 'r', encoding='cp932') as f:
        content = f.read()

    # 1. Add #include
    inc_target = '#include "../../../Graphics/Device/GraphicsDevice.h"'
    inc_rep = '#include "../../../Graphics/Device/GraphicsDevice.h"\n#include "../../../Framework/ECS/CompSystem/Systems/RenderSystem.h"'
    if '#include "../../../Framework/ECS/CompSystem/Systems/RenderSystem.h"' not in content:
        content = content.replace(inc_target, inc_rep)

    # 2. Add Flashlight toggle in Update
    upd_target = '''    if (Input::Instance().IsKeyTrigger(DirectX::Keyboard::Keys::F5)) { 
        m_fullscreenGame = !m_fullscreenGame;'''
    upd_rep = '''    if (Input::Instance().IsKeyTrigger('F')) { m_flashlightOn = !m_flashlightOn; }
    if (Input::Instance().IsKeyTrigger(DirectX::Keyboard::Keys::F5)) { 
        m_fullscreenGame = !m_fullscreenGame;'''
    if "m_flashlightOn = !m_flashlightOn;" not in content:
        content = content.replace(upd_target, upd_rep)

    # 3. Add SpotLight emission in UpdateCameras
    # We should add it at the end of UpdateCameras, where we have m_gameCameraEntity
    cam_target = '''        if (Input::Instance().IsMouseRightTrigger() && !ImGui::GetIO().WantCaptureMouse) {'''
    cam_rep = '''    if (m_flashlightOn && m_gameCameraEntity != INVALID_ENTITY) {
        auto& ecs = GameManager::Instance().GetECS();
        auto& camTrans = ecs.GetComponent<TransformData>(m_gameCameraEntity);
        CBufferData::SpotLight sl = {};
        sl.Pos = camTrans.m_worldMatrix.Translation();
        sl.Dir = Math::Vector3::TransformNormal(Math::Vector3(0, 0, 1), camTrans.m_worldMatrix);
        sl.Dir.Normalize();
        sl.Range = 30.0f;
        sl.Color = Math::Vector3(1.2f, 1.2f, 1.0f);
        sl.InnerCorn = 0.95f;
        sl.OuterCorn = 0.85f;
        m_spScene->GetRenderSystem()->AddSpotLight(sl);
    }

        if (Input::Instance().IsMouseRightTrigger() && !ImGui::GetIO().WantCaptureMouse) {'''
    if "AddSpotLight(sl)" not in content:
        content = content.replace(cam_target, cam_rep)

    with open(path, 'w', encoding='cp932') as f:
        f.write(content)

if __name__ == '__main__':
    update_gamescene_h()
    update_gamescene_cpp()
