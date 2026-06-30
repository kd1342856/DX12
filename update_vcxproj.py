import os

def add_to_vcxproj():
    path = r'c:\GitHub\DX12\DX12Framework.vcxproj'
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()

    target_cpp = '<ClCompile Include="Source\\Framework\\Manager\\GameManager.cpp" />'
    if target_cpp in content and 'NavMeshManager.cpp' not in content:
        content = content.replace(target_cpp, target_cpp + '\n    <ClCompile Include="Source\\Framework\\Manager\\NavMeshManager.cpp" />')

    target_h = '<ClInclude Include="Source\\Framework\\Manager\\GameManager.h" />'
    if target_h in content and 'NavMeshManager.h' not in content:
        content = content.replace(target_h, target_h + '\n    <ClInclude Include="Source\\Framework\\Manager\\NavMeshManager.h" />')

    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)

if __name__ == '__main__':
    add_to_vcxproj()
