# coding: utf-8
import os
import subprocess

ALREADY_FIXED = {
    r"c:\GitHub\DX12\Source\Application\Application.cpp",
    r"c:\GitHub\DX12\Source\Application\Application.h",
    r"c:\GitHub\DX12\Source\Application\Scene\GameScene\GameScene.cpp",
    r"c:\GitHub\DX12\Source\Application\Scene\TitleScene\TitleScene.cpp",
    r"c:\GitHub\DX12\Source\Application\Script\Character\Player\Player.cpp",
    r"c:\GitHub\DX12\Source\Framework\Editor\Editor.cpp",
    r"c:\GitHub\DX12\Source\Framework\Manager\Collision\CollisionManager.cpp",
    r"c:\GitHub\DX12\Source\Framework\Manager\Scene\SceneManager.cpp",
    r"c:\GitHub\DX12\Source\Application\Script\Character\Ghost\GhostAI.cpp", # Has my UI changes
    r"c:\GitHub\DX12\Source\Pch.h",
    r"c:\GitHub\DX12\Source\Framework\ECS\Components\Data\NativeScript.h"
}

# The user script mapped the old paths to new paths. We can reverse this mapping:
REVERSE_PROJ_MAP = [
    (r"Source/Application/Script/Player", r"Source/Application/Object/Script/Player"),
    (r"Source/Application/Script/Ghost", r"Source/Application/Object/Script/Ghost"),
    (r"Source/Application/Script/System", r"Source/Application/Object/Script/System"),
    (r"Source/Framework/Scene/SceneBase.h", r"Source/Application/Scene/SceneBase.h"),
    (r"Source/Framework/Scene/Scene.h", r"Source/Framework/Manager/Scene.h"),
    (r"Source/Framework/Scene/Scene.cpp", r"Source/Framework/Manager/Scene.cpp"),
    (r"Source/Framework/Scene/SceneManager.h", r"Source/Framework/Manager/SceneManager.h"),
    (r"Source/Framework/Scene/SceneManager.cpp", r"Source/Framework/Manager/SceneManager.cpp"),
    (r"Source/Framework/Collision/CollisionManager.h", r"Source/Framework/Manager/CollisionManager.h"),
    (r"Source/Framework/Collision/CollisionManager.cpp", r"Source/Framework/Manager/CollisionManager.cpp"),
    (r"Source/Framework/Collision/CollisionShape.h", r"Source/Framework/Manager/CollisionShape.h"),
    (r"Source/Framework/Collision/CollisionShape.cpp", r"Source/Framework/Manager/CollisionShape.cpp"),
    (r"Source/Framework/Collision/CollisionSolver.h", r"Source/Framework/Manager/CollisionSolver.h"),
    (r"Source/Framework/Collision/CollisionSolver.cpp", r"Source/Framework/Manager/CollisionSolver.cpp"),
    (r"Source/Framework/Resource/ResourceManager.h", r"Source/Framework/Manager/ResourceManager.h"),
    (r"Source/Framework/Resource/ResourceManager.cpp", r"Source/Framework/Manager/ResourceManager.cpp"),
    (r"Source/Framework/Resource/PrefabManager.h", r"Source/Framework/Manager/PrefabManager.h"),
    (r"Source/Framework/Resource/PrefabManager.cpp", r"Source/Framework/Manager/PrefabManager.cpp"),
    (r"Source/Framework/Audio/AudioManager.h", r"Source/Framework/Manager/AudioManager.h"),
    (r"Source/Framework/Audio/AudioManager.cpp", r"Source/Framework/Manager/AudioManager.cpp"),
    (r"Source/Framework/Animation/AnimationManager.h", r"Source/Framework/Manager/AnimationManager.h"),
    (r"Source/Framework/Animation/AnimationManager.cpp", r"Source/Framework/Manager/AnimationManager.cpp"),
    (r"Source/Framework/GameManager.h", r"Source/Framework/Manager/GameManager.h"),
    (r"Source/Framework/GameManager.cpp", r"Source/Framework/Manager/GameManager.cpp"),
    (r"Source/Framework/Utility", r"Source/Framework/DirectX/Utility"),
    (r"Source/Framework/Window", r"Source/Framework/DirectX/Window"),
    (r"Source/Graphics/GDF", r"Source/Framework/DirectX/GDF"),
    (r"Source/Framework/Editor", r"Source/Framework/ImGuiEditor/Editor"),
    (r"Source/Framework/JobSystem", r"Source/Framework/System/JobSystem")
]

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

def get_git_old_path(current_path):
    # Convert backslashes
    git_path = current_path.replace("\\", "/").replace("c:/GitHub/DX12/", "")
    old_git_path = git_path
    
    # Apply reverse mapping
    for new_prefix, old_prefix in REVERSE_PROJ_MAP:
        if git_path.startswith(new_prefix):
            old_git_path = git_path.replace(new_prefix, old_prefix, 1)
            break
            
    return old_git_path

for dirpath, _, filenames in os.walk(r"c:\GitHub\DX12\Source"):
    if ".git" in dirpath: continue
    for f in filenames:
        if not f.endswith((".cpp", ".h")): continue
        filepath = os.path.join(dirpath, f)
        
        # Normalize casing to be safe
        if filepath.lower() in [p.lower() for p in ALREADY_FIXED]:
            continue
            
        old_git_path = get_git_old_path(filepath)
        
        # Fetch from git
        res = subprocess.run(["git", "show", f"HEAD:{old_git_path}"], capture_output=True)
        if res.returncode != 0:
            print(f"Failed to fetch {old_git_path} from git. Skipping.")
            continue
            
        raw = res.stdout
        try:
            text = raw.decode("utf-8-sig")
        except:
            try:
                text = raw.decode("cp932")
            except:
                text = raw.decode("utf-8", errors="replace")
                
        text = text.replace("\r\n", "\n")
        
        # Apply include replacements
        for old_inc, new_inc in INCLUDE_REPLACE_MAP:
            text = text.replace(old_inc, new_inc)
            
        with open(filepath, "w", encoding="cp932", errors="replace") as out_f:
            out_f.write(text)

print("Restored all remaining files from HEAD and applied includes.")
