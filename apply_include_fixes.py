# coding: utf-8
import os

INCLUDE_REPLACE_MAP = [
    (r"Application/Scene/SceneBase.h",               r"Framework/Scene/SceneBase.h"),
    (r"Manager/Scene.h",                             r"Scene/Scene.h"),
    (r"Manager/SceneManager.h",                      r"Scene/SceneManager.h"),
    (r"Manager/CollisionManager.h",                  r"Collision/CollisionManager.h"),
    (r"Manager/CollisionShape.h",                    r"Collision/CollisionShape.h"),
    (r"Manager/CollisionSolver.h",                   r"Collision/CollisionSolver.h"),
    (r"Manager/ResourceManager.h",                   r"Resource/ResourceManager.h"),
    (r"Manager/PrefabManager.h",                     r"Resource/PrefabManager.h"),
    (r"Manager/AudioManager.h",                      r"Audio/AudioManager.h"),
    (r"Manager/AnimationManager.h",                  r"Animation/AnimationManager.h"),
    (r"Manager/GameManager.h",                       r"GameManager.h"),
    (r"DirectX/Utility/ClassAssembly.h",             r"Utility/ClassAssembly.h"),
    (r"DirectX/Utility/Input.h",                     r"Utility/Input.h"),
    (r"DirectX/Utility/Logger.h",                    r"Utility/Logger.h"),
    (r"DirectX/Utility/Random.h",                    r"Utility/Random.h"),
    (r"DirectX/Utility/Time.h",                      r"Utility/Time.h"),
    (r"DirectX/Utility/Utility.h",                   r"Utility/Utility.h"),
    (r"DirectX/Window/Window.h",                     r"Window/Window.h"),
    (r"DirectX/GDF/GDF.h",                           r"GDF/GDF.h"),
    (r"ImGuiEditor/Editor/Editor.h",                 r"Editor/Editor.h"),
    (r"System/JobSystem/JobSystem.h",                r"JobSystem/JobSystem.h"),
]

files_to_fix = [
    r"c:\GitHub\DX12\Source\Application\Application.cpp",
    r"c:\GitHub\DX12\Source\Application\Application.h",
    r"c:\GitHub\DX12\Source\Application\Scene\GameScene\GameScene.cpp",
    r"c:\GitHub\DX12\Source\Application\Scene\TitleScene\TitleScene.cpp",
    r"c:\GitHub\DX12\Source\Application\Script\Character\Player\Player.cpp",
    r"c:\GitHub\DX12\Source\Framework\Editor\Editor.cpp",
    r"c:\GitHub\DX12\Source\Framework\Manager\Collision\CollisionManager.cpp",
    r"c:\GitHub\DX12\Source\Framework\Manager\Scene\SceneManager.cpp"
]

for filepath in files_to_fix:
    with open(filepath, "r", encoding="cp932") as f:
        content = f.read()
    
    for old_str, new_str in INCLUDE_REPLACE_MAP:
        content = content.replace(old_str, new_str)
        
    with open(filepath, "w", encoding="cp932") as f:
        f.write(content)

print("Applied include fixes.")
