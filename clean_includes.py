import os, re

forbidden_exact = {
    '<windows.h>', '<iostream>', '<cassert>', '<wrl/client.h>',
    '<map>', '<unordered_map>', '<unordered_set>', '<string>', '<array>',
    '<vector>', '<stack>', '<list>', '<iterator>', '<queue>', '<algorithm>',
    '<memory>', '<random>', '<fstream>', '<sstream>', '<functional>',
    '<thread>', '<atomic>', '<mutex>', '<future>', '<filesystem>',
    '<chrono>', '<bitset>', '<set>', '<typeinfo>', '<math.h>',
    '<d3d12.h>', '<d3dcompiler.h>', '<dxgi1_6.h>', '<simplemath.h>',
    '<keyboard.h>', '<mouse.h>', '<directxtex.h>'
}

forbidden_basenames = {
    'input.h', 'time.h', 'logger.h', 'random.h', 'classassembly.h',
    'gdf.h', 'ecs.h', 'gamemanager.h', 'resourcemanager.h', 'editor.h',
    'nativescript.h', 'graphicsdevice.h', 'heap.h', 'rtvheap.h',
    'cbvsrvuavheap.h', 'dsvheap.h', 'mesh.h', 'buffer.h', 'cbufferallocator.h',
    'cbufferdata.h', 'texture.h', 'depthstencil.h', 'model.h', 'shadermanager.h',
    'entity.h', 'componentmanager.h', 'system.h', 'ecscoordinator.h',
    'transformdata.h', 'modelrenderdata.h', 'cameradata.h', 'shaderdata.h',
    'animationdata.h', 'spritedata.h', 'nativescriptdata.h', 'colliderdata.h',
    'rendersystem.h', 'imgui.h', 'imgui_impl_dx12.h', 'imgui_impl_win32.h'
}

skip_files = {'pch.h', 'graphics.h', 'ecs.h', 'system.h'}

def process_file(filepath):
    filename = os.path.basename(filepath).lower()
    if filename in skip_files:
        return
        
    try:
        with open(filepath, 'r', encoding='cp932') as f:
            lines = f.readlines()
    except Exception:
        with open(filepath, 'r', encoding='utf-8') as f:
            lines = f.readlines()
            
    new_lines = []
    changed = False
    
    for line in lines:
        stripped = line.strip().lower()
        if stripped.startswith('#include'):
            # extract the included part
            match = re.search(r'#include\s*([<\"][^>\"]+[>\"])', stripped)
            if match:
                inc = match.group(1)
                is_forbidden = False
                
                # Check exact match for standard libs
                if inc in forbidden_exact:
                    is_forbidden = True
                    
                # Check basename match for custom headers
                inc_inner = inc[1:-1] # remove < > or \" \"
                basename = os.path.basename(inc_inner)
                if basename in forbidden_basenames:
                    is_forbidden = True
                    
                if is_forbidden:
                    changed = True
                    continue
                    
        new_lines.append(line)
        
    if changed:
        with open(filepath, 'w', encoding='cp932', errors='replace') as f:
            f.writelines(new_lines)

for root, dirs, files in os.walk(r'c:\GitHub\DX12\Source'):
    for file in files:
        if file.endswith('.h') or file.endswith('.cpp'):
            process_file(os.path.join(root, file))

print('Done removing forbidden includes.')
