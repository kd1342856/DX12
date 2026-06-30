# coding: utf-8
import os

for dirpath, _, filenames in os.walk(r"c:\GitHub\DX12\Source\Framework\Manager"):
    for f in filenames:
        if not f.endswith((".cpp", ".h")): continue
        filepath = os.path.join(dirpath, f)
        
        with open(filepath, "r", encoding="cp932") as file:
            text = file.read()
            
        modified = False
        if '../ECS/' in text:
            text = text.replace('../ECS/', '../../ECS/')
            modified = True
            
        if '../System/' in text:
            text = text.replace('../System/', '../../System/')
            modified = True
            
        if modified:
            with open(filepath, "w", encoding="cp932") as file:
                file.write(text)

print("Fixed Manager includes")
