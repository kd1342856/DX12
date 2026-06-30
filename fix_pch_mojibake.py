# coding: utf-8
import subprocess

def restore_and_fix_pch():
    filepath = r"c:\GitHub\DX12\Source\Pch.h"
    res = subprocess.run(["git", "show", "HEAD:Source/Pch.h"], capture_output=True)
    raw = res.stdout
    try:
        text = raw.decode("utf-8-sig")
    except:
        text = raw.decode("cp932", errors="replace")
        
    text = text.replace("\r\n", "\n")
    
    INCLUDE_REPLACE_MAP = [
        (r"Framework/DirectX/Utility/Input.h", r"Framework/Utility/Input.h"),
        (r"Framework/DirectX/Utility/Time.h", r"Framework/Utility/Time.h"),
        (r"Framework/DirectX/Utility/Logger.h", r"Framework/Utility/Logger.h"),
        (r"Framework/DirectX/Utility/Random.h", r"Framework/Utility/Random.h"),
        (r"Framework/DirectX/GDF/GDF.h", r"Graphics/GDF/GDF.h"),
        (r"Framework/Manager/GameManager.h", r"Framework/GameManager.h"),
        (r"Framework/Manager/ResourceManager.h", r"Framework/Manager/Resource/ResourceManager.h"),
        (r"Framework/Manager/CollisionManager.h", r"Framework/Collision/CollisionManager.h"),
        (r"Framework/ImGuiEditor/Editor/Editor.h", r"Framework/Editor/Editor.h"),
    ]
    
    for old_str, new_str in INCLUDE_REPLACE_MAP:
        text = text.replace(old_str, new_str)
        
    with open(filepath, "w", encoding="cp932", errors="replace") as f:
        f.write(text)

restore_and_fix_pch()
print("Fixed Pch.h")
