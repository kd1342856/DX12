import re

with open('Source/Application/Scene/GameScene/GameScene.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# 1. Fix F5 and mouse hiding
content = content.replace(
    'if (Input::Instance().IsKeyTrigger(DirectX::Keyboard::Keys::F5))\n    {\n        m_fullscreenGame = !m_fullscreenGame;\n    }',
    '''if (Input::Instance().IsKeyTrigger(DirectX::Keyboard::Keys::F5) || Input::Instance().IsKeyTrigger(VK_F5))
    {
        m_fullscreenGame = !m_fullscreenGame;
        if (m_fullscreenGame) {
            Input::Instance().SetMouseModeRelative();
            ShowCursor(FALSE);
        } else {
            Input::Instance().SetMouseModeAbsolute();
            ShowCursor(TRUE);
        }
    }'''
)

# 1.5 Fix F1
content = content.replace(
    'if (Input::Instance().IsKeyTrigger(DirectX::Keyboard::Keys::F1))',
    'if (Input::Instance().IsKeyTrigger(DirectX::Keyboard::Keys::F1) || Input::Instance().IsKeyTrigger(VK_F1))'
)

# 2. Fix Relative Mouse Mode Toggle
content = content.replace(
    '''if (Input::Instance().IsMouseRightTrigger()) {
        Input::Instance().SetMouseModeRelative();
    } else if (Input::Instance().IsMouseRightRelease()) {
        Input::Instance().SetMouseModeAbsolute();
    }''',
    '''if (Input::Instance().IsMouseRightTrigger() && !ImGui::GetIO().WantCaptureMouse) {
        Input::Instance().SetMouseModeRelative();
        m_isCameraDragging = true;
    } else if (Input::Instance().IsMouseRightRelease() && m_isCameraDragging) {
        Input::Instance().SetMouseModeAbsolute();
        m_isCameraDragging = false;
    }'''
)

# 3. Fix Editor Camera condition
content = content.replace(
    'if (editorCameraEntity != INVALID_ENTITY && pEditorCameraObj && Input::Instance().IsMouseRightHold())',
    'if (editorCameraEntity != INVALID_ENTITY && pEditorCameraObj && m_isCameraDragging)'
)

# 4. Fix Game Camera mouse dragging condition
content = content.replace(
    'if (Input::Instance().IsMouseRightHold() || m_fullscreenGame) {',
    'if (m_isCameraDragging || m_fullscreenGame) {'
)

# 5. Remove Player Movement logic (WASD) and fix TPS Camera
old_tps_logic = '''                Math::Vector3 moveVec = Math::Vector3(0, 0, 0);
                if (Input::Instance().IsKeyHold(DirectX::Keyboard::Keys::W)) moveVec += forward;
                if (Input::Instance().IsKeyHold(DirectX::Keyboard::Keys::S)) moveVec -= forward;
                if (Input::Instance().IsKeyHold(DirectX::Keyboard::Keys::D)) moveVec += right;
                if (Input::Instance().IsKeyHold(DirectX::Keyboard::Keys::A)) moveVec -= right;

                if (moveVec.LengthSquared() > 0.0f) {
                    moveVec.Normalize();
                    pData.m_position += moveVec * camData.m_moveSpeed;
                }

                if (camData.m_cameraMode == CameraMode::FPS) {
                    cData.m_position = pData.m_position + Math::Vector3(0, 1.5f, 0);
                    cData.m_rotation.y = pData.m_rotation.y;
                } else if (camData.m_cameraMode == CameraMode::TPS) {
                    Math::Matrix camRot = Math::Matrix::CreateFromYawPitchRoll(pData.m_rotation.y, cData.m_rotation.x, 0.0f);
                    Math::Vector3 offset = Math::Vector3::TransformNormal(Math::Vector3(0, 2.0f, -5.0f), camRot);
                    cData.m_position = pData.m_position + offset;
                    cData.m_rotation.y = pData.m_rotation.y;
                }'''

new_tps_logic = '''                if (camData.m_cameraMode == CameraMode::FPS) {
                    cData.m_position = Math::Vector3(0, 1.5f, 0); // Local to player
                    cData.m_rotation.y = 0;
                } else if (camData.m_cameraMode == CameraMode::TPS) {
                    Math::Matrix localRot = Math::Matrix::CreateRotationX(cData.m_rotation.x);
                    Math::Vector3 offset = Math::Vector3::TransformNormal(Math::Vector3(0, 2.0f, -5.0f), localRot);
                    cData.m_position = offset; // Local to player
                    cData.m_rotation.y = 0;
                }'''

if old_tps_logic in content:
    content = content.replace(old_tps_logic, new_tps_logic)
else:
    print("Could not find old TPS logic")

with open('Source/Application/Scene/GameScene/GameScene.cpp', 'w', encoding='shift_jis', errors='ignore') as f:
    f.write(content)
print("GameScene.cpp updated.")
