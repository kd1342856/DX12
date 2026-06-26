import os

files_to_convert = [
    r'c:\GitHub\DX12\Source\Framework\ECS\ComponentManager.h',
    r'c:\GitHub\DX12\Source\Framework\ECS\Entity\Entity.h',
    r'c:\GitHub\DX12\Source\Framework\Manager\Scene.h',
    r'c:\GitHub\DX12\Source\Application\Scene\GameScene\GameScene.cpp',
    r'c:\GitHub\DX12\Source\Application\Scene\TitleScene\TitleScene.cpp',
    r'c:\GitHub\DX12\Source\Application\Object\Script\Player\Player.h',
    r'c:\GitHub\DX12\Source\Application\Object\Script\Ghost\GhostAI.h',
]

for filepath in files_to_convert:
    try:
        # Read as utf-8 first
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
    except UnicodeDecodeError:
        try:
            # Maybe it's already cp932, so we just read it to ensure it's loaded
            with open(filepath, 'r', encoding='cp932') as f:
                content = f.read()
        except:
            continue
    
    # Save as cp932
    try:
        with open(filepath, 'w', encoding='cp932') as f:
            f.write(content)
        print(f"Converted {filepath} to cp932")
    except Exception as e:
        print(f"Failed to convert {filepath}: {e}")

