# coding: utf-8
import os

headers = [
    r"c:\GitHub\DX12\Source\Framework\Editor\Editor.h",
    r"c:\GitHub\DX12\Source\Application\Application.h",
    r"c:\GitHub\DX12\Source\Framework\Manager\Scene\Scene.h",
    r"c:\GitHub\DX12\Source\Framework\Manager\Scene\SceneManager.h",
    r"c:\GitHub\DX12\Source\Framework\Manager\Collision\CollisionShape.h"
]

for filepath in headers:
    try:
        with open(filepath, "r", encoding="cp932") as f:
            lines = f.readlines()
        
        depth = 0
        for line in lines:
            if '//' in line:
                line = line[:line.find('//')]
            depth += line.count('{')
            depth -= line.count('}')
        print(f"{os.path.basename(filepath)} depth: {depth}")
    except Exception as e:
        print(f"Failed to check {filepath}: {e}")
