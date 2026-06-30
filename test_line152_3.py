# coding: utf-8
import subprocess

filepath = r"c:\GitHub\DX12\Source\Framework\Editor\Editor.cpp"
res = subprocess.run(["git", "show", "HEAD:Source/Framework/ImGuiEditor/Editor/Editor.cpp"], capture_output=True)
text = res.stdout.decode("cp932", errors="replace")

lines = text.split("\n")
lines[151] = "// " + lines[151] # line 152

with open(filepath, "w", encoding="cp932") as f:
    f.write("\n".join(lines))
    
subprocess.run(["py", r"c:\GitHub\DX12\fix_all_includes.py"], capture_output=True)

cl_path = r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.42.34433\bin\HostX86\x64\cl.exe"
args = [
    cl_path, "/c", "/IC:\GitHub\DX12\packages\directxtex_desktop_2019.2025.10.28.1\build\native\..\..\include",
    "/IC:\GitHub\DX12\packages\directxtk12_desktop_2019.2025.10.28.1\build\native\..\..\include",
    "/IC:\GitHub\DX12\Library\ImGui", "/IC:\GitHub\DX12\Source", "/IC:\GitHub\DX12\Source\Framework",
    "/IC:\GitHub\DX12\Library\nlohmann", "/IC:\GitHub\DX12\Library\assimp\include",
    "/IC:\GitHub\DX12\Library\recastnavigation\include",
    "/YuPch.h", "/FpDX12Framework\\x64\\Debug\\DX12Framework.pch",
    "/FIPch.h", "/D_DEBUG", "/MDd", "/std:c++17", filepath
]
res = subprocess.run(args, capture_output=True, cwd=r"C:\GitHub\DX12")
output = res.stdout.decode('cp932', errors='replace')
print("Output:", output)
