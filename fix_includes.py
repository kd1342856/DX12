import os

def fix_includes(filepath):
    with open(filepath, 'r', encoding='cp932') as f:
        content = f.read()

    # Remove the bad line at the end if it exists
    bad_line = '#include "../../../Framework/ECS/CompSystem/SpriteRenderSystem/SpriteRenderSystem.h"\n'
    if content.endswith(bad_line):
        content = content[:-len(bad_line)]

    # Add correct includes at the top
    target = '#include "Pch.h"\n'
    if target in content and "SpriteRenderSystem.h" not in content[:content.find(target)+100]:
        rep = target + '''#include "../../../Framework/ECS/CompSystem/SpriteRenderSystem/SpriteRenderSystem.h"
#include "../../../Framework/ECS/CompSystem/Systems/RenderSystem.h"
#include "../../../Framework/ECS/CompSystem/Systems/TransformSystem.h"
#include "../../../Framework/ECS/CompSystem/Systems/CameraSystem.h"
#include "../../../Framework/ECS/CompSystem/Systems/AnimationSystem.h"
'''
        content = content.replace(target, rep, 1)

    with open(filepath, 'w', encoding='cp932') as f:
        f.write(content)

if __name__ == '__main__':
    fix_includes(r'c:\GitHub\DX12\Source\Application\Scene\GameScene\GameScene.cpp')
    fix_includes(r'c:\GitHub\DX12\Source\Application\Scene\TitleScene\TitleScene.cpp')
