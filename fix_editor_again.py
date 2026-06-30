# coding: utf-8
import subprocess
import os

filepath = r"c:\GitHub\DX12\Source\Framework\Editor\Editor.cpp"
res = subprocess.run(["git", "show", "HEAD:Source/Framework/ImGuiEditor/Editor/Editor.cpp"], capture_output=True)
raw = res.stdout
try: text = raw.decode("utf-8-sig")
except: text = raw.decode("cp932", errors="replace")

text = text.replace('ImGuiEditor/Editor/Editor.h', 'Editor/Editor.h')
text = text.replace('../../../Manager/Scene/Scene.h', '../../Manager/Scene/Scene.h') # Wait, wait...
text = text.replace('../../../Manager/Scene/SceneManager.h', '../../Manager/Scene/SceneManager.h')
text = text.replace('../Manager/Collision/CollisionShape.h', '../../Manager/Collision/CollisionShape.h')

# Actually, my fix_all_includes script can fix it! Let me just save it and run fix_all_includes.py again.
with open(filepath, "w", encoding="cp932") as f:
    f.write(text)
print("Restored Editor.cpp")
