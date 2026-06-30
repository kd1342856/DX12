# coding: utf-8
import os
import subprocess

files_to_restore = [
    (r"c:\GitHub\DX12\Source\Application\Application.cpp", "Source/Application/Application.cpp"),
    (r"c:\GitHub\DX12\Source\Application\Application.h", "Source/Application/Application.h"),
    (r"c:\GitHub\DX12\Source\Application\Scene\GameScene\GameScene.cpp", "Source/Application/Scene/GameScene/GameScene.cpp"),
    (r"c:\GitHub\DX12\Source\Application\Scene\TitleScene\TitleScene.cpp", "Source/Application/Scene/TitleScene/TitleScene.cpp"),
    (r"c:\GitHub\DX12\Source\Application\Script\Character\Player\Player.cpp", "Source/Application/Object/Script/Player/Player.cpp"),
    (r"c:\GitHub\DX12\Source\Framework\Editor\Editor.cpp", "Source/Framework/ImGuiEditor/Editor/Editor.cpp"),
    (r"c:\GitHub\DX12\Source\Framework\Manager\Collision\CollisionManager.cpp", "Source/Framework/Manager/CollisionManager.cpp"),
    (r"c:\GitHub\DX12\Source\Framework\Manager\Scene\SceneManager.cpp", "Source/Framework/Manager/SceneManager.cpp")
]

def restore_from_git(current_path, git_path):
    res = subprocess.run(["git", "show", f"HEAD:{git_path}"], capture_output=True)
    if res.returncode != 0:
        print(f"Failed to get {git_path}")
        return
        
    raw = res.stdout
    try:
        text = raw.decode("utf-8-sig")
    except:
        try:
            text = raw.decode("cp932")
        except:
            text = raw.decode("utf-8", errors="replace")
            
    with open(current_path, "w", encoding="cp932", errors="replace") as f:
        f.write(text.replace("\r\n", "\n"))
        
for curr, orig in files_to_restore:
    restore_from_git(curr, orig)

print("Restored original files.")
