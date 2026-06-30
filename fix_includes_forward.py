# coding: utf-8
import os

FORWARD_INCLUDE_MAP = [
    (r"Framework/Scene/SceneBase.h", r"Application/Scene/SceneBase.h"),
    (r"Scene/Scene.h", r"Manager/Scene/Scene.h"),
    (r"Scene/SceneManager.h", r"Manager/Scene/SceneManager.h"),
    (r"Collision/CollisionManager.h", r"Manager/Collision/CollisionManager.h"),
    (r"Collision/CollisionShape.h", r"Manager/Collision/CollisionShape.h"),
    (r"Collision/CollisionSolver.h", r"Manager/Collision/CollisionSolver.h"),
    (r"Resource/ResourceManager.h", r"Manager/Resource/ResourceManager.h"),
    (r"Resource/PrefabManager.h", r"Manager/Resource/PrefabManager.h"),
    (r"Audio/AudioManager.h", r"Manager/Audio/AudioManager.h"),
    (r"Animation/AnimationManager.h", r"Manager/Animation/AnimationManager.h"),
    (r"Framework/GameManager.h", r"Framework/Manager/GameManager.h"), # Wait, GameManager went the OTHER way!
    (r"Utility/ClassAssembly.h", r"DirectX/Utility/ClassAssembly.h"), # Wait, DirectX/Utility went to Utility!
    (r"DirectX/Utility/Input.h", r"Utility/Input.h"),
    (r"DirectX/Utility/Logger.h", r"Utility/Logger.h"),
    (r"DirectX/Utility/Random.h", r"Utility/Random.h"),
    (r"DirectX/Utility/Time.h", r"Utility/Time.h"),
    (r"DirectX/Utility/Utility.h", r"Utility/Utility.h"),
    (r"DirectX/Window/Window.h", r"Window/Window.h"),
    (r"DirectX/GDF/GDF.h", r"Graphics/GDF/GDF.h"),
    (r"ImGuiEditor/Editor/Editor.h", r"Editor/Editor.h"),
    (r"JobSystem/JobSystem.h", r"System/JobSystem/JobSystem.h")
]

# Correct mappings based on user's move:
# User moved:
# Source/Framework/Scene -> Source/Framework/Manager/Scene
# Source/Framework/Collision -> Source/Framework/Manager/Collision
# Source/Framework/Resource -> Source/Framework/Manager/Resource
# Source/Framework/Audio -> Source/Framework/Manager/Audio
# Source/Framework/Animation -> Source/Framework/Manager/Animation
# Source/Framework/DirectX/Utility -> Source/Framework/Utility
# Source/Framework/DirectX/Window -> Source/Framework/Window
# Source/Framework/DirectX/GDF -> Source/Graphics/GDF
# Source/Framework/ImGuiEditor/Editor -> Source/Framework/Editor
# Source/Framework/JobSystem -> Source/Framework/System/JobSystem

# Also GameManager went from Source/Framework/Manager/GameManager -> Source/Framework/GameManager

FORWARD_INCLUDE_MAP = [
    (r"Framework/Scene/", r"Framework/Manager/Scene/"),
    (r"Framework/Collision/", r"Framework/Manager/Collision/"),
    (r"Framework/Resource/", r"Framework/Manager/Resource/"),
    (r"Framework/Audio/", r"Framework/Manager/Audio/"),
    (r"Framework/Animation/", r"Framework/Manager/Animation/"),
    (r"Framework/DirectX/Utility/", r"Framework/Utility/"),
    (r"Framework/DirectX/Window/", r"Framework/Window/"),
    (r"Framework/DirectX/GDF/", r"Graphics/GDF/"),
    (r"Framework/ImGuiEditor/Editor/", r"Framework/Editor/"),
    (r"Framework/JobSystem/", r"Framework/System/JobSystem/"),
    (r"Framework/Manager/GameManager", r"Framework/GameManager")
]

# Wait, the includes are often relative (e.g., "../Collision/CollisionShape.h")
# I should just do a very permissive replacement or specific ones.

REPLACEMENTS = [
    (r"../Collision/CollisionShape.h", r"../Manager/Collision/CollisionShape.h"),
    (r"../../../Collision/CollisionShape.h", r"../../../Manager/Collision/CollisionShape.h"),
    (r"../Scene/Scene.h", r"../Manager/Scene/Scene.h"),
    (r"../../../Scene/Scene.h", r"../../../Manager/Scene/Scene.h"),
    (r"../Scene/SceneManager.h", r"../Manager/Scene/SceneManager.h"),
    (r"../../Collision/CollisionManager.h", r"../../Manager/Collision/CollisionManager.h"),
    (r"../../Collision/CollisionShape.h", r"../../Manager/Collision/CollisionShape.h"),
    (r"../../Collision/CollisionSolver.h", r"../../Manager/Collision/CollisionSolver.h"),
    (r"../../Resource/ResourceManager.h", r"../../Manager/Resource/ResourceManager.h"),
    (r"../../Resource/PrefabManager.h", r"../../Manager/Resource/PrefabManager.h"),
    (r"../../Audio/AudioManager.h", r"../../Manager/Audio/AudioManager.h"),
    (r"../../Animation/AnimationManager.h", r"../../Manager/Animation/AnimationManager.h"),
    (r"../DirectX/Utility/Input.h", r"../Utility/Input.h"),
    (r"../DirectX/Utility/Logger.h", r"../Utility/Logger.h"),
    (r"../DirectX/Utility/Random.h", r"../Utility/Random.h"),
    (r"../DirectX/Utility/Time.h", r"../Utility/Time.h"),
    (r"../DirectX/Utility/Utility.h", r"../Utility/Utility.h"),
    (r"../DirectX/Window/Window.h", r"../Window/Window.h"),
    (r"../DirectX/GDF/GDF.h", r"../../Graphics/GDF/GDF.h"),
    (r"../../DirectX/GDF/GDF.h", r"../../../Graphics/GDF/GDF.h"),
    (r"../ImGuiEditor/Editor/Editor.h", r"../Editor/Editor.h")
]

for dirpath, _, filenames in os.walk(r"c:\GitHub\DX12\Source"):
    if ".git" in dirpath: continue
    for f in filenames:
        if not f.endswith((".cpp", ".h")): continue
        filepath = os.path.join(dirpath, f)
        
        with open(filepath, "r", encoding="cp932") as file:
            text = file.read()
            
        modified = False
        for old, new in REPLACEMENTS:
            if old in text:
                text = text.replace(old, new)
                modified = True
                
        if modified:
            with open(filepath, "w", encoding="cp932") as file:
                file.write(text)

print("Forward include replacements applied.")
