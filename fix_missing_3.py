# coding: utf-8
import subprocess

MISSING = [
    (r"c:\GitHub\DX12\Source\Application\Script\Character\Ghost\GhostAI.h", r"Source/Application/Object/Script/Ghost/GhostAI.h"),
    (r"c:\GitHub\DX12\Source\Application\Script\Character\Player\Player.h", r"Source/Application/Object/Script/Player/Player.h"),
    (r"c:\GitHub\DX12\Source\Application\Scene\SceneBase.h", r"Source/Framework/Scene/SceneBase.h")
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

for curr, orig in MISSING:
    res = subprocess.run(["git", "show", f"HEAD:{orig}"], capture_output=True)
    if res.returncode != 0:
        print(f"Failed to fetch {orig}")
        continue
    raw = res.stdout
    try: text = raw.decode("utf-8-sig")
    except: text = raw.decode("cp932", errors="replace")
    
    for o, n in INCLUDE_REPLACE_MAP:
        text = text.replace(o, n)
        
    with open(curr, "w", encoding="cp932") as f:
        f.write(text)

print("Done")
